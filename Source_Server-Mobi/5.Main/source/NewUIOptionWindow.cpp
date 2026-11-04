// NewUIOptionWindow.cpp: implementation of the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIOptionWindow.h"
#include "NewUISystem.h"
#include "MultiLanguage.h"
#include "ZzzTexture.h"
#include "DSPlaySound.h"
#include "ZzzInterface.h"
#include "CBInterface.h"
#include "GameConfig/GameConfig.h"
#include "VoiceClient.h"
#include "VoiceAudio.h"
#include "GameConfig/GameConfigConstants.h"

extern void SetSelectedLanguage(const char* pszLanguage);	// Winmain.cpp, GameConfig.cpp on mobile
#if !defined(__ANDROID__) && !defined(MU_IOS)
extern bool AskRestartForLanguage(int iWantLang);	// Winmain.cpp
#elif defined(__ANDROID__) || defined(MU_IOS)
extern void MU_MobileAskRestartForLanguage(int iWantLang);	// Platform/MobilePlatform.cpp
#endif

using namespace SEASON3B;

extern void MoveWindows(int eNumWindow);	// NewUIBCustomMenu.cpp
extern bool g_RenderEff;	// ZzzOpenglUtil.cpp
#if !defined(__ANDROID__) && !defined(MU_IOS)
extern int m_FontSizePlus;	// Winmain.cpp - the text size bonus the running game started with
#endif

extern char Mp3FileName[256];
extern HWND g_hWnd;

#if !defined(__ANDROID__) && !defined(MU_IOS)
// Not #include <wzAudio.h> - that header defines (not just declares) a
// global named m_enMixerMode as part of its enum, so including it from a
// second .cpp (Winmain.cpp already does) is an ODR violation and fails to
// link (LNK2005). Forward-declaring just what's needed here avoids it.
#define WZAOPT_STOPBEFOREPLAY 0
extern "C" int wzAudioCreate(HWND hParentWnd);
extern "C" void wzAudioOption(int nOption, int nVal);
#endif

#if defined(__ANDROID__) || defined(MU_IOS)
// mu_settings.cfg keys for OnOffGrap[], in ONOFFGRAP_OPTION order.
static const char* const kMobileGraphicsKeys[SEASON3B::CNewUIOptionWindow::eEndOnOffGrap] =
{
	"Graphics.GlowEffect",
	"Graphics.EffectDynamic",
	"Graphics.EffectStatic",
	"Graphics.BMDPlayer",
	"Graphics.BMDWings",
	"Graphics.BMDWeapons",
	"Graphics.BMDImg",
	"Graphics.BMDMonter",
	"Graphics.RenderObjects",
	"Graphics.RenderTerrain",
	"Graphics.ExcellentEffect",
	"Graphics.BMDZen",
};

// Text size, android_main.cpp. Declared here at file scope: inside the member
// functions below they would bind to SEASON3B:: names that do not exist.
int GetAndroidTextSizePercent();
void CycleAndroidTextSize();
void SetAndroidTextSizePercent(int percent);
int GetAndroidTextSizeOptions(int* out, int maxCount);
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SEASON3B::CNewUIOptionWindow::CNewUIOptionWindow()
{
	m_pNewUIMng = NULL;
	m_nHits = 0;
	m_iOpenDrop = 0;
	m_nDropOpts = 0;
	m_dropCur = 0;
	m_iOpacity = 100;
	m_iFontWanted = 0;
	m_dwOpenTick = 0;
	m_iLangWanted = -1;
	m_Pos.x = 0;
	m_Pos.y = 0;

	m_bAutoAttack = true;
	m_bWhisperSound = false;
	m_bSlideHelp = true;
	m_iVolumeLevel = 0;
#if defined(__ANDROID__) || defined(MU_IOS)
	// Mobile default is lower than PC's max (4): ZzzObject.cpp's Level clamp
	// (GetRenderLevel()<4 => Level=min(Level,GetRenderLevel()*2+5)) means this
	// caps every +11-15 item's glow at the +9/10 tier's 2-overlay treatment
	// instead of the full 3-overlay one, on a CPU-bound client where each
	// overlay is a full extra mesh pass. Items keep glowing - this doesn't
	// disable glow entirely - it just trims the most expensive tier.
	m_iRenderLevel = 2;
#else
	m_iRenderLevel = 4;
#endif
	for (int i = 0; i < eEndOnOffGrap; i++) this->OnOffGrap[i] = true;
	m_iOpacity = GetPrivateProfileInt("Option", "SettingsOpacity", 100, ".\\config.ini");
	m_iFontWanted = GetPrivateProfileInt("FONT", "FontSizeBonus", 0, ".\\config.ini");

	//==Graphics
	this->OnOffGrap[eGlowEffect] = GetPrivateProfileInt("Graphics", "GlowEffect", 1, ".\\config.ini");
	this->OnOffGrap[eEffectDynamic] = GetPrivateProfileInt("Graphics", "EffectDynamic", 1, ".\\config.ini");
	this->OnOffGrap[eEffectStatic] = GetPrivateProfileInt("Graphics", "EffectStatic", 1, ".\\config.ini");
	this->OnOffGrap[eBMDPlayer] = GetPrivateProfileInt("Graphics", "BMDPlayer",1, ".\\config.ini");
	this->OnOffGrap[eBMDWings] = GetPrivateProfileInt("Graphics", "BMDWings", 1, ".\\config.ini");
	this->OnOffGrap[eBMDWeapons] = GetPrivateProfileInt("Graphics", "BMDWeapons", 1, ".\\config.ini");
	this->OnOffGrap[eBMDImg] = GetPrivateProfileInt("Graphics", "BMDImg", 1 ,".\\config.ini");
	this->OnOffGrap[eBMDMonter] = GetPrivateProfileInt("Graphics", "BMDMonter", 1, ".\\config.ini");
	this->OnOffGrap[eRenderObjects] = GetPrivateProfileInt("Graphics", "RenderObjects", 1, ".\\config.ini");
	this->OnOffGrap[eRenderTerrain] = GetPrivateProfileInt("Graphics", "RenderTerrain", 1, ".\\config.ini");
	this->OnOffGrap[eExcellentEffect] = GetPrivateProfileInt("Graphics", "ExcellentEffect", 1, ".\\config.ini");
	this->OnOffGrap[eBMDZen] = GetPrivateProfileInt("Graphics", "BMDZen", 1, ".\\config.ini");

#if defined(__ANDROID__) || defined(MU_IOS)
	// GetPrivateProfileInt is a stub on mobile that always returns the default,
	// so every choice made in this window was lost on restart. The values come
	// from mu_settings.cfg instead; MobileSyncOptionSettings writes them back.
	m_bAutoAttack = MobileSettingsGetInt("Option.AutoAttack", m_bAutoAttack ? 1 : 0) != 0;
	m_bWhisperSound = MobileSettingsGetInt("Option.WhisperSound", m_bWhisperSound ? 1 : 0) != 0;
	m_bSlideHelp = MobileSettingsGetInt("Option.SlideHelp", m_bSlideHelp ? 1 : 0) != 0;
	m_iRenderLevel = MobileSettingsGetInt("Option.RenderLevel", m_iRenderLevel);
	m_iOpacity = MobileSettingsGetInt("Option.SettingsOpacity", 100);
	for (int i = 0; i < eEndOnOffGrap; i++)
	{
		this->OnOffGrap[i] = MobileSettingsGetInt(kMobileGraphicsKeys[i], this->OnOffGrap[i] ? 1 : 0) != 0;
	}
	// Globals owned by android_link_stubs.cpp; the Options window is the only
	// place a player sets them (plus the minimap's name button for ShowName).
	mShowHPBar = MobileSettingsGetInt("Custom.ShowHPBar", mShowHPBar);
	mShowName = MobileSettingsGetInt("Custom.ShowName", mShowName);
	mShowMiniMap = MobileSettingsGetInt("Custom.ShowMiniMap", mShowMiniMap);
	mShowDanhHieu = MobileSettingsGetInt("Custom.ShowTitle", mShowDanhHieu);
	mHideMarketNotice = MobileSettingsGetInt("Custom.HideMarketNotice", mHideMarketNotice);
	g_bGMObservation = (mShowName != 0);
#endif
}

#if defined(__ANDROID__) || defined(MU_IOS)
// Writes this window's settings to mu_settings.cfg whenever one differs from
// what is stored. Called every frame (android_main.cpp) rather than from each
// checkbox, so no toggle - including the minimap's ShowName button and the
// render-level bar - can be missed. ~25 map lookups; the file is only written
// on an actual change.
void MobileSyncOptionSettings()
{
	SEASON3B::CNewUIOptionWindow* w = g_pOption;
	if (w == NULL)
	{
		return;
	}

	bool dirty = false;
	auto put = [&dirty](const char* key, int value)
	{
		if (MobileSettingsGetInt(key, value - 1) != value)
		{
			MobileSettingsSetInt(key, value, false);
			dirty = true;
		}
	};

	put("Option.AutoAttack", w->IsAutoAttack() ? 1 : 0);
	put("Option.WhisperSound", w->IsWhisperSound() ? 1 : 0);
	put("Option.SlideHelp", w->IsSlideHelp() ? 1 : 0);
	put("Option.RenderLevel", w->GetRenderLevel());
	put("Option.SettingsOpacity", w->GetOpacityPercent());
	for (int i = 0; i < SEASON3B::CNewUIOptionWindow::eEndOnOffGrap; i++)
	{
		put(kMobileGraphicsKeys[i], w->OnOffGrap[i] ? 1 : 0);
	}
	put("Custom.ShowHPBar", mShowHPBar);
	put("Custom.ShowName", mShowName);
	put("Custom.ShowMiniMap", mShowMiniMap);
	put("Custom.ShowTitle", mShowDanhHieu);
	put("Custom.HideMarketNotice", mHideMarketNotice);

	// Built with the main scene, later than this window.
	if (CNewUINameWindow* pNameWindow = g_pNewUISystem->GetUI_NewNameWindow())
	{
		put("Custom.ShowItemNames", pNameWindow->IsShowItemName() ? 1 : 0);
	}

	if (dirty)
	{
		MobileSettingsSave();
	}
}
#endif

SEASON3B::CNewUIOptionWindow::~CNewUIOptionWindow()
{
	Release();
}

bool SEASON3B::CNewUIOptionWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if( NULL == pNewUIMng )
		return false;
	
	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_OPTION, this);
	SetPos(x, y);
	LoadImages();
	Show(false);
	return true;
}

void SEASON3B::CNewUIOptionWindow::Release()
{
	UnloadImages();
	
	if(m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj( this );
		m_pNewUIMng = NULL;
	}
}


void SEASON3B::CNewUIOptionWindow::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

bool SEASON3B::CNewUIOptionWindow::UpdateMouseEvent()
{
	// Clicks are resolved against the controls RenderModern() recorded on the last frame.
	if (SEASON3B::IsPress(VK_LBUTTON))
	{
		const bool bDropWasOpen = (m_iOpenDrop != 0);
		HandleModernClick();

		// A click outside the window closes it (first a drop-down that is open). The click is used up, so it does
		// not also walk the character. Ignored for a moment after opening, because the click that opened the
		// window arrives before it has a position.
		const bool bInside = (CheckMouseIn((int)gInterface.Data[eMenu_OPTION].X, (int)gInterface.Data[eMenu_OPTION].Y, (int)gInterface.Data[eMenu_OPTION].Width, (int)gInterface.Data[eMenu_OPTION].Height + 46) == true);
		if (!bInside && GetTickCount() - m_dwOpenTick > 400)
		{
			if (!bDropWasOpen)
			{
				gInterface.Data[eMenu_OPTION].Close();
			}
			return false;
		}
	}

	// The whole window blocks the world behind it, so a tap on it never walks the character.
	if (CheckMouseIn((int)gInterface.Data[eMenu_OPTION].X, (int)gInterface.Data[eMenu_OPTION].Y, (int)gInterface.Data[eMenu_OPTION].Width, (int)gInterface.Data[eMenu_OPTION].Height + 46) == true)
	{
		return false;
	}

	return true;
}

bool SEASON3B::CNewUIOptionWindow::UpdateKeyEvent()
{
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION) == true)
	{
		if(SEASON3B::IsPress(VK_ESCAPE) == true)
		{
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
			PlayBuffer(SOUND_CLICK01);
			return false;
		}
	}

	return true;
}

bool SEASON3B::CNewUIOptionWindow::Update()
{
	return true;
}

float SEASON3B::CNewUIOptionWindow::GetLayerDepth()	//. 10.5f
{
	return 10.5f;
}

float SEASON3B::CNewUIOptionWindow::GetKeyEventOrder()	// 10.f;
{
	return 10.0f;
}

void SEASON3B::CNewUIOptionWindow::OpenningProcess()
{
	m_dwOpenTick = GetTickCount();
	gInterface.Data[eMenu_OPTION].Open();
}

void SEASON3B::CNewUIOptionWindow::ClosingProcess()
{
	gInterface.Data[eMenu_OPTION].Close();
}

void SEASON3B::CNewUIOptionWindow::LoadImages()
{
	LoadBitmap("Interface\\newui_button_close.tga", IMAGE_OPTION_BTN_CLOSE, GL_LINEAR);
	LoadBitmap("Interface\\newui_msgbox_back.jpg", IMAGE_OPTION_FRAME_BACK, GL_LINEAR);
	LoadBitmap("Interface\\newui_item_back03.tga", IMAGE_OPTION_FRAME_DOWN, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_top.tga", IMAGE_OPTION_FRAME_UP, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_back06(L).tga", IMAGE_OPTION_FRAME_LEFT, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_back06(R).tga", IMAGE_OPTION_FRAME_RIGHT, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_line.jpg", IMAGE_OPTION_LINE, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_point.tga", IMAGE_OPTION_POINT, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_check.tga", IMAGE_OPTION_BTN_CHECK, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_effect03.tga", IMAGE_OPTION_EFFECT_BACK, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_effect04.tga", IMAGE_OPTION_EFFECT_COLOR, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_volume01.tga", IMAGE_OPTION_VOLUME_BACK, GL_LINEAR);	
	LoadBitmap("Interface\\newui_option_volume02.tga", IMAGE_OPTION_VOLUME_COLOR, GL_LINEAR);	
}

void SEASON3B::CNewUIOptionWindow::UnloadImages()
{
	DeleteBitmap(IMAGE_OPTION_BTN_CLOSE);
	DeleteBitmap(IMAGE_OPTION_FRAME_BACK);
	DeleteBitmap(IMAGE_OPTION_FRAME_DOWN);
	DeleteBitmap(IMAGE_OPTION_FRAME_UP);
	DeleteBitmap(IMAGE_OPTION_FRAME_LEFT);
	DeleteBitmap(IMAGE_OPTION_FRAME_RIGHT);
	DeleteBitmap(IMAGE_OPTION_LINE);
	DeleteBitmap(IMAGE_OPTION_POINT);
	DeleteBitmap(IMAGE_OPTION_BTN_CHECK);
	DeleteBitmap(IMAGE_OPTION_EFFECT_BACK);
	DeleteBitmap(IMAGE_OPTION_EFFECT_COLOR);
	DeleteBitmap(IMAGE_OPTION_VOLUME_BACK);
	DeleteBitmap(IMAGE_OPTION_VOLUME_COLOR);
}

//////////////////////////////////////////////////////////////////////
// Modern settings window.
//
// Flat dark panel, cards with a title each, toggle switches instead of tick boxes and drop-downs for every
// setting that has more than two values. Everything is laid out by RenderModern() in the 640x480 UI space;
// every clickable area it draws is also recorded in m_Hits, and UpdateMouseEvent() resolves a click against
// that list (the old window polled the mouse button inside its draw calls and needed a 500 ms debounce per
// control).
//////////////////////////////////////////////////////////////////////

namespace
{
	// Colours are 0xAABBGGRR, like every other TextDraw colour in this client.
	const DWORD kTextMain  = 0xFFF4EEE8u;	// soft white
	const DWORD kTextMuted = 0xFFA59A90u;	// grey-blue
	const DWORD kTextAccent = 0xFFFFB04Eu;	// light blue

	const float kRowH = 22.f;
	const float kHeadH = 26.f;
	const float kPad = 6.f;
	const float kGap = 10.f;
	const float kColW = 174.f;
	const float kWinW = 576.f;
	const float kTitleH = 40.f;

	// 0.3 .. 1.0, applied to the window's backgrounds (panel, title bar, cards) but not to text or controls.
	float g_fPanelAlpha = 1.0f;

	// A rounded-looking rectangle: three overlapping bars leave the four corners notched.
	void RRect(float x, float y, float w, float h, float r, float g, float b, float a)
	{
		gInterface.DrawBarForm(x + 2.f, y, w - 4.f, h, r, g, b, a);
		gInterface.DrawBarForm(x, y + 2.f, w, h - 4.f, r, g, b, a);
		gInterface.DrawBarForm(x + 1.f, y + 1.f, w - 2.f, h - 2.f, r, g, b, a);
	}

	// Same, but the alpha follows the window's transparency setting. The three bars overlap, so the middle
	// one is drawn first at full strength only where it would not double up: with a translucent panel the
	// overlap would show as lighter seams, so translucent backgrounds are drawn as one flat bar plus the
	// two edge strips that do not overlap it.
	void RRectBg(float x, float y, float w, float h, float r, float g, float b, float a)
	{
		const float al = a * g_fPanelAlpha;
		if (al >= 0.995f)
		{
			RRect(x, y, w, h, r, g, b, al);
			return;
		}
		gInterface.DrawBarForm(x + 2.f, y, w - 4.f, 1.f, r, g, b, al);					// top edge
		gInterface.DrawBarForm(x + 2.f, y + h - 1.f, w - 4.f, 1.f, r, g, b, al);		// bottom edge
		gInterface.DrawBarForm(x, y + 2.f, w, h - 4.f, r, g, b, al);					// body, full width
		gInterface.DrawBarForm(x + 1.f, y + 1.f, 1.f, 1.f, r, g, b, al);
		gInterface.DrawBarForm(x + w - 2.f, y + 1.f, 1.f, 1.f, r, g, b, al);
		gInterface.DrawBarForm(x + 1.f, y + h - 2.f, 1.f, 1.f, r, g, b, al);
		gInterface.DrawBarForm(x + w - 2.f, y + h - 2.f, 1.f, 1.f, r, g, b, al);
		gInterface.DrawBarForm(x + 2.f, y + 1.f, w - 4.f, 1.f, r, g, b, al);			// second row, between the corner pixels
		gInterface.DrawBarForm(x + 2.f, y + h - 2.f, w - 4.f, 1.f, r, g, b, al);
	}

	float CardH(int rows) { return kHeadH + rows * kRowH + kPad; }

	// Drop-down ids.
	enum { DD_NONE = 0, DD_LANG, DD_MIC, DD_BOOST, DD_TALK, DD_CHANNEL, DD_TEXTSIZE, DD_OPACITY };
}

void SEASON3B::CNewUIOptionWindow::AddHit(float x, float y, float w, float h, int id, int arg)
{
	if (m_nHits < MODERN_MAX_HITS)
	{
		m_Hits[m_nHits].x = x;
		m_Hits[m_nHits].y = y;
		m_Hits[m_nHits].w = w;
		m_Hits[m_nHits].h = h;
		m_Hits[m_nHits].id = id;
		m_Hits[m_nHits].arg = arg;
		++m_nHits;
	}
}

// Draws a card (background + title) and returns the y of its first row.
static float DrawCard(float x, float y, float w, float h, const char* title, DWORD titleColor)
{
	RRectBg(x, y, w, h, 0.095f, 0.11f, 0.15f, 1.0f);
	gInterface.DrawBarForm(x + 10.f, y + kHeadH - 3.f, w - 20.f, 1.f, 0.22f, 0.25f, 0.32f, 1.0f * g_fPanelAlpha);
	TextDraw(g_hFontBold, (int)(x + 10.f), (int)(y + 6.f), titleColor, 0x0, 0, 0, 1, "%s", title);
	return y + kHeadH;
}

// One row: label on the left, switch on the right.
void SEASON3B::CNewUIOptionWindow::DrawToggleRow(float x, float y, float w, const char* label, bool on, int id, int arg)
{
	const bool hover = (SEASON3B::CheckMouseIn(x, y, w, kRowH) == 1);
	if (hover)
	{
		RRect(x + 3.f, y, w - 6.f, kRowH - 2.f, 0.16f, 0.18f, 0.24f, 1.0f);
	}
	TextDraw(g_hFont, (int)(x + 12.f), (int)(y + 5.f), on ? kTextMain : kTextMuted, 0x0, 0, 0, 1, "%s", label);

	// Switch: 30x14 track, 10x10 knob.
	const float sx = x + w - 12.f - 30.f;
	const float sy = y + (kRowH - 2.f - 14.f) * 0.5f;
	if (on)
		RRect(sx, sy, 30.f, 14.f, 0.24f, 0.55f, 1.0f, 1.0f);
	else
		RRect(sx, sy, 30.f, 14.f, 0.26f, 0.28f, 0.34f, 1.0f);
	RRect(on ? sx + 18.f : sx + 2.f, sy + 2.f, 10.f, 10.f, 0.97f, 0.97f, 0.99f, 1.0f);

	AddHit(x, y, w, kRowH, id, arg);
}

// A drop-down. With a label it is a row (label left, value right); without one it is a field that fills the row.
// Clicking opens the list of options, which RenderModern() draws on top of everything at the end.
void SEASON3B::CNewUIOptionWindow::DrawDropRow(float x, float y, float w, const char* label, const char* value, int dd)
{
	const bool open = (m_iOpenDrop == dd);
	const bool hover = (SEASON3B::CheckMouseIn(x, y, w, kRowH) == 1);
	if (label != NULL)
	{
		if (hover || open)
		{
			RRect(x + 3.f, y, w - 6.f, kRowH - 2.f, 0.16f, 0.18f, 0.24f, 1.0f);
		}
		TextDraw(g_hFont, (int)(x + 12.f), (int)(y + 5.f), kTextMuted, 0x0, 0, 0, 1, "%s", label);
		TextDraw(g_hFont, (int)(x + w * 0.42f), (int)(y + 5.f), kTextAccent, 0x0, 0, 0, 1, "%s", value);
		TextDraw(g_hFont, (int)(x + w - 16.f), (int)(y + 5.f), kTextMuted, 0x0, 0, 0, 1, "%s", open ? "^" : "v");
	}
	else
	{
		const bool on = hover || open;
		RRect(x + 10.f, y + 1.f, w - 20.f, kRowH - 2.f, on ? 0.2f : 0.15f, on ? 0.23f : 0.17f, on ? 0.31f : 0.23f, 1.0f);
		TextDraw(g_hFont, (int)(x + 18.f), (int)(y + 5.f), kTextMain, 0x0, 0, 0, 1, "%s", value);
		TextDraw(g_hFont, (int)(x + w - 26.f), (int)(y + 5.f), kTextMuted, 0x0, 0, 0, 1, "%s", open ? "^" : "v");
	}
	if (open)
	{
		m_dropX = (label != NULL) ? x + 4.f : x + 10.f;
		m_dropY = y;
		m_dropW = (label != NULL) ? w - 8.f : w - 20.f;
	}
	AddHit(x, y, w, kRowH, MH_DROP, dd);
}

// Fills the option list of one drop-down: label, value and which one is current.
void SEASON3B::CNewUIOptionWindow::BuildDropOptions(int dd)
{
	m_nDropOpts = 0;
	m_dropCur = 0;
	auto add = [this](const char* label, int value, bool current)
	{
		if (m_nDropOpts < MODERN_MAX_DROP)
		{
			strncpy(m_dropLabels[m_nDropOpts], label, sizeof(m_dropLabels[0]) - 1);
			m_dropLabels[m_nDropOpts][sizeof(m_dropLabels[0]) - 1] = '\0';
			if (strlen(m_dropLabels[m_nDropOpts]) > 21)
			{
				m_dropLabels[m_nDropOpts][20] = '.';
				m_dropLabels[m_nDropOpts][21] = '\0';
			}
			m_dropValues[m_nDropOpts] = value;
			if (current) m_dropCur = m_nDropOpts;
			++m_nDropOpts;
		}
	};

	switch (dd)
	{
	case DD_LANG:
	{
		static const char* const kNames[LANG_INDEX_COUNT] = { "English", "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t", "Espa\xC3\xB1ol" };
		for (int i = 0; i < LANG_INDEX_COUNT; ++i) add(kNames[i], i, i == GetWantedLanguage());
		break;
	}
	case DD_MIC:
	{
		add("System default", -1, VoiceSettingGetMicDevice() < 0);
		const int count = VoiceAudioGetCaptureDeviceCount();
		for (int i = 0; i < count; ++i)
		{
			char name[64];
			if (!VoiceAudioGetCaptureDeviceName(i, name, sizeof(name))) strcpy(name, "?");
			add(name, i, VoiceSettingGetMicDevice() == i);
		}
		break;
	}
	case DD_BOOST:
	{
		// The stops of the boost cycle, listed from the current one round to where it comes back.
		int vals[12];
		int n = 0;
		int v = VoiceSettingGetMicBoost();
		do
		{
			vals[n++] = v;
			v = VoiceSettingNextMicBoost(v);
		} while (v != vals[0] && n < 12);
		for (int a = 0; a < n; ++a)		// ascending
			for (int b = a + 1; b < n; ++b)
				if (vals[b] < vals[a]) { const int t = vals[a]; vals[a] = vals[b]; vals[b] = t; }
		for (int i = 0; i < n; ++i)
		{
			char s[16];
			sprintf(s, "%d%%", vals[i]);
			add(s, vals[i], vals[i] == VoiceSettingGetMicBoost());
		}
		break;
	}
#if !defined(__ANDROID__) && !defined(MU_IOS)
	case DD_TALK:
	{
		const bool bToggle = (VoiceSettingGetTalkMode() == CfgDefaults::CfgVoiceTalkModeToggle);
		add("toggle on/off", CfgDefaults::CfgVoiceTalkModeToggle, bToggle);
		add("hold to talk", CfgDefaults::CfgVoiceTalkModeHold, !bToggle);
		break;
	}
#endif
	case DD_CHANNEL:
	{
		const bool bParty = (gVoiceClient.GetChannel() == VOICE_CHANNEL_PARTY);
		add("Nearby (proximity)", 0, !bParty);
		add("Party (anywhere)", 1, bParty);
		break;
	}
#if defined(__ANDROID__) || defined(MU_IOS)
	case DD_TEXTSIZE:
	{
		int sizes[16];
		const int n = GetAndroidTextSizeOptions(sizes, 16);
		for (int i = 0; i < n; ++i)
		{
			char s[16];
			sprintf(s, "%d%%", sizes[i]);
			add(s, sizes[i], sizes[i] == GetAndroidTextSizePercent());
		}
		break;
	}
#endif
#if !defined(__ANDROID__) && !defined(MU_IOS)
	case DD_TEXTSIZE:
	{
		// The text size bonus in pixels (config.ini [FONT] FontSizeBonus), read when the game starts.
		static const char* const kNames[5] = { "Small", "Normal", "Large", "Larger", "Largest" };
		for (int i = 0; i < 5; ++i) add(kNames[i], i - 1, (i - 1) == m_iFontWanted);
		break;
	}
#endif
	case DD_OPACITY:
	{
		for (int p = 100; p >= 30; p -= 10)
		{
			char s[16];
			sprintf(s, "%d%%", p);
			add(s, p, p == m_iOpacity);
		}
		break;
	}
	}
}

void SEASON3B::CNewUIOptionWindow::ApplyDropOption(int dd, int idx)
{
	if (idx < 0 || idx >= m_nDropOpts)
	{
		return;
	}
	const int v = m_dropValues[idx];
	switch (dd)
	{
	case DD_LANG:		ChooseLanguage(v); break;
	case DD_MIC:
		VoiceSettingSetMicDevice(v);
		// Closed now rather than left to expire, so a player who changes device mid-sentence does not keep
		// recording from the old one.
		gVoiceAudio.RequestCaptureReopen();
		break;
	case DD_BOOST:		VoiceSettingSetMicBoost(v); break;
#if !defined(__ANDROID__) && !defined(MU_IOS)
	case DD_TALK:		VoiceSettingSetTalkMode(v); break;
#endif
	case DD_CHANNEL:
		if ((gVoiceClient.GetChannel() == VOICE_CHANNEL_PARTY) != (v == 1))
		{
			gVoiceClient.ToggleChannel();
		}
		break;
#if defined(__ANDROID__) || defined(MU_IOS)
	case DD_TEXTSIZE:	SetAndroidTextSizePercent(v); break;
#endif
#if !defined(__ANDROID__) && !defined(MU_IOS)
	case DD_TEXTSIZE:
	{
		m_iFontWanted = v;
		char s[16];
		sprintf(s, "%d", v);
		WritePrivateProfileStringA("FONT", "FontSizeBonus", s, "./config.ini");
		break;
	}
#endif
	case DD_OPACITY:
		m_iOpacity = v;
#if !defined(__ANDROID__) && !defined(MU_IOS)
		{
			char s[16];
			sprintf(s, "%d", v);
			WritePrivateProfileStringA("Option", "SettingsOpacity", s, "./config.ini");
		}
#endif
		break;
	}
}

void SEASON3B::CNewUIOptionWindow::ToggleMusic()
{
	m_MusicOnOff ^= 1;

	// SaveConfigDword (Winmain.h/.cpp) only exists on PC - Winmain.cpp is excluded from the Android build
	// entirely, so calling it unconditionally is an Android link failure. Android already has its own
	// persisted audio settings (GameConfig, read at startup - android_main.cpp).
#if !defined(__ANDROID__) && !defined(MU_IOS)
	SaveConfigDword("MusicOnOff", m_MusicOnOff);
#else
	GameConfig::GetInstance().SetMusicEnabled(m_MusicOnOff != 0);
	GameConfig::GetInstance().Save();
#endif

	if (m_MusicOnOff == 0)
	{
		StopMp3(Mp3FileName, TRUE);
	}
#if !defined(__ANDROID__) && !defined(MU_IOS)
	else if (g_bWzAudioCreated == false)
	{
		// wzAudioCreate is only ever called at startup, gated on MusicOnOff already being true then, which it
		// never is (nothing writes the registry value that startup read checks). Calling wzAudioPlay into an
		// instance that was never set up crashed inside the wzAudio DLL, so it is created here, once, the
		// first time music is turned on. Android does not use wzAudio.
		if (wzAudioCreate(g_hWnd) == 0)
		{
			g_bWzAudioCreated = true;
			wzAudioOption(WZAOPT_STOPBEFOREPLAY, 1);
		}
	}
#endif
}

void SEASON3B::CNewUIOptionWindow::ToggleSound()
{
	m_SoundOnOff ^= 1;

#if !defined(__ANDROID__) && !defined(MU_IOS)
	SaveConfigDword("SoundOnOff", m_SoundOnOff);
#else
	GameConfig::GetInstance().SetSoundEnabled(m_SoundOnOff != 0);
	GameConfig::GetInstance().Save();
#endif

	// Order matters: SetMasterVolume() (DSplaysound.cpp, reached via SetEffectVolumeLevel) early-returns while
	// g_EnableSound is false, so the mute has to be lifted before the volume is applied.
#if !defined(__ANDROID__) && !defined(MU_IOS)
	// InitDirectSound now runs at startup, so this is only a fallback for a device that failed to initialise then.
	if (m_SoundOnOff != 0 && g_bDirectSoundCreated == false)
	{
		if (SUCCEEDED(InitDirectSound(g_hWnd)))
		{
			g_bDirectSoundCreated = true;
		}
	}

	SetEnableSound(g_bDirectSoundCreated && m_SoundOnOff != 0);
#else
	// m_SoundOnOff alone is only consulted at startup; the per-effect gate is g_androidSoundEnabled, which
	// SetEnableSound() updates.
	SetEnableSound(m_SoundOnOff != 0);
#endif

	// A stale VolumeLevel=0 silences every effect while music keeps playing (wzAudio has its own volume), so
	// turning sound on restores a real level instead of leaving whatever silenced it.
	if (m_SoundOnOff != 0)
	{
		if (m_iVolumeLevel < 1)
		{
			m_iVolumeLevel = SOUND_VOLUME_FULL;

#if !defined(__ANDROID__) && !defined(MU_IOS)
			SaveConfigDword("VolumeLevel", m_iVolumeLevel);
#else
			GameConfig::GetInstance().SetVolumeLevel(m_iVolumeLevel);
			GameConfig::GetInstance().Save();
#endif
		}

		SetEffectVolumeLevel(m_iVolumeLevel);
	}
}

// The two anti-lag presets, the same sets of switches the F6 (Antilag I) and F7 (Antilag II) keys flip in
// CBInterface.cpp. A preset is "on" when everything it covers is off; turning it on switches those effects
// off, turning it off brings them back.
static const int kAntilag1[4] = { CNewUIOptionWindow::eGlowEffect, CNewUIOptionWindow::eEffectDynamic, CNewUIOptionWindow::eEffectStatic, CNewUIOptionWindow::eExcellentEffect };
static const int kAntilag2[5] = { CNewUIOptionWindow::eBMDImg, CNewUIOptionWindow::eBMDWeapons, CNewUIOptionWindow::eBMDWings, CNewUIOptionWindow::eBMDMonter, CNewUIOptionWindow::eBMDZen };

bool SEASON3B::CNewUIOptionWindow::IsPresetOn(int preset)
{
	const int* list = (preset == 1) ? kAntilag1 : kAntilag2;
	const int n = (preset == 1) ? 4 : 5;
	for (int i = 0; i < n; ++i)
	{
		if (OnOffGrap[list[i]] != 0) return false;
	}
	return true;
}

void SEASON3B::CNewUIOptionWindow::TogglePreset(int preset)
{
	const bool wasOn = IsPresetOn(preset);
	const int* list = (preset == 1) ? kAntilag1 : kAntilag2;
	const int n = (preset == 1) ? 4 : 5;
	for (int i = 0; i < n; ++i)
	{
		OnOffGrap[list[i]] = wasOn ? 1 : 0;
	}
	if (preset == 1)
	{
		g_RenderEff = wasOn;	// effects are drawn again when the preset is switched off
	}
}

// Language the next start will use. Mobile keeps the choice in GameConfig; PC keeps it in this window.
int SEASON3B::CNewUIOptionWindow::GetWantedLanguage()
{
#if defined(MU_IOS)
	const std::wstring sel = GameConfig::GetInstance().GetLanguageSelection();
	return (sel == L"Spn") ? LANG_INDEX_SPANISH : (sel == L"Vtm") ? LANG_INDEX_VIETNAMESE : LANG_INDEX_ENGLISH;
#else
	if (m_iLangWanted < 0)
	{
		m_iLangWanted = GetLanguageIndex();
	}
	return m_iLangWanted;
#endif
}

void SEASON3B::CNewUIOptionWindow::ChooseLanguage(int iLang)
{
	const int iStarted = GetLanguageIndex();
	if (iLang == GetWantedLanguage())
	{
		return;
	}
	m_iLangWanted = iLang;
	SetSelectedLanguage(LanguageCodeFromIndex(iLang));

	// Only when it differs from the language this run started in; going back is silent.
	if (iLang != iStarted)
	{
#if !defined(__ANDROID__) && !defined(MU_IOS)
		AskRestartForLanguage(iLang);
#else
		MU_MobileAskRestartForLanguage(iLang);
#endif
	}
}

// Runs one recorded click. Returns true when the click was used by the window.
bool SEASON3B::CNewUIOptionWindow::HandleModernClick()
{
	int hit = -1;
	for (int i = m_nHits - 1; i >= 0; --i)	// last drawn is on top
	{
		if (SEASON3B::CheckMouseIn(m_Hits[i].x, m_Hits[i].y, m_Hits[i].w, m_Hits[i].h) == 1)
		{
			hit = i;
			break;
		}
	}

	const int openDrop = m_iOpenDrop;
	if (hit < 0)
	{
		m_iOpenDrop = DD_NONE;
		return false;
	}

	const int id = m_Hits[hit].id;
	const int arg = m_Hits[hit].arg;
	if (id != MH_DROP && id != MH_DROPOPT)
	{
		m_iOpenDrop = DD_NONE;
	}
	PlayBuffer(SOUND_CLICK01);

	switch (id)
	{
	case MH_CLOSE:			gInterface.Data[eMenu_OPTION].Close(); break;
	case MH_AUTOATTACK:		m_bAutoAttack = !m_bAutoAttack; break;
	case MH_WHISPER:		m_bWhisperSound = !m_bWhisperSound; break;
	case MH_SLIDEHELP:		m_bSlideHelp = !m_bSlideHelp; break;
	case MH_MUSIC:			ToggleMusic(); break;
	case MH_SOUND:			ToggleSound(); break;
	case MH_GRAPH:			if (arg >= 0 && arg < eEndOnOffGrap) OnOffGrap[arg] ^= 1; break;
	case MH_PRESET:			TogglePreset(arg); break;
	case MH_RENDERLEVEL:	m_iRenderLevel = arg; break;
	case MH_HPBAR:			mShowHPBar ^= 1; break;
	case MH_NAME:			mShowName ^= 1; g_bGMObservation = mShowName; break;
	case MH_MINIMAP:		mShowMiniMap ^= 1; break;
	case MH_TITLE:			mShowDanhHieu ^= 1; break;
	case MH_MARKETMSG:		mHideMarketNotice ^= 1; break;
	case MH_ITEMNAMES:
		if (CNewUINameWindow* pNameWindow = g_pNewUISystem->GetUI_NewNameWindow())
		{
			pNameWindow->SetShowItemName(!pNameWindow->IsShowItemName());
		}
		break;
	case MH_VOICE:			VoiceSettingSetEnabled(!VoiceSettingGetEnabled()); break;
	case MH_MUTE:			VoiceSettingSetMuteOthers(!VoiceSettingGetMuteOthers()); break;
	case MH_NOISEGATE:		VoiceSettingSetNoiseGate(!VoiceSettingGetNoiseGate()); break;
	case MH_DROP:
		if (openDrop == arg)
		{
			m_iOpenDrop = DD_NONE;
		}
		else
		{
			m_iOpenDrop = arg;
			BuildDropOptions(arg);
		}
		break;
	case MH_DROPOPT:
		ApplyDropOption(openDrop, arg);
		m_iOpenDrop = DD_NONE;
		break;
	}
	return true;
}

bool SEASON3B::CNewUIOptionWindow::Render()
{
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	RenderModern();
	DisableAlphaBlend();
	return true;
}

void SEASON3B::CNewUIOptionWindow::RenderModern()
{
	m_nHits = 0;
	g_fPanelAlpha = (float)m_iOpacity / 100.f;
	if (g_fPanelAlpha < 0.3f) g_fPanelAlpha = 0.3f;
	if (g_fPanelAlpha > 1.f) g_fPanelAlpha = 1.f;

	if (!gInterface.Data[eMenu_OPTION].OnShow)
	{
		m_iOpenDrop = DD_NONE;
		g_pNewUISystem->Hide(INTERFACE_OPTION);
		return;
	}

	// ---- layout pass: how tall is each column ----
	const bool bVoice = gVoiceClient.IsEnabled();
	const bool bMicPicker = bVoice && VoiceAudioGetCaptureDeviceCount() > 0;
	int micRows = 0;
	if (bVoice)
	{
		micRows = (bMicPicker ? 1 : 0) + 1 /*boost*/ + 1 /*channel*/;
#if !defined(__ANDROID__) && !defined(MU_IOS)
		micRows += 1;	// talk mode, PC only
#endif
	}
	CNewUINameWindow* pNameWindow = g_pNewUISystem->GetUI_NewNameWindow();
	const bool bItemNames = (pNameWindow != NULL);
	int dispRows = 7 /*5 switches + text size + opacity*/ + (bItemNames ? 1 : 0);
	const float qualityH = kHeadH + 34.f + kPad;
	const float hA = CardH(3) + kGap + CardH(2) + (bVoice ? kGap + CardH(3 + micRows) : 0.f);
	const float hB = CardH(eEndOnOffGrap) + kGap + CardH(2);
	const float hC = CardH(dispRows) + kGap + qualityH + kGap + CardH(1);
	float total = hA;
	if (hB > total) total = hB;
	if (hC > total) total = hC;
	const float winH = kTitleH + total + 14.f;

	// ---- window position ----
	float StartX = (MAX_WIN_WIDTH / 2) - (kWinW / 2);
	float StartY = (float)((DisplayHeight - (int)winH) / 2);
	if (StartY < 8.f) StartY = 8.f;
	if (!gInterface.Data[eMenu_OPTION].FirstLoad)
	{
		gInterface.Data[eMenu_OPTION].X = StartX;
		gInterface.Data[eMenu_OPTION].Y = StartY;
		gInterface.Data[eMenu_OPTION].FirstLoad = true;
	}
	gInterface.Data[eMenu_OPTION].Width = kWinW;
	gInterface.Data[eMenu_OPTION].Height = winH - 46.f;	// MoveWindows keeps Height + 46 inside the screen
	if (gInterface.Data[eMenu_OPTION].AllowMove)
	{
		MoveWindows(eMenu_OPTION);
	}
	const float wx = gInterface.Data[eMenu_OPTION].X;
	const float wy = gInterface.Data[eMenu_OPTION].Y;
	SetPos((int)wx, (int)wy);

	if (SEASON3B::CheckMouseIn(wx, wy, kWinW, winH) == 1)
	{
		g_pBCustomMenuInfo->SetBlockCur(TRUE);
		gInterface.Data[eMenu_OPTION].Hover = true;
	}
	else
	{
		gInterface.Data[eMenu_OPTION].Hover = false;
	}

	// ---- panel ----
	RRectBg(wx - 1.f, wy - 1.f, kWinW + 2.f, winH + 2.f, 0.30f, 0.52f, 0.95f, 0.55f);	// thin accent edge
	RRectBg(wx, wy, kWinW, winH, 0.055f, 0.065f, 0.095f, 0.97f);
	RRectBg(wx, wy, kWinW, kTitleH, 0.075f, 0.09f, 0.13f, 1.0f);
	gInterface.DrawBarForm(wx, wy + kTitleH - 2.f, kWinW, 2.f, 0.24f, 0.55f, 1.0f, 1.0f);
	TextDraw(g_hFontBold, (int)wx, (int)(wy + 13.f), kTextMain, 0x0, (int)kWinW, 0, 3, "%s", "Settings");

	// close button
	{
		const float cx = wx + kWinW - 34.f, cy = wy + 8.f;
		const bool hover = (SEASON3B::CheckMouseIn(cx, cy, 24.f, 24.f) == 1);
		RRect(cx, cy, 24.f, 24.f, hover ? 0.85f : 0.16f, hover ? 0.25f : 0.18f, hover ? 0.25f : 0.24f, 1.0f);
		TextDraw(g_hFontBold, (int)cx, (int)(cy + 5.f), kTextMain, 0x0, 24, 0, 3, "%s", "x");
		AddHit(cx, cy, 24.f, 24.f, MH_CLOSE, 0);
	}

	const float colX[3] = { wx + 16.f, wx + 16.f + kColW + kGap, wx + 16.f + 2.f * (kColW + kGap) };
	const float top = wy + kTitleH + 10.f;

	// ================= column A: general, sound, voice =================
	{
		float y = top;
		float yy = DrawCard(colX[0], y, kColW, CardH(3), "General", kTextAccent);
		DrawToggleRow(colX[0], yy, kColW, GlobalText[386], m_bAutoAttack, MH_AUTOATTACK, 0); yy += kRowH;
		DrawToggleRow(colX[0], yy, kColW, GlobalText[387], m_bWhisperSound, MH_WHISPER, 0); yy += kRowH;
		DrawToggleRow(colX[0], yy, kColW, GlobalText[919], m_bSlideHelp, MH_SLIDEHELP, 0);
		y += CardH(3) + kGap;

		yy = DrawCard(colX[0], y, kColW, CardH(2), "Sound", kTextAccent);
		DrawToggleRow(colX[0], yy, kColW, "Music", m_MusicOnOff != 0, MH_MUSIC, 0); yy += kRowH;
		DrawToggleRow(colX[0], yy, kColW, "Sound effects", m_SoundOnOff != 0, MH_SOUND, 0);
		y += CardH(2) + kGap;

		if (bVoice)
		{
			yy = DrawCard(colX[0], y, kColW, CardH(3 + micRows), "Voice chat", kTextAccent);
			DrawToggleRow(colX[0], yy, kColW, "Voice chat", VoiceSettingGetEnabled(), MH_VOICE, 0); yy += kRowH;
			DrawToggleRow(colX[0], yy, kColW, "Mute voices", VoiceSettingGetMuteOthers(), MH_MUTE, 0); yy += kRowH;
			DrawToggleRow(colX[0], yy, kColW, "Noise gate", VoiceSettingGetNoiseGate(), MH_NOISEGATE, 0); yy += kRowH;

			if (bMicPicker)
			{
				char szDevice[64];
				VoiceAudioGetSelectedCaptureDeviceName(szDevice, sizeof(szDevice));
				if (strlen(szDevice) > 12)
				{
					szDevice[11] = '.';
					szDevice[12] = '\0';
				}
				DrawDropRow(colX[0], yy, kColW, "Microphone", szDevice, DD_MIC); yy += kRowH;
			}
			{
				char szBoost[16];
				sprintf(szBoost, "%d%%", VoiceSettingGetMicBoost());
				DrawDropRow(colX[0], yy, kColW, "Mic boost", szBoost, DD_BOOST); yy += kRowH;
			}
#if !defined(__ANDROID__) && !defined(MU_IOS)
			{
				const bool bToggle = (VoiceSettingGetTalkMode() == CfgDefaults::CfgVoiceTalkModeToggle);
				DrawDropRow(colX[0], yy, kColW, "Talk", bToggle ? "toggle on/off" : "hold to talk", DD_TALK); yy += kRowH;
			}
#endif
			{
				const bool bParty = (gVoiceClient.GetChannel() == VOICE_CHANNEL_PARTY);
				DrawDropRow(colX[0], yy, kColW, "Channel", bParty ? "Party (anywhere)" : "Nearby (proximity)", DD_CHANNEL);
			}
		}
	}

	// ================= column B: graphics + anti-lag presets =================
	{
		static const char* const kGraphLabels[eEndOnOffGrap] = {
			"Glow Efflect", "Effect Dynamic", "Effect Static", "BMD Player", "BMD Wings", "BMD Weapons",
			"BMD Img", "BMD Monter", "Objects", "Terrain", "Excellent Effect", "BMD Zen Drop" };
		float y = top;
		float yy = DrawCard(colX[1], y, kColW, CardH(eEndOnOffGrap), "Graphics", kTextAccent);
		for (int i = 0; i < eEndOnOffGrap; ++i)
		{
			DrawToggleRow(colX[1], yy, kColW, kGraphLabels[i], OnOffGrap[i] == 1, MH_GRAPH, i);
			yy += kRowH;
		}
		y += CardH(eEndOnOffGrap) + kGap;

		yy = DrawCard(colX[1], y, kColW, CardH(2), "Anti-lag presets", kTextAccent);
		DrawToggleRow(colX[1], yy, kColW, "Antilag I  (F6)", IsPresetOn(1), MH_PRESET, 1); yy += kRowH;
		DrawToggleRow(colX[1], yy, kColW, "Antilag II  (F7)", IsPresetOn(2), MH_PRESET, 2);
	}

	// ================= column C: display, effect limit, language =================
	{
		float y = top;
		float yy = DrawCard(colX[2], y, kColW, CardH(dispRows), "Display", kTextAccent);
		DrawToggleRow(colX[2], yy, kColW, "Show HP Bar", mShowHPBar == 1, MH_HPBAR, 0); yy += kRowH;
		DrawToggleRow(colX[2], yy, kColW, "Show Name", mShowName == 1, MH_NAME, 0); yy += kRowH;
		DrawToggleRow(colX[2], yy, kColW, "Show Minimap", mShowMiniMap == 1, MH_MINIMAP, 0); yy += kRowH;
		DrawToggleRow(colX[2], yy, kColW, "Show Title", mShowDanhHieu == 1, MH_TITLE, 0); yy += kRowH;
		DrawToggleRow(colX[2], yy, kColW, "Hide Market notices", mHideMarketNotice == 1, MH_MARKETMSG, 0); yy += kRowH;
		if (bItemNames)
		{
			DrawToggleRow(colX[2], yy, kColW, "Show ItemNames", pNameWindow->IsShowItemName(), MH_ITEMNAMES, 0); yy += kRowH;
		}
		{
#if defined(__ANDROID__) || defined(MU_IOS)
			char szSize[16];
			sprintf(szSize, "%d%%", GetAndroidTextSizePercent());
#else
			// PC: applies at the next start, like the language.
			static const char* const kSizeNames[5] = { "Small", "Normal", "Large", "Larger", "Largest" };
			int iSel = m_iFontWanted + 1;
			if (iSel < 0) iSel = 0;
			if (iSel > 4) iSel = 4;
			const char* szSize = kSizeNames[iSel];
#endif
			DrawDropRow(colX[2], yy, kColW, "Text size", szSize, DD_TEXTSIZE); yy += kRowH;
#if !defined(__ANDROID__) && !defined(MU_IOS)
			if (m_iFontWanted != m_FontSizePlus)
			{
				TextDraw(g_hFont, (int)(colX[2] + kColW - 80.f), (int)(y + 7.f), 0xFF60A8FFu, 0x0, 70, 0, 2, "%s", "restart needed");
			}
#endif
		}
		{
			char szAlpha[16];
			sprintf(szAlpha, "%d%%", m_iOpacity);
			DrawDropRow(colX[2], yy, kColW, "Transparency", szAlpha, DD_OPACITY);
		}
		y += CardH(dispRows) + kGap;

		// Effect limit: five steps, the same 5/7/9/11/13 scale the old bar showed.
		yy = DrawCard(colX[2], y, kColW, qualityH, "Effect limit", kTextAccent);
		{
			static const char* const kSteps[5] = { "5", "7", "9", "11", "13" };
			const float segW = (kColW - 20.f - 4.f * 3.f) / 5.f;
			for (int i = 0; i < 5; ++i)
			{
				const float sx = colX[2] + 10.f + i * (segW + 3.f);
				const bool on = (i <= m_iRenderLevel);
				const bool sel = (i == m_iRenderLevel);
				const bool hover = (SEASON3B::CheckMouseIn(sx, yy + 2.f, segW, 28.f) == 1);
				if (sel)
					RRect(sx, yy + 2.f, segW, 28.f, 0.24f, 0.55f, 1.0f, 1.0f);
				else if (on)
					RRect(sx, yy + 2.f, segW, 28.f, 0.14f, 0.30f, 0.58f, 1.0f);
				else
					RRect(sx, yy + 2.f, segW, 28.f, hover ? 0.22f : 0.16f, hover ? 0.25f : 0.18f, hover ? 0.32f : 0.24f, 1.0f);
				TextDraw(g_hFontBold, (int)sx, (int)(yy + 10.f), (on || hover) ? kTextMain : kTextMuted, 0x0, (int)segW, 0, 3, "%s", kSteps[i]);
				AddHit(sx, yy + 2.f, segW, 28.f, MH_RENDERLEVEL, i);
			}
		}
		y += qualityH + kGap;

		yy = DrawCard(colX[2], y, kColW, CardH(1), "Language", kTextAccent);
		{
			static const char* const kLangNames[LANG_INDEX_COUNT] = { "English", "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t", "Espa\xC3\xB1ol" };
			DrawDropRow(colX[2], yy, kColW, NULL, kLangNames[GetWantedLanguage()], DD_LANG);
			if (GetWantedLanguage() != GetLanguageIndex())
			{
				TextDraw(g_hFont, (int)(colX[2] + kColW - 80.f), (int)(y + 7.f), 0xFF60A8FFu, 0x0, 70, 0, 2, "%s", "restart needed");
			}
		}
	}

	// ================= the open drop-down list, over everything =================
	if (m_iOpenDrop != DD_NONE && m_nDropOpts > 0)
	{
		const float lh = m_nDropOpts * kRowH;
		float ly = m_dropY + kRowH;
		if (ly + lh > (float)DisplayHeight - 6.f)
		{
			ly = m_dropY - lh;	// no room below: open upward
		}
		const float lx = m_dropX, lw = m_dropW;
		RRect(lx - 1.f, ly - 1.f, lw + 2.f, lh + 2.f, 0.30f, 0.52f, 0.95f, 0.85f);
		RRect(lx, ly, lw, lh, 0.075f, 0.09f, 0.13f, 1.0f);
		for (int i = 0; i < m_nDropOpts; ++i)
		{
			const float oy = ly + i * kRowH;
			const bool oh = (SEASON3B::CheckMouseIn(lx, oy, lw, kRowH) == 1);
			if (oh)
				gInterface.DrawBarForm(lx + 2.f, oy + 1.f, lw - 4.f, kRowH - 2.f, 0.16f, 0.2f, 0.28f, 1.0f);
			if (i == m_dropCur)
				gInterface.DrawBarForm(lx + 2.f, oy + 4.f, 3.f, kRowH - 8.f, 0.24f, 0.55f, 1.0f, 1.0f);
			TextDraw(g_hFont, (int)(lx + 12.f), (int)(oy + 5.f), i == m_dropCur ? kTextMain : kTextMuted, 0x0, 0, 0, 1, "%s", m_dropLabels[i]);
			AddHit(lx, oy, lw, kRowH, MH_DROPOPT, i);
		}
	}
}

void SEASON3B::CNewUIOptionWindow::SetAutoAttack(bool bAuto)
{
	m_bAutoAttack = bAuto;
}

bool SEASON3B::CNewUIOptionWindow::IsAutoAttack()
{
	return m_bAutoAttack;
}

void SEASON3B::CNewUIOptionWindow::SetWhisperSound(bool bSound)
{
	m_bWhisperSound = bSound;
}

bool SEASON3B::CNewUIOptionWindow::IsWhisperSound()
{
	return m_bWhisperSound;
}

void SEASON3B::CNewUIOptionWindow::SetSlideHelp(bool bHelp)
{
	m_bSlideHelp = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::IsSlideHelp()
{
	return m_bSlideHelp;
}

void SEASON3B::CNewUIOptionWindow::SetVolumeLevel(int iVolume)
{
	m_iVolumeLevel = iVolume;
}

int SEASON3B::CNewUIOptionWindow::GetVolumeLevel()
{
	return m_iVolumeLevel;
}

void SEASON3B::CNewUIOptionWindow::SetRenderLevel(int iRender)
{
	m_iRenderLevel = iRender;
}

int SEASON3B::CNewUIOptionWindow::GetRenderLevel()
{
	return m_iRenderLevel;
}
