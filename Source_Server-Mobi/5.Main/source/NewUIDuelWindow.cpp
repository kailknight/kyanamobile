// NewUIDuelWindow.cpp: implementation of the CNewUIDuelWindow class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIDuelWindow.h"
#include "ZzzTexture.h"
#include "ZzzInventory.h"
#include "ZzzBMD.h"
#include "ZzzCharacter.h"
#include "UIControls.h"
#include "DuelMgr.h"
#include "NewUIRenderNumber.h"

using namespace SEASON3B;

SEASON3B::CNewUIDuelWindow::CNewUIDuelWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = m_Pos.y = 0;
}

SEASON3B::CNewUIDuelWindow::~CNewUIDuelWindow()
{
	Release();
}

bool SEASON3B::CNewUIDuelWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (NULL == pNewUIMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_DUEL_WINDOW, this);

	SetPos(x, y);

	LoadImages();

	Show(false);

	return true;
}

void SEASON3B::CNewUIDuelWindow::Release()
{
	if(m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj( this );
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIDuelWindow::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

bool SEASON3B::CNewUIDuelWindow::UpdateMouseEvent()
{
	return true;
}

bool SEASON3B::CNewUIDuelWindow::UpdateKeyEvent()
{
	return true;
}

bool SEASON3B::CNewUIDuelWindow::Update()
{
	return true;
}

bool SEASON3B::CNewUIDuelWindow::Render()
{
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	RenderFrame();
	RenderContents();

	DisableAlphaBlend();

	return true;
}

void SEASON3B::CNewUIDuelWindow::RenderFrame()
{
	RenderImage(IMAGE_DUEL_BACK, m_Pos.x, m_Pos.y, 131, 70);
}

void SEASON3B::CNewUIDuelWindow::RenderContents()
{
	unicode::t_char strMyScore[12];
	unicode::t_char strDuelScore[12];
	unicode::_sprintf(strMyScore, "%d", g_DuelMgr.GetScore(DUEL_HERO));
	unicode::_sprintf(strDuelScore, "%d", g_DuelMgr.GetScore(DUEL_ENEMY));

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(0, 0, 0, 255);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(0, 150, 255, 255);
	g_pRenderText->RenderText(m_Pos.x+55, m_Pos.y+33, g_DuelMgr.GetDuelPlayerID(DUEL_HERO));
	g_pRenderText->RenderText(m_Pos.x+31, m_Pos.y+33, strMyScore);
	g_pRenderText->SetTextColor(255, 25, 25, 255);
	g_pRenderText->RenderText(m_Pos.x+55, m_Pos.y+56, g_DuelMgr.GetDuelPlayerID(DUEL_ENEMY));
	g_pRenderText->RenderText(m_Pos.x+31, m_Pos.y+56, strDuelScore);
}

float SEASON3B::CNewUIDuelWindow::GetLayerDepth()
{
	return 1.1f;
}

void SEASON3B::CNewUIDuelWindow::LoadImages()
{
	LoadBitmap("Interface\\newui_Figure_ground.tga", IMAGE_DUEL_BACK, GL_LINEAR);
	// The kill score's digits (RenderDuelKillScore below). CNewUIRenderNumber
	// only loads them the first time something asks it for its instance, and
	// nothing ever does, so the score drew as plain tinted boxes.
	LoadBitmap("Interface\\newui_number1.tga", CNewUIRenderNumber::IMAGE_NUMBER1, GL_LINEAR);
}

void SEASON3B::CNewUIDuelWindow::UnloadImages()
{
	DeleteBitmap(IMAGE_DUEL_BACK);
	DeleteBitmap(CNewUIRenderNumber::IMAGE_NUMBER1);
}

// -----------------------------------------------------------------------------
// Duel kill score
//
// Every time someone scores in a duel the score is shown big in the middle of
// the screen - the hero's name and score in blue on the left, the opponent's in
// red on the right - fading in, holding for kDuelKillScoreHoldMs and fading out.
// It replaces the small score box this window used to draw in the corner
// (ReceiveDuelStart no longer shows that box).
//
// The names and scores are copied when the kill is reported, so the final kill
// of a duel still shows after DuelMgr has been reset by the duel's end.
// -----------------------------------------------------------------------------

extern int DisplayWinMid;
extern int DisplayHeight;

namespace
{
	const DWORD kDuelKillScoreFadeInMs = 250;
	const DWORD kDuelKillScoreHoldMs = 2000;
	const DWORD kDuelKillScoreFadeOutMs = 600;

	DWORD g_DuelKillScoreTick = 0;          // when the last kill was shown, 0 = nothing showing
	int g_DuelKillScoreLastTotal = 0;       // both scores added up, to tell a kill from a repeat
	char g_DuelKillScoreHero[MAX_ID_SIZE + 1] = { 0 };
	char g_DuelKillScoreEnemy[MAX_ID_SIZE + 1] = { 0 };
	int g_DuelKillScoreHeroScore = 0;
	int g_DuelKillScoreEnemyScore = 0;

	// The interface's bitmap digits, big and tinted.
	void RenderDuelKillScoreNumber(float centerX, float y, int number, float r, float g, float b, float a)
	{
		const float scale = 4.0f;
		const float width = 12.f * (scale - 0.3f);
		const float height = 16.f * (scale - 0.3f);

		char text[16];
		sprintf(text, "%d", number);
		const int length = (int)strlen(text);

		float x = centerX - ((width * 0.8f * (float)(length - 1)) + width) * 0.5f;

		EnableAlphaTest();
		glColor4f(r, g, b, a);

		for (int i = 0; i < length; ++i)
		{
			const float u = (float)(text[i] - '0') * 12.f / 128.f;
			RenderBitmap(SEASON3B::CNewUIRenderNumber::IMAGE_NUMBER1, x, y, width, height, u, 0.f, 12.f / 128.f, 14.f / 16.f);
			x += width * 0.8f;
		}

		glColor4f(1.f, 1.f, 1.f, 1.f);
	}
}

// A new duel: forget the last one's total so its first kill shows.
void ResetDuelKillScore()
{
	g_DuelKillScoreLastTotal = 0;
}

// From ReceiveDuelScore, after DuelMgr has the new scores.
void NotifyDuelKillScore(int heroScore, int enemyScore)
{
	const int total = heroScore + enemyScore;

	// A different pair, or a total that went down, is a new duel even without a
	// start packet: a spectator in a duel room never gets one, so after watching
	// a long duel the next one's kills never beat the old total and never showed.
	if (total < g_DuelKillScoreLastTotal
		|| strncmp(g_DuelKillScoreHero, g_DuelMgr.GetDuelPlayerID(DUEL_HERO), MAX_ID_SIZE) != 0
		|| strncmp(g_DuelKillScoreEnemy, g_DuelMgr.GetDuelPlayerID(DUEL_ENEMY), MAX_ID_SIZE) != 0)
	{
		g_DuelKillScoreLastTotal = 0;
	}

	if (total > g_DuelKillScoreLastTotal)
	{
		strncpy(g_DuelKillScoreHero, g_DuelMgr.GetDuelPlayerID(DUEL_HERO), MAX_ID_SIZE);
		g_DuelKillScoreHero[MAX_ID_SIZE] = 0;
		strncpy(g_DuelKillScoreEnemy, g_DuelMgr.GetDuelPlayerID(DUEL_ENEMY), MAX_ID_SIZE);
		g_DuelKillScoreEnemy[MAX_ID_SIZE] = 0;
		g_DuelKillScoreHeroScore = heroScore;
		g_DuelKillScoreEnemyScore = enemyScore;
		g_DuelKillScoreTick = GetTickCount();
	}

	g_DuelKillScoreLastTotal = total;
}

// Every frame (CNewUIBCustomMenuInfo::Render). Draws nothing when no kill is showing.
void RenderDuelKillScore()
{
	if (g_DuelKillScoreTick == 0)
	{
		return;
	}

	const DWORD elapsed = GetTickCount() - g_DuelKillScoreTick;

	if (elapsed >= kDuelKillScoreFadeInMs + kDuelKillScoreHoldMs + kDuelKillScoreFadeOutMs)
	{
		g_DuelKillScoreTick = 0;
		return;
	}

	float alpha = 1.f;

	if (elapsed < kDuelKillScoreFadeInMs)
	{
		alpha = (float)elapsed / (float)kDuelKillScoreFadeInMs;
	}
	else if (elapsed > kDuelKillScoreFadeInMs + kDuelKillScoreHoldMs)
	{
		alpha = 1.f - ((float)(elapsed - kDuelKillScoreFadeInMs - kDuelKillScoreHoldMs) / (float)kDuelKillScoreFadeOutMs);
	}

	const DWORD a = (DWORD)(alpha * 255.f);
	// The real middle of the interface: 320 on mobile, further right on a
	// widescreen PC, where the interface is wider than 640.
	const float centerX = (float)DisplayWinMid;
	const float nameY = (float)DisplayHeight * 0.40f;
	const float scoreY = nameY + 16.f;

	// Names over their scores: hero blue, opponent red. TextDraw takes its
	// colour as 0xAABBGGRR.
	TextDraw(g_hFontBold, (int)centerX - 150, (int)nameY, (a << 24) | 0x00FF9933, 0x0, 130, 0, 3, "%s", g_DuelKillScoreHero);
	TextDraw(g_hFontBold, (int)centerX + 20, (int)nameY, (a << 24) | 0x002E2EE6, 0x0, 130, 0, 3, "%s", g_DuelKillScoreEnemy);

	RenderDuelKillScoreNumber(centerX - 85.f, scoreY, g_DuelKillScoreHeroScore, 0.25f, 0.6f, 1.f, alpha);
	RenderDuelKillScoreNumber(centerX + 85.f, scoreY, g_DuelKillScoreEnemyScore, 0.95f, 0.2f, 0.2f, alpha);

	// The colon between them: two small white squares.
	EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, alpha);
	RenderColor(centerX - 4.f, scoreY + 14.f, 8.f, 8.f);
	RenderColor(centerX - 4.f, scoreY + 34.f, 8.f, 8.f);
	EndRenderColor();
	glColor4f(1.f, 1.f, 1.f, 1.f);
}
