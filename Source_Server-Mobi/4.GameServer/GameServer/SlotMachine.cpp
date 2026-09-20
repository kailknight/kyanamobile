// SlotMachine.cpp: implementation of the CSlotMachine class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SlotMachine.h"
#include "CriticalSection.h"
#include "DSProtocol.h"
#include "GameMain.h"
#include "ItemManager.h"
#include "Log.h"
#include "Map.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "Path.h"
#include "RandomManager.h"
#include "ServerDisplayer.h"
#include "SocketManager.h"
#include "Util.h"
#if(JEWELBANKVER2)
#include "BCustomItemBank.h"
#endif

CSlotMachine gSlotMachine;

namespace
{
	// GetLargeRand()'s std::mt19937 is a plain global shared between the single
	// packet-handler thread and the monster-AI timer pool, which hold DIFFERENT
	// locks. Spins cannot race each other, but they do race AI. This makes the 15
	// draws of one spin come from an uninterrupted stretch of the stream.
	CCriticalSection g_SlotRandCritical;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSlotMachine::CSlotMachine() // OK
{
	this->m_Enable = 0;
	this->m_NpcClass = -1;
	this->m_NpcMap = -1;
	this->m_NpcX = -1;
	this->m_NpcY = -1;
	this->m_NpcDir = 0;
	this->m_Lines = 0;
	this->m_FreeSpins = 0;
	this->m_MaxRetrigger = 0;
	this->m_MultiCap = 10;
	this->m_MaxPayoutCredits = 0;
	this->m_MaxBet = 1;
	this->m_SymbolCount = 0;
	memset(&this->m_Bonus,0,sizeof(this->m_Bonus));
	this->m_LineCount = 0;
	this->m_StakeCount = 0;
	memset(this->m_ReelWeight,0,sizeof(this->m_ReelWeight));
	memset(this->m_Pay,0,sizeof(this->m_Pay));
	memset(this->m_DefaultMessage,0,sizeof(this->m_DefaultMessage));
}

CSlotMachine::~CSlotMachine() // OK
{

}

char* CSlotMachine::GetMessage(int index) // OK
{
	std::map<int,SLOT_MESSAGE_INFO>::iterator it = this->m_MessageInfo.find(index);

	if(it != this->m_MessageInfo.end())
	{
		return it->second.Message;
	}

	// A member, not a local: the caller formats through the pointer this returns,
	// so a stack buffer would dangle.
	wsprintf(this->m_DefaultMessage,"[SlotMachine] message %d missing",index);

	return this->m_DefaultMessage;
}

//////////////////////////////////////////////////////////////////////
// Config
//////////////////////////////////////////////////////////////////////

void CSlotMachine::Load(char* path) // OK
{
	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	// Reset everything first. A reload that only overwrote what the new file
	// happens to declare would leave stale symbols and paylines behind.
	this->m_Enable = 0;
	this->m_SymbolCount = 0;
	memset(&this->m_Bonus,0,sizeof(this->m_Bonus));
	this->m_LineCount = 0;
	this->m_StakeCount = 0;
	memset(this->m_ReelWeight,0,sizeof(this->m_ReelWeight));
	memset(this->m_Pay,0,sizeof(this->m_Pay));
	memset(this->m_Symbol,0,sizeof(this->m_Symbol));
	memset(this->m_Line,0,sizeof(this->m_Line));
	memset(this->m_Stake,0,sizeof(this->m_Stake));
	this->m_MessageInfo.clear();

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			int section = lpMemScript->GetNumber();

			while(true)
			{
				if(section == 0)			// machine + NPC
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					this->m_Enable = lpMemScript->GetNumber();
					this->m_NpcClass = lpMemScript->GetAsNumber();
					this->m_NpcMap = lpMemScript->GetAsNumber();
					this->m_NpcX = lpMemScript->GetAsNumber();
					this->m_NpcY = lpMemScript->GetAsNumber();
					this->m_NpcDir = lpMemScript->GetAsNumber();
					this->m_Lines = lpMemScript->GetAsNumber();
					this->m_FreeSpins = lpMemScript->GetAsNumber();
					this->m_MaxRetrigger = lpMemScript->GetAsNumber();
					this->m_MultiCap = lpMemScript->GetAsNumber();
					this->m_MaxPayoutCredits = lpMemScript->GetAsNumber();
					this->m_MaxBet = lpMemScript->GetAsNumber();
				}
				else if(section == 1)		// symbols
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					int id = lpMemScript->GetNumber();

					SLOT_SYMBOL_INFO info;
					memset(&info,0,sizeof(info));

					info.Kind = lpMemScript->GetAsNumber();

					int cat = lpMemScript->GetAsNumber();
					int idx = lpMemScript->GetAsNumber();

					info.Level = lpMemScript->GetAsNumber();
					info.Value = lpMemScript->GetAsNumber();
					strcpy_s(info.Name,lpMemScript->GetAsString());

					info.ItemIndex = GET_ITEM(cat,idx);

					// Refused rather than clamped. A symbol id past the cap would
					// otherwise be silently dropped and every reel weight after it
					// would line up against the wrong symbol.
					if(id < 0 || id >= SLOT_MAX_SYMBOL)
					{
						LogAdd(LOG_RED,"[SlotMachine] symbol id %d out of range (0-%d) - ignored",id,SLOT_MAX_SYMBOL-1);
						continue;
					}

					this->m_Symbol[id] = info;

					if((id + 1) > this->m_SymbolCount)
					{
						this->m_SymbolCount = id + 1;
					}
				}
				else if(section == 2)		// reel strips
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					int reel = lpMemScript->GetNumber();

					// Read every weight regardless, so a bad reel index does not
					// leave the tokeniser mid-row and shift the whole rest of the
					// file by one value.
					int weight[SLOT_MAX_SYMBOL] = {0};

					for(int n=0;n < this->m_SymbolCount;n++)
					{
						weight[n] = lpMemScript->GetAsNumber();
					}

					if(reel < 0 || reel >= SLOT_REELS)
					{
						LogAdd(LOG_RED,"[SlotMachine] reel %d out of range (0-%d) - ignored",reel,SLOT_REELS-1);
						continue;
					}

					memcpy(this->m_ReelWeight[reel],weight,sizeof(weight));
				}
				else if(section == 3)		// paylines
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					int line = lpMemScript->GetNumber();

					SLOT_LINE_INFO info;
					memset(&info,0,sizeof(info));

					bool bad = false;

					for(int n=0;n < SLOT_REELS;n++)
					{
						info.Row[n] = lpMemScript->GetAsNumber();

						if(info.Row[n] < 0 || info.Row[n] >= SLOT_ROWS)
						{
							bad = true;
						}
					}

					if(bad)
					{
						LogAdd(LOG_RED,"[SlotMachine] payline %d has a row index outside 0-%d - ignored",line,SLOT_ROWS-1);
						continue;
					}

					if(line < 0 || line >= SLOT_MAX_LINE)
					{
						LogAdd(LOG_RED,"[SlotMachine] payline %d out of range (0-%d) - ignored",line,SLOT_MAX_LINE-1);
						continue;
					}

					this->m_Line[line] = info;

					if((line + 1) > this->m_LineCount)
					{
						this->m_LineCount = line + 1;
					}
				}
				else if(section == 4)		// paytable
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					int id = lpMemScript->GetNumber();

					SLOT_PAY_INFO info;
					memset(&info,0,sizeof(info));

					info.Pay[0] = lpMemScript->GetAsNumber();
					info.Pay[1] = lpMemScript->GetAsNumber();
					info.Pay[2] = lpMemScript->GetAsNumber();

					if(id < 0 || id >= SLOT_MAX_SYMBOL)
					{
						LogAdd(LOG_RED,"[SlotMachine] paytable symbol %d out of range - ignored",id);
						continue;
					}

					this->m_Pay[id] = info;
				}
				else if(section == 5)		// accepted stakes
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					SLOT_STAKE_INFO info;
					memset(&info,0,sizeof(info));

					info.Kind = lpMemScript->GetNumber();

					int cat = lpMemScript->GetAsNumber();
					int idx = lpMemScript->GetAsNumber();

					info.Level = lpMemScript->GetAsNumber();
					info.Amount = lpMemScript->GetAsNumber();
					info.Credits = lpMemScript->GetAsNumber();
					strcpy_s(info.Name,lpMemScript->GetAsString());

					info.ItemIndex = GET_ITEM(cat,idx);

					if(info.Kind < SLOT_STAKE_ITEM || info.Kind > SLOT_STAKE_GOBLIN)
					{
						LogAdd(LOG_RED,"[SlotMachine] stake '%s' has Kind %d - must be 0 item, 1 WCoinC, 2 WCoinP, 3 GoblinPoint, ignored",info.Name,info.Kind);
						continue;
					}

					// An Amount of 0 would stake nothing and pay nothing, which reads
					// in game as a machine that eats presses.
					if(info.Amount <= 0)
					{
						LogAdd(LOG_RED,"[SlotMachine] stake '%s' has Amount %d - must be > 0, ignored",info.Name,info.Amount);
						continue;
					}

					if(this->m_StakeCount >= SLOT_MAX_STAKE)
					{
						LogAdd(LOG_RED,"[SlotMachine] more than %d stake items - '%s' ignored",SLOT_MAX_STAKE,info.Name);
						continue;
					}

					// Credits of 0 would make the payout cap divide by zero and
					// would make the audit log meaningless.
					if(info.Credits <= 0)
					{
						LogAdd(LOG_RED,"[SlotMachine] stake '%s' has Credits %d - must be > 0, ignored",info.Name,info.Credits);
						continue;
					}

					this->m_Stake[this->m_StakeCount++] = info;
				}
				else if(section == 7)		// scatter bonus, pity, beginner luck
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					int cat = lpMemScript->GetNumber();
					int idx = lpMemScript->GetAsNumber();

					this->m_Bonus.ScatterLevel = lpMemScript->GetAsNumber();
					this->m_Bonus.ScatterCount = lpMemScript->GetAsNumber();

					this->m_Bonus.ScatterItemIndex = ((cat < 0 || idx < 0)?-1:GET_ITEM(cat,idx));

					this->m_Bonus.LossStreak = lpMemScript->GetAsNumber();
					this->m_Bonus.LossStreakPerDay = lpMemScript->GetAsNumber();

					this->m_Bonus.PityWeightMin = lpMemScript->GetAsNumber();
					this->m_Bonus.PityWeightX2 = lpMemScript->GetAsNumber();
					this->m_Bonus.PityWeightX4 = lpMemScript->GetAsNumber();
					this->m_Bonus.PityWeightScatter = lpMemScript->GetAsNumber();

					this->m_Bonus.BeginnerSpins = lpMemScript->GetAsNumber();
					this->m_Bonus.BeginnerPercent = lpMemScript->GetAsNumber();
				}
				else if(section == 8)		// win sizes worth announcing
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					const int threshold = lpMemScript->GetNumber();

					if(this->m_Bonus.AnnounceCount >= SLOT_MAX_ANNOUNCE)
					{
						LogAdd(LOG_RED,"[SlotMachine] more than %d announce thresholds - %d ignored",SLOT_MAX_ANNOUNCE,threshold);
						continue;
					}

					if(threshold <= 0)
					{
						LogAdd(LOG_RED,"[SlotMachine] announce threshold %d must be > 0 - ignored",threshold);
						continue;
					}

					this->m_Bonus.Announce[this->m_Bonus.AnnounceCount++] = threshold;
				}
				else if(section == 6)		// messages
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					SLOT_MESSAGE_INFO info;

					info.Index = lpMemScript->GetNumber();

					strcpy_s(info.Message,lpMemScript->GetAsString());

					this->m_MessageInfo.insert(std::pair<int,SLOT_MESSAGE_INFO>(info.Index,info));
				}
				else
				{
					break;
				}
			}
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;

	// ---- Sanity, after the whole file is in ------------------------------
	// Each of these would otherwise surface as a machine that takes stakes and
	// never pays, which is the worst possible way for a gambling feature to fail.

	if(this->m_Lines <= 0 || this->m_Lines > this->m_LineCount)
	{
		if(this->m_Enable != 0)
		{
			LogAdd(LOG_RED,"[SlotMachine] Lines is %d but only %d paylines loaded - disabled",this->m_Lines,this->m_LineCount);
		}

		this->m_Enable = 0;
	}

	if(this->m_SymbolCount <= 0 || this->m_StakeCount <= 0)
	{
		if(this->m_Enable != 0)
		{
			LogAdd(LOG_RED,"[SlotMachine] %d symbols and %d stake items loaded - both must be non-zero, disabled",this->m_SymbolCount,this->m_StakeCount);
		}

		this->m_Enable = 0;
	}

	if(this->m_MultiCap < 1)
	{
		this->m_MultiCap = 1;
	}

	// Clamped rather than refused: a MaxBet of 0 is much more likely to be an admin
	// leaving the column off an older config than an intent to forbid betting, and
	// treating it as 1 keeps that machine playable.
	if(this->m_MaxBet < 1)
	{
		this->m_MaxBet = 1;
	}

	if(this->m_MaxBet > SLOT_BET_LIMIT)
	{
		LogAdd(LOG_RED,"[SlotMachine] MaxBet %d exceeds the %d limit - clamped",this->m_MaxBet,SLOT_BET_LIMIT);
		this->m_MaxBet = SLOT_BET_LIMIT;
	}

	// A reel whose weights are all zero can never produce a symbol, so the grid
	// would be a column of symbol 0 forever.
	for(int reel=0;reel < SLOT_REELS && this->m_Enable != 0;reel++)
	{
		int total = 0;

		for(int n=0;n < this->m_SymbolCount;n++)
		{
			total += this->m_ReelWeight[reel][n];
		}

		if(total <= 0)
		{
			LogAdd(LOG_RED,"[SlotMachine] reel %d has no non-zero weights - disabled",reel);
			this->m_Enable = 0;
		}
	}

	if(this->m_Enable != 0 && (this->m_NpcClass < 0 || this->m_NpcMap < 0))
	{
		LogAdd(LOG_RED,"[SlotMachine] enabled but Npc/Map is unset - disabled");
		this->m_Enable = 0;
	}

	// CustomSlotMachine (CustomConfig.ini): master switch, separate from the
	// Enable column in section 0 of CustomSlotMachine.txt. Either one being 0
	// turns the machine off.
	//
	// Read straight from the ini here rather than through gServerInfo, because
	// ReadCustomInfo() calls this Load() BEFORE it calls ReadCustomConfig() -
	// a gServerInfo member would still hold its pre-parse value on first load.
	//
	// Folded into m_Enable so every existing gate honours it at once: SetNPC()
	// does not spawn the NPC, Dialog() refuses to open the window, and both
	// CGSpinRecv and CGClaimRecv reject the packet.
	//
	// Held prizes are deliberately NOT affected - GDSlotClaimSend and
	// GDSlotClaimSaveSend have no m_Enable check, so a box still loads and saves
	// while the machine is off. Switching it off never destroys winnings players
	// are already holding, and switching it back on returns them intact.
	if(GetPrivateProfileInt("CustomConfig","CustomSlotMachine",1,".\\Data\\CustomConfig.ini") == 0)
	{
		LogAddCat(LOG_CAT_SLOTMACHINE,LOG_BLUE,"[SlotMachine] disabled by CustomSlotMachine in CustomConfig.ini");
		this->m_Enable = 0;
	}

	LogAddCat(LOG_CAT_SLOTMACHINE,LOG_BLUE,"[SlotMachine] Enabled %d, %d symbols, %d paylines (%d active), %d stake items, NPC %d at %d/%d/%d",
		this->m_Enable,this->m_SymbolCount,this->m_LineCount,this->m_Lines,this->m_StakeCount,
		this->m_NpcClass,this->m_NpcMap,this->m_NpcX,this->m_NpcY);
}

void CSlotMachine::SetNPC() // OK
{
	if(this->m_Enable == 0)
	{
		return;
	}

	// Pushed into the spawn table rather than edited into MonsterSetBase, the same
	// way SauChangeItem places its NPC. Must run after gMonsterSetBase.LoadFolder
	// and before SetMonsterData, or the entry is either wiped or never
	// instantiated.
	MONSTER_SET_BASE_INFO info;

	memset(&info,0,sizeof(info));

	info.Type = 0;					// section 0 = fixed position, which is where NPCs go
	info.MonsterClass = this->m_NpcClass;
	info.Map = this->m_NpcMap;
	info.Dis = 0;
	info.Dir = this->m_NpcDir;
	info.X = this->m_NpcX;
	info.Y = this->m_NpcY;

	gMonsterSetBase.SetInfo(info);
}

//////////////////////////////////////////////////////////////////////
// NPC
//////////////////////////////////////////////////////////////////////

bool CSlotMachine::Dialog(LPOBJ lpObj,LPOBJ lpNpc) // OK
{
	if(this->m_Enable == 0)
	{
		return 0;
	}

	if(lpNpc->Class != this->m_NpcClass || lpNpc->Map != this->m_NpcMap || lpNpc->X != this->m_NpcX || lpNpc->Y != this->m_NpcY)
	{
		return 0;
	}

	// Past this point the click belongs to us and is consumed either way, so a
	// refusal still returns 1 - falling through would open the generic shop window
	// on top of the refusal message.
	if(lpObj->Interface.use != 0)
	{
		return 1;
	}

	lpObj->SlotMachineOpen = 1;
	lpObj->Interface.use = 1;
	lpObj->Interface.type = INTERFACE_SLOTMACHINE;
	lpObj->Interface.state = 1;

	this->SendOpen(lpObj->Index);
	this->SendClaimList(lpObj->Index,SLOT_RESULT_OK);

	return 1;
}

void CSlotMachine::CloseMachine(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->SlotMachineOpen == 0)
	{
		return;
	}

	lpObj->SlotMachineOpen = 0;

	if(lpObj->Interface.type == INTERFACE_SLOTMACHINE)
	{
		lpObj->Interface.use = 0;
		lpObj->Interface.type = 0;
		lpObj->Interface.state = 0;
	}

	// Free spins deliberately SURVIVE closing the window - they were won, and they
	// persist with the reward box. Only the open flag is cleared here.
}

//////////////////////////////////////////////////////////////////////
// The roll
//////////////////////////////////////////////////////////////////////

bool CSlotMachine::IsWild(int symbol) // OK
{
	if(symbol < 0 || symbol >= this->m_SymbolCount)
	{
		return 0;
	}

	return ((this->m_Symbol[symbol].Kind == SLOT_SYMBOL_MULTIPLIER)?1:0);
}

int CSlotMachine::MultiplierValue(int symbol) // OK
{
	if(this->IsWild(symbol) == 0)
	{
		return 0;
	}

	return this->m_Symbol[symbol].Value;
}

void CSlotMachine::DrawGrid(BYTE* cell) // OK
{
	g_SlotRandCritical.lock();

	for(int reel=0;reel < SLOT_REELS;reel++)
	{
		// A fresh CRandomManager per reel, per spin - the same stack-local shape
		// the spin wheel's DrawWheelPrize uses.
		CRandomManager RandomMng;

		for(int n=0;n < this->m_SymbolCount;n++)
		{
			if(this->m_ReelWeight[reel][n] > 0)
			{
				RandomMng.AddElement(n,this->m_ReelWeight[reel][n]);
			}
		}

		for(int row=0;row < SLOT_ROWS;row++)
		{
			WORD picked = 0;

			RandomMng.GetRandomElement(&picked);

			if(picked >= (WORD)this->m_SymbolCount)
			{
				picked = 0;
			}

			cell[(reel * SLOT_ROWS) + row] = (BYTE)picked;
		}
	}

	g_SlotRandCritical.unlock();
}

int CSlotMachine::LowestPayingSymbol() // OK
{
	// The cheapest symbol that actually pays. A forced win is meant to be a
	// consolation, not a jackpot, so the pity and beginner paths both land here
	// unless the roll says otherwise.
	int best = -1;
	int bestPay = 0;

	for(int n=0;n < this->m_SymbolCount;n++)
	{
		if(this->m_Symbol[n].Kind != SLOT_SYMBOL_NORMAL)
		{
			continue;
		}

		if(this->m_Pay[n].Pay[0] <= 0)
		{
			continue;
		}

		if(best < 0 || this->m_Pay[n].Pay[0] < bestPay)
		{
			best = n;
			bestPay = this->m_Pay[n].Pay[0];
		}
	}

	return best;
}

int CSlotMachine::FindWildSymbol(int value) // OK
{
	for(int n=0;n < this->m_SymbolCount;n++)
	{
		if(this->m_Symbol[n].Kind == SLOT_SYMBOL_MULTIPLIER && this->m_Symbol[n].Value == value)
		{
			return n;
		}
	}

	return -1;
}

int CSlotMachine::FindScatterSymbol() // OK
{
	for(int n=0;n < this->m_SymbolCount;n++)
	{
		if(this->m_Symbol[n].Kind == SLOT_SYMBOL_SCATTER)
		{
			return n;
		}
	}

	return -1;
}

int CSlotMachine::RollPityShape() // OK
{
	CRandomManager RandomMng;

	// Weights, not percentages, so 90 / 5 / 2.5 / 2.5 can be written 900 / 50 / 25
	// / 25 without needing fractions.
	if(this->m_Bonus.PityWeightMin > 0)		RandomMng.AddElement(0,this->m_Bonus.PityWeightMin);
	if(this->m_Bonus.PityWeightX2 > 0)		RandomMng.AddElement(1,this->m_Bonus.PityWeightX2);
	if(this->m_Bonus.PityWeightX4 > 0)		RandomMng.AddElement(2,this->m_Bonus.PityWeightX4);
	if(this->m_Bonus.PityWeightScatter > 0)	RandomMng.AddElement(3,this->m_Bonus.PityWeightScatter);

	WORD picked = 0;

	g_SlotRandCritical.lock();
	RandomMng.GetRandomElement(&picked);
	g_SlotRandCritical.unlock();

	return (int)picked;
}

void CSlotMachine::ForceWinGrid(BYTE* cell,int shape) // OK
{
	// Every cell is filled with junk first, then the winning shape is written over
	// it. The junk deliberately avoids the paying symbol and every wild, so the
	// forced grid cannot accidentally pay MORE than intended on another line.
	const int paySymbol = this->LowestPayingSymbol();

	if(paySymbol < 0)
	{
		return;						// nothing pays; leave the rolled grid alone
	}

	const int scatter = this->FindScatterSymbol();

	// Two DIFFERENT normal symbols for the background.
	//
	// The background is laid out so that every payline is dead before it can pay:
	// reel 1 is filled with fillerA, reel 2 with fillerB, and a run has to start on
	// reel 1 and continue on reel 2 to reach the three needed. With those two reels
	// never matching, no line can pay whatever reels 3-5 hold.
	//
	// The first version filled all fifteen cells with a SINGLE symbol, which is the
	// exact opposite: it made five-of-a-kind on every active payline at once, and a
	// forced "smallest win" paid out like a jackpot.
	int fillerA = -1;
	int fillerB = -1;

	for(int n=0;n < this->m_SymbolCount;n++)
	{
		if(this->m_Symbol[n].Kind != SLOT_SYMBOL_NORMAL)
		{
			continue;			// never a wild (it would substitute) or a scatter
		}

		if(n == paySymbol)
		{
			continue;			// never the paying symbol, or it extends the run
		}

		if(fillerA < 0)
		{
			fillerA = n;
		}
		else if(fillerB < 0)
		{
			fillerB = n;
			break;
		}
	}

	// A machine with fewer than three normal symbols cannot have a background that
	// is guaranteed not to pay, so the grid is left exactly as it was rolled. That
	// forfeits the forced win rather than risking an unbounded one.
	if(fillerA < 0 || fillerB < 0)
	{
		LogAdd(LOG_RED,"[SlotMachine] a forced win needs at least 3 normal symbols - the spin was left as rolled");
		return;
	}

	for(int reel=0;reel < SLOT_REELS;reel++)
	{
		for(int row=0;row < SLOT_ROWS;row++)
		{
			// Reels 1 and 2 carry the two fillers; the rest alternate, which only has
			// to look unremarkable because those reels can no longer form a run.
			int sym = fillerA;

			if(reel == 1)
			{
				sym = fillerB;
			}
			else if(reel > 1)
			{
				sym = (((reel + row) % 2) == 0) ? fillerA : fillerB;
			}

			cell[(reel * SLOT_ROWS) + row] = (BYTE)sym;
		}
	}

	if(shape == 3)
	{
		// Three scatters. They ignore paylines, so any three cells will do - but
		// they must land on reels that actually carry the scatter, or the grid would
		// be one the reels could never produce.
		if(scatter < 0)
		{
			shape = 0;				// no scatter configured; fall back to a line win
		}
		else
		{
			int placed = 0;

			for(int reel=0;reel < SLOT_REELS && placed < 3;reel++)
			{
				if(this->m_ReelWeight[reel][scatter] <= 0)
				{
					continue;
				}

				cell[(reel * SLOT_ROWS) + (placed % SLOT_ROWS)] = (BYTE)scatter;
				placed++;
			}

			if(placed >= 3)
			{
				return;
			}

			shape = 0;				// could not place three; fall back
		}
	}

	// A line win on payline 0. Three in a row from reel 1, which is the shortest
	// paying run and so the smallest win the paytable allows.
	const int line = 0;

	int wild = -1;

	if(shape == 1)
	{
		wild = this->FindWildSymbol(2);
	}
	else if(shape == 2)
	{
		wild = this->FindWildSymbol(4);
	}

	for(int reel=0;reel < 3;reel++)
	{
		const int row = this->m_Line[line].Row[reel];

		cell[(reel * SLOT_ROWS) + row] = (BYTE)paySymbol;
	}

	// The wild goes on the MIDDLE reel of the run: it still substitutes for the
	// paying symbol, so the run is unbroken, and a run has to start on reel 1 with a
	// real symbol for the line to pay at all.
	if(wild >= 0 && this->m_ReelWeight[1][wild] > 0)
	{
		const int row = this->m_Line[line].Row[1];

		cell[(1 * SLOT_ROWS) + row] = (BYTE)wild;
	}

	// Reel 4 must NOT continue the run, or a three-of-a-kind quietly becomes a
	// four-of-a-kind and pays more than the shape promised.
	{
		const int row = this->m_Line[line].Row[3];
		const int idx = ((3 * SLOT_ROWS) + row);

		if(cell[idx] == (BYTE)paySymbol)
		{
			cell[idx] = (BYTE)((paySymbol == fillerA) ? fillerB : fillerA);
		}
	}
}

int CSlotMachine::Evaluate(const BYTE* cell,PMSG_SLOT_RESULT_LINE* lines,int* lineCount,int* scatterCount) // OK
{
	(*lineCount) = 0;
	(*scatterCount) = 0;

	// Scatters ignore paylines entirely - counted anywhere in the 15 cells. That is
	// the whole point of a scatter and the reason it cannot be folded into the line
	// loop below.
	for(int n=0;n < SLOT_CELLS;n++)
	{
		int symbol = cell[n];

		if(symbol >= 0 && symbol < this->m_SymbolCount && this->m_Symbol[symbol].Kind == SLOT_SYMBOL_SCATTER)
		{
			(*scatterCount)++;
		}
	}

	int total = 0;

	for(int line=0;line < this->m_Lines;line++)
	{
		// Read the line's symbols out of the grid once.
		int onLine[SLOT_REELS] = {0};

		for(int reel=0;reel < SLOT_REELS;reel++)
		{
			onLine[reel] = cell[(reel * SLOT_ROWS) + this->m_Line[line].Row[reel]];
		}

		// The line pays whatever its first NON-wild symbol is. A leading wild
		// substitutes for it, so the paying symbol may not be on reel 1 - but the
		// run still has to start there.
		int paying = -1;

		for(int reel=0;reel < SLOT_REELS;reel++)
		{
			if(this->IsWild(onLine[reel]) == 0)
			{
				paying = onLine[reel];
				break;
			}
		}

		// Every cell on the line is a multiplier. There is no symbol to pay, so the
		// line pays nothing however large those multipliers are.
		if(paying < 0)
		{
			continue;
		}

		// Scatters never pay on a line - they have already been counted.
		if(this->m_Symbol[paying].Kind == SLOT_SYMBOL_SCATTER)
		{
			continue;
		}

		int run = 0;

		for(int reel=0;reel < SLOT_REELS;reel++)
		{
			if(onLine[reel] == paying || this->IsWild(onLine[reel]) != 0)
			{
				run++;
			}
			else
			{
				break;
			}
		}

		if(run < 3)
		{
			continue;
		}

		int pay = this->m_Pay[paying].Pay[run - 3];

		if(pay <= 0)
		{
			continue;
		}

		// ADDITIVE, not multiplicative: a x2 and a x4 on the same winning line make
		// x6, never x8. Clamped to MultiCap, and only the multipliers inside the
		// winning run count - one sitting past where the run broke did not help.
		int multi = 0;

		for(int reel=0;reel < run;reel++)
		{
			multi += this->MultiplierValue(onLine[reel]);
		}

		if(multi > this->m_MultiCap)
		{
			multi = this->m_MultiCap;
		}

		if(multi < 1)
		{
			multi = 1;
		}

		int linePay = pay * multi;

		if((*lineCount) < SLOT_MAX_LINE)
		{
			lines[*lineCount].line = (BYTE)line;
			lines[*lineCount].count = (BYTE)run;
			lines[*lineCount].multi = (BYTE)multi;
			lines[*lineCount].pay = (DWORD)linePay;
			(*lineCount)++;
		}

		total += linePay;
	}

	return total;
}

//////////////////////////////////////////////////////////////////////
// Stake
//////////////////////////////////////////////////////////////////////

DWORD CSlotMachine::TodayStamp() // OK
{
	time_t now = time(0);

	struct tm t = {0};

	localtime_s(&t,&now);

	return (DWORD)(((t.tm_year + 1900) * 10000) + ((t.tm_mon + 1) * 100) + t.tm_mday);
}

void CSlotMachine::AnnounceWin(LPOBJ lpObj,int amount,const char* what) // OK
{
	if(this->m_Bonus.AnnounceCount <= 0 || amount <= 0)
	{
		return;
	}

	// The highest threshold the win reaches, so a single spin is announced once
	// however many tiers it passes. Order in the config does not matter.
	int best = 0;

	for(int n=0;n < this->m_Bonus.AnnounceCount;n++)
	{
		if(amount >= this->m_Bonus.Announce[n] && this->m_Bonus.Announce[n] > best)
		{
			best = this->m_Bonus.Announce[n];
		}
	}

	if(best <= 0)
	{
		return;
	}

	char text[256] = {0};

	// Formatted here rather than through PostMessageItemNotice, which runs the text
	// through wsprintf with the player name as its one and only substitution - an
	// amount and an item name have nowhere to go in it.
	_snprintf(text,sizeof(text)-1,this->GetMessage(5),lpObj->Name,amount,what);

	// Notice type 0, which the client draws as scrolling text in (255,200,80) - the
	// gold banner every event on this server already uses.
	//
	// NOT GCNewMessageSend, which was the first attempt: the GameServer happily
	// sends its C1:F3:E4, but this client has no case for that subcode in either of
	// its two 0xF3 dispatchers, so the packet is silently discarded and nothing
	// appears. The same is true of PostMessageItemNotice, which is built on it.
	//
	// The text is passed as an ARGUMENT to a "%s" rather than as the format string,
	// because GCNoticeSendToAll runs it through vsprintf_s - and an item name
	// containing a % would otherwise be read as a conversion.
	gNotice.GCNoticeSendToAll(0,0,0,0,0,0,"%s",text);

	LogAddCat(LOG_CAT_SLOTMACHINE,LOG_GREEN,"[SlotMachine] ANNOUNCE %s/%s won %d x %s (tier %d)",
		lpObj->Account,lpObj->Name,amount,what,best);
}

bool CSlotMachine::CanAffordStake(LPOBJ lpObj,const SLOT_STAKE_INFO& stake,int count) // OK
{
	if(count <= 0)
	{
		return 0;
	}

	// The cash currencies are a cached balance on the object, kept in step by
	// GDSetCoinSend, so unlike the cash shop's own purchase path this needs no
	// DataServer round trip and the spin can stay synchronous.
	if(stake.Kind != SLOT_STAKE_ITEM)
	{
		const int cost = (stake.Amount * count);

		if(stake.Kind == SLOT_STAKE_WCOINC)	return ((lpObj->Coin1 >= cost)?1:0);
		if(stake.Kind == SLOT_STAKE_WCOINP)	return ((lpObj->Coin2 >= cost)?1:0);

		return ((lpObj->Coin3 >= cost)?1:0);
	}

	// An item stake of Amount per unit.
	count *= stake.Amount;

	// Inventory plus bank has to cover the whole bet. Inventory is spent first - it
	// is the stock the player can see, so draining the bank while items sat in the
	// inventory would look like the bank had been raided.
	int available = gItemManager.GetInventoryItemCount(lpObj,stake.ItemIndex,stake.Level);

	if(available >= count)
	{
		return 1;
	}

#if(JEWELBANKVER2)

	// Checked here rather than left to CongTruBank, which refuses on these two maps
	// but would do so AFTER the spin had already resolved.
	if(lpObj->Map == 107 || lpObj->Map == 109)
	{
		return 0;
	}

	available += gBCustomItemBank.CheckCountItemBank(lpObj->Index,stake.ItemIndex,stake.Level);

#endif

	return ((available >= count)?1:0);
}

bool CSlotMachine::TakeStake(LPOBJ lpObj,const SLOT_STAKE_INFO& stake,int count) // OK
{
	if(count <= 0)
	{
		return 0;
	}

	if(stake.Kind != SLOT_STAKE_ITEM)
	{
		const int cost = (stake.Amount * count);

		// Re-checked rather than trusted from CanAffordStake: the balance is shared
		// with the cash shop, which can move it between the two calls.
		if(stake.Kind == SLOT_STAKE_WCOINC && lpObj->Coin1 < cost)	return 0;
		if(stake.Kind == SLOT_STAKE_WCOINP && lpObj->Coin2 < cost)	return 0;
		if(stake.Kind == SLOT_STAKE_GOBLIN && lpObj->Coin3 < cost)	return 0;

		GDSetCoinSend(lpObj->Index,
			((stake.Kind == SLOT_STAKE_WCOINC)?-(cost):0),
			((stake.Kind == SLOT_STAKE_WCOINP)?-(cost):0),
			((stake.Kind == SLOT_STAKE_GOBLIN)?-(cost):0),
			"SlotMachineBet");

		return 1;
	}

	count *= stake.Amount;

	const int inInventory = gItemManager.GetInventoryItemCount(lpObj,stake.ItemIndex,stake.Level);

	const int fromInventory = ((inInventory > count)?count:inInventory);

	const int fromBank = count - fromInventory;

#if(JEWELBANKVER2)

	// The bank is charged FIRST, because it is the only half that can still refuse -
	// CongTruBank has guards of its own. Taking the inventory half first and then
	// being refused here would have destroyed part of the stake for a spin that never
	// happened.
	if(fromBank > 0 && gBCustomItemBank.CongTruBank(lpObj->Index,stake.ItemIndex,stake.Level,-(fromBank),0) == 0)
	{
		return 0;
	}

#else

	if(fromBank > 0)
	{
		return 0;
	}

#endif

	if(fromInventory > 0)
	{
		gItemManager.DeleteInventoryItemCount(lpObj,stake.ItemIndex,stake.Level,fromInventory);
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////
// Spin
//////////////////////////////////////////////////////////////////////

void CSlotMachine::CGSpinRecv(int aIndex,BYTE* lpRecv) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	PMSG_SLOT_SPIN_RECV* lpMsg = (PMSG_SLOT_SPIN_RECV*)lpRecv;

	LPOBJ lpObj = &gObj[aIndex];

	if(this->m_Enable == 0)
	{
		this->SendResult(aIndex,SLOT_RESULT_DISABLED);
		return;
	}

	// The open flag is the only thing tying a spin to having walked to the NPC.
	// Without it the packet alone would be a machine in your pocket.
	if(lpObj->SlotMachineOpen == 0)
	{
		this->SendResult(aIndex,SLOT_RESULT_NOT_OPEN);
		return;
	}

	// A DEDICATED field, not lpObj->ClickClientSend. That one is shared across
	// features under two incompatible conventions, has a checker with no writer,
	// and is never reset on login - so it carries a stale value onto a recycled
	// object slot, which on a gambling feature means the first spin of a session
	// can skip its own throttle.
	if(GetTickCount() < (lpObj->SlotMachineTick + 1000))
	{
		this->SendResult(aIndex,SLOT_RESULT_TOO_FAST);
		return;
	}

	if(lpObj->Transaction == 1 || gItemManager.ChaosBoxHasItem(lpObj) != 0 || gItemManager.TradeHasItem(lpObj) != 0)
	{
		this->SendResult(aIndex,SLOT_RESULT_BUSY);
		return;
	}

	const bool freeSpin = (lpObj->SlotFreeSpins > 0);

	// During free spins the stake is locked to whatever triggered them, so the
	// client's choice is ignored rather than trusted.
	int stakeIndex = ((freeSpin != false)?lpObj->SlotFreeStake:(int)lpMsg->stake);

	if(stakeIndex < 0 || stakeIndex >= this->m_StakeCount)
	{
		this->SendResult(aIndex,SLOT_RESULT_BAD_STAKE);
		return;
	}

	const SLOT_STAKE_INFO& stake = this->m_Stake[stakeIndex];

	// The bet is locked during free spins for the same reason the stake item is:
	// the round was won at one bet level and paying it out at another would let a
	// player trigger on the minimum and harvest on the maximum.
	int betCount = ((freeSpin != false)?lpObj->SlotFreeBet:(int)lpMsg->bet);

	// Clamped, not trusted. bet arrives as a BYTE and multiplies every payout, so
	// this is the only thing between a hand-built packet and a 255x win.
	if(betCount < 1)
	{
		betCount = 1;
	}

	if(betCount > this->m_MaxBet)
	{
		betCount = this->m_MaxBet;
	}

	if(freeSpin == false && this->CanAffordStake(lpObj,stake,betCount) == 0)
	{
		gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,this->GetMessage(1),stake.Name);
		this->SendResult(aIndex,SLOT_RESULT_NO_FUNDS);
		return;
	}

	// Space for the winnings is checked BEFORE the stake is taken. The spin wheel
	// learned this the hard way: it used to delete the entrance fee first and could
	// then still refuse on the space check, with no refund and no log line. A row
	// already holding this same item counts as available, since wins merge.
	if(this->GetClaimFreeCount(aIndex) <= 0)
	{
		bool merge = 0;

		for(int n=0;n < SLOT_CLAIM_SIZE;n++)
		{
			if(lpObj->SlotClaim[n].ItemIndex == stake.ItemIndex && lpObj->SlotClaim[n].Level == stake.Level)
			{
				merge = 1;
				break;
			}
		}

		if(merge == 0)
		{
			gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,this->GetMessage(2));
			this->SendResult(aIndex,SLOT_RESULT_BOX_FULL);
			return;
		}
	}

	lpObj->SlotMachineTick = GetTickCount();

	// Past every check - the spin is definitely happening, so now take the fee.
	if(freeSpin == false)
	{
		if(this->TakeStake(lpObj,stake,betCount) == 0)
		{
			// Only reachable if the balance moved between the check and here.
			this->SendResult(aIndex,SLOT_RESULT_NO_FUNDS);
			return;
		}
	}
	else
	{
		lpObj->SlotFreeSpins--;
	}

	BYTE cell[SLOT_CELLS] = {0};

	this->DrawGrid(cell);

	PMSG_SLOT_RESULT_LINE lines[SLOT_MAX_LINE];

	memset(lines,0,sizeof(lines));

	int lineCount = 0;
	int scatterCount = 0;

	int rawPay = this->Evaluate(cell,lines,&lineCount,&scatterCount);

	// ---- Losing-streak pity and beginner luck ----------------------------
	// Both only ever turn a LOSS into a win, never a win into a bigger one, and
	// both work by rebuilding the grid and letting Evaluate price it again. That
	// is what stops either of them paying something the paytable does not allow.
	const DWORD today = this->TodayStamp();

	if(lpObj->SlotPityDate != today)
	{
		lpObj->SlotPityDate = today;
		lpObj->SlotPityUsedToday = 0;
	}

	bool forced = 0;
	int forcedShape = 0;

	if(rawPay <= 0 && scatterCount < 3 && freeSpin == false)
	{
		// Pity first: it is the promise to the player, so it takes precedence over
		// the softer beginner nudge.
		if(this->m_Bonus.LossStreak > 0
			&& lpObj->SlotLossStreak >= this->m_Bonus.LossStreak
			&& (int)lpObj->SlotPityUsedToday < this->m_Bonus.LossStreakPerDay)
		{
			forced = 1;
			forcedShape = this->RollPityShape();

			lpObj->SlotPityUsedToday++;
			lpObj->SlotLossStreak = 0;
		}
		else if(this->m_Bonus.BeginnerSpins > 0
			&& this->m_Bonus.BeginnerPercent > 0
			&& (int)lpObj->SlotSpinCount < this->m_Bonus.BeginnerSpins
			&& (GetLargeRand() % 100) < this->m_Bonus.BeginnerPercent)
		{
			// Beginner luck only ever gives the smallest win. It is there to make the
			// first session feel warm, not to hand out jackpots.
			forced = 1;
			forcedShape = 0;
		}

		if(forced != 0)
		{
			this->ForceWinGrid(cell,forcedShape);

			lineCount = 0;
			scatterCount = 0;

			memset(lines,0,sizeof(lines));

			rawPay = this->Evaluate(cell,lines,&lineCount,&scatterCount);
		}
	}

	lpObj->SlotSpinCount++;

	// The paytable is written for a one-unit bet, so the bet multiplies it - exactly
	// as a real machine multiplies its line bet. Applied to the TOTAL rather than per
	// line so the per-line figures the client shows stay comparable to the paytable.
	int totalPay = rawPay * betCount;

	for(int n=0;n < lineCount;n++)
	{
		lines[n].pay *= (DWORD)betCount;
	}

	// Cap expressed in credits so one number covers every stake item, then
	// converted back into items. Stake credits are guaranteed > 0 by Load.
	if(this->m_MaxPayoutCredits > 0 && (totalPay * stake.Credits) > this->m_MaxPayoutCredits)
	{
		totalPay = this->m_MaxPayoutCredits / stake.Credits;
	}

	// Free spins. 3+ scatters anywhere, retrigger included - a retrigger is just
	// this same branch reached while already inside a round.
	int awarded = 0;

	if(scatterCount >= 3)
	{
		if(freeSpin == false)
		{
			lpObj->SlotRetriggerCount = 0;
			lpObj->SlotFreeStake = stakeIndex;
			lpObj->SlotFreeBet = betCount;
			awarded = this->m_FreeSpins;
		}
		else if(lpObj->SlotRetriggerCount < this->m_MaxRetrigger)
		{
			lpObj->SlotRetriggerCount++;
			awarded = this->m_FreeSpins;
		}

		lpObj->SlotFreeSpins += awarded;
	}

	if(lpObj->SlotFreeSpins <= 0)
	{
		lpObj->SlotFreeStake = SLOT_NO_STAKE;
		lpObj->SlotFreeBet = 1;
		lpObj->SlotRetriggerCount = 0;
	}

	// The losing streak is what drives pity. Counted on real spins only - a free
	// spin costs nothing, so letting it break the streak would punish the bonus.
	if(freeSpin == false)
	{
		if(totalPay > 0 || scatterCount >= 3)
		{
			lpObj->SlotLossStreak = 0;
		}
		else
		{
			lpObj->SlotLossStreak++;
		}
	}

	if(totalPay > 0)
	{
		// What the player actually receives, which is what the announcement and the
		// log both have to quote - totalPay alone is in bet units, not items.
		const int wonAmount = (totalPay * stake.Amount);

		// Announced on the amount RECEIVED, whether that is items or coins, so a
		// currency machine and an item machine use the same thresholds.
		this->AnnounceWin(lpObj,wonAmount,stake.Name);

		if(stake.Kind == SLOT_STAKE_ITEM)
		{
			this->PushClaim(aIndex,stake.ItemIndex,stake.Level,wonAmount);
		}
		else
		{
			// Currency is paid straight out. It has no inventory footprint, so the
			// reward box exists for it only as a source of delay - and the box's whole
			// purpose is to deal with items that might not fit.
			GDSetCoinSend(aIndex,
				((stake.Kind == SLOT_STAKE_WCOINC)?wonAmount:0),
				((stake.Kind == SLOT_STAKE_WCOINP)?wonAmount:0),
				((stake.Kind == SLOT_STAKE_GOBLIN)?wonAmount:0),
				"SlotMachineWin");
		}
	}

	// The scatter bonus item, on top of whatever the free spins did. Parked in the
	// reward box like any other item win, so it cannot be lost to a full inventory.
	if(scatterCount >= 3 && this->m_Bonus.ScatterCount > 0 && this->m_Bonus.ScatterItemIndex >= 0)
	{
		if(this->PushClaim(aIndex,this->m_Bonus.ScatterItemIndex,this->m_Bonus.ScatterLevel,this->m_Bonus.ScatterCount) != 0)
		{
			LogAddCat(LOG_CAT_SLOTMACHINE,LOG_GREEN,"[SlotMachine] %s/%s scatter bonus: %d x %s",
				lpObj->Account,lpObj->Name,this->m_Bonus.ScatterCount,gItemManager.GetItemName(this->m_Bonus.ScatterItemIndex));
		}
	}

	// Logged whether it won or lost. Once the packet is sent the grid exists
	// nowhere else, and a gambling feature with no record of its losing spins
	// cannot be audited at all.
	LogAddCat(LOG_CAT_SLOTMACHINE,((totalPay > 0)?LOG_GREEN:LOG_BLACK),
		"[SlotMachine] %s/%s %s %d x %s (%d cr) -> %d lines, %d scatter, won %d x %s%s%s streak=%d",
		lpObj->Account,lpObj->Name,((freeSpin != false)?"FREESPIN":"staked"),betCount,stake.Name,(betCount * stake.Credits),
		lineCount,scatterCount,(totalPay * stake.Amount),stake.Name,((awarded > 0)?" [+free spins]":""),
		// Forced wins are called out so the log can be audited for how often the
		// machine is being generous on purpose rather than by chance.
		((forced == 0)?"":((forcedShape == 0 && lpObj->SlotPityUsedToday == 0)?" [BEGINNER]":" [PITY]")),
		lpObj->SlotLossStreak);

	// Flushes the reward box and the free-spin count together - nothing else
	// pushes per-player item state, and a crash in between would lose winnings the
	// player has already staked for.
	GDCharacterInfoSaveSend(aIndex);

	PMSG_SLOT_RESULT_SEND pMsg;

	pMsg.header.set(0xD3,0x91,(sizeof(pMsg) + (sizeof(PMSG_SLOT_RESULT_LINE) * lineCount)));

	pMsg.result = SLOT_RESULT_OK;
	memcpy(pMsg.Cell,cell,sizeof(pMsg.Cell));
	pMsg.ScatterCount = (BYTE)scatterCount;
	pMsg.WinLineCount = (BYTE)lineCount;
	pMsg.FreeSpinAwarded = (BYTE)awarded;
	pMsg.FreeSpinsLeft = (WORD)lpObj->SlotFreeSpins;
	pMsg.TotalPay = (DWORD)totalPay;
	pMsg.StakeItemIndex = (WORD)stake.ItemIndex;
	pMsg.StakeLevel = (BYTE)stake.Level;
	pMsg.WasFreeSpin = ((freeSpin != false)?1:0);
	pMsg.BetCount = (BYTE)betCount;

	BYTE buffer[sizeof(PMSG_SLOT_RESULT_SEND) + (sizeof(PMSG_SLOT_RESULT_LINE) * SLOT_MAX_LINE)];

	memcpy(buffer,&pMsg,sizeof(pMsg));
	memcpy((buffer + sizeof(pMsg)),lines,(sizeof(PMSG_SLOT_RESULT_LINE) * lineCount));

	// The size passed separately rather than read back out of the header: a
	// PSWMSG_HEAD stores it as a two-byte big-endian field, not a number.
	DataSend(aIndex,buffer,(sizeof(pMsg) + (sizeof(PMSG_SLOT_RESULT_LINE) * lineCount)));

	// Sent after the result so the client can update its reward panel once the
	// reels have stopped.
	this->SendClaimList(aIndex,SLOT_RESULT_OK);
}

void CSlotMachine::SendResult(int aIndex,BYTE result) // OK
{
	PMSG_SLOT_RESULT_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0xD3,0x91,sizeof(pMsg));

	pMsg.result = result;

	if(OBJECT_RANGE(aIndex) != 0)
	{
		pMsg.FreeSpinsLeft = (WORD)gObj[aIndex].SlotFreeSpins;
	}

	DataSend(aIndex,(BYTE*)&pMsg,sizeof(pMsg));
}

void CSlotMachine::SendOpen(int aIndex) // OK
{
	PMSG_SLOT_OPEN_SEND pMsg;

	pMsg.header.set(0xD3,0x8F,0);

	pMsg.SymbolCount = (BYTE)this->m_SymbolCount;
	pMsg.LineCount = (BYTE)this->m_Lines;
	pMsg.StakeCount = (BYTE)this->m_StakeCount;
	pMsg.MultiCap = (BYTE)this->m_MultiCap;
	pMsg.Reels = SLOT_REELS;
	pMsg.Rows = SLOT_ROWS;
	pMsg.MaxBet = (BYTE)this->m_MaxBet;
	pMsg.FreeSpinsLeft = (WORD)gObj[aIndex].SlotFreeSpins;

	BYTE buffer[sizeof(PMSG_SLOT_OPEN_SEND)
		+ (sizeof(PMSG_SLOT_OPEN_SYMBOL) * SLOT_MAX_SYMBOL)
		+ (sizeof(PMSG_SLOT_OPEN_LINE) * SLOT_MAX_LINE)
		+ (sizeof(PMSG_SLOT_OPEN_STAKE) * SLOT_MAX_STAKE)];

	int size = sizeof(pMsg);

	for(int n=0;n < this->m_SymbolCount;n++)
	{
		PMSG_SLOT_OPEN_SYMBOL row;

		row.Kind = (BYTE)this->m_Symbol[n].Kind;
		row.ItemIndex = (WORD)this->m_Symbol[n].ItemIndex;
		row.Level = (BYTE)this->m_Symbol[n].Level;
		row.Value = (BYTE)this->m_Symbol[n].Value;

		memcpy((buffer + size),&row,sizeof(row));
		size += sizeof(row);
	}

	for(int n=0;n < this->m_Lines;n++)
	{
		PMSG_SLOT_OPEN_LINE row;

		for(int reel=0;reel < SLOT_REELS;reel++)
		{
			row.Row[reel] = (BYTE)this->m_Line[n].Row[reel];
		}

		memcpy((buffer + size),&row,sizeof(row));
		size += sizeof(row);
	}

	for(int n=0;n < this->m_StakeCount;n++)
	{
		PMSG_SLOT_OPEN_STAKE row;

		memset(&row,0,sizeof(row));

		row.ItemIndex = (WORD)this->m_Stake[n].ItemIndex;
		row.Level = (BYTE)this->m_Stake[n].Level;
		row.Credits = (WORD)this->m_Stake[n].Credits;
		row.Kind = (BYTE)this->m_Stake[n].Kind;
		row.Amount = (DWORD)this->m_Stake[n].Amount;
		memcpy(row.Name,this->m_Stake[n].Name,sizeof(row.Name));

		memcpy((buffer + size),&row,sizeof(row));
		size += sizeof(row);
	}

	pMsg.header.set(0xD3,0x8F,size);

	memcpy(buffer,&pMsg,sizeof(pMsg));

	DataSend(aIndex,buffer,size);
}

//////////////////////////////////////////////////////////////////////
// Reward box
//////////////////////////////////////////////////////////////////////

int CSlotMachine::GetClaimFreeCount(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return 0;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Type != OBJECT_USER)
	{
		return 0;
	}

	int free = 0;

	for(int n=0;n < SLOT_CLAIM_SIZE;n++)
	{
		if(lpObj->SlotClaim[n].ItemIndex < 0)
		{
			free++;
		}
	}

	return free;
}

bool CSlotMachine::PushClaim(int aIndex,int itemIndex,int level,int count) // OK
{
	if(OBJECT_RANGE(aIndex) == 0 || count <= 0)
	{
		return 0;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Type != OBJECT_USER)
	{
		return 0;
	}

	// Merge first. Without this a session of small wins would fill all 8 rows with
	// the same item and lock the player out of spinning.
	for(int n=0;n < SLOT_CLAIM_SIZE;n++)
	{
		if(lpObj->SlotClaim[n].ItemIndex == itemIndex && lpObj->SlotClaim[n].Level == level)
		{
			lpObj->SlotClaim[n].Count += count;
			return 1;
		}
	}

	for(int n=0;n < SLOT_CLAIM_SIZE;n++)
	{
		if(lpObj->SlotClaim[n].ItemIndex >= 0)
		{
			continue;
		}

		lpObj->SlotClaim[n].ItemIndex = itemIndex;
		lpObj->SlotClaim[n].Level = level;
		lpObj->SlotClaim[n].Count = count;

		return 1;
	}

	return 0;
}

int CSlotMachine::InventoryRoomFor(LPOBJ lpObj,int itemIndex,int want) // OK
{
	if(want <= 0)
	{
		return 0;
	}

	// No inventory can hold more items than it has cells, so the search never has
	// to consider a row's whole count - which can run into five figures.
	int hi = ((want > INVENTORY_SIZE) ? INVENTORY_SIZE : want);
	int lo = 0;

	while(lo < hi)
	{
		const int mid = (lo + ((hi - lo + 1) / 2));

		REDEEM_ROOM_CHECK_ITEM check;

		check.ItemIndex = itemIndex;
		check.Quantity = mid;

		if(gItemManager.CheckItemInventorySpaceForBundle(lpObj,&check,1) != 0)
		{
			lo = mid;
		}
		else
		{
			hi = (mid - 1);
		}
	}

	return lo;
}

BYTE CSlotMachine::ClaimRow(int aIndex,int slot) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(slot < 0 || slot >= SLOT_CLAIM_SIZE)
	{
		return SLOT_RESULT_NO_ROOM;
	}

	const int itemIndex = lpObj->SlotClaim[slot].ItemIndex;
	const int level = lpObj->SlotClaim[slot].Level;
	const int count = lpObj->SlotClaim[slot].Count;

	if(itemIndex < 0 || count <= 0)
	{
		return SLOT_RESULT_OK;			// nothing here; not an error
	}

	// How many the bank could absorb. 0 when the item is not one of the bank's
	// configured slots, the bank is off, or the account-level ceiling is reached -
	// all of which are "the bank is not available for this", which is exactly the
	// case that must refuse rather than half-pay.
	int bankRoom = 0;

#if(JEWELBANKVER2)

	if(lpObj->Map != 107 && lpObj->Map != 109)
	{
		int bankSlot = gBCustomItemBank.CheckInfoListItemBank(itemIndex,level);

		if(bankSlot >= 0 && bankSlot < (int)gBCustomItemBank.mListItemBank.size())
		{
			int held = gBCustomItemBank.CheckCountItemBank(aIndex,itemIndex,level);

			int ceiling = gBCustomItemBank.mListItemBank[bankSlot].MaxCountType[lpObj->AccountLevel];

			bankRoom = ((ceiling > held)?(ceiling - held):0);
		}
	}

#endif

	// The bank takes as much as it will hold, and the inventory covers what is left.
	// Bank first because it holds thousands where the inventory holds dozens, so a
	// large win moves in one step instead of dozens of claims.
	const int toBank = ((count > bankRoom)?bankRoom:count);

	const int remainder = (count - toBank);

	const int toInventory = this->InventoryRoomFor(lpObj,itemIndex,remainder);

	const int taken = (toBank + toInventory);

	// PARTIAL claims are allowed. The original rule was all-or-nothing, which read
	// correctly for a handful of jewels and then stranded anything larger: once a row
	// grew past what the bank ceiling plus one inventory could hold, it could never
	// be claimed at all, no matter how much room the player made.
	if(taken <= 0)
	{
		return ((bankRoom > 0)?SLOT_RESULT_NO_ROOM:SLOT_RESULT_NO_BANK);
	}

	// Nothing above this point has moved anything. From here on both steps are known
	// to fit, so neither can fail and leave the row wrong.
#if(JEWELBANKVER2)

	if(toBank > 0)
	{
		gBCustomItemBank.CongTruBank(aIndex,itemIndex,level,toBank,0);
	}

#endif

	for(int n=0;n < toInventory;n++)
	{
		GDCreateItemSend(aIndex,0xEB,0,0,itemIndex,(BYTE)level,0,0,0,0,-1,0,0,0,0,0,0xFF,0);
	}

	const int left = (count - taken);

	if(left > 0)
	{
		lpObj->SlotClaim[slot].Count = left;
	}
	else
	{
		lpObj->SlotClaim[slot].ItemIndex = -1;
		lpObj->SlotClaim[slot].Level = 0;
		lpObj->SlotClaim[slot].Count = 0;
	}

	LogAddCat(LOG_CAT_SLOTMACHINE,LOG_GREEN,"[SlotMachine] %s/%s claimed %d x %s (%d inventory, %d bank), %d still held",
		lpObj->Account,lpObj->Name,taken,gItemManager.GetItemName(itemIndex),toInventory,toBank,left);

	GDCharacterInfoSaveSend(aIndex);

	// Told explicitly, because a claim that moves only part of a row and silently
	// leaves the rest looks like the button half-worked.
	if(left > 0)
	{
		gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,this->GetMessage(4),taken,gItemManager.GetItemName(itemIndex),left);
	}

	return SLOT_RESULT_OK;
}

void CSlotMachine::CGClaimRecv(int aIndex,BYTE* lpRecv) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	PMSG_SLOT_CLAIM_RECV* lpMsg = (PMSG_SLOT_CLAIM_RECV*)lpRecv;

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->SlotMachineOpen == 0)
	{
		this->SendClaimList(aIndex,SLOT_RESULT_NOT_OPEN);
		return;
	}

	if(lpObj->Transaction == 1 || gItemManager.ChaosBoxHasItem(lpObj) != 0 || gItemManager.TradeHasItem(lpObj) != 0)
	{
		this->SendClaimList(aIndex,SLOT_RESULT_BUSY);
		return;
	}

	BYTE result = SLOT_RESULT_OK;

	if(lpMsg->slot == 0xFF)
	{
		// Each row is independent, so a big win that will not fit does not stop a
		// small one being taken. The last refusal is what gets reported.
		for(int n=0;n < SLOT_CLAIM_SIZE;n++)
		{
			BYTE one = this->ClaimRow(aIndex,n);

			if(one != SLOT_RESULT_OK)
			{
				result = one;
			}
		}
	}
	else
	{
		if(lpMsg->slot >= SLOT_CLAIM_SIZE)
		{
			this->SendClaimList(aIndex,SLOT_RESULT_NO_ROOM);
			return;
		}

		result = this->ClaimRow(aIndex,lpMsg->slot);
	}

	if(result == SLOT_RESULT_NO_ROOM)
	{
		gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,this->GetMessage(3),"");
	}
	else if(result == SLOT_RESULT_NO_BANK)
	{
		gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,this->GetMessage(3),"");
	}

	this->SendClaimList(aIndex,result);
}

void CSlotMachine::SendClaimList(int aIndex,BYTE result) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	PMSG_SLOT_CLAIM_LIST_SEND pMsg;

	pMsg.header.set(0xD3,0x93,0);

	BYTE buffer[sizeof(PMSG_SLOT_CLAIM_LIST_SEND) + (sizeof(PMSG_SLOT_CLAIM_ROW_SEND) * SLOT_CLAIM_SIZE)];

	int size = sizeof(pMsg);
	int count = 0;

	for(int n=0;n < SLOT_CLAIM_SIZE;n++)
	{
		if(lpObj->SlotClaim[n].ItemIndex < 0 || lpObj->SlotClaim[n].Count <= 0)
		{
			continue;
		}

		PMSG_SLOT_CLAIM_ROW_SEND row;

		row.slot = (BYTE)n;
		row.ItemIndex = (WORD)lpObj->SlotClaim[n].ItemIndex;
		row.Level = (BYTE)lpObj->SlotClaim[n].Level;
		row.Count = (DWORD)lpObj->SlotClaim[n].Count;

		memcpy((buffer + size),&row,sizeof(row));
		size += sizeof(row);
		count++;
	}

	pMsg.count = (BYTE)count;
	pMsg.free = (BYTE)this->GetClaimFreeCount(aIndex);
	pMsg.result = result;

	pMsg.header.set(0xD3,0x93,size);

	memcpy(buffer,&pMsg,sizeof(pMsg));

	DataSend(aIndex,buffer,size);
}

//////////////////////////////////////////////////////////////////////
// Reward box persistence
//////////////////////////////////////////////////////////////////////

void CSlotMachine::GDSlotClaimSend(int aIndex) // OK
{
	if(gObjIsAccountValid(aIndex,gObj[aIndex].Account) == 0)
	{
		return;
	}

	// Guard against a second load overwriting a box that is already live.
	if(gObj[aIndex].SlotClaimLoad != 0)
	{
		return;
	}

	SDHP_SLOTCLAIM_RECV pMsg;

	pMsg.header.set(0xD9,0x34,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,gObj[aIndex].Account,sizeof(pMsg.account));

	memcpy(pMsg.name,gObj[aIndex].Name,sizeof(pMsg.name));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CSlotMachine::DGSlotClaimRecv(SDHP_SLOTCLAIM_SEND* lpMsg) // OK
{
	// Re-validate the account against the echoed name before applying anything.
	// Object slots are recycled on login, so a reply that arrived late could
	// otherwise be applied to whoever now occupies the slot - which for an item
	// store means moving real items between accounts.
	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGSlotClaimRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	if(lpObj->Type != OBJECT_USER)
	{
		return;
	}

	if(lpObj->SlotClaimLoad != 0)
	{
		return;
	}

	lpObj->SlotClaimLoad = 1;

	{
		SLOTCLAIM_STATE_DATA state;

		memcpy(&state,lpMsg->State,sizeof(state));

		// A never-saved blob arrives 0xFF-filled. Those would read as enormous
		// counters - an all-ones spin count would switch beginner luck off forever
		// for a brand new character - so they are treated as "no state yet".
		if(state.LossStreak == 0xFFFFFFFF)
		{
			lpObj->SlotLossStreak = 0;
			lpObj->SlotPityDate = 0;
			lpObj->SlotPityUsedToday = 0;
			lpObj->SlotSpinCount = 0;
		}
		else
		{
			lpObj->SlotLossStreak = state.LossStreak;
			lpObj->SlotPityDate = state.PityDate;
			lpObj->SlotPityUsedToday = state.PityUsedToday;
			lpObj->SlotSpinCount = state.SpinCount;
		}
	}

	for(int n=0;n < SLOT_CLAIM_SIZE;n++)
	{
		SLOTCLAIM_ROW_DATA row;

		memcpy(&row,lpMsg->SlotClaim[n],sizeof(row));

		lpObj->SlotClaim[n].ItemIndex = -1;
		lpObj->SlotClaim[n].Level = 0;
		lpObj->SlotClaim[n].Count = 0;

		// 0xFFFF is the empty marker, which is also what a 0xFF-filled blob (a
		// character with no row yet) decodes to.
		if(row.ItemIndex == 0xFFFF)
		{
			continue;
		}

		if(row.ItemIndex >= MAX_ITEM || row.Count == 0 || row.Count == 0xFFFFFFFF)
		{
			continue;
		}

		lpObj->SlotClaim[n].ItemIndex = row.ItemIndex;
		lpObj->SlotClaim[n].Level = row.Level;
		lpObj->SlotClaim[n].Count = (int)row.Count;
	}

	// Only push to the client if it is already in game - on the login path the
	// window is not open, and opening it sends the list anyway.
	if(gObjIsConnected(lpMsg->index) != false)
	{
		this->SendClaimList(lpMsg->index,SLOT_RESULT_OK);
	}
}

void CSlotMachine::GDSlotClaimSaveSend(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Type != OBJECT_USER)
	{
		return;
	}

	// Never save a box that was never loaded - that would write 8 blank rows over
	// whatever the database actually holds.
	if(lpObj->SlotClaimLoad == 0)
	{
		return;
	}

	SDHP_SLOTCLAIM_SAVE_RECV pMsg;

	pMsg.header.set(0xD9,0x35,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	memcpy(pMsg.name,lpObj->Name,sizeof(pMsg.name));

	memset(pMsg.SlotClaim,0xFF,sizeof(pMsg.SlotClaim));

	{
		SLOTCLAIM_STATE_DATA state;

		state.LossStreak = lpObj->SlotLossStreak;
		state.PityDate = lpObj->SlotPityDate;
		state.PityUsedToday = lpObj->SlotPityUsedToday;
		state.SpinCount = lpObj->SlotSpinCount;

		memcpy(pMsg.State,&state,sizeof(state));
	}

	for(int n=0;n < SLOT_CLAIM_SIZE;n++)
	{
		if(lpObj->SlotClaim[n].ItemIndex < 0 || lpObj->SlotClaim[n].Count <= 0)
		{
			continue;
		}

		SLOTCLAIM_ROW_DATA row;

		memset(&row,0,sizeof(row));

		row.ItemIndex = (WORD)lpObj->SlotClaim[n].ItemIndex;
		row.Level = (BYTE)lpObj->SlotClaim[n].Level;
		row.Reserved = 0;
		row.Count = (DWORD)lpObj->SlotClaim[n].Count;

		memcpy(pMsg.SlotClaim[n],&row,sizeof(row));
	}

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));
}
