// CB_KingOfKira.cpp: see CB_KingOfKira.h. Code-drawn window + counter, same drawing helpers as the ranked duel overlay.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CB_KingOfKira.h"
#include "CB_DuelRanked.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "ZzzOpenglUtil.h"
#include "ZzzCharacter.h"
#include "ZzzScene.h"
#include "wsclientinline.h"
#include "Input.h"
#include "ZzzBMD.h"
#include "ZzzTexture.h"
#include "MapManager.h"
#include "GlobalBitmap.h"
#include <stdarg.h>
#include <stdio.h>

CB_KingOfKira* gCB_KingOfKira = NULL;

// Packet layouts, identical to PMSG_KOK_* in the GameServer's KingOfKira.h (packed, fixed width only).
#pragma pack(push,1)
struct KOK_RESULT_RECV { BYTE c, size, head, sub; BYTE Result; };
struct KOK_HUD_RECV { BYTE c, size, head, sub; BYTE State, Contested; WORD RemainSec, Points1, Points2; char Guild1[9]; char Guild2[9]; char OnHill[9]; };
struct KOK_FLAG_RECV { BYTE c, size, head, sub; BYTE Has; char Guild[9]; BYTE Mark[32]; };
#pragma pack(pop)

static_assert(sizeof(KOK_RESULT_RECV) == 5, "KOK result");
static_assert(sizeof(KOK_HUD_RECV) == 4+2+2+2+2+9+9+9, "KOK hud");
static_assert(sizeof(KOK_FLAG_RECV) == 4+1+9+32, "KOK flag");

namespace
{
	const DWORD kWhite = RankedUi::Col(255, 255, 255);
	const DWORD kText = RankedUi::Col(236, 228, 208);
	const DWORD kGold = RankedUi::Col(255, 200, 80);
	const DWORD kGreen = RankedUi::Col(120, 230, 130);
	const DWORD kRed = RankedUi::Col(255, 110, 110);
	const DWORD kDim = RankedUi::Col(160, 160, 166);

	const float kWinW = 290.0f, kWinH = 248.0f, kWinY = 90.0f;
	const float kHudW = 230.0f, kHudH = 62.0f, kHudY = 6.0f;
	const DWORD kHudStaleMs = 3500;

	bool PrimaryDown()
	{
#if defined(MU_TOUCH_UI)
		return SEASON3B::IsPress(VK_LBUTTON) || CInput::Instance().IsLBtnDn();
#else
		return ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	}

	void CopyName(char* dest, const char* src)
	{
		int n = 0;
		for (; n < 8 && src[n] != '\0'; n++)
		{
			dest[n] = src[n];
		}
		dest[n] = '\0';
	}

	void PublishRect(int id, bool show, float x, float y, float w, float h)
	{
		InterfaceObject& d = gInterface.Data[id];
		d.OnShow = show;
		if (show)
		{
			d.X = x;
			d.Y = y;
			d.Width = w;
			d.Height = h;
			d.FirstLoad = true;
		}
	}

	void Clock(int seconds, char* out, size_t cb)
	{
		if (seconds < 0) seconds = 0;
		snprintf(out, cb, "%02d:%02d", seconds / 60, seconds % 60);
	}
}

CB_KingOfKira::CB_KingOfKira()
{
	m_Click = false;
	m_WasDown = false;
	m_Open = false;
	m_State = m_CanRegister = m_IsMaster = m_MyRegistered = 0;
	m_OpenRemain = 0;
	m_OpenTick = 0;
	memset(m_Entry, 0, sizeof(m_Entry));
	m_Count = 0;
	m_Page = 0;
	m_Message[0] = '\0';
	m_MessageTick = 0;
	m_HudTick = 0;
	m_HudState = m_HudContested = 0;
	m_HudRemain = m_HudP1 = m_HudP2 = 0;
	m_HudG1[0] = m_HudG2[0] = m_HudHill[0] = '\0';
	m_MarkDirty = false;
	m_KingHas = false;
	m_KingGuild[0] = '\0';
	memset(m_KingMark, 0xFF, sizeof(m_KingMark));
}

//////////////////////////////////////////////////////////////////////
// Packets
//////////////////////////////////////////////////////////////////////

void CB_KingOfKira::RecvOpen(const BYTE* lpMsg, int size)
{
	// C2:D3:60, see PMSG layout comment in the GameServer's KingOfKira.h: 5 byte header, 4 bytes, WORD, count, entries (55 bytes each).
	if (size < 12 || lpMsg[0] != 0xC2)
	{
		return;
	}

	const BYTE* d = lpMsg + 5;
	const int entrySize = 9 + 1 + 5 * 9;
	int count = d[7];

	if (count > MAX_ENTRIES || size < 5 + 8 + count * entrySize)
	{
		return;
	}

	m_Open = true;
	m_State = d[0];
	m_CanRegister = d[1];
	m_IsMaster = d[2];
	m_MyRegistered = d[3];
	m_OpenRemain = (WORD)(d[4] | (d[5] << 8));
	m_OpenTick = GetTickCount();
	m_Count = count;
	m_Page = 0;
	m_Message[0] = '\0';

	const BYTE* e = d + 8;

	for (int n = 0; n < count; n++)
	{
		memcpy(m_Entry[n].guild, e, 9);
		m_Entry[n].guild[8] = '\0';
		m_Entry[n].allyCount = (e[9] > 5) ? 5 : e[9];
		memcpy(m_Entry[n].allies, e + 10, 45);

		for (int a = 0; a < 5; a++)
		{
			m_Entry[n].allies[a][8] = '\0';
		}

		e += entrySize;
	}
}

void CB_KingOfKira::RecvResult(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(KOK_RESULT_RECV))
	{
		return;
	}

	const char* text = "";

	switch (((const KOK_RESULT_RECV*)lpMsg)->Result)
	{
	case 0: text = "Your guild and its alliance are registered. Reopen the NPC to see the list."; m_MyRegistered = 1; break;
	case 1: text = "Registration is not open right now."; break;
	case 2: text = "Two guilds are already registered."; break;
	case 3: text = "Your guild is already registered."; break;
	case 4: text = "Your alliance already has a registered guild."; break;
	case 5: text = "Only a guild master can register."; break;
	case 6: text = "You have no guild."; break;
	case 7: text = "Registration withdrawn."; m_MyRegistered = 0; break;
	case 8: text = "King of Kira is switched off."; break;
	case 9: text = "The registration list is full."; break;
	default: text = "Unknown answer."; break;
	}

	snprintf(m_Message, sizeof(m_Message), "%s", text);
	m_MessageTick = GetTickCount();
}

void CB_KingOfKira::RecvHud(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(KOK_HUD_RECV))
	{
		return;
	}

	const KOK_HUD_RECV* p = (const KOK_HUD_RECV*)lpMsg;

	m_HudTick = GetTickCount();
	m_HudState = p->State;
	m_HudContested = p->Contested;
	m_HudRemain = p->RemainSec;
	m_HudP1 = p->Points1;
	m_HudP2 = p->Points2;
	CopyName(m_HudG1, p->Guild1);
	CopyName(m_HudG2, p->Guild2);
	CopyName(m_HudHill, p->OnHill);
}

void CB_KingOfKira::RecvFlag(const BYTE* lpMsg, int size)
{

	if (size < (int)sizeof(KOK_FLAG_RECV))
	{
		return;
	}

	const KOK_FLAG_RECV* p = (const KOK_FLAG_RECV*)lpMsg;

	char before[9];
	memcpy(before, m_KingGuild, sizeof(before));

	m_KingHas = (p->Has != 0);
	CopyName(m_KingGuild, p->Guild);

	// a new King while playing: say so (also confirms the flag packet arrived)
	if (m_KingHas && before[0] != '\0' && strcmp(before, m_KingGuild) != 0 && g_pChatListBox != NULL)
	{
		char line[96];
		snprintf(line, sizeof(line), "King of Kira: the flags of Devias now carry the mark of %s.", m_KingGuild);
		g_pChatListBox->AddText("", line, SEASON3B::TYPE_SYSTEM_MESSAGE);
	}
	memcpy(m_KingMark, p->Mark, sizeof(m_KingMark));
	m_MarkDirty = true;
}

void CB_KingOfKira::SendRegister(BYTE action)
{
	// C1:D3:61, UNENCRYPTED like every 0xD3 packet (see wsclientinline.h SendClientBuild).
	CStreamPacketEngine spe;
	spe.Init(0xC1, 0xD3);
	spe << (BYTE)0x61;
	spe << (BYTE)action;
	spe.Send();
}

//////////////////////////////////////////////////////////////////////
// Drawing
//////////////////////////////////////////////////////////////////////

void CB_KingOfKira::DrawOpenWindow()
{
	const float x = (float)MAX_WIN_WIDTH * 0.5f - kWinW * 0.5f;
	const float y = kWinY;

	PublishRect(eWindowKingOfKira, true, x, y, kWinW, kWinH);

	if (RankedUi::Inside(x, y, kWinW, kWinH) && g_pBCustomMenuInfo != NULL)
	{
		g_pBCustomMenuInfo->SetBlockCur(TRUE);
	}

	RankedUi::Bar(x, y, kWinW, kWinH, RankedUi::Col(12, 12, 16, 240));
	RankedUi::Outline(x, y, kWinW, kWinH, RankedUi::Col(214, 170, 70));
	RankedUi::Outline(x + 2.0f, y + 2.0f, kWinW - 4.0f, kWinH - 4.0f, RankedUi::Col(70, 58, 34));
	RankedUi::DrawDiamond(x + 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::DrawDiamond(x + kWinW - 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::Text(g_hFontBold, x, y + 9.0f, kWinW, RT3_SORT_CENTER, kGold, "KING OF KIRA");

	const char* status = "Not scheduled.";
	DWORD statusColor = kDim;
	char line[96];
	char clock[16];

	int left = (int)m_OpenRemain - (int)((GetTickCount() - m_OpenTick) / 1000);

	if (m_State == 2)
	{
		Clock(left, clock, sizeof(clock));
		snprintf(line, sizeof(line), "Registration is open - war starts in %s", clock);
		status = line;
		statusColor = kGreen;
	}
	else if (m_State == 5 || m_State == 1)
	{
		Clock(left, clock, sizeof(clock));
		snprintf(line, sizeof(line), "Registration is closed - war starts in %s", clock);
		status = line;
	}
	else if (m_State == 3 || m_State == 4)
	{
		status = "King of Kira is being fought right now.";
		statusColor = kRed;
	}

	RankedUi::Text(g_hFont, x, y + 28.0f, kWinW, RT3_SORT_CENTER, statusColor, status);

	// the current owner (the King's guild), from the flag packet the server sends in Devias
	char owner[64];
	if (m_KingHas && m_KingGuild[0] != '\0')
	{
		snprintf(owner, sizeof(owner), "Current King: %s", m_KingGuild);
		RankedUi::Text(g_hFontBold, x, y + 44.0f, kWinW, RT3_SORT_CENTER, kGold, owner);
	}
	else
	{
		RankedUi::Text(g_hFont, x, y + 44.0f, kWinW, RT3_SORT_CENTER, kDim, "Current King: none yet");
	}

	char head[64];
	snprintf(head, sizeof(head), "Registered guilds: %d (at least 2 needed, all of them fight)", m_Count);
	RankedUi::Text(g_hFont, x, y + 60.0f, kWinW, RT3_SORT_CENTER, kDim, head);

	// the list: guild, then its allies (like the Castle Siege registration list)
	const int pages = (m_Count + PAGE_ROWS - 1) / PAGE_ROWS;

	if (m_Page >= pages) m_Page = (pages > 0) ? pages - 1 : 0;

	const float rowH = 34.0f;
	const float listY = y + 76.0f;

	for (int r = 0; r < PAGE_ROWS; r++)
	{
		const int n = m_Page * PAGE_ROWS + r;
		const float ry = listY + r * rowH;

		RankedUi::Bar(x + 12.0f, ry, kWinW - 24.0f, rowH - 3.0f, RankedUi::Col(24, 24, 30, 220));

		if (n >= m_Count)
		{
			continue;
		}

		RankedUi::Text(g_hFontBold, x + 18.0f, ry + 3.0f, kWinW - 36.0f, RT3_SORT_LEFT, kWhite, m_Entry[n].guild);

		char allies[160] = "";

		for (int a = 0; a < m_Entry[n].allyCount; a++)
		{
			if (a > 0) strncat(allies, ", ", sizeof(allies) - strlen(allies) - 1);
			strncat(allies, m_Entry[n].allies[a], sizeof(allies) - strlen(allies) - 1);
		}

		if (allies[0] == '\0') strcpy(allies, "no alliance");

		char allyLine[190];
		snprintf(allyLine, sizeof(allyLine), "Allies: %s", allies);
		RankedUi::Text(g_hFont, x + 18.0f, ry + 17.0f, kWinW - 36.0f, RT3_SORT_LEFT, kDim, allyLine);
	}

	// paging
	const float pageY = listY + PAGE_ROWS * rowH + 2.0f;
	const float arrowW = 28.0f;

	if (pages > 1)
	{
		RankedUi::DrawButton(x + 12.0f, pageY, arrowW, 18.0f, "<", 0, m_Page > 0, true);
		RankedUi::DrawButton(x + kWinW - 12.0f - arrowW, pageY, arrowW, 18.0f, ">", 0, m_Page < pages - 1, true);

		char pg[24];
		snprintf(pg, sizeof(pg), "%d / %d", m_Page + 1, pages);
		RankedUi::Text(g_hFont, x, pageY + 3.0f, kWinW, RT3_SORT_CENTER, kDim, pg);

		if (m_Click)
		{
			if (m_Page > 0 && RankedUi::Inside(x + 12.0f, pageY, arrowW, 18.0f)) m_Page--;
			else if (m_Page < pages - 1 && RankedUi::Inside(x + kWinW - 12.0f - arrowW, pageY, arrowW, 18.0f)) m_Page++;
		}
	}

	if (m_Message[0] != '\0' && (GetTickCount() - m_MessageTick) < 6000)
	{
		RankedUi::Text(g_hFont, x, y + kWinH - 62.0f, kWinW, RT3_SORT_CENTER, kGold, m_Message);
	}
	else if (m_IsMaster == 0)
	{
		RankedUi::Text(g_hFont, x, y + kWinH - 62.0f, kWinW, RT3_SORT_CENTER, kDim, "Only a guild master can register.");
	}
	else if (m_MyRegistered == 2)
	{
		RankedUi::Text(g_hFont, x, y + kWinH - 62.0f, kWinW, RT3_SORT_CENTER, kDim, "Your alliance is already registered.");
	}

	const float btnY = y + kWinH - 40.0f;
	const float btnW = (kWinW - 32.0f - 20.0f) / 3.0f;
	const float regX = x + 16.0f;
	const float wdX = regX + btnW + 10.0f;
	const float closeX = wdX + btnW + 10.0f;

	const bool canReg = (m_IsMaster != 0 && m_State == 2 && m_MyRegistered == 0);
	const bool canWd = (m_IsMaster != 0 && m_State == 2 && m_MyRegistered == 1);

	RankedUi::DrawButton(regX, btnY, btnW, 30.0f, "Register", 1, canReg, true);
	RankedUi::DrawButton(wdX, btnY, btnW, 30.0f, "Withdraw", 2, canWd, true);
	RankedUi::DrawButton(closeX, btnY, btnW, 30.0f, "Close", 0, true, true);

	if (m_Click)
	{
		if (canReg && RankedUi::Inside(regX, btnY, btnW, 30.0f))
		{
			SendRegister(0);
		}
		else if (canWd && RankedUi::Inside(wdX, btnY, btnW, 30.0f))
		{
			SendRegister(1);
		}
		else if (RankedUi::Inside(closeX, btnY, btnW, 30.0f))
		{
			m_Open = false;
		}
	}
}

void CB_KingOfKira::DrawHud(DWORD now)
{
	const float x = (float)MAX_WIN_WIDTH * 0.5f - kHudW * 0.5f;
	const float y = kHudY;

	RankedUi::Bar(x, y, kHudW, kHudH, RankedUi::Col(12, 12, 16, 200));
	RankedUi::Outline(x, y, kHudW, kHudH, RankedUi::Col(214, 170, 70));

	char clock[16];
	Clock((int)m_HudRemain, clock, sizeof(clock));

	char title[48];
	snprintf(title, sizeof(title), "KING OF KIRA   %s", clock);
	RankedUi::Text(g_hFontBold, x, y + 4.0f, kHudW, RT3_SORT_CENTER, kGold, title);

	char left[40], right[40];
	snprintf(left, sizeof(left), "%s  %d", m_HudG1, (int)m_HudP1);
	snprintf(right, sizeof(right), "%d  %s", (int)m_HudP2, m_HudG2);
	RankedUi::Text(g_hFontBold, x + 6.0f, y + 22.0f, kHudW * 0.5f - 8.0f, RT3_SORT_LEFT, kWhite, left);
	RankedUi::Text(g_hFontBold, x + kHudW * 0.5f + 2.0f, y + 22.0f, kHudW * 0.5f - 8.0f, RT3_SORT_RIGHT, kWhite, right);

	char hill[64];
	DWORD hillColor = kDim;

	if (m_HudState == 4)
	{
		snprintf(hill, sizeof(hill), "The war is over - leaving Devias in %s", clock);
		hillColor = kGreen;
	}
	else if (m_HudContested)
	{
		snprintf(hill, sizeof(hill), "Contested! Counting stopped");
		hillColor = kRed;
	}
	else if (m_HudHill[0] != '\0')
	{
		snprintf(hill, sizeof(hill), "On the spot: %s (+1 / second)", m_HudHill);
		hillColor = kGreen;
	}
	else
	{
		snprintf(hill, sizeof(hill), "Nobody holds the spot");
	}

	RankedUi::Text(g_hFont, x, y + 42.0f, kHudW, RT3_SORT_CENTER, hillColor, hill);
}

void CB_KingOfKira::DrawWindow()
{
	const DWORD now = GetTickCount();

	const bool down = PrimaryDown();
	m_Click = down && (m_WasDown == false);
	m_WasDown = down;

	const bool inWorld = (Hero != NULL && SceneFlag == MAIN_SCENE);

	if (inWorld == false)
	{
		m_Open = false;
		m_HudTick = 0;
	}

	// The window closes by itself when the player walks away from the NPC.
	const bool showHud = (m_HudTick != 0 && (DWORD)(now - m_HudTick) < kHudStaleMs && inWorld);

	ApplyFlags(inWorld);

	if (m_Open == false)
	{
		PublishRect(eWindowKingOfKira, false, 0, 0, 0, 0);
	}

	if (m_Open == false && showHud == false)
	{
		return;
	}

	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (showHud)
	{
		DrawHud(now);
	}

	if (m_Open)
	{
		DrawOpenWindow();
	}
}

//////////////////////////////////////////////////////////////////////
// The King's guild mark on the Devias flags
//
// The flags are the banner objects with the "badge_0N.tga" emblem texture (Devias object types 83-85: spear + banner,
// found by decoding Object84-86.bmd of Data/Object3: textures Javelins03.jpg + badge_0N.tga). Their emblem mesh is
// pointed at BITMAP_KOK_FLAG, a private copy of guild.tga that holds the King's mark. No model file changes.
//////////////////////////////////////////////////////////////////////

extern int CachTexture; // ZzzOpenglUtil.cpp

// The flag's own GL texture. The texture manager keeps zeroing Bitmaps[BITMAP_KOK_FLAG].TextureNumber (it is a plain guild.tga
// slot as far as it knows), and a flag drawn with texture 0 is white: a debug log showed the slot at 0 most of the time
// while the uploads themselves were correct. So the slot is pointed back at this texture on every frame.
static GLuint s_kokOwnTexture = 0;

void CB_KingOfKira::UploadMark()
{
	static const unsigned int colors[16] =
	{
		(255u<<24)+(40<<16)+(32<<8)+(28), // 0 = empty cell of the mark: opaque dark background, so every mark colour (yellow, white too) stands out
		(255u<<24)+(0<<16)+(0<<8)+(0),
		(255u<<24)+(128<<16)+(128<<8)+(128),
		(255u<<24)+(255<<16)+(255<<8)+(255),
		(255u<<24)+(0<<16)+(0<<8)+(255),
		(255u<<24)+(0<<16)+(128<<8)+(255),
		(255u<<24)+(0<<16)+(255<<8)+(255),
		(255u<<24)+(0<<16)+(255<<8)+(128),
		(255u<<24)+(0<<16)+(255<<8)+(0),
		(255u<<24)+(128<<16)+(255<<8)+(0),
		(255u<<24)+(255<<16)+(255<<8)+(0),
		(255u<<24)+(255<<16)+(128<<8)+(0),
		(255u<<24)+(255<<16)+(0<<8)+(0),
		(255u<<24)+(255<<16)+(0<<8)+(128),
		(255u<<24)+(255<<16)+(0<<8)+(255),
		(255u<<24)+(128<<16)+(0<<8)+(255),
	};

	BITMAP_t* b = &Bitmaps[BITMAP_KOK_FLAG];

	// The flag gets a GL texture of its own: nothing else may draw into it (the guild marks above the players' heads
	// are drawn into a shared texture all the time).
	// (it may also have been deleted along with the slot, then a new one is made)
	if (s_kokOwnTexture != 0 && !glIsTexture(s_kokOwnTexture))
	{
		s_kokOwnTexture = 0;
	}

	if (s_kokOwnTexture == 0)
	{
		glGenTextures(1, &s_kokOwnTexture);
	}

	if (s_kokOwnTexture == 0)
	{
		return;
	}

	b->TextureNumber = s_kokOwnTexture;
	b->Width = 8;
	b->Height = 8;
	b->Components = 4;

	// our own pixel buffer: the bitmap's CPU copy can be released or reloaded by the texture manager, which used to
	// leave the flag with the plain guild.tga until the next restart
	static unsigned int out[64];

	for (int i = 0; i < 64; i++)
	{
		const BYTE v = (i % 2 == 0) ? ((m_KingMark[i / 2] >> 4) & 0x0f) : (m_KingMark[i / 2] & 0x0f);
		out[i] = colors[v];
	}

	glBindTexture(GL_TEXTURE_2D, b->TextureNumber);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
	// only level 0 is replaced: no mipmaps, or a far flag would keep showing the old mark
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// the bind above bypassed BindTexture(): forget its cached texture so the next BindTexture really binds
	CachTexture = -1;

}

void CB_KingOfKira::ApplyFlags(bool inWorld)
{
	// Original texture indices of the emblem meshes, so the stock banner comes back when there is no King.
	static GLuint original[3][8];
	static bool saved[3][8];
	static bool initialised = false;

	if (initialised == false)
	{
		memset(original, 0, sizeof(original));
		memset(saved, 0, sizeof(saved));
		initialised = true;
	}

	if (Models == NULL)
	{
		return;
	}

	const bool want = (inWorld && m_KingHas && gMapManager.WorldActive == WD_2DEVIAS);

	// upload on a new King, and again once a second while the flags show it: a reloaded/evicted texture heals by itself
	static DWORD lastUpload = 0;

	if (want && (m_MarkDirty || (DWORD)(GetTickCount() - lastUpload) >= 1000))
	{
		UploadMark();
		m_MarkDirty = false;
		lastUpload = GetTickCount();
	}

	if (want && s_kokOwnTexture != 0)
	{
		BITMAP_t* flagBitmap = &Bitmaps[BITMAP_KOK_FLAG];
		flagBitmap->TextureNumber = s_kokOwnTexture;
		flagBitmap->Width = 8;
		flagBitmap->Height = 8;
		flagBitmap->Components = 4;
	}

	for (int k = 0; k < 3; k++)
	{
		BMD* b = &Models[83 + k];

		if (b->Meshs == NULL || b->Textures == NULL || b->IndexTexture == NULL || b->NumMeshs <= 0)
		{
			continue;
		}

		for (int t = 0; t < b->NumMeshs && t < 8; t++)
		{
			if (_strnicmp(b->Textures[t].FileName, "badge", 5) != 0)
			{
				continue;
			}

			if (want)
			{
				if (b->IndexTexture[t] != (GLuint)BITMAP_KOK_FLAG)
				{
					original[k][t] = b->IndexTexture[t];
					saved[k][t] = true;
					b->IndexTexture[t] = (GLuint)BITMAP_KOK_FLAG;
				}
			}
			else if (b->IndexTexture[t] == (GLuint)BITMAP_KOK_FLAG && saved[k][t])
			{
				b->IndexTexture[t] = original[k][t];
			}
		}
	}
}
