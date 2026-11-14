// CB_Mining.h: Tarkan Mining (server event) client side - "Mine this stone?" window, the mining progress bar and the event line.
// Server side: 4.GameServer\GameServer\CustomMining.cpp (C1:D3:6D window/result + buttons, C1:D3:6E event line).
// Code drawn (no textures), same pattern as CB_DungeonRace. The server decides every result; this only shows it.
#pragma once

class CB_Mining
{
public:
	CB_Mining();

	// Every frame, from Interface::Work().
	void DrawWindow();

	// Packet handlers (Protocol.cpp, case 0xD3).
	void RecvState(const BYTE* lpMsg, int size);
	void RecvHud(const BYTE* lpMsg, int size);

private:
	void SendAction(BYTE action);
	void DrawStateWindow();
	void DrawHud();
	void StartBar();

	bool  m_Click;
	bool  m_WasDown;

	// window / current stone
	bool  m_Open;
	BYTE  m_Code, m_Class, m_Step, m_Steps;
	WORD  m_JewelItem, m_Pending, m_NextTotal, m_Chance, m_AttemptMs;
	BYTE  m_DurCost, m_PickDur;

	// an attempt in progress: the bar runs for AttemptMs, then the client reports "done" (action 3) and waits for the result
	bool  m_Mining;
	DWORD m_MineEnd;
	bool  m_WaitResult;
	DWORD m_WaitTick;

	// event line
	DWORD m_HudTick;
	BYTE  m_HudPhase;
	WORD  m_HudRemain;
};

extern CB_Mining* gCB_Mining;
