// NewUINPCShop.cpp: implementation of the CNewUINPCShop class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUINPCShop.h"
#include "NewUISystem.h"
#include "NewUICommonMessageBox.h"
#include "ZzzInventory.h"
#include "wsclientinline.h"
#include "GambleSystem.h"
#include "NewUIBCustomMenu.h"
#include "ZzzOpenglUtil.h"

using namespace SEASON3B;

// ---- Shop page wire, mirrored from the GameServer's Shop.h ------------------------------
// All BYTE after a 4-byte C1 header, so there is no padding to disagree about.
#pragma pack(push,1)

struct PMSG_SHOP_PAGE_INFO_RECV
{
	PSBMSG_HEAD header;	// C1:D3:B7
	BYTE page;			// 0-based
	BYTE pages;
};

struct PMSG_SHOP_PAGE_REQUEST_SEND
{
	PSBMSG_HEAD header;	// C1:D3:B8
	BYTE page;			// the page wanted, 0-based
};

#pragma pack(pop)

static_assert(sizeof(PMSG_SHOP_PAGE_INFO_RECV) == 6, "must match the GameServer's PMSG_SHOP_PAGE_SEND");
static_assert(sizeof(PMSG_SHOP_PAGE_REQUEST_SEND) == 5, "must match the GameServer's PMSG_SHOP_PAGE_RECV");

namespace
{
	// A page request the server never answers (out of step, or throttled) must not lock
	// buying for good. Long enough that a slow mobile link still gets its answer in first.
	const DWORD kPageRequestTimeoutMs = 3000;
}

SEASON3B::CNewUINPCShop::CNewUINPCShop() 
{ 
	Init(); 
}

SEASON3B::CNewUINPCShop::~CNewUINPCShop() 
{ 
	Release(); 
}

void SEASON3B::CNewUINPCShop::Init()
{
	m_pNewUIMng = NULL;
	m_pNewInventoryCtrl = NULL;
	m_Pos.x = m_Pos.y = 0;
	m_dwShopState = SHOP_STATE_BUYNSELL;
	m_iTaxRate = 0;
	m_bRepairShop = false;
	m_bIsNPCShopOpen = false;
	m_dwStandbyItemKey = 0;
	m_bSellingItem = false;
	m_iPage = 0;
	m_iPageCount = 0;
	m_bPagePending = false;
	m_dwPageRequestMs = 0;
}

bool SEASON3B::CNewUINPCShop::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if(NULL == pNewUIMng || NULL == g_pNewItemMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_NPCSHOP, this);

	m_pNewInventoryCtrl = new CNewUIInventoryCtrl;
	if(false == m_pNewInventoryCtrl->Create(STORAGE_TYPE::UNDEFINED, g_pNewUI3DRenderMng, g_pNewItemMng, this, x+15, y+50, 8, 15))
	{
		SAFE_DELETE(m_pNewInventoryCtrl);
		return false;
	}

	if(m_pNewInventoryCtrl)
	{
		m_pNewInventoryCtrl->LockInventory();
		m_pNewInventoryCtrl->SetToolTipType(TOOLTIP_TYPE_NPC_SHOP);
	}

	SetPos(x, y);

	LoadImages();

	SetButtonInfo();

	Show(false);

	return true;
}

void SEASON3B::CNewUINPCShop::Release()
{
	UnloadImages();

	SAFE_DELETE(m_pNewInventoryCtrl);

	if(m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUINPCShop::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;

	m_pNewInventoryCtrl->SetPos(x + 15, y + 50);
}

bool SEASON3B::CNewUINPCShop::UpdateMouseEvent()
{
	if(m_pNewInventoryCtrl)
	{	
		if(false == m_pNewInventoryCtrl->UpdateMouseEvent())
		{
			return false;
		}
		
		if(InventoryProcess() == true)
		{
			return false;
		}

		if(m_pNewInventoryCtrl->CheckPtInRect(MouseX, MouseY) == true)
		{
			ITEM* pItem = m_pNewInventoryCtrl->FindItemAtPt(MouseX, MouseY);

			// m_bPagePending: see the note on the member. A release over an item while a
			// page is still being fetched falls through to the plain-release branch below.
			if((m_bIsNPCShopOpen == true) && (m_bPagePending == false) && (pItem) && (SEASON3B::IsRelease(VK_LBUTTON)))
			{
				int iIndex = (pItem->y * m_pNewInventoryCtrl->GetNumberOfColumn()) + pItem->x;
				GambleSystem& _gambleSys = GambleSystem::Instance();

				if( _gambleSys.IsGambleShop() )
				{
					_gambleSys.SetBuyItemInfo(iIndex, ItemValue(pItem, 0));
					g_pNPCShop->SetStandbyItemKey(pItem->Key);
					
					SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CGambleBuyMsgBoxLayout));

					return false;
				}
				else
				{
					if(BuyCost == 0)
					{
						SendRequestBuy(iIndex, ItemValue(pItem, 0));
					}	

					return false;
				}
			}
			else if(SEASON3B::IsRelease(VK_LBUTTON))
			{
				m_bIsNPCShopOpen = true;
				return false;
			}
			else if(SEASON3B::IsPress(VK_LBUTTON))
			{
				return false;
			}
		}
	}

	if(BtnProcess() == true)
	{
		return false;
	}

	if(CheckMouseIn(m_Pos.x, m_Pos.y, NPCSHOP_WIDTH, NPCSHOP_HEIGHT))
	{
		if(SEASON3B::IsPress(VK_RBUTTON))
		{
			MouseRButton = false;
			MouseRButtonPop = false;
			MouseRButtonPush = false;
			return false;
		}

		if(SEASON3B::IsNone(VK_LBUTTON) == false)
		{
			return false;
		}
	}

	return true;
}

bool SEASON3B::CNewUINPCShop::UpdateKeyEvent()
{
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_NPCSHOP) == false)
	{
		return true;
	}

	if(SEASON3B::IsRepeat(VK_SHIFT) && SEASON3B::IsPress('L'))
	{
		SendRequestRepair(255, 0);
		return false;
	}

	else if(SEASON3B::IsPress('L'))
    {
		if(m_bRepairShop && CNewUIInventoryCtrl::GetPickedItem() == NULL )	
        {
			ToggleState();
			return false;
        }
    }

	
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_NPCSHOP) == true)
	{
		if(SEASON3B::IsPress(VK_ESCAPE) == true	&& m_bSellingItem == false)
		{
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_NPCSHOP);
			PlayBuffer(SOUND_CLICK01);
			return false;
		}
	}
	return true;
}

bool SEASON3B::CNewUINPCShop::Update()
{
	if(m_bPagePending && (GetTickCount() - m_dwPageRequestMs) > kPageRequestTimeoutMs)
	{
		m_bPagePending = false;
	}

	if(m_bRepairShop)
	{
		RepairAllGold();
	}
	if(m_pNewInventoryCtrl && false == m_pNewInventoryCtrl->Update())
	{
		return false;
	}
	return true;
}

bool SEASON3B::CNewUINPCShop::Render()
{
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	RenderFrame();
	RenderTexts();
	RenderPageBar();
	RenderButton();
	RenderRepairMoney();

	if(m_pNewInventoryCtrl)
	{
		m_pNewInventoryCtrl->Render();
	}

	DisableAlphaBlend();
	return true;
}

void SEASON3B::CNewUINPCShop::RenderFrame()
{
	RenderImage(IMAGE_NPCSHOP_BACK, m_Pos.x, m_Pos.y, 190.f, 429.f);
	RenderImage(IMAGE_NPCSHOP_TOP, m_Pos.x, m_Pos.y, 190.f, 64.f);
	RenderImage(IMAGE_NPCSHOP_LEFT, m_Pos.x, m_Pos.y+64, 21.f, 320.f);
	RenderImage(IMAGE_NPCSHOP_RIGHT, m_Pos.x+190-21, m_Pos.y+64, 21.f, 320.f);
	RenderImage(IMAGE_NPCSHOP_BOTTOM, m_Pos.x, m_Pos.y+429-45, 190.f, 45.f);
}

void SEASON3B::CNewUINPCShop::RenderTexts()
{
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(220, 220, 220, 255);

	g_pRenderText->RenderText(m_Pos.x, m_Pos.y + 12, GlobalText[230], NPCSHOP_WIDTH, 0, RT3_SORT_CENTER);
	
	unicode::t_char strText[256];
	unicode::_sprintf(strText, GlobalText[1623], m_iTaxRate);
	g_pRenderText->RenderText(m_Pos.x,m_Pos.y + 27, strText, NPCSHOP_WIDTH, 0, RT3_SORT_CENTER);
}

void SEASON3B::CNewUINPCShop::RenderButton()
{
	if(m_bRepairShop)
	{
		m_BtnRepair.Render();
		m_BtnRepairAll.Render();
	}
}

void SEASON3B::CNewUINPCShop::RenderRepairMoney()
{
	if(m_bRepairShop)
	{
		RenderImage(IMAGE_NPCSHOP_REPAIR_MONEY, m_Pos.x+10, m_Pos.y+355, 170.f, 24.f);
		g_pRenderText->SetBgColor(255, 255, 255, 0);
		g_pRenderText->SetTextColor(255, 220, 150, 255);
		unicode::t_char strText[256];
        ConvertGold(AllRepairGold, strText);
		g_pRenderText->SetFont(g_hFontBold);
		g_pRenderText->RenderText(m_Pos.x+20, m_Pos.y+362, GlobalText[239]);
        g_pRenderText->SetTextColor(getGoldColor(AllRepairGold));
		g_pRenderText->RenderText(m_Pos.x+100, m_Pos.y+362, strText);
	}
}

float SEASON3B::CNewUINPCShop::GetLayerDepth()
{ 
	return 4.55;	
}

void SEASON3B::CNewUINPCShop::LoadImages()
{
	LoadBitmap("Interface\\newui_msgbox_back.jpg", IMAGE_NPCSHOP_BACK, GL_LINEAR);
	LoadBitmap("Interface\\newui_item_back04.tga", IMAGE_NPCSHOP_TOP, GL_LINEAR);
	LoadBitmap("Interface\\newui_item_back02-L.tga", IMAGE_NPCSHOP_LEFT, GL_LINEAR);
	LoadBitmap("Interface\\newui_item_back02-R.tga", IMAGE_NPCSHOP_RIGHT, GL_LINEAR);
	LoadBitmap("Interface\\newui_item_back03.tga", IMAGE_NPCSHOP_BOTTOM, GL_LINEAR);
	LoadBitmap("Interface\\newui_repair_00.tga", IMAGE_NPCSHOP_BTN_REPAIR, GL_LINEAR);
	LoadBitmap("Interface\\newui_item_money2.tga", IMAGE_NPCSHOP_REPAIR_MONEY, GL_LINEAR);
}

void SEASON3B::CNewUINPCShop::UnloadImages()
{
	DeleteBitmap(IMAGE_NPCSHOP_BACK);
	DeleteBitmap(IMAGE_NPCSHOP_TOP);
	DeleteBitmap(IMAGE_NPCSHOP_LEFT);
	DeleteBitmap(IMAGE_NPCSHOP_LEFT);
	DeleteBitmap(IMAGE_NPCSHOP_BOTTOM);
	DeleteBitmap(IMAGE_NPCSHOP_BTN_REPAIR);
	DeleteBitmap(IMAGE_NPCSHOP_REPAIR_MONEY);
}

void SEASON3B::CNewUINPCShop::SetTaxRate(int iTaxRate)
{
	m_iTaxRate = iTaxRate;
}

int SEASON3B::CNewUINPCShop::GetTaxRate()
{	
	return m_iTaxRate;
}

bool SEASON3B::CNewUINPCShop::InsertItem(int iIndex, BYTE* pbyItemPacket)
{
	if(m_pNewInventoryCtrl)
	{
		return m_pNewInventoryCtrl->AddItem(iIndex, pbyItemPacket);
	}
	
	return false;
}

bool SEASON3B::CNewUINPCShop::InventoryProcess()
{
	CNewUIPickedItem* pPickedItem = CNewUIInventoryCtrl::GetPickedItem();

	if( !m_pNewInventoryCtrl )	return false;
	if( !pPickedItem )			return false;
	ITEM* pItem = pPickedItem->GetItem();

#ifdef LEM_ADD_LUCKYITEM
	if( IsSellingBan( pItem ) )	m_pNewInventoryCtrl->SetSquareColorNormal(1.0f, 0.0f, 0.0f );
	else	m_pNewInventoryCtrl->SetSquareColorNormal(0.1f, 0.4f, 0.8f );
#endif // LEM_ADD_LUCKYITEM

	if(	SEASON3B::IsRelease(VK_LBUTTON) == true	&& m_pNewInventoryCtrl->CheckPtInRect(MouseX, MouseY) == true && m_bSellingItem == false)
	{
		if (CharacterMachine->Gold + ItemValue(pItem) > 2000000000)
		{
			g_pChatListBox->AddText("", GlobalText[3148], SEASON3B::TYPE_SYSTEM_MESSAGE);
			
			return true;
		}

		if( pItem && pItem->Jewel_Of_Harmony_Option != 0 )
		{
			g_pChatListBox->AddText("", GlobalText[2211], SEASON3B::TYPE_ERROR_MESSAGE);

			return true;
		}
		else if(pItem && IsSellingBan(pItem) == true)
		{
			g_pChatListBox->AddText("", GlobalText[668], SEASON3B::TYPE_ERROR_MESSAGE);
			m_pNewInventoryCtrl->BackupPickedItem();
			
			return true;
		}
		else if(pItem && IsHighValueItem(pItem) == true)
		{
			SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CHighValueItemCheckMsgBoxLayout));
			pPickedItem->HidePickedItem();
			
			return true;
		}

		if (pPickedItem->GetOwnerInventory() == g_pMyInventory->GetInventoryCtrl() || g_pMyInventoryExt->GetOwnerOf(pPickedItem))
		{
			int iSourceIndex = pPickedItem->GetSourceLinealPos();
			SendRequestSell(iSourceIndex);

			return true;
		}
		else if(pPickedItem->GetOwnerInventory() == NULL)
		{
			int iSourceIndex = pPickedItem->GetSourceLinealPos();
			SendRequestSell(iSourceIndex);
			
			return true;
		}
	}

	return false;
}

bool SEASON3B::CNewUINPCShop::BtnProcess()
{
	if(PageButtonProcess() == true)
	{
		return true;
	}

	POINT ptExitBtn1 = { m_Pos.x+169, m_Pos.y+7 };

	if(SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(ptExitBtn1.x, ptExitBtn1.y, 13, 12) && m_bSellingItem == false	)
	{
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_NPCSHOP);
		
		return true;
	}

	if(m_bRepairShop)
	{
		if(m_BtnRepair.UpdateMouseEvent() == true)
		{
			ToggleState();
			
			return true;
		}
		if(m_BtnRepairAll.UpdateMouseEvent() == true)
		{
			SendRequestRepair(255, 0);
			
			return true;
		}
	}

	return false;
}

void SEASON3B::CNewUINPCShop::OpenningProcess()
{
	if( SEASON3B::IsRepeat(VK_LBUTTON))
	{
		m_bIsNPCShopOpen = false;
	}
	else
	{
		m_bIsNPCShopOpen = true;
	}
}

void SEASON3B::CNewUINPCShop::ClosingProcess()
{
	SendExitInventory();

	m_dwShopState = SHOP_STATE_BUYNSELL;
	m_iTaxRate = 0;
	m_bRepairShop = false;
	m_dwStandbyItemKey = 0;

	// Back to an ordinary one-screen shop. The next shop that opens is a paged one only
	// if the server says so again with C1:D3:B7 - this is what stops a one-page shop
	// opened after a paged one from showing the last shop's arrows.
	m_iPage = 0;
	m_iPageCount = 0;
	m_bPagePending = false;

	m_bIsNPCShopOpen = false;

	if(m_pNewInventoryCtrl)
	{
		m_pNewInventoryCtrl->RemoveAllItems();
	}

	GambleSystem::Instance().SetGambleShop(false);
	m_bSellingItem = false;
}

void SEASON3B::CNewUINPCShop::SetButtonInfo()
{
	m_BtnRepair.ChangeButtonImgState(true, IMAGE_NPCSHOP_BTN_REPAIR, false);
	m_BtnRepair.ChangeButtonInfo(m_Pos.x + 54, m_Pos.y + 390, 36, 29);
	m_BtnRepair.ChangeToolTipText(GlobalText[233], true);

	m_BtnRepairAll.ChangeButtonImgState(true, IMAGE_NPCSHOP_BTN_REPAIR, false);
	m_BtnRepairAll.ChangeButtonInfo(m_Pos.x + 98, m_Pos.y + 390, 36, 29);
	m_BtnRepairAll.ChangeToolTipText(GlobalText[237], true);
}

void SEASON3B::CNewUINPCShop::SetRepairShop(bool bRepair)
{
	m_bRepairShop = bRepair;
}

bool SEASON3B::CNewUINPCShop::IsRepairShop()
{
	return m_bRepairShop;
}

void SEASON3B::CNewUINPCShop::ToggleState()
{
	if(m_dwShopState == SHOP_STATE_BUYNSELL)
	{
		m_dwShopState = SHOP_STATE_REPAIR;
		
		g_pMyInventory->SetRepairMode(true);
	}
	else
	{
		m_dwShopState = SHOP_STATE_BUYNSELL;
		g_pMyInventory->SetRepairMode(false);
	}
}

DWORD SEASON3B::CNewUINPCShop::GetShopState()
{	
	return m_dwShopState;
}


int SEASON3B::CNewUINPCShop::GetPointedItemIndex()
{
	return m_pNewInventoryCtrl->GetPointedSquareIndex();
}

void SEASON3B::CNewUINPCShop::SetStandbyItemKey(DWORD dwItemKey)
{ 
	m_dwStandbyItemKey = dwItemKey; 
}

DWORD SEASON3B::CNewUINPCShop::GetStandbyItemKey() const
{ 
	return m_dwStandbyItemKey; 
}

int SEASON3B::CNewUINPCShop::GetStandbyItemIndex()
{
	ITEM* pItem = GetStandbyItem();
	if(pItem)
		return pItem->y*m_pNewInventoryCtrl->GetNumberOfColumn()+pItem->x;
	return -1;
}

ITEM* SEASON3B::CNewUINPCShop::GetStandbyItem()
{
	if(m_pNewInventoryCtrl)
		return m_pNewInventoryCtrl->FindItemByKey(m_dwStandbyItemKey);
	return NULL;
}

void SEASON3B::CNewUINPCShop::SetSellingItem(bool bFlag)
{
	m_bSellingItem = bFlag;
}

bool SEASON3B::CNewUINPCShop::IsSellingItem()
{
	return m_bSellingItem;
}

//////////////////////////////////////////////////////////////////////
// Shop pages
//////////////////////////////////////////////////////////////////////

// Where an arrow sits, in the window's own coordinates. ONE definition for drawing and
// hit-testing, so what is drawn and what is tappable cannot drift apart.
//
// In the HEADER, either side of the tax line, rather than under the grid: the strip under
// the grid is the repair-money bar in a repair shop (Hanzo-type NPCs, which is where a whole
// catalogue is most likely to be put), and the row below that holds the repair buttons.
// The header is free in every kind of shop. The exit button is at x 169, above these.
bool SEASON3B::CNewUINPCShop::PageButtonRect(bool bNext, int* px, int* py, int* pw, int* ph)
{
	*px = m_Pos.x + (bNext ? 140 : 22);
	*py = m_Pos.y + 22;
	*pw = 28;
	*ph = 18;

	return true;
}

void SEASON3B::CNewUINPCShop::RequestPage(int iPage)
{
	if(iPage < 0 || iPage >= m_iPageCount)
	{
		return;
	}

	// Unencrypted, like every 0xD3 the client sends - HackPacketCheck lists head 0xD3 with
	// Encrypt = 0 and closes the connection on a mismatch.
	PMSG_SHOP_PAGE_REQUEST_SEND pMsg;

	pMsg.header.set(0xD3, 0xB8, sizeof(pMsg));

	pMsg.page = (BYTE)iPage;

	DataSend((LPBYTE)&pMsg, pMsg.header.size);

	m_bPagePending = true;
	m_dwPageRequestMs = GetTickCount();
}

bool SEASON3B::CNewUINPCShop::PageButtonProcess()
{
	if(m_iPageCount <= 1 || m_bPagePending == true)
	{
		return false;
	}

	if(SEASON3B::IsPress(VK_LBUTTON) == false)
	{
		return false;
	}

	int x = 0, y = 0, w = 0, h = 0;

	PageButtonRect(false, &x, &y, &w, &h);

	if(m_iPage > 0 && CheckMouseIn(x, y, w, h))
	{
		RequestPage(m_iPage - 1);

		return true;
	}

	PageButtonRect(true, &x, &y, &w, &h);

	if((m_iPage + 1) < m_iPageCount && CheckMouseIn(x, y, w, h))
	{
		RequestPage(m_iPage + 1);

		return true;
	}

	return false;
}

void SEASON3B::CNewUINPCShop::RenderPageBar()
{
	if(m_iPageCount <= 1)
	{
		return;
	}

	// 0xRRGGBBAA, which is DrawInfoBox's order (TextDraw takes the opposite).
	const DWORD kBtnNormal   = 0x2E3B5CD8;
	const DWORD kBtnHover    = 0x4A6094E8;
	const DWORD kBtnDisabled = 0x1C1C22A0;

	bool bEnabled[2];

	for(int i = 0; i < 2; i++)
	{
		const bool bNext = (i == 1);

		int x = 0, y = 0, w = 0, h = 0;

		PageButtonRect(bNext, &x, &y, &w, &h);

		bEnabled[i] = (m_bPagePending == false) && (bNext ? ((m_iPage + 1) < m_iPageCount) : (m_iPage > 0));

		const bool bHover = bEnabled[i] && CheckMouseIn(x, y, w, h);

		g_pBCustomMenuInfo->DrawInfoBox((float)x, (float)y, (float)w, (float)h, bHover ? kBtnHover : (bEnabled[i] ? kBtnNormal : kBtnDisabled), 0, 0);
	}

	// DrawInfoBox leaves textures off; everything drawn after this expects them on.
	DisableTexture(false);
	EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetBgColor(0);

	for(int i = 0; i < 2; i++)
	{
		int x = 0, y = 0, w = 0, h = 0;

		PageButtonRect(i == 1, &x, &y, &w, &h);

		if(bEnabled[i])
		{
			g_pRenderText->SetTextColor(255, 220, 150, 255);
		}
		else
		{
			g_pRenderText->SetTextColor(110, 110, 110, 255);
		}

		// A char buffer, not `cond ? ">" : "<"`: a ternary of two string literals is a hard
		// error on the Android toolchain when it lands in a char* parameter.
		char strArrow[2] = { (char)((i == 1) ? '>' : '<'), 0 };

		g_pRenderText->RenderText(x, y + 3, strArrow, w, 0, RT3_SORT_CENTER);
	}

	// "Page 2 / 5" just above the grid, under the tax line.
	unicode::t_char strPage[64];

	unicode::_sprintf(strPage, "Page %d / %d", m_iPage + 1, m_iPageCount);

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(220, 220, 220, 255);
	g_pRenderText->RenderText(m_Pos.x, m_Pos.y + 38, strPage, NPCSHOP_WIDTH, 0, RT3_SORT_CENTER);
}

void SEASON3B::CNewUINPCShop::RecvPageInfo(BYTE* lpMsg)
{
	if(lpMsg == NULL || m_pNewInventoryCtrl == NULL)
	{
		return;
	}

	// The shop has to be open. This arrives right after the open (or a page turn), so a
	// straggler after the window closed is ignored rather than resurrecting page state.
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_NPCSHOP) == false)
	{
		return;
	}

	PMSG_SHOP_PAGE_INFO_RECV info;

	memcpy(&info, lpMsg, sizeof(info));

	m_iPageCount = info.pages;

	m_iPage = info.page;

	if(m_iPage >= m_iPageCount)
	{
		m_iPage = m_iPageCount - 1;
	}

	if(m_iPage < 0)
	{
		m_iPage = 0;
	}

	// The answer to the request (or the open itself): buying is safe again, because from
	// here the grid and the server's idea of the page are about to agree.
	m_bPagePending = false;

	// The ordinary C2:31 list follows this on the same stream and refills the grid, so the
	// old page must go first or its items would still be there under the new ones.
	m_pNewInventoryCtrl->RemoveAllItems();
}
