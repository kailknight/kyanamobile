#include "StdAfx.h"
#include "CB_DangKyInGame.h"

#if defined(__ANDROID__) || defined(MU_IOS)
// Defined in android_main.cpp. Declared here rather than in a header because
// that file exports nothing else and is not part of the PC build.
void SetAndroidRegisterOverlayVisible(bool visible);
void RenderAndroidRegisterOverlay();
const char* AndroidRegisterOverlayAccountText();
const char* AndroidRegisterOverlayPasswordText();
#endif
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"
#include "UIMng.h"
#include "Input.h"
#include "CustomEventTime.h"
#include "Protect.h"

CB_DangKyInGame* gCB_DangKyInGame;

namespace
{
	const DWORD kRegisterTextColor = 0xFFFFFFFF;
	const float kRegisterWindowWidth = 262.0f;
	const float kRegisterWindowHeight = 226.0f;   // was 250 (captcha row removed), then 210
	const float kInputWidth = 110.0f;

	// 26, not 20. DoMouseAction() - which is what actually focuses a box on tap -
	// hit-tests a rect padded 4px above and 8px taller than the box, so a 14px
	// box tests as 22px. At 20px spacing those padded rects overlapped, and the
	// row above would swallow a tap meant for the row below. 26 leaves a 4px gap
	// between them so each tap lands on exactly one field.
	const float kInputSpacing = 26.0f;
	const char* kDefaultRegisterPhone = "0000000000";
	const char* kPlaceholderRegisterPin = "0000000";

	bool IsRegisterInputTap()
	{
#if defined(__ANDROID__) || defined(MU_IOS)
		return SEASON3B::IsPress(VK_LBUTTON)
			|| CInput::Instance().IsLBtnDn()
			|| ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#else
		return ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	}

	const char* ResolveRegisterLabel(const char* text, const char* fallback)
	{
		if (text == NULL || text[0] == '\0' || strcmp(text, "Null") == 0 || strcmp(text, "NULL") == 0)
		{
			return fallback;
		}

		return text;
	}

	void RenderRegisterText(HFONT font, float boxX, float boxY, float boxW, float boxH, int align, const char* text)
	{
		if (text == NULL || text[0] == '\0')
		{
			return;
		}

		DWORD backupTextColor = g_pRenderText->GetTextColor();
		DWORD backupBgColor = g_pRenderText->GetBgColor();

		gInterface.DrawBarForm(boxX, boxY, boxW, boxH, 0.0f, 0.0f, 0.0f, 0.60f);
		g_pRenderText->SetFont(font != NULL ? font : g_hFont);
		g_pRenderText->SetShadowText(0);
		g_pRenderText->SetTextColor(kRegisterTextColor);
		g_pRenderText->SetBgColor(0);
		g_pRenderText->RenderText((int)boxX, (int)boxY + 1, text, (int)boxW, 0, align);
		g_pRenderText->SetFont(g_hFont);
		g_pRenderText->SetTextColor(backupTextColor);
		g_pRenderText->SetBgColor(backupBgColor);
	}

	void RenderRegisterInput(
		CUITextInputBox*& input,
		float posX,
		float posY,
		float width,
		float height,
		UIOPTIONS option,
		int maxLength,
		bool isPassword)
	{
		if (input == NULL)
		{
			input = new CUITextInputBox;
			input->Init(g_hWnd, static_cast<int>(width), static_cast<int>(height), maxLength, isPassword ? TRUE : FALSE);
			input->SetTextColor(255, 255, 255, 255);
			input->SetBackColor(255, 0, 0, 0);
			input->SetFont(g_hFont);
			input->SetState(UISTATE_NORMAL);
			input->SetOption(option);
			input->SetText("");
		}

		input->SetPosition(static_cast<int>(posX), static_cast<int>(posY));
		input->Render();

		// DoAction() is what actually drives the box - it drains the message
		// queue, which is where the caret and the typed characters come from.
		// Skipping it entirely on mobile left the fields drawn and focusable but
		// unable to take a single keystroke.
		//
		// The full call, on every platform. DoMouseAction() inside it is what
		// reliably focuses a box on tap - CLoginWin depends on the same thing on
		// Android and its fields have always focused first time, while this
		// window's own FocusRegisterInputOnTap was the only path here and needed
		// two or three taps to catch. Restricting this to bMessageOnly on mobile
		// let typing work but took that second path away again.
		//
		// The reason it was restricted - DoMouseAction's padded hit rect
		// overlapping the neighbouring row - is fixed at the source instead, by
		// spacing the rows past the padding (see kInputSpacing).
		input->DoAction();
	}

	void FocusRegisterInputOnTap(float posX, float posY, float width, float height, CUITextInputBox* input)
	{
#if defined(__ANDROID__) || defined(MU_IOS)
		if (input == NULL)
		{
			return;
		}

		if (SEASON3B::CheckMouseIn((int)posX, (int)posY, (int)width, (int)height) == 1)
		{
			if (IsRegisterInputTap())
			{
				input->GiveFocus(TRUE);
				pSetCursorFocus = true;
				PlayBuffer(25, 0, 0);
			}
		}
#else
		(void)posX;
		(void)posY;
		(void)width;
		(void)height;
		(void)input;
#endif
	}

}

CB_DangKyInGame::CB_DangKyInGame()
{
	for (int i = 0; i < TYPE_INPUT_DKTK::eMaxINPUT; ++i)
	{
		CInputData[i] = NULL;
	}

	TimeSendRegTK = GetTickCount();
	OpenDKTK = false;
	m_RegPending = false;
	m_RegSentTick = 0;
}

CB_DangKyInGame::~CB_DangKyInGame()
{
	Clear();
}

void CB_DangKyInGame::Clear()
{
	for (int i = 0; i < TYPE_INPUT_DKTK::eMaxINPUT; ++i)
	{
		SAFE_DELETE(CInputData[i]);
	}


	TimeSendRegTK = GetTickCount();
	OpenDKTK = false;
	m_RegPending = false;
}

void CB_DangKyInGame::CheckRegisterTimeout()
{
	const DWORD kRegisterReplyTimeoutMs = 10000;

	if (m_RegPending && (GetTickCount() - m_RegSentTick) > kRegisterReplyTimeoutMs)
	{
		m_RegPending = false;
		gInterface.OpenMessageBox("Register", "No response from the server.\nPlease try again.");
	}
}

void CB_DangKyInGame::OpenOnOff()
{
	if (GetTickCount() - gInterface.Data[eWindow_DangKyInGame].EventTick <= 300)
	{
		return;
	}

	gInterface.Data[eWindow_DangKyInGame].EventTick = GetTickCount();

	if (gInterface.Data[eWindow_DangKyInGame].OnShow)
	{
		gInterface.Data[eWindow_DangKyInGame].Close();
		Clear();
		return;
	}

	gInterface.Data[eWindow_DangKyInGame].Open();
	OpenDKTK = true;
}

bool CB_DangKyInGame::RenderWindow(int X, int Y)
{
	(void)X;
	(void)Y;

	if (!gInterface.Data[eWindow_DangKyInGame].OnShow || gCB_DangKyInGame == NULL)
	{
		if (OpenDKTK)
		{
			Clear();
		}

#if defined(__ANDROID__) || defined(MU_IOS)
		SetAndroidRegisterOverlayVisible(false);
#endif
		return false;
	}

	OpenDKTK = true;
	CheckRegisterTimeout();

#if defined(__ANDROID__) || defined(MU_IOS)
	// Mobile gets its own overlay instead of the CUITextInputBox form below.
	// Those controls are fake HWND edit controls under touch, with a caret
	// drawn from font metrics and hit rects padded for a mouse - none of which
	// behaved, and the caret never appeared at all. The overlay keeps its own
	// text buffers and draws its own caret; everything around it (this window's
	// show flag, RecvKQRegInGame, and SubmitRegistration) is shared unchanged.
	SetAndroidRegisterOverlayVisible(true);
	RenderAndroidRegisterOverlay();
	return true;
#else

	const char* labels[TYPE_INPUT_DKTK::eMaxINPUT] =
	{
		"ACCOUNT :",
		"PASSWORD :",
		"7 DIGIT PIN :",
		"PHONE NUMBER :"
	};

	// ENTERASTAB on the first two rows so the soft keyboard's Enter walks
	// Account -> Pass -> 7 digit number. Without it the only way to reach a
	// field was to tap it, and the number row is the lowest of the three - on a
	// phone the keyboard is usually sitting on top of it, so players could not
	// reach it at all. The tab targets below were already wired up; only the
	// option that makes VK_RETURN follow them was missing (see UIControls.cpp,
	// where that branch is compiled in for Android and iOS).
	//
	// Deliberately not set on the number row: its Enter already submits the
	// form further down, and its tab target is the hidden Phone box.
	const int inputOptions[TYPE_INPUT_DKTK::eMaxINPUT] =
	{
		UIOPTION_NOLOCALIZEDCHARACTERS | UIOPTION_ENTERASTAB,
		UIOPTION_NOLOCALIZEDCHARACTERS | UIOPTION_ENTERASTAB,
		UIOPTION_NUMBERONLY,
		UIOPTION_NUMBERONLY
	};

	const int maxInput[TYPE_INPUT_DKTK::eMaxINPUT] =
	{
		MAX_ID_SIZE,
		MAX_PASSWORD_SIZE,
		7,
		11
	};

	// The last row actually shown: the PIN row only exists with RegisterPinCode on.
	const int lastRow = PinCodeEnabled() ? Snonumber : Pass;
	const float windowHeight = PinCodeEnabled() ? kRegisterWindowHeight : (kRegisterWindowHeight - kInputSpacing);

	float startX = (MAX_WIN_WIDTH / 2) - (kRegisterWindowWidth / 2);
	float startY = 40.0f;
	float inputPosX = 0.0f;
	float inputPosY[TYPE_INPUT_DKTK::eMaxINPUT] = { 0.0f };
	gInterface.Data[eWindow_DangKyInGame].AllowMove = false;

	g_pBCustomMenuInfo->gDrawWindowCustom(
		&startX,
		&startY,
		kRegisterWindowWidth,
		windowHeight,
		eWindow_DangKyInGame,
		"Register Account");

	if (!gInterface.Data[eWindow_DangKyInGame].OnShow)
	{
		Clear();
		return false;
	}

	inputPosX = startX + 120.0f;

	// Captcha removed from in-game registration: submit straight away.
	auto submitRegister = [&]()
	{
		RequsetDKTK();
	};

	RenderRegisterText(g_hFontBold, startX + 20.0f, startY + 48.0f, kRegisterWindowWidth - 40.0f, 14.0f, 3, "Account may only use the");
	RenderRegisterText(g_hFontBold, startX + 20.0f, startY + 62.0f, kRegisterWindowWidth - 40.0f, 14.0f, 3, "characters 0-9, a-z");

	startY += 30.0f;

	for (int i = Account; i <= lastRow; ++i)
	{
		RenderRegisterText(g_hFontBold, startX + 18.0f, startY + 48.0f, 96.0f, 16.0f, 1, labels[i]);
		inputPosY[i] = startY + 50.0f;
		gInterface.DrawBarForm(inputPosX - 3.0f, inputPosY[i] - 3.0f, kInputWidth, 16.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		RenderRegisterInput(
			CInputData[i],
			inputPosX,
			inputPosY[i],
			kInputWidth,
			14.0f,
			(UIOPTIONS)inputOptions[i],
			maxInput[i],
			(i == Pass));

		startY += kInputSpacing;
	}

	// The captcha row used to sit here (label, generated code and its input),
	// preceded by two 10px spacers. Both the row and the spacers are gone, and
	// kRegisterWindowHeight was reduced to match so the window does not keep a
	// blank gap above the submit button.
	if (CInputData[Account] != NULL && CInputData[Pass] != NULL)
	{
		CInputData[Account]->SetTabTarget(CInputData[Pass]);
	}

	if (lastRow == Snonumber && CInputData[Pass] != NULL && CInputData[Snonumber] != NULL)
	{
		CInputData[Pass]->SetTabTarget(CInputData[Snonumber]);
	}

	if (CInputData[Snonumber] != NULL && CInputData[Phone] != NULL)
	{
		CInputData[Snonumber]->SetTabTarget(CInputData[Phone]);
	}

	for (int i = Account; i <= lastRow; ++i)
	{
		// 24 tall against 26 spacing - a 2px gap, so this path cannot claim a tap
		// meant for the neighbouring row either.
		FocusRegisterInputOnTap(inputPosX - 3.0f, inputPosY[i] - 3.0f, kInputWidth + 6.0f, 24.0f, CInputData[i]);
	}

	startY += 30.0f;

	if (g_pBCustomMenuInfo->DrawButton(
		startX + 100.0f,
		startY + 45.0f,
		100.0f,
		12,
		const_cast<char*>(ResolveRegisterLabel(gOther.TextVN_NAPGAME[14], "Dang Ky")),
		80.0f,
		true))
	{
		submitRegister();
	}
#if defined(__ANDROID__) || defined(MU_IOS)
	// Enter used to submit from the captcha box, which was the last field.
	// With the captcha gone, submit from the last remaining input instead so
	// the on-screen keyboard's done key still finishes registration.
	else if (CInputData[lastRow] != NULL && CInputData[lastRow]->HaveFocus() && SEASON3B::IsPress(VK_RETURN))
	{
		submitRegister();
	}
#endif

	gInterface.DrawMessageBox();
	return true;
#endif   // PC form; mobile returned above via the register overlay
}

bool CB_DangKyInGame::RequsetDKTK()
{
	if (CInputData[Account] == NULL
		|| CInputData[Pass] == NULL
		|| (PinCodeEnabled() && CInputData[Snonumber] == NULL))
	{
		return false;
	}

	char szID[MAX_ID_SIZE + 1] = { 0 };
	char szPass[MAX_PASSWORD_SIZE + 1] = { 0 };
	char szSno[7 + 1] = { 0 };

	CInputData[Account]->GetText(szID, MAX_ID_SIZE + 1);
	CInputData[Pass]->GetText(szPass, MAX_PASSWORD_SIZE + 1);
	if (CInputData[Snonumber] != NULL)
	{
		CInputData[Snonumber]->GetText(szSno, sizeof(szSno));
	}

	return SubmitRegistration(szID, szPass, szSno);
}

// Split out of RequsetDKTK so the account can be submitted from somewhere other
// than the three CUITextInputBox controls. The PC window still gathers from
// those and calls straight through; the Android registration overlay keeps its
// own text buffers (the PC edit-control path never worked properly under touch)
// and calls this with them, so both share one copy of the rate limit, the
// validation, the messages and the packet.
bool CB_DangKyInGame::PinCodeEnabled()
{
	return gProtect.m_MainInfo.RegisterPinCode != 0;
}

bool CB_DangKyInGame::SubmitRegistration(const char* accountText, const char* passText, const char* snoText)
{
	char szID[MAX_ID_SIZE + 1] = { 0 };
	char szPass[MAX_PASSWORD_SIZE + 1] = { 0 };
	char szSno[7 + 1] = { 0 };
	char szSDT[11 + 1] = { 0 };

	if (accountText != NULL)
	{
		std::memcpy(szID, accountText, min(sizeof(szID) - 1, std::strlen(accountText)));
	}
	if (passText != NULL)
	{
		std::memcpy(szPass, passText, min(sizeof(szPass) - 1, std::strlen(passText)));
	}
	// With the PIN turned off the server still gets a well-formed 7 digits: it
	// stores them in MEMB_INFO.sno__numb, and PersonalCodeCheck = 0 means nothing
	// reads them back.
	const char* pinSource = PinCodeEnabled() ? snoText : kPlaceholderRegisterPin;
	if (pinSource != NULL)
	{
		std::memcpy(szSno, pinSource, min(sizeof(szSno) - 1, std::strlen(pinSource)));
	}
	std::memcpy(szSDT, kDefaultRegisterPhone, min(sizeof(szSDT) - 1, std::strlen(kDefaultRegisterPhone)));

	// The first request is still on its way; its reply (or the timeout) will
	// report. Answering the duplicate with an error box is what players saw.
	if (m_RegPending)
	{
		return false;
	}

	if (TimeSendRegTK > GetTickCount())
	{
		gInterface.OpenMessageBox("Error", "Please wait a moment before trying again.");
		return false;
	}

	if (strlen(szID) < 1)
	{
		gInterface.OpenMessageBox("Error", "Please enter an account name.");
		return false;
	}

	if (strlen(szPass) < 1)
	{
		gInterface.OpenMessageBox("Error", "Please enter a password.");
		return false;
	}

	if (strlen(szSno) < 7)
	{
		gInterface.OpenMessageBox("Error", "Please enter the 7-digit PIN.");
		return false;
	}

	if (!CheckChuoiKyTuDacBiet(szID) || !CheckChuoiKyTuDacBiet(szPass))
	{
		gInterface.OpenMessageBox("Error", "The account or password contains characters that are not allowed.");
		return false;
	}

	PMSG_REGISTER_MAIN_SEND pMsg;
	pMsg.header.set(0xD3, 0x05, sizeof(pMsg));
	pMsg.TypeSend = 0x01;

	memset(pMsg.account, 0, sizeof(pMsg.account));
	memset(pMsg.password, 0, sizeof(pMsg.password));
	memset(pMsg.numcode, 0, sizeof(pMsg.numcode));
	memset(pMsg.sodienthoai, 0, sizeof(pMsg.sodienthoai));

	memcpy(pMsg.account, szID, min(sizeof(pMsg.account) - 1, strlen(szID)));
	memcpy(pMsg.password, szPass, min(sizeof(pMsg.password) - 1, strlen(szPass)));
	memcpy(pMsg.numcode, szSno, min(sizeof(pMsg.numcode) - 1, strlen(szSno)));
	memcpy(pMsg.sodienthoai, szSDT, min(sizeof(pMsg.sodienthoai) - 1, strlen(szSDT)));

	DataSend((LPBYTE)&pMsg, pMsg.header.size);

	TimeSendRegTK = GetTickCount() + 3000;
	m_RegPending = true;
	m_RegSentTick = GetTickCount();

	return true;
}

void CB_DangKyInGame::RecvKQRegInGame(XULY_CGPACKET* lpMsg)
{
	if (lpMsg == NULL)
	{
		return;
	}

	m_RegPending = false;

	char szID[MAX_ID_SIZE + 1] = { 0 };
	char szPass[MAX_PASSWORD_SIZE + 1] = { 0 };

#if defined(__ANDROID__) || defined(MU_IOS)
	// Mobile registers through the overlay, which keeps its own text - the edit
	// controls below are never filled there, so reading them would confirm an
	// empty account and copy nothing into the login boxes.
	{
		const char* overlayAccount = AndroidRegisterOverlayAccountText();
		const char* overlayPassword = AndroidRegisterOverlayPasswordText();
		if (overlayAccount != NULL)
		{
			strncpy(szID, overlayAccount, sizeof(szID) - 1);
		}
		if (overlayPassword != NULL)
		{
			strncpy(szPass, overlayPassword, sizeof(szPass) - 1);
		}
	}
#else
	if (CInputData[Account] != NULL)
	{
		CInputData[Account]->GetText(szID, sizeof(szID));
	}

	if (CInputData[Pass] != NULL)
	{
		CInputData[Pass]->GetText(szPass, sizeof(szPass));
	}
#endif

	switch (lpMsg->ThaoTac)
	{
	case CB_DangKyInGame::eDangKyThanhCong:
		{
			gInterface.OpenMessageBox("Register Completed", "Registration completed.\nAccount : %s\n\nYou can now log in.", szID);
			CUIMng& rUIMng = CUIMng::Instance();
			rUIMng.m_LoginWin.GetIDInputBox()->SetText(szID);
			rUIMng.m_LoginWin.GetPassInputBox()->SetText(szPass);
			gInterface.Data[eWindow_DangKyInGame].Close();
			Clear();
		}
		break;

	case CB_DangKyInGame::eTaiKhoanDaTonTai:
		gInterface.OpenMessageBox("Register", "Account %s already exists.", szID);
		break;

	case CB_DangKyInGame::eDuLieuNhapKhongDung:
		gInterface.OpenMessageBox("Register", "The details entered are not valid.");
		break;

	default:
		// Silence here used to leave players tapping Register again.
		gInterface.OpenMessageBox("Register", "Registration failed (code %d).", (int)lpMsg->ThaoTac);
		break;
	}
}
