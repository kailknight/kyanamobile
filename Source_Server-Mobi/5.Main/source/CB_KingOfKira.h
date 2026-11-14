// CB_KingOfKira.h: King of Kira (guild war in Devias) client side - registration window, the
// on-screen score counter, and the King's guild mark for the Devias flags.
// Server side: 4.GameServer\GameServer\KingOfKira.cpp (packets C1:D3:60..64).
// Everything is code drawn (no textures), same pattern as CB_DuelRanked.
#pragma once

class CB_KingOfKira
{
public:
	CB_KingOfKira();

	// Every frame, from Interface::Work().
	void DrawWindow();

	// Packet handlers (WSclient / Protocol.cpp, case 0xD3).
	void RecvOpen(const BYTE* lpMsg, int size);
	void RecvResult(const BYTE* lpMsg, int size);
	void RecvHud(const BYTE* lpMsg, int size);
	void RecvFlag(const BYTE* lpMsg, int size);

	// The current King (for the Devias flags). Mark is 32 bytes, the same 8x8 4-bit bitmap as any guild mark.
	bool HasKing() const { return m_KingHas; }
	const char* KingName() const { return m_KingGuild; }
	const BYTE* KingMark() const { return m_KingMark; }

private:
	void ApplyFlags(bool inWorld);
	void UploadMark();
	void SendRegister(BYTE action);
	void DrawOpenWindow();
	void DrawHud(DWORD now);

	bool  m_Click;
	bool  m_WasDown;

	// registration window
	bool  m_Open;
	BYTE  m_State, m_CanRegister, m_IsMaster, m_MyRegistered;
	WORD  m_OpenRemain;
	DWORD m_OpenTick;
	struct ENTRY
	{
		char guild[9];
		BYTE allyCount;
		char allies[5][9];
	};
	enum { MAX_ENTRIES = 64, PAGE_ROWS = 3 };
	ENTRY m_Entry[MAX_ENTRIES];
	int   m_Count;
	int   m_Page;
	char  m_Message[96];
	DWORD m_MessageTick;

	// counter
	DWORD m_HudTick;
	BYTE  m_HudState, m_HudContested;
	WORD  m_HudRemain, m_HudP1, m_HudP2;
	char  m_HudG1[9], m_HudG2[9], m_HudHill[9];

	// the King
	bool  m_MarkDirty;
	bool  m_KingHas;
	char  m_KingGuild[9];
	BYTE  m_KingMark[32];
};

extern CB_KingOfKira* gCB_KingOfKira;
