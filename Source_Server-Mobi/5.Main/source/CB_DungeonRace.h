// CB_DungeonRace.h: Dungeon Race (server event, 1.0.50) client side - NPC window with Join / Leave and the on-screen counter.
// Server side: 4.GameServer\GameServer\DungeonRace.cpp (C1:D3:65 window, C1:D3:66 counter, C1:D3:67 buttons).
// Code drawn (no textures), same pattern as CB_KingOfKira.
#pragma once

class CB_DungeonRace
{
public:
	CB_DungeonRace();

	// Every frame, from Interface::Work().
	void DrawWindow();

	// Packet handlers (Protocol.cpp, case 0xD3).
	void RecvOpen(const BYTE* lpMsg, int size);
	void RecvHud(const BYTE* lpMsg, int size);

private:
	void SendAction(BYTE action);
	void DrawOpenWindow();
	void DrawHud();

	bool  m_Click;
	bool  m_WasDown;

	// NPC window
	bool  m_Open;
	BYTE  m_State, m_Mine, m_Winners;
	WORD  m_OpenRemain, m_Players, m_MinPlayers, m_MaxPlayers;
	DWORD m_OpenTick;

	// counter
	DWORD m_HudTick;
	BYTE  m_HudState, m_HudMine, m_HudPlace, m_HudFinished;
	WORD  m_HudRemain, m_HudPlayers;
};

// Appends a timestamped line to DungeonRaceClient.log next to Main.exe (diagnostics: is the client receiving / talking?).
void DungeonRaceLog(const char* fmt, ...);

extern CB_DungeonRace* gCB_DungeonRace;
