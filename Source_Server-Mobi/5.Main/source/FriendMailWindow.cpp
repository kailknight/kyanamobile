// FriendMailWindow.cpp: the modern Friends window. See FriendMailWindow.h.
//
// Drawn like the other custom windows (CB_RedeemCodeWindow): plain GL bars and
// text through gInterface / g_pBCustomMenuInfo, so it needs no new art and
// works the same on PC and mobile.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FriendMailWindow.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "NewUIMyInventory.h"
#include "NewUIInventoryExtension.h"
#include "NewUIInventoryCtrl.h"
#include "NewUIFriendWindow.h"
#include "NewUIChatInputBox.h"
#include "UIControls.h"
#include "UIWindows.h"
#include "ZzzInterface.h"
#include "ZzzInventory.h"
#include "ZzzInfomation.h"
#include "ZzzOpenglUtil.h"
#include "wsclientinline.h"
#include "DSPlaySound.h"
#include "MultiLanguage.h"
#include "Input.h"

CFriendMailWindow* gFriendMailWindow = NULL;

namespace
{
	// Live text controls (own focus and caret), created once and hidden when a
	// view does not use them - the same file-scope reasoning as the redeem
	// window's input box.
	CUITextInputBox* g_ToBox = NULL;
	CUITextInputBox* g_SubjectBox = NULL;
	CUITextInputBox* g_TextBox = NULL;
	CUITextInputBox* g_FriendNameBox = NULL;
	CUITextInputBox* g_CoinBox = NULL;

	const float kWindowW = 450.0f;
	const float kWindowH = 345.0f;
	const int kFriendRows = 6;
	const int kMailRows = 8;
	const int kPickColumns = 10;
	const int kPickRows = 4;

	const DWORD kWhite = 0xFFFFFFFF;
	const DWORD kGrey = 0xFFB4B4B4;
	const DWORD kDim = 0xFF8C8C8C;
	const DWORD kGreen = 0xFF6EE66E;
	const DWORD kGold = 0xFF50D2FF;		// ABGR: a warm gold
	const DWORD kRed = 0xFF6E6EFF;		// ABGR: soft red
	const DWORD kBlue = 0xFFFFC878;		// ABGR: light blue

	bool PrimaryDown()
	{
#if defined(__ANDROID__) || defined(MU_IOS)
		return SEASON3B::IsPress(VK_LBUTTON) || CInput::Instance().IsLBtnDn();
#else
		return ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	}

	CUITextInputBox* MakeInput(int w,int h,int limit,bool multiline)
	{
		CUITextInputBox* box = new CUITextInputBox;

		if(multiline)
		{
			box->SetMultiline(TRUE);
		}

		box->Init(pGameWindow,w,h,limit,FALSE);
		box->SetBackColor(0,0,0,0);
		box->SetTextColor(255,255,255,0);
		box->SetFont((HFONT)g_hFont);
		box->SetTextLimit(limit);
		box->SetState(UISTATE_HIDE);
		return box;
	}

	void CopyName(char* out,int outSize,const char* in,int inSize)
	{
		memset(out,0,outSize);
		int n = (inSize < outSize - 1) ? inSize : (outSize - 1);
		memcpy(out,in,n);
		out[outSize - 1] = 0;
	}

	bool IsOnline(BYTE server)
	{
		return server < 0xFC;
	}

	// The class byte as the server stores it: race * 16 + evolution.
	const char* ClassShortName(BYTE cls)
	{
		static const char* kNames[7][3] =
		{
			{ "DW", "SM", "GM" },
			{ "DK", "BK", "BM" },
			{ "ELF", "ME", "HE" },
			{ "MG", "DM", "DM" },
			{ "DL", "LE", "LE" },
			{ "SUM", "BS", "DiM" },
			{ "RF", "FM", "FM" },
		};

		int race = cls >> 4;
		int evo = cls & 0x0F;

		if(cls == 0xFF || race > 6)
		{
			return "?";
		}

		if(evo > 2)
		{
			evo = 2;
		}

		return kNames[race][evo];
	}

	void ClassColor(BYTE cls,float* r,float* g,float* b)
	{
		static const float kColors[7][3] =
		{
			{ 0.30f, 0.45f, 0.95f },	// DW blue
			{ 0.85f, 0.30f, 0.25f },	// DK red
			{ 0.35f, 0.80f, 0.40f },	// ELF green
			{ 0.75f, 0.45f, 0.90f },	// MG purple
			{ 0.95f, 0.70f, 0.20f },	// DL gold
			{ 0.95f, 0.45f, 0.75f },	// SUM pink
			{ 0.95f, 0.55f, 0.25f },	// RF orange
		};

		int race = cls >> 4;

		if(cls == 0xFF || race > 6)
		{
			*r = *g = *b = 0.4f;
			return;
		}

		*r = kColors[race][0];
		*g = kColors[race][1];
		*b = kColors[race][2];
	}

	// Every inventory grid that can hold a mailable item: the main bag plus
	// each unlocked expansion page.
	int CollectInventoryCtrls(SEASON3B::CNewUIInventoryCtrl** out,int max)
	{
		int count = 0;

		if(g_pMyInventory != NULL && g_pMyInventory->GetInventoryCtrl() != NULL && count < max)
		{
			out[count++] = g_pMyInventory->GetInventoryCtrl();
		}

		if(g_pMyInventoryExt != NULL)
		{
			for(int n = 0; n < CharacterAttribute->InventoryExtensions && count < max; n++)
			{
				SEASON3B::CNewUIInventoryCtrl* pCtrl = g_pMyInventoryExt->GetInventoryCtrl(n);

				if(pCtrl != NULL)
				{
					out[count++] = pCtrl;
				}
			}
		}

		return count;
	}

	ITEM* FindInventoryItem(int slot)
	{
		SEASON3B::CNewUIInventoryCtrl* ctrls[8];
		int ctrlCount = CollectInventoryCtrls(ctrls,8);

		for(int c = 0; c < ctrlCount; c++)
		{
			int itemCount = (int)ctrls[c]->GetNumberOfItems();

			for(int n = 0; n < itemCount; n++)
			{
				ITEM* pItem = ctrls[c]->GetItem(n);

				if(pItem != NULL && ctrls[c]->GetIndexByItem(pItem) == slot)
				{
					return pItem;
				}
			}
		}

		return NULL;
	}
}

void ToggleFriendWindowRouted()
{
	if(gFriendMailWindow != NULL && gFriendMailWindow->IsModern())
	{
		if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_FRIEND))
		{
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_FRIEND);
		}

		gFriendMailWindow->Toggle();
		return;
	}

	g_pNewUISystem->Toggle(SEASON3B::INTERFACE_FRIEND);
}

CFriendMailWindow::CFriendMailWindow()
{
	this->m_ConfigReceived = false;
	this->m_Modern = false;
	this->m_FeeType = 0;
	this->m_Fee = 0;
	this->m_FeeItem = 0;
	this->m_FeeItemLevel = 0;
	this->m_MaxPerLetter = 3;
	this->m_ExpireDays = 7;
	this->m_SendZen = 0;
	this->m_CoinTypes = 0;
	this->m_CoinMax = 0;
	this->m_CoinType = 0;
	memset(this->m_ReadCoins,0,sizeof(this->m_ReadCoins));

	this->m_View = VIEW_FRIENDS;
	this->m_Page = 0;
	this->m_SelectedLetter = 0;
	this->m_Notice[0] = 0;
	this->m_NoticeColor = kWhite;
	this->m_NoticeTick = 0;
	this->m_Click = false;
	this->m_WasDown = false;
	this->m_InputsShown = false;
	this->m_DetailRequestTick = 0;

	this->m_ReadLetter = 0;
	this->m_ReadTextLoaded = false;
	this->m_ReadText[0] = 0;
	this->m_ReadItemCount = 0;
	memset(this->m_ReadItems,0,sizeof(this->m_ReadItems));
	this->m_ClaimPending = false;

	for(int n = 0; n < FRIENDMAIL_MAX_ITEMS; n++)
	{
		this->m_Attach[n].Slot = -1;
		this->m_Attach[n].Type = -1;
	}

	this->m_PickTarget = 0;
	this->m_SendPending = false;
	this->m_SendTick = 0;

	this->m_HoverItem = NULL;
	this->m_HoverX = 0.0f;
	this->m_HoverY = 0.0f;
}

CFriendMailWindow::~CFriendMailWindow()
{
	this->ClearReadItems();
}

bool CFriendMailWindow::IsOpen() const
{
	return gInterface.Data[eWindowFriendMail].OnShow != 0;
}

void CFriendMailWindow::Toggle()
{
	if((GetTickCount() - gInterface.Data[eWindowFriendMail].EventTick) < 300)
	{
		return;
	}

	gInterface.Data[eWindowFriendMail].EventTick = GetTickCount();

	if(this->IsOpen())
	{
		this->Close();
		return;
	}

	gInterface.Data[eWindowFriendMail].OnShow = true;
	this->SetView(VIEW_FRIENDS);
	this->RequestDetails();
	PlayBuffer(25,0,0);
}

void CFriendMailWindow::Close()
{
	gInterface.Data[eWindowFriendMail].OnShow = false;
	this->HideInputs();
}

void CFriendMailWindow::SetView(View view)
{
	this->m_View = view;
	this->m_Page = 0;
	this->HideInputs();

	if(view != VIEW_READ)
	{
		this->m_ClaimPending = false;
	}
}

void CFriendMailWindow::SetNotice(const char* text,DWORD color)
{
	strncpy_s(this->m_Notice,sizeof(this->m_Notice),text,_TRUNCATE);
	this->m_NoticeColor = color;
	this->m_NoticeTick = GetTickCount();
}

void CFriendMailWindow::RequestDetails()
{
	// The list changes rarely; refresh on open, at most every few seconds.
	if((GetTickCount() - this->m_DetailRequestTick) < 3000 && this->m_DetailRequestTick != 0)
	{
		return;
	}

	this->m_DetailRequestTick = GetTickCount();

	PMSG_FRIENDMAIL_EMPTY pMsg;
	pMsg.header.set(0xD3,0xE0,sizeof(pMsg));
	DataSend((LPBYTE)&pMsg,pMsg.header.size);
}

void CFriendMailWindow::HideInputs()
{
	this->m_InputsShown = false;

	CUITextInputBox* boxes[5] = { g_ToBox, g_SubjectBox, g_TextBox, g_FriendNameBox, g_CoinBox };

	for(int n = 0; n < 5; n++)
	{
		if(boxes[n] != NULL)
		{
			boxes[n]->SetState(UISTATE_HIDE);
		}
	}

	SetFocus(g_hWnd);
}

void CFriendMailWindow::ShowInputs(bool to,bool subject,bool text,bool friendName,bool coin)
{
	if(g_ToBox == NULL)
	{
		g_ToBox = MakeInput(150,14,MAX_ID_SIZE,false);
		g_SubjectBox = MakeInput(300,14,31,false);
		g_TextBox = MakeInput(390,70,FRIENDMAIL_MEMO_MAX - 1,true);
		g_FriendNameBox = MakeInput(150,14,MAX_ID_SIZE,false);
		g_CoinBox = MakeInput(90,14,10,false);
		g_CoinBox->SetOption(UIOPTION_NUMBERONLY);
		g_ToBox->SetTabTarget(g_SubjectBox);
		g_SubjectBox->SetTabTarget(g_TextBox);
		g_TextBox->SetTabTarget(g_ToBox);
	}

	this->m_InputsShown = (to || subject || text || friendName || coin);
	g_ToBox->SetState(to ? UISTATE_NORMAL : UISTATE_HIDE);
	g_SubjectBox->SetState(subject ? UISTATE_NORMAL : UISTATE_HIDE);
	g_TextBox->SetState(text ? UISTATE_NORMAL : UISTATE_HIDE);
	g_FriendNameBox->SetState(friendName ? UISTATE_NORMAL : UISTATE_HIDE);
	g_CoinBox->SetState(coin ? UISTATE_NORMAL : UISTATE_HIDE);
}

// -----------------------------------------------------------------------------
// drawing helpers
// -----------------------------------------------------------------------------

bool CFriendMailWindow::Button(float x,float y,float w,const char* text,bool enabled)
{
	if(enabled == false)
	{
		gInterface.DrawBarForm(x,y,w,16.0f,0.2f,0.2f,0.2f,0.6f);
		TextDraw((HFONT)g_hFont,(int)x,(int)y + 2,kDim,0x0,(int)w,0,3,text);
		return false;
	}

	// DrawButton takes its height from the third argument (20% of it); 80
	// gives every button the same 16px whatever its width.
	return g_pBCustomMenuInfo->DrawButton(x,y,80.0f,12,(char*)text,w);
}

bool CFriendMailWindow::RowClicked(float x,float y,float w,float h) const
{
	return this->m_Click && SEASON3B::CheckMouseIn((int)x,(int)y,(int)w,(int)h) == 1;
}

void CFriendMailWindow::RenderAvatar(float x,float y,float size,BYTE cls) const
{
	float r,g,b;
	ClassColor(cls,&r,&g,&b);

	// A rounded-looking tile: a darker rim around the class colour.
	gInterface.DrawBarForm(x,y,size,size,r * 0.45f,g * 0.45f,b * 0.45f,0.95f);
	gInterface.DrawBarForm(x + 2.0f,y + 2.0f,size - 4.0f,size - 4.0f,r,g,b,0.85f);

	TextDraw(g_hFontBold,(int)x,(int)(y + (size / 2.0f) - 6.0f),kWhite,0x0,(int)size,0,3,ClassShortName(cls));
}

void CFriendMailWindow::RenderItemTile(float x,float y,float size,const ITEM* pItem,bool hovered) const
{
	float bg = hovered ? 0.35f : 0.22f;
	gInterface.DrawBarForm(x,y,size,size,bg,bg,bg + 0.04f,0.9f);

	if(pItem == NULL || pItem->Type < 0 || pItem->Type >= MAX_ITEM)
	{
		return;
	}

	// Sized the way the bag sizes it, so the icon keeps the same proportions
	// to the UI at every resolution: the bag draws an item into its grid
	// footprint (cells x INVENTORY_SQUARE_WIDTH) with a camera whose zoom
	// already compensates for resolution (NewUI3DRenderMng.cpp; the batch
	// helper below uses the same one at FOV 1). Scale that footprint to fit
	// the tile - big items shrink, 1x1 items grow a little to fill it - and
	// draw into the scaled box, centred.
	const float kCell = (float)SEASON3B::INVENTORY_SQUARE_WIDTH;
	const float kMaxGrow = 1.5f;
	float cellsW = (float)ItemAttribute[pItem->Type].Width;
	float cellsH = (float)ItemAttribute[pItem->Type].Height;

	if(cellsW < 1.0f) { cellsW = 1.0f; }
	if(cellsH < 1.0f) { cellsH = 1.0f; }

	float inner = size - 6.0f;
	float scale = inner / (((cellsW > cellsH) ? cellsW : cellsH) * kCell);

	if(scale > kMaxGrow)
	{
		scale = kMaxGrow;
	}

	float boxW = cellsW * kCell * scale;
	float boxH = cellsH * kCell * scale;
	float boxX = x + (size - boxW) / 2.0f;
	float boxY = y + (size - boxH) / 2.0f;

	// Plain RenderItem3D needs a 3D camera that only the NewUI 3D pass sets up;
	// called from here it drew nothing. RenderItem3DInBatch sets the camera up
	// and tears it down per item, so the 2D bars and text drawn between tiles
	// stay in screen space. Callers bracket their loops with
	// BeginItem3DFreeBatch/EndItem3DFreeBatch (see CB_JewelBank.cpp).
	// Arg 10 is the camera FOV (1 = the bag's view), not a size - the size
	// goes in ItemScale. FixY off: its offsets are for other windows' rows.
	glColor4f(1.0f,1.0f,1.0f,1.0f);
	g_pNewUISystem->RenderItem3DInBatch(boxX,boxY,boxW,boxH,pItem->Type,pItem->Level,pItem->Option1,pItem->ExtOption,false,1.0f,false,scale);
}

void CFriendMailWindow::RenderWrappedText(float x,float y,float w,int maxLines,const char* text,DWORD color) const
{
	char line[256];
	int lineLen = 0;
	int lines = 0;
	const float lineH = 13.0f;

	for(const char* p = text; *p != 0 && lines < maxLines; )
	{
		if(*p == '\n')
		{
			line[lineLen] = 0;
			TextDraw((HFONT)g_hFont,(int)x,(int)(y + lines * lineH),color,0x0,(int)w,0,1,line);
			lines++;
			lineLen = 0;
			p++;
			continue;
		}

		if(lineLen < (int)sizeof(line) - 2)
		{
			line[lineLen++] = *p;
			line[lineLen] = 0;
		}

		SIZE size = { 0, 0 };
		g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(),line,lineLen,&size);

		if(size.cx / g_fScreenRate_x > w)
		{
			// Break at the last space if there is one on this line.
			int cut = lineLen - 1;

			while(cut > 0 && line[cut] != ' ')
			{
				cut--;
			}

			int keep = (cut > 0) ? cut : (lineLen - 1);
			char rest[256];
			int restLen = lineLen - keep - ((cut > 0) ? 1 : 0);
			memcpy(rest,line + lineLen - restLen,restLen);
			line[keep] = 0;

			TextDraw((HFONT)g_hFont,(int)x,(int)(y + lines * lineH),color,0x0,(int)w,0,1,line);
			lines++;

			memcpy(line,rest,restLen);
			lineLen = restLen;
			line[lineLen] = 0;
		}

		p++;
	}

	if(lineLen > 0 && lines < maxLines)
	{
		TextDraw((HFONT)g_hFont,(int)x,(int)(y + lines * lineH),color,0x0,(int)w,0,1,line);
	}
}

namespace
{
	const char* CoinName(int coinType)
	{
		switch(coinType)
		{
			case 1: return "WCoin (C)";
			case 2: return "WCoin (P)";
			case 3: return "Goblin Points";
		}

		return "";
	}
}

// The extra fee for attached items only - postage is shown on its own line.
void CFriendMailWindow::ItemFeeText(char* out,int outSize,int itemCount) const
{
	DWORD total = this->m_Fee * (DWORD)itemCount;

	if(total == 0 || itemCount == 0)
	{
		strcpy_s(out,outSize,"Item fee: none");
		return;
	}

	switch(this->m_FeeType)
	{
		case 0:
			sprintf_s(out,outSize,"Item fee: %s Zen",gInterface.NumberFormat((int)total));
			break;
		case 1:
			{
				char itemName[64] = { 0 };
				GetItemName(this->m_FeeItem,this->m_FeeItemLevel,itemName);
				sprintf_s(out,outSize,"Item fee: %u x %s",total,itemName);
			}
			break;
		default:
			sprintf_s(out,outSize,"Item fee: %s %s",gInterface.NumberFormat((int)total),CoinName(this->m_FeeType - 1));
			break;
	}
}

// Steps to the next coin the server lets you mail (0 when there is none).
void CFriendMailWindow::NextCoinType()
{
	for(int step = 1; step <= 3; step++)
	{
		int type = ((this->m_CoinType + step - 1) % 3) + 1;

		if((this->m_CoinTypes & (1 << (type - 1))) != 0)
		{
			this->m_CoinType = (BYTE)type;
			return;
		}
	}

	this->m_CoinType = 0;
}

// The amount typed in the coin box, 0 when empty or not a number.
DWORD CFriendMailWindow::ReadCoinAmount() const
{
	if(g_CoinBox == NULL || this->m_CoinType == 0)
	{
		return 0;
	}

	char text[16] = { 0 };
	g_CoinBox->GetText(text,sizeof(text));

	DWORD amount = 0;

	for(int n = 0; text[n] != 0; n++)
	{
		if(text[n] < '0' || text[n] > '9' || amount > 100000000)
		{
			return 0;
		}

		amount = amount * 10 + (DWORD)(text[n] - '0');
	}

	return amount;
}

int CFriendMailWindow::ReadCoinCount() const
{
	return (this->m_ReadCoins[0] > 0 ? 1 : 0) + (this->m_ReadCoins[1] > 0 ? 1 : 0) + (this->m_ReadCoins[2] > 0 ? 1 : 0);
}

// -----------------------------------------------------------------------------
// the window
// -----------------------------------------------------------------------------

void CFriendMailWindow::DrawWindow()
{
	if(this->IsOpen() == false)
	{
		// Closed by its X button: let go of any text box that still has focus.
		if(this->m_InputsShown)
		{
			this->HideInputs();
		}

		return;
	}

	if(gInterface.CheckWindow(Interface::MoveList) || gInterface.CheckWindow(Interface::ObjWindow::CashShop) || gInterface.CheckWindow(Interface::ObjWindow::FullMap))
	{
		this->Close();
		return;
	}

	// One click per press, not one per frame the button is held.
	bool down = PrimaryDown();
	this->m_Click = down && (this->m_WasDown == false);
	this->m_WasDown = down;

	float x = (MAX_WIN_WIDTH / 2) - (kWindowW / 2);
	float y = 35.0f;

	if(g_pBCustomMenuInfo->gDrawWindowCustom(&x,&y,kWindowW,kWindowH,eWindowFriendMail,"Friends") == 0)
	{
		this->HideInputs();
		return;
	}

	this->m_HoverItem = NULL;

	// Tabs.
	int online = 0;
	int total = 0;
	{
		std::deque<GUILDLIST_TEXT> friends;
		g_pFriendList->UpdateFriendList(friends,"");
		total = (int)friends.size();

		for(size_t n = 0; n < friends.size(); n++)
		{
			if(IsOnline(friends[n].m_Server))
			{
				online++;
			}
		}
	}

	int unread = 0;
	{
		std::deque<LETTERLIST_TEXT> letters;
		g_pLetterList->UpdateLetterList(letters,0);

		for(size_t n = 0; n < letters.size(); n++)
		{
			if(letters[n].m_bIsRead == FALSE)
			{
				unread++;
			}
		}
	}

	char tab1[48];
	char tab2[48];
	sprintf_s(tab1,sizeof(tab1),"Friends %d/%d",online,total);
	sprintf_s(tab2,(unread > 0) ? "Mailbox (%d new)" : "Mailbox",unread);

	float tabY = y + 34.0f;
	bool friendsTab = (this->m_View == VIEW_FRIENDS || this->m_View == VIEW_ADD_FRIEND);

	gInterface.DrawBarForm(x + 20.0f,tabY,120.0f,20.0f,friendsTab ? 0.85f : 0.2f,friendsTab ? 0.6f : 0.2f,0.1f,friendsTab ? 0.55f : 0.5f);
	TextDraw((HFONT)g_hFont,(int)(x + 20.0f),(int)tabY + 4,friendsTab ? kWhite : kGrey,0x0,120,0,3,tab1);

	gInterface.DrawBarForm(x + 145.0f,tabY,130.0f,20.0f,friendsTab ? 0.2f : 0.85f,friendsTab ? 0.2f : 0.6f,0.1f,friendsTab ? 0.5f : 0.55f);
	TextDraw((HFONT)g_hFont,(int)(x + 145.0f),(int)tabY + 4,friendsTab ? kGrey : kWhite,0x0,130,0,3,tab2);

	if(this->RowClicked(x + 20.0f,tabY,120.0f,20.0f))
	{
		this->SetView(VIEW_FRIENDS);
		this->RequestDetails();
		PlayBuffer(25,0,0);
	}
	else if(this->RowClicked(x + 145.0f,tabY,130.0f,20.0f))
	{
		this->SetView(VIEW_MAIL);
		PlayBuffer(25,0,0);
	}

	// Notice line (results, errors), fades after a few seconds.
	if(this->m_Notice[0] != 0 && (GetTickCount() - this->m_NoticeTick) < 6000)
	{
		TextDraw((HFONT)g_hFont,(int)(x + 285.0f),(int)tabY + 4,this->m_NoticeColor,0x0,145,0,1,this->m_Notice);
	}

	float cx = x + 20.0f;
	float cy = tabY + 28.0f;
	float cw = kWindowW - 40.0f;
	float ch = kWindowH - (cy - y) - 45.0f;

	switch(this->m_View)
	{
		case VIEW_FRIENDS:		this->RenderFriends(cx,cy,cw,ch); break;
		case VIEW_MAIL:			this->RenderMail(cx,cy,cw,ch); break;
		case VIEW_READ:			this->RenderRead(cx,cy,cw,ch); break;
		case VIEW_COMPOSE:		this->RenderCompose(cx,cy,cw,ch); break;
		case VIEW_PICK:			this->RenderPick(cx,cy,cw,ch); break;
		case VIEW_ADD_FRIEND:	this->RenderAddFriend(cx,cy,cw,ch); break;
	}

	if(this->m_HoverItem != NULL)
	{
		float tipX = this->m_HoverX + 40.0f;

		if(tipX + 230.0f > (float)MAX_WIN_WIDTH)
		{
			tipX = this->m_HoverX - 230.0f;
		}

		RenderItemInfo((int)tipX,(int)this->m_HoverY,(ITEM*)this->m_HoverItem,false);
	}
}

void CFriendMailWindow::RenderRequestBanner(float x,float y,float w)
{
	if(this->m_FriendRequests.empty())
	{
		return;
	}

	const std::string& name = this->m_FriendRequests.front();

	gInterface.DrawBarForm(x,y,w,26.0f,0.15f,0.35f,0.2f,0.8f);

	char text[96];
	sprintf_s(text,sizeof(text),"%s wants to be your friend",name.c_str());
	TextDraw((HFONT)g_hFont,(int)x + 8,(int)y + 7,kWhite,0x0,(int)(w - 170.0f),0,1,text);

	char nameBuf[MAX_ID_SIZE + 1];
	CopyName(nameBuf,sizeof(nameBuf),name.c_str(),MAX_ID_SIZE);

	if(this->Button(x + w - 160.0f,y + 3.0f,75.0f,"Accept"))
	{
		SendAcceptAddFriend(1,nameBuf);
		this->m_FriendRequests.erase(this->m_FriendRequests.begin());
		this->m_DetailRequestTick = 0;
	}
	else if(this->Button(x + w - 80.0f,y + 3.0f,75.0f,"Decline"))
	{
		SendAcceptAddFriend(0,nameBuf);
		this->m_FriendRequests.erase(this->m_FriendRequests.begin());
	}
}

void CFriendMailWindow::RenderFriends(float x,float y,float w,float h)
{
	float top = y;

	if(this->m_FriendRequests.empty() == false)
	{
		this->RenderRequestBanner(x,y,w);
		top += 30.0f;
	}

	std::deque<GUILDLIST_TEXT> friends;
	g_pFriendList->UpdateFriendList(friends,"");

	// Online first, then by name - the list players actually look for.
	std::sort(friends.begin(),friends.end(),[](const GUILDLIST_TEXT& a,const GUILDLIST_TEXT& b)
	{
		bool ao = IsOnline(a.m_Server);
		bool bo = IsOnline(b.m_Server);

		if(ao != bo)
		{
			return ao;
		}

		return _stricmp(a.m_szID,b.m_szID) < 0;
	});

	int rows = (this->m_FriendRequests.empty() ? kFriendRows : kFriendRows - 1);
	int pages = ((int)friends.size() + rows - 1) / rows;

	if(pages < 1)
	{
		pages = 1;
	}

	if(this->m_Page >= pages)
	{
		this->m_Page = pages - 1;
	}

	if(friends.empty())
	{
		TextDraw((HFONT)g_hFont,(int)x,(int)top + 30,kGrey,0x0,(int)w,0,3,"No friends yet - press Add to send a friend request.");
	}

	const float rowH = 34.0f;

	for(int n = 0; n < rows; n++)
	{
		int index = this->m_Page * rows + n;

		if(index >= (int)friends.size())
		{
			break;
		}

		const GUILDLIST_TEXT& f = friends[index];
		float ry = top + n * (rowH + 2.0f);
		bool selected = (this->m_SelectedFriend == f.m_szID);
		bool hovered = SEASON3B::CheckMouseIn((int)x,(int)ry,(int)w,(int)rowH) == 1;

		float bg = selected ? 0.32f : (hovered ? 0.24f : 0.14f);
		gInterface.DrawBarForm(x,ry,w,rowH,bg,bg,bg + 0.05f,0.85f);

		std::map<std::string,DETAIL>::const_iterator it = this->m_Details.find(f.m_szID);
		const DETAIL* d = (it != this->m_Details.end()) ? &it->second : NULL;

		this->RenderAvatar(x + 3.0f,ry + 3.0f,28.0f,(d != NULL) ? d->Class : 0xFF);

		bool online = IsOnline(f.m_Server);

		// Name and status.
		TextDraw(g_hFontBold,(int)x + 38,(int)ry + 3,online ? kWhite : kGrey,0x0,140,0,1,(char*)f.m_szID);

		gInterface.DrawBarForm(x + 38.0f,ry + 21.0f,7.0f,7.0f,online ? 0.3f : 0.4f,online ? 0.9f : 0.4f,online ? 0.3f : 0.4f,1.0f);

		char status[32];

		bool pending = (d != NULL && d->Pending);
		DWORD statusColor = online ? kGreen : kDim;

		// One-sided: the server reports these as offline whatever their real
		// state, and item mail refuses them, so say why instead.
		if(pending)
		{
			strcpy_s(status,sizeof(status),"Not accepted yet");
			statusColor = kGold;
		}
		else if(online)
		{
			sprintf_s(status,sizeof(status),"Server %d",f.m_Server + 1);
		}
		else if(f.m_Server == 0xFD)
		{
			strcpy_s(status,sizeof(status),"Chat off");
		}
		else
		{
			strcpy_s(status,sizeof(status),"Offline");
		}

		TextDraw((HFONT)g_hFont,(int)x + 49,(int)ry + 18,statusColor,0x0,100,0,1,status);

		// Level / master level / resets, then guild.
		if(d != NULL && d->Class != 0xFF)
		{
			char stats[64];

			if(d->MasterLevel > 0)
			{
				sprintf_s(stats,sizeof(stats),"Lv %d  ML %d",d->Level,d->MasterLevel);
			}
			else
			{
				sprintf_s(stats,sizeof(stats),"Lv %d",d->Level);
			}

			TextDraw((HFONT)g_hFont,(int)x + 150,(int)ry + 4,kWhite,0x0,110,0,1,stats);

			char resets[32];
			sprintf_s(resets,sizeof(resets),"Resets %d",d->Resets);
			TextDraw((HFONT)g_hFont,(int)x + 150,(int)ry + 18,kGold,0x0,110,0,1,resets);

			if(d->Guild[0] != 0)
			{
				char guild[24];
				sprintf_s(guild,sizeof(guild),"<%s>",d->Guild);
				TextDraw((HFONT)g_hFont,(int)x + 265,(int)ry + 11,kBlue,0x0,(int)(w - 270.0f),0,1,guild);
			}
		}
		else if(d != NULL)
		{
			TextDraw((HFONT)g_hFont,(int)x + 150,(int)ry + 11,kDim,0x0,150,0,1,"(character deleted)");
		}

		if(this->RowClicked(x,ry,w,rowH))
		{
			this->m_SelectedFriend = f.m_szID;
			PlayBuffer(25,0,0);
		}
	}

	// Footer.
	float fy = y + h + 6.0f;
	bool has = this->m_SelectedFriend.empty() == false;

	if(this->Button(x,fy,62.0f,"Add"))
	{
		this->SetView(VIEW_ADD_FRIEND);
	}

	if(this->Button(x + 66.0f,fy,62.0f,"Mail",has))
	{
		this->BeginCompose(this->m_SelectedFriend.c_str(),"");
	}

	if(this->Button(x + 132.0f,fy,72.0f,"Whisper",has))
	{
		g_pChatInputBox->SetWhsprID(this->m_SelectedFriend.c_str());

		if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX) == false)
		{
			g_pNewUISystem->Show(SEASON3B::INTERFACE_CHATINPUTBOX);
		}

		this->Close();
	}

	if(this->Button(x + 208.0f,fy,62.0f,"Delete",has))
	{
		char nameBuf[MAX_ID_SIZE + 1];
		CopyName(nameBuf,sizeof(nameBuf),this->m_SelectedFriend.c_str(),MAX_ID_SIZE);
		SendRequestDeleteFriend(nameBuf);
		this->m_SelectedFriend.clear();
	}

	char pageText[16];
	sprintf_s(pageText,sizeof(pageText),"%d/%d",this->m_Page + 1,pages);
	TextDraw((HFONT)g_hFont,(int)(x + w - 82.0f),(int)fy + 4,kGrey,0x0,40,0,3,pageText);

	if(this->Button(x + w - 120.0f,fy,34.0f,"<",this->m_Page > 0))
	{
		this->m_Page--;
	}

	if(this->Button(x + w - 38.0f,fy,34.0f,">",this->m_Page < pages - 1))
	{
		this->m_Page++;
	}
}

void CFriendMailWindow::RenderAddFriend(float x,float y,float w,float h)
{
	this->ShowInputs(false,false,false,true);

	TextDraw((HFONT)g_hFont,(int)x,(int)y + 10,kWhite,0x0,(int)w,0,1,"Character name to add:");

	gInterface.DrawBarForm(x,y + 30.0f,160.0f,18.0f,0.0f,0.0f,0.0f,0.8f);
	g_FriendNameBox->SetPosition((int)x + 4,(int)y + 33);
	g_FriendNameBox->Render();

	if(this->RowClicked(x,y + 30.0f,160.0f,18.0f))
	{
		g_FriendNameBox->GiveFocus(1);
	}

	TextDraw((HFONT)g_hFont,(int)x,(int)y + 60,kDim,0x0,(int)w,0,1,"They will get a friend request they can accept or decline.");

	float fy = y + h + 6.0f;

	if(this->Button(x,fy,90.0f,"Send request"))
	{
		char name[MAX_ID_SIZE + 1] = { 0 };
		g_FriendNameBox->GetText(name,sizeof(name));

		if(name[0] != 0)
		{
			SendRequestAddFriend(name);
			g_FriendNameBox->SetText("");
			this->SetNotice("Friend request sent.",kGreen);
			this->SetView(VIEW_FRIENDS);
		}
	}

	if(this->Button(x + 96.0f,fy,70.0f,"Cancel"))
	{
		this->SetView(VIEW_FRIENDS);
	}
}

void CFriendMailWindow::RenderMail(float x,float y,float w,float h)
{
	std::deque<LETTERLIST_TEXT> letters;
	g_pLetterList->UpdateLetterList(letters,0);

	int pages = ((int)letters.size() + kMailRows - 1) / kMailRows;

	if(pages < 1)
	{
		pages = 1;
	}

	if(this->m_Page >= pages)
	{
		this->m_Page = pages - 1;
	}

	if(letters.empty())
	{
		TextDraw((HFONT)g_hFont,(int)x,(int)y + 30,kGrey,0x0,(int)w,0,3,"Your mailbox is empty.");
	}

	const float rowH = 26.0f;

	for(int n = 0; n < kMailRows; n++)
	{
		int index = this->m_Page * kMailRows + n;

		if(index >= (int)letters.size())
		{
			break;
		}

		const LETTERLIST_TEXT& l = letters[index];
		float ry = y + n * (rowH + 2.0f);
		bool selected = (this->m_SelectedLetter == l.m_dwLetterID);
		bool hovered = SEASON3B::CheckMouseIn((int)x,(int)ry,(int)w,(int)rowH) == 1;

		float bg = selected ? 0.32f : (hovered ? 0.24f : 0.14f);
		gInterface.DrawBarForm(x,ry,w,rowH,bg,bg,bg + 0.05f,0.85f);

		if(l.m_bIsRead == FALSE)
		{
			gInterface.DrawBarForm(x + 5.0f,ry + 10.0f,6.0f,6.0f,0.95f,0.75f,0.2f,1.0f);
		}

		DWORD color = l.m_bIsRead ? kGrey : kWhite;
		TextDraw(l.m_bIsRead ? (HFONT)g_hFont : g_hFontBold,(int)x + 16,(int)ry + 7,color,0x0,90,0,1,(char*)l.m_szID);
		TextDraw((HFONT)g_hFont,(int)x + 108,(int)ry + 7,color,0x0,190,0,1,(char*)l.m_szText);
		TextDraw((HFONT)g_hFont,(int)(x + w - 110.0f),(int)ry + 7,kDim,0x0,70,0,1,(char*)l.m_szDate);

		std::map<DWORD,BYTE>::const_iterator it = this->m_WaitingItems.find(l.m_dwLetterID);

		if(it != this->m_WaitingItems.end() && it->second > 0)
		{
			char badge[16];
			sprintf_s(badge,sizeof(badge),"%d item%s",it->second,(it->second == 1) ? "" : "s");
			gInterface.DrawBarForm(x + w - 40.0f,ry + 5.0f,36.0f,16.0f,0.8f,0.55f,0.1f,0.85f);
			TextDraw((HFONT)g_hFont,(int)(x + w - 40.0f),(int)ry + 7,kWhite,0x0,36,0,3,badge);
		}

		if(this->RowClicked(x,ry,w,rowH))
		{
			if(this->m_SelectedLetter == l.m_dwLetterID)
			{
				this->OpenLetter(l.m_dwLetterID);
			}
			else
			{
				this->m_SelectedLetter = l.m_dwLetterID;
			}

			PlayBuffer(25,0,0);
		}
	}

	float fy = y + h + 6.0f;
	bool has = (this->m_SelectedLetter != 0 && g_pLetterList->GetLetter(this->m_SelectedLetter) != NULL);

	if(this->Button(x,fy,62.0f,"Write"))
	{
		this->BeginCompose("","");
	}

	if(this->Button(x + 66.0f,fy,62.0f,"Open",has))
	{
		this->OpenLetter(this->m_SelectedLetter);
	}

	if(this->Button(x + 132.0f,fy,62.0f,"Delete",has))
	{
		std::map<DWORD,BYTE>::const_iterator it = this->m_WaitingItems.find(this->m_SelectedLetter);

		if(it != this->m_WaitingItems.end() && it->second > 0)
		{
			this->SetNotice("Claim the items first.",kRed);
		}
		else
		{
			SendRequestDeleteLetter(this->m_SelectedLetter);
			this->m_SelectedLetter = 0;
		}
	}

	char pageText[16];
	sprintf_s(pageText,sizeof(pageText),"%d/%d",this->m_Page + 1,pages);
	TextDraw((HFONT)g_hFont,(int)(x + w - 82.0f),(int)fy + 4,kGrey,0x0,40,0,3,pageText);

	if(this->Button(x + w - 120.0f,fy,34.0f,"<",this->m_Page > 0))
	{
		this->m_Page--;
	}

	if(this->Button(x + w - 38.0f,fy,34.0f,">",this->m_Page < pages - 1))
	{
		this->m_Page++;
	}
}

void CFriendMailWindow::ClearReadItems()
{
	for(int n = 0; n < FRIENDMAIL_MAX_ITEMS; n++)
	{
		if(this->m_ReadItems[n] != NULL)
		{
			g_pNewItemMng->DeleteItem(this->m_ReadItems[n]);
			this->m_ReadItems[n] = NULL;
		}
	}

	this->m_ReadItemCount = 0;
	memset(this->m_ReadCoins,0,sizeof(this->m_ReadCoins));
}

void CFriendMailWindow::OpenLetter(DWORD letterId)
{
	if(g_pLetterList->GetLetter(letterId) == NULL)
	{
		return;
	}

	this->m_ReadLetter = letterId;
	this->m_ReadTextLoaded = false;
	this->m_ReadText[0] = 0;
	this->ClearReadItems();
	this->SetView(VIEW_READ);

	// Already read once this session: the text is cached client-side.
	LPFS_LETTER_TEXT cached = g_pLetterList->GetLetterText(letterId);

	if(cached != NULL)
	{
		CopyName(this->m_ReadText,sizeof(this->m_ReadText),(const char*)cached->Memo,FRIENDMAIL_MEMO_MAX);
		this->m_ReadTextLoaded = true;
	}
	else
	{
		SendRequestLetterText(letterId);
	}

	std::map<DWORD,BYTE>::const_iterator it = this->m_WaitingItems.find(letterId);

	if(it != this->m_WaitingItems.end() && it->second > 0)
	{
		PMSG_FRIENDMAIL_MEMO pMsg;
		pMsg.header.set(0xD3,0xE2,sizeof(pMsg));
		pMsg.MemoIndex = (WORD)letterId;
		DataSend((LPBYTE)&pMsg,pMsg.header.size);
	}
}

void CFriendMailWindow::RenderRead(float x,float y,float w,float h)
{
	LETTERLIST_TEXT* pLetter = g_pLetterList->GetLetter(this->m_ReadLetter);

	if(pLetter == NULL)
	{
		this->SetView(VIEW_MAIL);
		return;
	}

	gInterface.DrawBarForm(x,y,w,40.0f,0.16f,0.16f,0.2f,0.85f);

	char from[64];
	sprintf_s(from,sizeof(from),"From: %s",pLetter->m_szID);
	TextDraw(g_hFontBold,(int)x + 8,(int)y + 5,kWhite,0x0,200,0,1,from);

	char when[48];
	sprintf_s(when,sizeof(when),"%s %s",pLetter->m_szDate,pLetter->m_szTime);
	TextDraw((HFONT)g_hFont,(int)(x + w - 150.0f),(int)y + 5,kDim,0x0,145,0,1,when);

	TextDraw((HFONT)g_hFont,(int)x + 8,(int)y + 22,kGold,0x0,(int)(w - 16.0f),0,1,pLetter->m_szText);

	float textY = y + 46.0f;
	gInterface.DrawBarForm(x,textY,w,110.0f,0.08f,0.08f,0.1f,0.8f);

	if(this->m_ReadTextLoaded)
	{
		this->RenderWrappedText(x + 8.0f,textY + 6.0f,w - 16.0f,8,this->m_ReadText,kWhite);
	}
	else
	{
		TextDraw((HFONT)g_hFont,(int)x + 8,(int)textY + 6,kDim,0x0,(int)w,0,1,"Loading...");
	}

	// Attachments.
	float itemY = textY + 116.0f;
	const float tile = 40.0f;

	int coinKinds = this->ReadCoinCount();

	if(this->m_ReadItemCount > 0 || coinKinds > 0)
	{
		TextDraw((HFONT)g_hFont,(int)x,(int)itemY + 13,kWhite,0x0,80,0,1,"Attached:");

		g_pNewUISystem->BeginItem3DFreeBatch();

		for(int n = 0; n < this->m_ReadItemCount; n++)
		{
			float tx = x + 70.0f + n * (tile + 6.0f);
			bool hovered = SEASON3B::CheckMouseIn((int)tx,(int)itemY,(int)tile,(int)tile) == 1;
			this->RenderItemTile(tx,itemY,tile,this->m_ReadItems[n],hovered);

			if(hovered && this->m_ReadItems[n] != NULL)
			{
				this->m_HoverItem = this->m_ReadItems[n];
				this->m_HoverX = tx;
				this->m_HoverY = itemY;
			}
		}

		g_pNewUISystem->EndItem3DFreeBatch();

		// Coins, one line per currency, after the item tiles.
		float coinX = x + 70.0f + this->m_ReadItemCount * (tile + 6.0f) + 4.0f;
		int line = 0;

		for(int c = 0; c < 3; c++)
		{
			if(this->m_ReadCoins[c] == 0)
			{
				continue;
			}

			char coinText[64];
			sprintf_s(coinText,sizeof(coinText),"+ %s %s",gInterface.NumberFormat((int)this->m_ReadCoins[c]),CoinName(c + 1));
			TextDraw(g_hFontBold,(int)coinX,(int)itemY + 6 + line * 14,kGold,0x0,(int)((x + w) - coinX),0,1,coinText);
			line++;
		}
	}

	float fy = y + h + 6.0f;

	if(this->Button(x,fy,62.0f,"Back"))
	{
		this->ClearReadItems();
		this->SetView(VIEW_MAIL);
		return;
	}

	if(this->Button(x + 66.0f,fy,62.0f,"Reply"))
	{
		char subject[40];
		sprintf_s(subject,sizeof(subject),"Re: %.27s",pLetter->m_szText);
		this->BeginCompose(pLetter->m_szID,subject);
		return;
	}

	int attachments = this->m_ReadItemCount + this->ReadCoinCount();
	bool hasItems = (attachments > 0);

	if(this->Button(x + 132.0f,fy,62.0f,"Delete",hasItems == false))
	{
		SendRequestDeleteLetter(this->m_ReadLetter);
		this->ClearReadItems();
		this->SetView(VIEW_MAIL);
		return;
	}

	if(hasItems)
	{
		char claimText[32];
		if(this->m_ClaimPending)
		{
			strcpy_s(claimText,sizeof(claimText),"Claiming...");
		}
		else if(this->m_ReadItemCount == 0)
		{
			strcpy_s(claimText,sizeof(claimText),"Claim coins");
		}
		else
		{
			sprintf_s(claimText,sizeof(claimText),"Claim %d item%s%s",this->m_ReadItemCount,(this->m_ReadItemCount == 1) ? "" : "s",(attachments > this->m_ReadItemCount) ? " + coins" : "");
		}

		if(this->Button(x + w - 120.0f,fy,120.0f,claimText,this->m_ClaimPending == false))
		{
			PMSG_FRIENDMAIL_MEMO pMsg;
			pMsg.header.set(0xD3,0xE3,sizeof(pMsg));
			pMsg.MemoIndex = (WORD)this->m_ReadLetter;
			DataSend((LPBYTE)&pMsg,pMsg.header.size);
			this->m_ClaimPending = true;
		}
	}
}

int CFriendMailWindow::AttachmentCount() const
{
	int count = 0;

	for(int n = 0; n < FRIENDMAIL_MAX_ITEMS; n++)
	{
		if(this->m_Attach[n].Slot >= 0)
		{
			count++;
		}
	}

	return count;
}

void CFriendMailWindow::BeginCompose(const char* toName,const char* subject)
{
	this->SetView(VIEW_COMPOSE);
	this->ShowInputs(true,true,true,false,this->m_CoinType != 0);

	g_ToBox->SetText(toName);
	g_SubjectBox->SetText(subject);
	g_TextBox->SetText("");
	g_CoinBox->SetText("");

	for(int n = 0; n < FRIENDMAIL_MAX_ITEMS; n++)
	{
		this->m_Attach[n].Slot = -1;
		this->m_Attach[n].Type = -1;
	}

	this->m_SendPending = false;

	if(toName[0] == 0)
	{
		g_ToBox->GiveFocus(1);
	}
	else
	{
		g_TextBox->GiveFocus(1);
	}
}

void CFriendMailWindow::RenderCompose(float x,float y,float w,float h)
{
	this->ShowInputs(true,true,true,false,this->m_CoinType != 0);

	TextDraw((HFONT)g_hFont,(int)x,(int)y + 4,kGrey,0x0,40,0,1,"To:");
	gInterface.DrawBarForm(x + 50.0f,y,160.0f,18.0f,0.0f,0.0f,0.0f,0.8f);
	g_ToBox->SetPosition((int)x + 54,(int)y + 3);
	g_ToBox->Render();

	TextDraw((HFONT)g_hFont,(int)x,(int)y + 28,kGrey,0x0,50,0,1,"Subject:");
	gInterface.DrawBarForm(x + 50.0f,y + 24.0f,w - 50.0f,18.0f,0.0f,0.0f,0.0f,0.8f);
	g_SubjectBox->SetPosition((int)x + 54,(int)y + 27);
	g_SubjectBox->Render();

	gInterface.DrawBarForm(x,y + 48.0f,w,80.0f,0.0f,0.0f,0.0f,0.8f);
	g_TextBox->SetPosition((int)x + 4,(int)y + 52);
	g_TextBox->Render();

	if(this->RowClicked(x + 50.0f,y,160.0f,18.0f)) { g_ToBox->GiveFocus(1); }
	if(this->RowClicked(x + 50.0f,y + 24.0f,w - 50.0f,18.0f)) { g_SubjectBox->GiveFocus(1); }
	if(this->RowClicked(x,y + 48.0f,w,80.0f)) { g_TextBox->GiveFocus(1); }

	// Attachment slots.
	float itemY = y + 136.0f;
	const float tile = 40.0f;
	int maxItems = (this->m_MaxPerLetter < FRIENDMAIL_MAX_ITEMS) ? this->m_MaxPerLetter : FRIENDMAIL_MAX_ITEMS;

	TextDraw((HFONT)g_hFont,(int)x,(int)itemY + 13,kWhite,0x0,80,0,1,"Items:");

	g_pNewUISystem->BeginItem3DFreeBatch();

	for(int n = 0; n < maxItems; n++)
	{
		float tx = x + 50.0f + n * (tile + 6.0f);
		bool hovered = SEASON3B::CheckMouseIn((int)tx,(int)itemY,(int)tile,(int)tile) == 1;
		ITEM* pItem = NULL;

		if(this->m_Attach[n].Slot >= 0)
		{
			pItem = FindInventoryItem(this->m_Attach[n].Slot);

			// Moved, sold or used since it was picked: drop it from the letter.
			if(pItem == NULL || pItem->Type != this->m_Attach[n].Type)
			{
				this->m_Attach[n].Slot = -1;
				this->m_Attach[n].Type = -1;
				pItem = NULL;
			}
		}

		this->RenderItemTile(tx,itemY,tile,pItem,hovered);

		if(pItem == NULL)
		{
			TextDraw((HFONT)g_hFont,(int)tx,(int)itemY + 13,kDim,0x0,(int)tile,0,3,"+");
		}
		else if(hovered)
		{
			this->m_HoverItem = pItem;
			this->m_HoverX = tx;
			this->m_HoverY = itemY;
		}

		if(this->RowClicked(tx,itemY,tile,tile))
		{
			if(pItem != NULL)
			{
				this->m_Attach[n].Slot = -1;	// tap a filled slot to remove it
				this->m_Attach[n].Type = -1;
			}
			else
			{
				this->m_PickTarget = n;
				this->m_View = VIEW_PICK;
				this->m_Page = 0;
				this->HideInputs();
			}

			PlayBuffer(25,0,0);
		}
	}

	g_pNewUISystem->EndItem3DFreeBatch();

	int attached = this->AttachmentCount();
	float infoX = x + 50.0f + maxItems * (tile + 6.0f) + 8.0f;
	float infoW = (x + w) - infoX;

	// Postage is paid on every letter; the item fee only for attached items.
	char postage[64];

	if(this->m_SendZen > 0)
	{
		sprintf_s(postage,sizeof(postage),"Postage: %s Zen",gInterface.NumberFormat((int)this->m_SendZen));
	}
	else
	{
		strcpy_s(postage,sizeof(postage),"Postage: free");
	}

	TextDraw((HFONT)g_hFont,(int)infoX,(int)itemY,kGold,0x0,(int)infoW,0,1,postage);

	char fee[96];
	this->ItemFeeText(fee,sizeof(fee),attached);
	TextDraw((HFONT)g_hFont,(int)infoX,(int)itemY + 13,attached > 0 ? kGold : kDim,0x0,(int)infoW,0,1,fee);

	if(this->m_ExpireDays > 0)
	{
		char expire[64];
		sprintf_s(expire,sizeof(expire),"Unclaimed items return after %d day%s",this->m_ExpireDays,(this->m_ExpireDays == 1) ? "" : "s");
		TextDraw((HFONT)g_hFont,(int)infoX,(int)itemY + 26,kDim,0x0,(int)infoW,0,1,expire);
	}

	// Coins: which one (tap to switch between those the server allows) and how many.
	if(this->m_CoinType != 0)
	{
		float coinY = itemY + tile + 10.0f;

		TextDraw((HFONT)g_hFont,(int)x,(int)coinY + 4,kWhite,0x0,50,0,1,"Coins:");

		if(this->Button(x + 50.0f,coinY,100.0f,CoinName(this->m_CoinType)))
		{
			this->NextCoinType();
		}

		gInterface.DrawBarForm(x + 156.0f,coinY,100.0f,18.0f,0.0f,0.0f,0.0f,0.8f);
		g_CoinBox->SetPosition((int)x + 160,(int)coinY + 3);
		g_CoinBox->Render();

		if(this->RowClicked(x + 156.0f,coinY,100.0f,18.0f)) { g_CoinBox->GiveFocus(1); }

		char hint[64];

		if(this->m_CoinMax > 0)
		{
			sprintf_s(hint,sizeof(hint),"optional, up to %s",gInterface.NumberFormat((int)this->m_CoinMax));
		}
		else
		{
			strcpy_s(hint,sizeof(hint),"optional");
		}

		TextDraw((HFONT)g_hFont,(int)x + 264,(int)coinY + 4,kDim,0x0,(int)(w - 264.0f),0,1,hint);
	}

	float fy = y + h + 6.0f;

	if(this->m_SendPending && (GetTickCount() - this->m_SendTick) > 12000)
	{
		this->m_SendPending = false;	// no reply: let the player try again
	}

	if(this->Button(x,fy,70.0f,this->m_SendPending ? "Sending..." : "Send",this->m_SendPending == false))
	{
		this->SendCompose();
	}

	if(this->Button(x + 76.0f,fy,70.0f,"Cancel"))
	{
		this->SetView(VIEW_MAIL);
	}
}

void CFriendMailWindow::SendCompose()
{
	char to[MAX_ID_SIZE + 1] = { 0 };
	char subject[64] = { 0 };
	char text[FRIENDMAIL_MEMO_MAX + 1] = { 0 };

	g_ToBox->GetText(to,sizeof(to));
	g_SubjectBox->GetText(subject,sizeof(subject));
	g_TextBox->GetText(text,sizeof(text));

	if(to[0] == 0)
	{
		this->SetNotice("Enter who the letter is for.",kRed);
		return;
	}

	if(subject[0] == 0)
	{
		strcpy_s(subject,sizeof(subject),"(no subject)");
	}

	// The edit control hands back CR LF; the letter reader wants plain LF.
	int len = 0;

	for(int n = 0; text[n] != 0; n++)
	{
		if(text[n] != '\r')
		{
			text[len++] = text[n];
		}
	}

	text[len] = 0;

	DWORD coins = this->ReadCoinAmount();

	if(this->m_CoinMax > 0 && coins > this->m_CoinMax)
	{
		char notice[64];
		sprintf_s(notice,sizeof(notice),"At most %s coins per letter.",gInterface.NumberFormat((int)this->m_CoinMax));
		this->SetNotice(notice,kRed);
		return;
	}

	if(this->AttachmentCount() == 0 && coins == 0)
	{
		// A plain letter: the legacy packet, so it works with any server.
		char subjectWire[MAX_LETTER_TITLE_LENGTH + 1] = { 0 };
		strncpy_s(subjectWire,sizeof(subjectWire),subject,31);

		SendRequestSendLetter(FRIENDMAIL_LETTER_WINDOW_GUID,to,subjectWire,0,0,(WORD)len,text);
	}
	else
	{
		PMSG_FRIENDMAIL_SEND pMsg;
		memset(&pMsg,0,sizeof(pMsg));
		CopyName(pMsg.ToName,sizeof(pMsg.ToName),to,MAX_ID_SIZE);
		memcpy(pMsg.Subject,subject,(strlen(subject) < sizeof(pMsg.Subject)) ? strlen(subject) : sizeof(pMsg.Subject) - 1);

		for(int n = 0; n < FRIENDMAIL_MAX_ITEMS; n++)
		{
			if(this->m_Attach[n].Slot >= 0)
			{
				pMsg.Slot[pMsg.ItemCount++] = (BYTE)this->m_Attach[n].Slot;
			}
		}

		if(coins > 0)
		{
			pMsg.CoinType = this->m_CoinType;
			pMsg.CoinAmount = coins;
		}

		pMsg.MemoSize = (WORD)len;
		memcpy(pMsg.Memo,text,len);

		int size = (int)(sizeof(pMsg) - sizeof(pMsg.Memo) + len);
		pMsg.header.set(0xD3,0xE1,size);
		DataSend((LPBYTE)&pMsg,size);
	}

	this->m_SendPending = true;
	this->m_SendTick = GetTickCount();
}

void CFriendMailWindow::RenderPick(float x,float y,float w,float h)
{
	TextDraw((HFONT)g_hFont,(int)x,(int)y,kWhite,0x0,(int)w,0,1,"Choose an item to attach (equipped items cannot be mailed):");

	SEASON3B::CNewUIInventoryCtrl* ctrls[8];
	int ctrlCount = CollectInventoryCtrls(ctrls,8);

	std::vector<std::pair<int,ITEM*> > items;

	for(int c = 0; c < ctrlCount; c++)
	{
		int itemCount = (int)ctrls[c]->GetNumberOfItems();

		for(int n = 0; n < itemCount; n++)
		{
			ITEM* pItem = ctrls[c]->GetItem(n);

			if(pItem == NULL)
			{
				continue;
			}

			int slot = ctrls[c]->GetIndexByItem(pItem);
			bool already = false;

			for(int a = 0; a < FRIENDMAIL_MAX_ITEMS; a++)
			{
				if(this->m_Attach[a].Slot == slot)
				{
					already = true;
				}
			}

			if(already == false)
			{
				items.push_back(std::make_pair(slot,pItem));
			}
		}
	}

	const int perPage = kPickColumns * kPickRows;
	int pages = ((int)items.size() + perPage - 1) / perPage;

	if(pages < 1)
	{
		pages = 1;
	}

	if(this->m_Page >= pages)
	{
		this->m_Page = pages - 1;
	}

	const float tile = 38.0f;

	g_pNewUISystem->BeginItem3DFreeBatch();

	for(int n = 0; n < perPage; n++)
	{
		int index = this->m_Page * perPage + n;

		if(index >= (int)items.size())
		{
			break;
		}

		float tx = x + (n % kPickColumns) * (tile + 3.0f);
		float ty = y + 18.0f + (n / kPickColumns) * (tile + 3.0f);
		bool hovered = SEASON3B::CheckMouseIn((int)tx,(int)ty,(int)tile,(int)tile) == 1;

		this->RenderItemTile(tx,ty,tile,items[index].second,hovered);

		if(hovered)
		{
			this->m_HoverItem = items[index].second;
			this->m_HoverX = tx;
			this->m_HoverY = ty;
		}

		if(this->RowClicked(tx,ty,tile,tile))
		{
			int target = this->m_PickTarget;

			if(target < 0 || target >= FRIENDMAIL_MAX_ITEMS)
			{
				target = 0;
			}

			this->m_Attach[target].Slot = items[index].first;
			this->m_Attach[target].Type = items[index].second->Type;
			this->m_View = VIEW_COMPOSE;
			this->m_Page = 0;
			PlayBuffer(25,0,0);
			g_pNewUISystem->EndItem3DFreeBatch();
			return;
		}
	}

	g_pNewUISystem->EndItem3DFreeBatch();

	if(items.empty())
	{
		TextDraw((HFONT)g_hFont,(int)x,(int)y + 40,kGrey,0x0,(int)w,0,3,"Your inventory is empty.");
	}

	float fy = y + h + 6.0f;

	if(this->Button(x,fy,70.0f,"Back"))
	{
		this->m_View = VIEW_COMPOSE;
		this->m_Page = 0;
	}

	char pageText[16];
	sprintf_s(pageText,sizeof(pageText),"%d/%d",this->m_Page + 1,pages);
	TextDraw((HFONT)g_hFont,(int)(x + w - 82.0f),(int)fy + 4,kGrey,0x0,40,0,3,pageText);

	if(this->Button(x + w - 120.0f,fy,34.0f,"<",this->m_Page > 0))
	{
		this->m_Page--;
	}

	if(this->Button(x + w - 38.0f,fy,34.0f,">",this->m_Page < pages - 1))
	{
		this->m_Page++;
	}
}

// -----------------------------------------------------------------------------
// packets
// -----------------------------------------------------------------------------

void CFriendMailWindow::RecvConfig(PMSG_FRIENDMAIL_CONFIG_RECV* lpMsg)
{
	this->m_ConfigReceived = true;
	this->m_Modern = (lpMsg->Modern != 0);
	this->m_FeeType = lpMsg->FeeType;
	this->m_Fee = lpMsg->Fee;
	this->m_FeeItem = lpMsg->FeeItem;
	this->m_FeeItemLevel = lpMsg->FeeItemLevel;
	this->m_MaxPerLetter = (lpMsg->MaxPerLetter < 1) ? 1 : lpMsg->MaxPerLetter;
	this->m_ExpireDays = lpMsg->ExpireDays;
	this->m_SendZen = lpMsg->SendZen;
	this->m_CoinTypes = lpMsg->CoinTypes & 7;
	this->m_CoinMax = lpMsg->CoinMax;

	if(this->m_CoinType == 0 || (this->m_CoinTypes & (1 << (this->m_CoinType - 1))) == 0)
	{
		this->m_CoinType = 0;
		this->NextCoinType();
	}
}

void CFriendMailWindow::RecvDetails(PMSG_FRIENDMAIL_DETAIL_RECV* lpMsg)
{
	this->m_Details.clear();

	int count = (lpMsg->Count > FRIENDMAIL_MAX_DETAILS) ? FRIENDMAIL_MAX_DETAILS : lpMsg->Count;

	for(int n = 0; n < count; n++)
	{
		const PMSG_FRIENDMAIL_DETAIL& in = lpMsg->List[n];

		char name[MAX_ID_SIZE + 1];
		CopyName(name,sizeof(name),in.Name,MAX_ID_SIZE);

		DETAIL d;
		d.Class = in.Class;
		d.Level = in.Level;
		d.MasterLevel = in.MasterLevel;
		d.Resets = in.Resets;
		CopyName(d.Guild,sizeof(d.Guild),in.Guild,8);
		d.Pending = (in.Pending != 0);

		this->m_Details[name] = d;
	}
}

void CFriendMailWindow::RecvCounts(PMSG_FRIENDMAIL_COUNTS_RECV* lpMsg)
{
	this->m_WaitingItems.clear();

	int count = (lpMsg->Count > FRIENDMAIL_MAX_COUNTS) ? FRIENDMAIL_MAX_COUNTS : lpMsg->Count;

	for(int n = 0; n < count; n++)
	{
		this->m_WaitingItems[lpMsg->List[n].MemoIndex] = lpMsg->List[n].Count;
	}
}

void CFriendMailWindow::RecvItems(PMSG_FRIENDMAIL_ITEMS_RECV* lpMsg)
{
	if(lpMsg->MemoIndex != (WORD)this->m_ReadLetter)
	{
		return;
	}

	this->ClearReadItems();

	int count = (lpMsg->Count > FRIENDMAIL_MAX_ITEMS) ? FRIENDMAIL_MAX_ITEMS : lpMsg->Count;

	for(int n = 0; n < count; n++)
	{
		ITEM* pItem = g_pNewItemMng->CreateItem(lpMsg->Items[n]);

		if(pItem != NULL)
		{
			this->m_ReadItems[this->m_ReadItemCount++] = pItem;
		}
	}

	for(int c = 0; c < 3; c++)
	{
		this->m_ReadCoins[c] = lpMsg->Coins[c];
	}
}

void CFriendMailWindow::RecvSendResult(PMSG_FRIENDMAIL_RESULT_RECV* lpMsg)
{
	this->m_SendPending = false;

	switch(lpMsg->Result)
	{
		case 1:
			this->SetNotice("Letter sent.",kGreen);
			this->SetView(VIEW_MAIL);
			break;
		case 2:
			this->SetNotice("Items only go to friends who accepted you.",kRed);
			break;
		case 3:
			this->SetNotice("Their mailbox is full - items are in yours.",kGold);
			this->SetView(VIEW_MAIL);
			break;
		case 4:
			this->SetNotice("Not enough fee items (bag + Jewel Bank).",kRed);
			break;
		case 5:
			this->SetNotice("That item cannot be mailed.",kRed);
			break;
		case 6:
			this->SetNotice("Busy - close other windows and try again.",kRed);
			break;
		case 7:
			this->SetNotice("Too many items in one letter.",kRed);
			break;
		case 10:
			this->SetNotice("Those coins cannot be mailed (type or amount).",kRed);
			break;
		case 11:
			this->SetNotice("Not enough zen for postage and fees.",kRed);
			break;
		case 12:
			this->SetNotice("Not enough coins.",kRed);
			break;
		default:
			this->SetNotice("The letter could not be sent.",kRed);
			break;
	}
}

void CFriendMailWindow::RecvClaimResult(PMSG_FRIENDMAIL_CLAIM_RECV* lpMsg)
{
	this->m_ClaimPending = false;

	switch(lpMsg->Result)
	{
		case 1:
			{
				char text[64];
				sprintf_s(text,sizeof(text),"Claimed %d attachment%s.",lpMsg->Claimed,(lpMsg->Claimed == 1) ? "" : "s");
				this->SetNotice(text,kGreen);

				if(lpMsg->MemoIndex == (WORD)this->m_ReadLetter)
				{
					this->ClearReadItems();
				}

				this->m_WaitingItems.erase(lpMsg->MemoIndex);
			}
			break;
		case 8:
			this->SetNotice("Not enough inventory space.",kRed);
			break;
		case 9:
			this->SetNotice("Nothing left to claim.",kGrey);
			this->ClearReadItems();
			this->m_WaitingItems.erase(lpMsg->MemoIndex);
			break;
		case 6:
			this->SetNotice("Busy - close other windows and try again.",kRed);
			break;
		default:
			this->SetNotice("Could not claim the items.",kRed);
			break;
	}
}

// -----------------------------------------------------------------------------
// legacy hooks
// -----------------------------------------------------------------------------

bool CFriendMailWindow::OnLetterText(DWORD letterId,const char* text)
{
	// Anything this window asked for stays here even if the player has moved
	// on to another view - the legacy read window must not pop up over it.
	if(this->IsOpen() == false || letterId != this->m_ReadLetter)
	{
		return false;
	}

	CopyName(this->m_ReadText,sizeof(this->m_ReadText),text,FRIENDMAIL_MEMO_MAX);
	this->m_ReadTextLoaded = true;
	return true;
}

bool CFriendMailWindow::OnLetterSendResult(DWORD windowGuid,BYTE result)
{
	if(windowGuid != FRIENDMAIL_LETTER_WINDOW_GUID)
	{
		return false;
	}

	this->m_SendPending = false;

	// Same texts the legacy write window shows (WSclient.cpp
	// ReceiveLetterSendResult), so the wording stays localised.
	switch(result)
	{
		case 1:
			this->SetNotice("Letter sent.",kGreen);
			this->SetView(VIEW_MAIL);
			break;
		case 0:		this->SetNotice(GlobalText[1053],kRed); break;
		case 2:		this->SetNotice(GlobalText[1061],kRed); break;
		case 3:		this->SetNotice(GlobalText[1064],kRed); break;
		case 4:		this->SetNotice(GlobalText[1065],kRed); break;
		case 6:		this->SetNotice(GlobalText[1068],kRed); break;
		case 7:		this->SetNotice(GlobalText[423],kRed); break;
		default:	this->SetNotice("The letter could not be sent.",kRed); break;
	}

	return true;
}

bool CFriendMailWindow::OnFriendRequest(const char* name)
{
	if(this->IsModern() == false)
	{
		return false;
	}

	for(size_t n = 0; n < this->m_FriendRequests.size(); n++)
	{
		if(this->m_FriendRequests[n] == name)
		{
			return true;
		}
	}

	this->m_FriendRequests.push_back(name);

	if(this->IsOpen() == false)
	{
		gInterface.Data[eWindowFriendMail].OnShow = true;
	}

	this->SetView(VIEW_FRIENDS);
	return true;
}

bool CFriendMailWindow::OnLegacyMessage(const char* text)
{
	if(this->IsModern() == false)
	{
		return false;
	}

	if(this->IsOpen())
	{
		this->SetNotice(text,kGold);
	}
	else
	{
		g_pChatListBox->AddText("",text,SEASON3B::TYPE_SYSTEM_MESSAGE);
	}

	return true;
}

void CFriendMailWindow::OnLetterDeleted(DWORD letterId)
{
	this->m_WaitingItems.erase(letterId);

	if(this->m_ReadLetter == letterId && this->m_View == VIEW_READ)
	{
		this->ClearReadItems();
		this->SetView(VIEW_MAIL);
	}
}
