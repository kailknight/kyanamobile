//////////////////////////////////////////////////////////////////////
// NewUIQuickCommandWindow.cpp: implementation of the CNewUIQuickCommandWindow class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIQuickCommandWindow.h"
#include "NewUISystem.h"
// For gProtect.m_MainInfo.HidePlayerMenu and the PLAYER_MENU_HIDE_* bits - not
// reachable via stdafx.h.
#include "Protect.h"
#include "DSPlaySound.h"
#if(CB_VIEWCHARITEM)
#include "CB_ViewCharItem.h"
#endif
using namespace SEASON3B;

SEASON3B::CNewUIQuickCommandWindow::CNewUIQuickCommandWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;

	m_iSelectedIndex = -1;
	m_iSelectedCharacterIndex = -1;

	// Every entry visible until the first OpenQuickCommand consults the mask, so
	// a stray render before the window is ever opened cannot walk a garbage map.
	for (int i = 0; i < QCE_COUNT; ++i)
	{
		m_aVisibleEntry[i] = i;
	}
	m_nVisibleCount = QCE_COUNT;
}

SEASON3B::CNewUIQuickCommandWindow::~CNewUIQuickCommandWindow()
{
	Release();
}

bool SEASON3B::CNewUIQuickCommandWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if( NULL == pNewUIMng )
		return false;
	
	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_QUICK_COMMAND, this);

	LoadImages();
	
	SetPos(x, y);

	Show(false);

	return true;
}

void SEASON3B::CNewUIQuickCommandWindow::Release()
{
	UnloadImages();

	if(m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj( this );
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIQuickCommandWindow::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

bool SEASON3B::CNewUIQuickCommandWindow::UpdateMouseEvent()
{
	if(m_iSelectedCharacterIndex < 0)
	{
		return true;
	}

	POINT pt = { m_Pos.x, m_Pos.y+38 };

	for(int i = 0; i < m_nVisibleCount; ++i)
	{
		if(CheckMouseIn(pt.x, pt.y, 112, 19) == true)
		{
			m_iSelectedIndex = i;
			break;
		}

		pt.y += 20.f;
	}

	if(m_iSelectedIndex > -1 && m_iSelectedIndex < m_nVisibleCount && SEASON3B::IsRelease(VK_LBUTTON))
	{
		// The clicked ROW, mapped back to the action it actually stands for -
		// with rows hidden they are no longer the same number.
		switch(m_aVisibleEntry[m_iSelectedIndex])
		{
		case QCE_TRADE:
			{
				CHARACTER* pCha = &CharactersClient[m_iSelectedCharacterIndex];
				g_pCommandWindow->CommandTrade(pCha);
				CloseQuickCommand();

				return false;
			}
			break;
		case QCE_BUY:
			{
				CHARACTER* pCha = &CharactersClient[m_iSelectedCharacterIndex];
				g_pCommandWindow->CommandPurchase(pCha);
				CloseQuickCommand();

				return false;
			}
			break;
		case QCE_PARTY:
			{
				CHARACTER* pCha = &CharactersClient[m_iSelectedCharacterIndex];
				g_pCommandWindow->CommandParty(pCha->Key);
				CloseQuickCommand();
				
				return false;
			}
			break;
		case QCE_FOLLOW:
			{
				g_pCommandWindow->CommandFollow(m_iSelectedCharacterIndex);
				CloseQuickCommand();

				return false;
			}
			break;
		case QCE_DUEL:
			{
				CHARACTER* pCha = &CharactersClient[m_iSelectedCharacterIndex];
				g_pCommandWindow->CommandDual(pCha);
				CloseQuickCommand();

				return false;
			}
			break;
#if(CB_VIEWCHARITEM)
		case QCE_VIEWITEM:
		{
			CHARACTER* pCha = &CharactersClient[m_iSelectedCharacterIndex];
			if(gCB_ViewCharItem) gCB_ViewCharItem->SendRequestViewItem(pCha->Key);
			CloseQuickCommand();
			return false;
		}
		break;
#endif
		}
	}

	// Both boxes were hardcoded to the six-row height (130 and 160). With rows
	// hidden they have to shrink with the art, or a click in the empty space
	// below a short menu still counts as inside it - the menu would neither
	// highlight anything nor close.
	if(CheckMouseIn(m_Pos.x, m_Pos.y+30, 112, (19 * GetVisibleRowCount()) + 16) == false)
	{
		m_iSelectedIndex = -1;

		if(SEASON3B::IsRelease(VK_LBUTTON))
		{
			CloseQuickCommand();
			return false;
		}
	}

	if(CheckMouseIn(m_Pos.x, m_Pos.y, 112, (int)(95.f + GetMenuFillerHeight())))
	{
		return false;
	}

	return true;
}

bool SEASON3B::CNewUIQuickCommandWindow::UpdateKeyEvent()
{
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_QUICK_COMMAND) == true)
	{
		if(SEASON3B::IsPress(VK_ESCAPE) == true)
		{
			CloseQuickCommand();
			PlayBuffer(SOUND_CLICK01);

			return false;
		}
	}

	return true;
}

bool SEASON3B::CNewUIQuickCommandWindow::Update()
{
	if(m_iSelectedCharacterIndex >= 0)
	{
		CHARACTER* pCha = &CharactersClient[m_iSelectedCharacterIndex];	

		if(strcmp(pCha->ID, m_strID) != 0	
			|| pCha->Object.Live == false
			|| pCha->Object.Kind != KIND_PLAYER)
		{
			CloseQuickCommand();
		}

		float fPos_x = pCha->Object.Position[0] - Hero->Object.Position[0];
		float fPos_y = pCha->Object.Position[1] - Hero->Object.Position[1];
		float fDistance = sqrtf((fPos_x * fPos_x) + (fPos_y * fPos_y));

		if(fDistance > 300.f)
		{
			CloseQuickCommand();
		}
	}
	else
	{
		CloseQuickCommand();
	}
	
	return true;
}

bool SEASON3B::CNewUIQuickCommandWindow::Render()
{
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	if(m_iSelectedCharacterIndex < 0)
	{
		return true;
	}

	RenderFrame();
	RenderContents();
	RenderArrow();
	
	DisableAlphaBlend();

	return true;
}

int SEASON3B::CNewUIQuickCommandWindow::GetVisibleRowCount() const
{
	// Falls back to the full list rather than 0: Render() can run a frame before
	// OpenQuickCommand has built the map, and a zero-row frame would flash as a
	// collapsed sliver.
	return (m_nVisibleCount > 0) ? m_nVisibleCount : (int)QCE_COUNT;
}

float SEASON3B::CNewUIQuickCommandWindow::GetMenuFillerHeight() const
{
	/*
		Height of the stretchable middle section. 65px was the hardcoded original
		(4 x 15 + 5) for six rows, and each removed row takes 19px - the row pitch
		- off it, so a full menu reproduces the old geometry exactly.

		Clamped at 0 because the two 45px caps already carry the title and the
		last row between them: below about three entries there is simply no filler
		left to remove, and a negative height would flip the art inside out.
	*/
	float fFiller = 65.f - ((float)(QCE_COUNT - GetVisibleRowCount()) * 19.f);

	return (fFiller < 0.f) ? 0.f : fFiller;
}

void SEASON3B::CNewUIQuickCommandWindow::RenderFrame()
{
	const int nRows = GetVisibleRowCount();
	const float fFiller = GetMenuFillerHeight();

	float x, y, width, height;

	x = m_Pos.x; y = m_Pos.y; width = 112.f; height = 85.f + fFiller;

	RenderImage(IMAGE_QUICKCOMMAND_BACK, x, y, width, height);

	y = m_Pos.y;
	RenderImage(IMAGE_QUICKCOMMAND_FRAME_UP, m_Pos.x, y, 112.f, 45.f);
	y += 45.f;

	// Tiled in 15px slices with the remainder last, rather than one stretched
	// call: the filler art is a repeating strip, and stretching it would smear
	// whatever gradient it carries.
	const int nFullSlices = (int)(fFiller / 15.f);

	for(int i = 0; i < nFullSlices; ++i)
	{
		RenderImage(IMAGE_QUICKCOMMAND_FRAME_MIDDLE, m_Pos.x, y, 112.f, 15.f);
		y += 15.f;
	}

	const float fRemainder = fFiller - ((float)nFullSlices * 15.f);

	if(fRemainder > 0.f)
	{
		RenderImage(IMAGE_QUICKCOMMAND_FRAME_MIDDLE, m_Pos.x, y, 112.f, fRemainder);
		y += fRemainder;
	}

	RenderImage(IMAGE_QUICKCOMMAND_FRAME_DOWN, m_Pos.x, y, 112.f, 45.f);

	y = m_Pos.y + 55.f;

	// One separator BETWEEN rows, so one fewer than there are rows.
	for(int i = 0; i < (nRows - 1); ++i)
	{
		RenderImage(IMAGE_QUICKCOMMAND_LINE, m_Pos.x + 15.f, y, 82.f, 2.f);
		y += 19.f;
	}
}

void SEASON3B::CNewUIQuickCommandWindow::RenderContents()
{
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(0, 255, 0, 255);
	g_pRenderText->SetBgColor(0);

	int y = m_Pos.y + 14;
	g_pRenderText->RenderText(m_Pos.x, y, m_strID, 112, 0, RT3_SORT_CENTER);
	y += 30;

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(255, 255, 255, 255);

	// Indexed by QUICK_COMMAND_ENTRY, so it stays aligned with the enum even as
	// rows are hidden. QCE_VIEWITEM has no GlobalText id, hence the -1 sentinel
	// and the literal below.
	static const int iGlobalText[QCE_COUNT] = { 943, 1124, 944, 948, 949, -1 };

	const int nRows = GetVisibleRowCount();

	for(int i = 0; i < nRows; ++i)
	{
		const int iEntry = m_aVisibleEntry[i];

		if(m_iSelectedIndex == i)
		{
			g_pRenderText->SetTextColor(255, 255, 0, 255);
		}
		else
		{
			g_pRenderText->SetTextColor(255, 255, 255, 255);
		}

		if(iEntry >= 0 && iEntry < QCE_COUNT && iGlobalText[iEntry] >= 0)
		{
			g_pRenderText->RenderText(m_Pos.x, y, GlobalText[iGlobalText[iEntry]], 112, 0, RT3_SORT_CENTER);
		}
		else
		{
			g_pRenderText->RenderText(m_Pos.x, y, "View Item Char", 112, 0, RT3_SORT_CENTER);
		}

		y += 19.f;
	}
}

void SEASON3B::CNewUIQuickCommandWindow::RenderArrow()
{
	if(m_iSelectedIndex < 0)
	{
		return;
	}

	float x, y;
	x = m_Pos.x + 16.f;
	y = m_Pos.y + 43.f + (m_iSelectedIndex * 19);
	RenderImage(IMAGE_QUICKCOMMAND_ARROWL, x, y, 6.f, 9.f);
	x = m_Pos.x + 90.f;
	RenderImage(IMAGE_QUICKCOMMAND_ARROWR, x, y, 6.f, 9.f);	
}

float SEASON3B::CNewUIQuickCommandWindow::GetLayerDepth()
{
	return 2.0f;
}

float SEASON3B::CNewUIQuickCommandWindow::GetKeyEventOrder()
{
	return 10.f;
}

void SEASON3B::CNewUIQuickCommandWindow::OpenningProcess()
{
	m_iSelectedIndex = -1;
	m_iSelectedCharacterIndex = -1;
}

void SEASON3B::CNewUIQuickCommandWindow::ClosingProcess()
{
	m_iSelectedIndex = -1;
	m_iSelectedCharacterIndex = -1;
}

void SEASON3B::CNewUIQuickCommandWindow::LoadImages()
{
	LoadBitmap("Interface\\newui_msgbox_back.jpg", IMAGE_QUICKCOMMAND_BACK, GL_LINEAR);
	LoadBitmap("Interface\\newui_commamd04.tga", IMAGE_QUICKCOMMAND_FRAME_UP, GL_LINEAR);
	LoadBitmap("Interface\\newui_commamd02.tga", IMAGE_QUICKCOMMAND_FRAME_MIDDLE, GL_LINEAR);
	LoadBitmap("Interface\\newui_commamd03.tga", IMAGE_QUICKCOMMAND_FRAME_DOWN, GL_LINEAR);
	LoadBitmap("Interface\\newui_commamd_Line.jpg", IMAGE_QUICKCOMMAND_LINE, GL_LINEAR );
	LoadBitmap("Interface\\newui_arrow(L).tga", IMAGE_QUICKCOMMAND_ARROWL, GL_LINEAR );
	LoadBitmap("Interface\\newui_arrow(R).tga", IMAGE_QUICKCOMMAND_ARROWR, GL_LINEAR );
}

void SEASON3B::CNewUIQuickCommandWindow::UnloadImages()
{
	DeleteBitmap(IMAGE_QUICKCOMMAND_BACK);
	DeleteBitmap(IMAGE_QUICKCOMMAND_FRAME_UP);
	DeleteBitmap(IMAGE_QUICKCOMMAND_FRAME_MIDDLE);
	DeleteBitmap(IMAGE_QUICKCOMMAND_FRAME_DOWN);
	DeleteBitmap(IMAGE_QUICKCOMMAND_LINE);
	DeleteBitmap(IMAGE_QUICKCOMMAND_ARROWL);
	DeleteBitmap(IMAGE_QUICKCOMMAND_ARROWR);
}

/*
	Packs the entries HidePlayerMenu leaves alone into m_aVisibleEntry, so the
	popup closes the gaps rather than drawing holes where removed rows were.

	View Item Char is additionally gated on CB_VIEWCHARITEM, exactly as the
	dispatch switch is - without that the row would be listed on a build that has
	no handler compiled in and clicking it would do nothing.
*/
int SEASON3B::CNewUIQuickCommandWindow::BuildVisibleEntries()
{
	const DWORD dwHide = gProtect.m_MainInfo.HidePlayerMenu;

	m_nVisibleCount = 0;

	if ((dwHide & PLAYER_MENU_HIDE_ALL) != 0)
	{
		return 0;
	}

	static const struct { int iEntry; DWORD dwFlag; } aEntryFlag[] =
	{
		{ QCE_TRADE,	PLAYER_MENU_HIDE_TRADE		},
		{ QCE_BUY,		PLAYER_MENU_HIDE_BUY		},
		{ QCE_PARTY,	PLAYER_MENU_HIDE_PARTY		},
		{ QCE_FOLLOW,	PLAYER_MENU_HIDE_FOLLOW		},
		{ QCE_DUEL,		PLAYER_MENU_HIDE_DUEL		},
		{ QCE_VIEWITEM,	PLAYER_MENU_HIDE_VIEWITEM	},
	};

	for (int i = 0; i < (sizeof(aEntryFlag) / sizeof(aEntryFlag[0])); ++i)
	{
#if(!CB_VIEWCHARITEM)
		if (aEntryFlag[i].iEntry == QCE_VIEWITEM)
		{
			continue;
		}
#endif
		if ((dwHide & aEntryFlag[i].dwFlag) != 0)
		{
			continue;
		}

		m_aVisibleEntry[m_nVisibleCount++] = aEntryFlag[i].iEntry;
	}

	return m_nVisibleCount;
}

void SEASON3B::CNewUIQuickCommandWindow::OpenQuickCommand(const char* strID, int iIndex, int x, int y)
{
	// Operator switch: HidePlayerMenu in MainInfo.ini removes individual entries,
	// or the whole popup when nothing survives the mask.
	//
	// Gated here rather than at the call site (NewUIHotKey.cpp) so any future
	// caller is covered too, and so the window's own state machine never sees a
	// half-open menu.
	if (BuildVisibleEntries() <= 0)
	{
		return;
	}

	g_pNewUISystem->Show(SEASON3B::INTERFACE_QUICK_COMMAND);

	SetID(strID);
	SetSelectedCharacterIndex(iIndex);
	SetPos(x, y);
}

void SEASON3B::CNewUIQuickCommandWindow::CloseQuickCommand()
{
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_QUICK_COMMAND) == true)
	{
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_QUICK_COMMAND);
	}
}

void SEASON3B::CNewUIQuickCommandWindow::SetID(const char* strID)
{
	strcpy(m_strID, strID);
}

void SEASON3B::CNewUIQuickCommandWindow::SetSelectedCharacterIndex(int iIndex)
{
	m_iSelectedCharacterIndex = iIndex;
}
