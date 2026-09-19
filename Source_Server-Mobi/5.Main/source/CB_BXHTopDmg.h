#pragma once
#include "Protocol.h"

#include "NewUIBase.h"
#include "NewUIManager.h"
#include "NewUIMessageBox.h"
#include "NewUIMyInventory.h"
#include "NewUIButton.h"
#include "NewUIScrollBar.h"
#include "NewUITextBox.h"


using namespace SEASON3B;

class CB_BXHTopDmg
{

public:
	struct DATA_BXH_SEND
	{
		char Name[11];
		QWORD damage;
	};
	struct PMSG_COUNT_SEND_DATA
	{
		PSWMSG_HEAD header; // C2:F3:E2
		BYTE count;
		WORD ClassMonter;

		// Mirrors the same struct in the GameServer's Monster.cpp. Must stay
		// LAST and stay in step with it - the DATA_BXH_SEND rows that follow
		// are addressed as sizeof(*this) + n * sizeof(row).
		QWORD Life;
		QWORD MaxLife;
	};

	CB_BXHTopDmg();
	~CB_BXHTopDmg();
	void RecvProtocol(BYTE* Recv);

	int ClassMonter;

	// The boss's health as of the last damage-table refresh. MaxLife stays 0
	// until a packet arrives, which suppresses the bar rather than drawing an
	// empty one.
	QWORD Life;
	QWORD MaxLife;

	// When the last packet landed, and which map we were on at the time.
	//
	// The server only sends this to players on the boss's map, within 20
	// tiles, who are in its hit list - so walking away or warping simply stops
	// the packets. Nothing tells the client that happened, so without these
	// the last packet's contents sat on screen forever.
	DWORD LastRecvTick;
	int   RecvWorld;

	std::vector<CB_BXHTopDmg::DATA_BXH_SEND> m_DataTopBXHDMG;
	void DrawWindowMini();

	// The boss HP bar: name and a proportional bar, moved as one block. Fed by
	// the same packet as the damage table below.
	void DrawBossHpBar();

	// Whether that bar is on screen right now. Its own draw uses this, and so
	// does the over-head health plate, so the two cannot disagree about
	// whether a boss is being shown.
	bool IsBossHpBarActive();

	// True when the big bar is up AND it belongs to this monster class - the
	// test the over-head plate uses to stand down for that one boss while
	// still drawing for everything else on screen.
	bool IsBossHpBarShownFor(int MonsterClass);
};

extern CB_BXHTopDmg* gCB_BXHTopDmg;