// CItemOptionBEx.h: the server's ItemOptionBEx.txt table, received at login, and the
// item tooltip lines built from it.
//
// The client keeps no copy of that file. The server sends the whole table on every
// login and after every Reload Item (C2:D3:B6), so editing the file on the server is
// all it takes - no client patch, no data.zip re-upload.
//
// The matching below mirrors the server's CItemOption::InsertOptionBEX and
// CheckItemSlotEx exactly, including their quirks. The tooltip is only worth having
// if it says what the server actually does.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "WSclient.h"

#pragma pack(push,1)

// Mirrored from the GameServer's ItemOptionBEx.cpp. Explicit widths only: no `long`
// (8 bytes on Android) and no plain `char` for anything that can be -1 (unsigned on
// Android). Both have corrupted packets in this codebase before.
struct PMSG_ITEM_OPTION_BEX_ROW_RECV
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

struct PMSG_ITEM_OPTION_BEX_RECV
{
	PSWMSG_HEAD header;
	WORD  total;
	WORD  start;		// 0 = a new table; the client drops whatever it had
	BYTE  count;
};

#pragma pack(pop)

static_assert(sizeof(PMSG_ITEM_OPTION_BEX_ROW_RECV) == 18, "must match the GameServer's PMSG_ITEM_OPTION_BEX_ROW_SEND");
static_assert(sizeof(PMSG_ITEM_OPTION_BEX_RECV) == 10, "must match the GameServer's PMSG_ITEM_OPTION_BEX_SEND");

class CItemOptionBEx
{
public:
	CItemOptionBEx();

	// C2:D3:B6. One call per chunk; the table arrives in pieces of 200 rows.
	void Recv(BYTE* lpMsg);

	// Appends this item's BEx lines to the tooltip, the same shape as
	// CSItemOption::BuildSetPartsList: takes TextNum, returns the new TextNum.
	int AttachToolTip(const ITEM* ip, int TextNum);

private:
	// InsertOptionBEX's test, with the level passed in rather than read off the item,
	// so the tooltip can ask "what would this give at +N" for levels it isn't at.
	bool RowMatches(const PMSG_ITEM_OPTION_BEX_ROW_RECV& row, int type, int level, int excBits) const;
	bool IsSetActive(const PMSG_ITEM_OPTION_BEX_ROW_RECV& row) const;

	// Row indices that apply at `level`.
	std::vector<size_t> MatchSingleRows(const ITEM* ip, int level) const;
	std::vector<size_t> MatchSetRows(int hoverIdx, int level) const;

	int RenderSingleBlock(int TextNum, const std::vector<size_t>& rows, const char* header, int headerColor, int lineColor) const;
	int RenderSetBlock(int TextNum, const std::vector<size_t>& rows, int level, bool preview) const;

	std::vector<PMSG_ITEM_OPTION_BEX_ROW_RECV> m_Rows;
};

extern CItemOptionBEx g_ItemOptionBEx;
