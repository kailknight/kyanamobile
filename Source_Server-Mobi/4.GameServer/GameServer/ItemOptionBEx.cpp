// ItemOptionBEx.cpp: the ItemOptionBEx.txt loader, and the client sync that lets the
// item tooltip show those options.
//
// These are CItemOption members (declared in ItemOption.h) but live in their own file
// on purpose: ItemOption.cpp is not valid UTF-8 - one line is in a legacy code page -
// and editing it risks re-encoding the whole file. The original LoadBEX (the .xml
// reader) and InsertOptionBEX (which applies the options) are untouched there; this
// file only fills the same m_ItemOptionInfoBEX table from a different source, and
// ships that table to the client.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ItemOption.h"
#include "ItemManager.h"
#include "MemScript.h"
#include "Protocol.h"
#include "User.h"
#include "Util.h"

// ---- Wire: C2:D3:B6, mirrored in the client's CItemOptionBEx.h ------------------
//
// Every field has an explicit width. Nothing here is `long` (4 bytes on PC, 8 on
// Android) and nothing that can be -1 is a plain `char` (unsigned on Android, so -1
// arrives as 255). Both of those have already corrupted packets in this codebase.
#pragma pack(push,1)

struct PMSG_ITEM_OPTION_BEX_ROW_SEND
{
	WORD  ItemMinIndex;
	WORD  ItemMaxIndex;
	short ItemLevelMin;
	short ItemLevelMax;
	short ItemSet;
	short ItemExc;
	WORD  OptionIndex;
	int   OptionValue;
};

struct PMSG_ITEM_OPTION_BEX_SEND
{
	PSWMSG_HEAD header;
	WORD  total;				// rows in the whole table
	WORD  start;				// index of this packet's first row; 0 = start of a new table
	BYTE  count;				// rows in this packet
};

#pragma pack(pop)

static_assert(sizeof(PMSG_ITEM_OPTION_BEX_ROW_SEND) == 18, "PMSG_ITEM_OPTION_BEX_ROW_SEND must stay 18 bytes - the client reads it at that stride");
static_assert(sizeof(PMSG_ITEM_OPTION_BEX_SEND) == 10, "PMSG_ITEM_OPTION_BEX_SEND must stay 10 bytes");

// Rows per packet.
//
// NOT a free choice. The client receives into a fixed 8192-byte buffer (MAX_RECVBUF
// in wsctlc.h) and a packet bigger than that can never be completely received - the
// buffer fills, recv() is skipped, and the connection stalls. The whole table is
// ~1400 rows x 18 bytes = ~25 KB, so a single packet would hang every client at
// login. 200 rows is 3610 bytes, well under the limit even with other traffic
// sharing the buffer.
#define ITEM_OPTION_BEX_ROWS_PER_PACKET 200

static_assert(sizeof(PMSG_ITEM_OPTION_BEX_SEND) + (sizeof(PMSG_ITEM_OPTION_BEX_ROW_SEND) * ITEM_OPTION_BEX_ROWS_PER_PACKET) <= 4096, "a BEx packet must stay far below the client's 8192-byte receive buffer");

// Options whose effect is (level / Value). A 0 there is an integer divide by zero
// the moment someone equips the item, which takes the whole GameServer down.
static bool IsBExOptionDividedByValue(int option)
{
	switch(option)
	{
		case ITEM_OPTION_ADD_PHYSI_DAMAGE_BY_LEVEL:
		case ITEM_OPTION_ADD_MAGIC_DAMAGE_BY_LEVEL:
		case ITEM_OPTION_ADD_CURSE_DAMAGE_BY_LEVEL:
		case ITEM_OPTION_ADD_HP_BY_LEVEL:
		case ITEM_OPTION_ADD_MP_BY_LEVEL:
		case ITEM_OPTION_ADD_DAMAGE_BY_LEVEL:
		case ITEM_OPTION_ADD_DEFENSE_BY_LEVEL:
			return 1;
	}

	return 0;
}

bool CItemOption::LoadBExTxt(char* path) // OK
{
	// Checked before CMemScript ever sees the path. SetBuffer on a missing file raises
	// a modal ErrorMessageBox, which at startup would leave the server sitting on a
	// dialog nobody is watching. A missing .txt is not an error: the caller falls back
	// to ItemOptionBEx.xml, so the two can be swapped on a live server in either order.
	if(GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
	{
		return 0;
	}

	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return 0;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return 0;
	}

	this->m_ItemOptionInfoBEX.clear();

	int line = 0;
	int dropped = 0;

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if(strcmp("end",lpMemScript->GetString()) == 0)
			{
				break;
			}

			line++;

			ITEM_OPTION_BEX info;

			info.ItemSet = lpMemScript->GetNumber();

			int cat = lpMemScript->GetAsNumber();

			int minIdx = lpMemScript->GetAsNumber();

			int maxIdx = lpMemScript->GetAsNumber();

			info.ItemLevelMin = lpMemScript->GetAsNumber();

			info.ItemLevelMax = lpMemScript->GetAsNumber();

			info.ItemExc = lpMemScript->GetAsNumber();

			info.OptionIndex = lpMemScript->GetAsNumber();

			info.OptionValue = lpMemScript->GetAsNumber();

			// A bad row is dropped with a red line rather than loaded. Every one of these
			// would otherwise either never match anything - silently - or crash.
			if(cat < 0 || cat >= MAX_ITEM_SECTION || minIdx < 0 || maxIdx < minIdx || maxIdx >= MAX_ITEM_TYPE)
			{
				LogAdd(LOG_RED,"[ItemOptionBEx] row %d: item %d %d..%d is out of range - dropped",line,cat,minIdx,maxIdx);
				dropped++;
				continue;
			}

			if(info.ItemSet != -1 && info.ItemSet != 1)
			{
				LogAdd(LOG_RED,"[ItemOptionBEx] row %d: Set must be -1 or 1, not %d - dropped",line,info.ItemSet);
				dropped++;
				continue;
			}

			if(IsBExOptionDividedByValue(info.OptionIndex) != 0 && info.OptionValue <= 0)
			{
				LogAdd(LOG_RED,"[ItemOptionBEx] row %d: option %d divides by its value, so %d would crash the server - dropped",line,info.OptionIndex,info.OptionValue);
				dropped++;
				continue;
			}

			// Loaded, but called out. CheckItemSlotEx (ItemOption.cpp) only counts a set
			// piece when Exc is 1 (and the piece is excellent) or -1 - an Exc of 0 matches
			// neither branch, so no piece is ever counted and the bonus can never switch
			// on. Kept rather than dropped so the file still round-trips; the red line is
			// what stops someone losing an evening wondering why a set does nothing.
			if(info.ItemSet == 1 && info.ItemExc == 0)
			{
				LogAdd(LOG_RED,"[ItemOptionBEx] row %d: a set row with Exc 0 can never activate - use 1 (excellent pieces) or -1 (any pieces)",line);
			}

			info.ItemMinIndex = GET_ITEM(cat,minIdx);

			info.ItemMaxIndex = GET_ITEM(cat,maxIdx);

			this->m_ItemOptionInfoBEX.push_back(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;

	LogAdd(((dropped == 0) ? LOG_BLUE : LOG_RED),"[ItemOptionBEx] %d rows loaded from ItemOptionBEx.txt, %d dropped",(int)this->m_ItemOptionInfoBEX.size(),dropped);

	return 1;
}

void CItemOption::GCItemOptionBExSend(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	int total = (int)this->m_ItemOptionInfoBEX.size();

	// The count travels in a WORD. Not a real limit - the live table is ~1400 rows -
	// but a table past it must be cut short visibly, not wrapped to a small number.
	if(total > 0xFFFF)
	{
		LogAdd(LOG_RED,"[ItemOptionBEx] %d rows is more than the client sync can carry - only the first 65535 are sent",total);
		total = 0xFFFF;
	}

	BYTE buffer[sizeof(PMSG_ITEM_OPTION_BEX_SEND) + (sizeof(PMSG_ITEM_OPTION_BEX_ROW_SEND) * ITEM_OPTION_BEX_ROWS_PER_PACKET)];

	int start = 0;

	// do/while, so an EMPTY table still sends one packet. start = 0 is what tells the
	// client to discard what it had - without it, emptying the file would leave every
	// client showing the old options until it restarted.
	do
	{
		int count = total - start;

		if(count > ITEM_OPTION_BEX_ROWS_PER_PACKET)
		{
			count = ITEM_OPTION_BEX_ROWS_PER_PACKET;
		}

		PMSG_ITEM_OPTION_BEX_SEND pMsg;

		int size = sizeof(pMsg);

		for(int n=0;n < count;n++)
		{
			ITEM_OPTION_BEX* lpInfo = &this->m_ItemOptionInfoBEX[start+n];

			PMSG_ITEM_OPTION_BEX_ROW_SEND row;

			row.ItemMinIndex = (WORD)lpInfo->ItemMinIndex;
			row.ItemMaxIndex = (WORD)lpInfo->ItemMaxIndex;
			row.ItemLevelMin = (short)lpInfo->ItemLevelMin;
			row.ItemLevelMax = (short)lpInfo->ItemLevelMax;
			row.ItemSet = (short)lpInfo->ItemSet;
			row.ItemExc = (short)lpInfo->ItemExc;
			row.OptionIndex = (WORD)lpInfo->OptionIndex;
			row.OptionValue = lpInfo->OptionValue;

			memcpy((buffer + size),&row,sizeof(row));

			size += sizeof(row);
		}

		pMsg.total = (WORD)total;
		pMsg.start = (WORD)start;
		pMsg.count = (BYTE)count;

		pMsg.header.set(0xD3,0xB6,size);

		memcpy(buffer,&pMsg,sizeof(pMsg));

		DataSend(aIndex,buffer,size);

		start += count;
	}
	while(start < total);
}

void CItemOption::GCItemOptionBExSendToAll() // OK
{
	// Called at the end of every item reload, so an edited ItemOptionBEx.txt reaches
	// players who are already online, not just the ones who log in afterwards. At
	// startup nobody is connected yet and this does nothing.
	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) != 0)
		{
			this->GCItemOptionBExSend(n);
		}
	}
}
