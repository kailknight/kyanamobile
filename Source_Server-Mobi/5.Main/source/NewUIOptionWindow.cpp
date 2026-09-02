// NewUIOptionWindow.cpp: implementation of the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIOptionWindow.h"
#include "NewUISystem.h"
#include "ZzzTexture.h"
#include "DSPlaySound.h"
#include "ZzzInterface.h"
#include "CBInterface.h"
#include "GameConfig/GameConfig.h"

using namespace SEASON3B;

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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SEASON3B::CNewUIOptionWindow::CNewUIOptionWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;

	m_bAutoAttack = true;
	m_bWhisperSound = false;
	m_bSlideHelp = true;
	m_iVolumeLevel = 0;
#ifdef __ANDROID__
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
}

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
	if(SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x+150, m_Pos.y+43, 15, 15))
	{
		m_bAutoAttack = !m_bAutoAttack;
	}
	if(SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x+150, m_Pos.y+65, 15, 15))
	{
		m_bWhisperSound = !m_bWhisperSound;
	}
	if(SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x+150, m_Pos.y+127, 15, 15))
	{
		m_bSlideHelp = !m_bSlideHelp;
	}
	
	// Volume's mouse-wheel/drag handling used to live here - removed along
	// with the slider itself (RenderButtons()); Music ON/Off and Sound
	// ON/Off are RenderCheckBox()es now, which handle their own clicks
	// during Render(), so nothing is needed in this function for them.

	if(CheckMouseIn(m_Pos.x+25, m_Pos.y+168, 141, 29))
	{
		if(SEASON3B::IsRepeat(VK_LBUTTON))
		{
			int x = MouseX - (m_Pos.x + 25);
			float fValue = (5.f * x) / 141.f;
			m_iRenderLevel = (int)fValue;
		}
	}

	if(CheckMouseIn(m_Pos.x, m_Pos.y, 190, 249) == true)
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

bool SEASON3B::CNewUIOptionWindow::Render()
{
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	RenderFrame();
	RenderContents();
	RenderButtons();
	RenderCustomFrame();
	DisableAlphaBlend();
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

void SEASON3B::CNewUIOptionWindow::RenderFrame()
{
	//float x, y;
	//x = m_Pos.x;
	//y = m_Pos.y;
	//RenderImage(IMAGE_OPTION_FRAME_BACK, x, y, 190.f, 285);
	//RenderImage(IMAGE_OPTION_FRAME_UP, x, y, 190.f, 64.f);
	//y += 64.f;
	//for(int i=0; i<18; ++i)
	//{
	//	RenderImage(IMAGE_OPTION_FRAME_LEFT, x, y, 21.f, 10.f);
	//	RenderImage(IMAGE_OPTION_FRAME_RIGHT, x+190-21, y, 21.f, 10.f);
	//	y += 10.f;
	//}
	//RenderImage(IMAGE_OPTION_FRAME_DOWN, x, y, 190.f, 45.f);

	//y = m_Pos.y + 60.f;
	//RenderImage(IMAGE_OPTION_LINE, x+18, y, 154.f, 2.f);
	//y += 22.f;
	//RenderImage(IMAGE_OPTION_LINE, x+18, y, 154.f, 2.f);
	//y += 40.f;
	//RenderImage(IMAGE_OPTION_LINE, x+18, y, 154.f, 2.f);
	//y += 22.f;
	//RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
	//y += 60.f;
	//RenderImage(IMAGE_OPTION_LINE, x+18, y, 154.f, 2.f);
	if (!gInterface.Data[eMenu_OPTION].OnShow)
	{
		g_pNewUISystem->Hide(INTERFACE_OPTION);
		return;
	}
	float MainWidth = 360;
	float MainHeight = 275;
	float StartX = (MAX_WIN_WIDTH / 2) - (MainWidth / 2);
	float StartY = m_Pos.y;
	//--
	g_pBCustomMenuInfo->gDrawWindowCustom(&StartX, &StartY, MainWidth, MainHeight, eMenu_OPTION, "Config System"); //
	SetPos(StartX, StartY);

	float x, y;
	x = m_Pos.x;
	y = m_Pos.y;


	y = m_Pos.y + 60.f;
	RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
	y += 22.f;
	RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
	y += 40.f;
	RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
	y += 22.f;
	RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
	y += 60.f;
	RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
}
//==Config Custom
void SEASON3B::CNewUIOptionWindow::RenderCustomFrame()
{
	float x, y;
	x = m_Pos.x +175;
	y = m_Pos.y + 46.f;
	//===OPtion Custom
	RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
	g_pRenderText->RenderText(x + 20, y, "On/Off Graphics");

	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15, y + 15, 0xFFCC00C8, this->OnOffGrap[eGlowEffect] == 1 ? TRUE : FALSE, "Glow Efflect"))
	{
		this->OnOffGrap[eGlowEffect] ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 + 80, y + 15, 0xFFCC00C8, this->OnOffGrap[eEffectDynamic] == 1 ? TRUE : FALSE, "Effect Dynamic"))
	{
		this->OnOffGrap[eEffectDynamic] ^= 1;
	}
	y = y + 17;
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15, y + 15, 0xFFCC00C8, this->OnOffGrap[eEffectStatic] == 1 ? TRUE : FALSE, "Effect Static"))
	{
		this->OnOffGrap[eEffectStatic] ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 + 80, y + 15, 0xFFCC00C8, this->OnOffGrap[eBMDPlayer] == 1 ? TRUE : FALSE, "BMD Player"))
	{
		this->OnOffGrap[eBMDPlayer] ^= 1;
	}
	y = y + 17;
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15, y + 15, 0xFFCC00C8, this->OnOffGrap[eBMDWings] == 1 ? TRUE : FALSE, "BMD Wings"))
	{
		this->OnOffGrap[eBMDWings] ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 + 80, y + 15, 0xFFCC00C8, this->OnOffGrap[eBMDWeapons] == 1 ? TRUE : FALSE, "BMD Weapons"))
	{
		this->OnOffGrap[eBMDWeapons] ^= 1;
	}
	y = y + 17;
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 , y + 15, 0xFFCC00C8, this->OnOffGrap[eBMDImg] == 1 ? TRUE : FALSE, "BMD Img"))
	{
		this->OnOffGrap[eBMDImg] ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 + 80, y + 15, 0xFFCC00C8, this->OnOffGrap[eBMDMonter] == 1 ? TRUE : FALSE, "BMD Monter"))
	{
		this->OnOffGrap[eBMDMonter] ^= 1;
	}
	y = y + 17;
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 , y + 15, 0xFFCC00C8, this->OnOffGrap[eRenderObjects] == 1 ? TRUE : FALSE, "Objects"))
	{
		this->OnOffGrap[eRenderObjects] ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 + 80, y + 15, 0xFFCC00C8, this->OnOffGrap[eRenderTerrain] == 1 ? TRUE : FALSE, "Terrain"))
	{
		this->OnOffGrap[eRenderTerrain] ^= 1;
	}
	y = y + 17;
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15, y + 15, 0xFFCC00C8, this->OnOffGrap[eExcellentEffect] == 1 ? TRUE : FALSE, "Excellent Effect"))
	{
		this->OnOffGrap[eExcellentEffect] ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 + 80, y + 15, 0xFFCC00C8, this->OnOffGrap[eBMDZen] == 1 ? TRUE : FALSE, "BMD Zen Drop"))
	{
		this->OnOffGrap[eBMDZen] ^= 1;
	}
	//==================
	y = y + 40;
	RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
	g_pRenderText->RenderText(x+20, y, "On/Off Custom");

	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15, y+15, 0xFFCC00C8, mShowHPBar == 1 ? TRUE : FALSE, "Show HP Bar"))
	{
		mShowHPBar ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15 + 80,y + 15, 0xFFCC00C8, mShowName == 1 ? TRUE : FALSE, "Show Name"))
	{
		mShowName ^= 1;
		g_bGMObservation = mShowName;
	}
	y = y + 17;
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15, y + 15, 0xFFCC00C8, mShowMiniMap == 1 ? TRUE : FALSE, "Show Minimap"))
	{
		mShowMiniMap ^= 1;
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(x + 15+80, y + 15, 0xFFCC00C8, mShowDanhHieu == 1 ? TRUE : FALSE, "Show Title"))
	{
		mShowDanhHieu ^= 1;
	}
	y = y + 17;
	// Ground item names default off and only show via a momentary ALT-hold
	// (CNewUINameWindow::UpdateKeyEvent) or per-item mouseover - no keyboard
	// on mobile to hold, so this makes the "hold ALT" state a persistent
	// on/off choice here instead, same as the other On/Off Custom flags.
	// CNewUINameWindow isn't built until the main scene loads
	// (LoadMainSceneInterface, ZzzScene.cpp) - null-checked since this Config
	// System window itself is created earlier than that, even though in
	// practice it is only ever opened from inside the already-loaded game.
	CNewUINameWindow* pNameWindow = g_pNewUISystem->GetUI_NewNameWindow();

	if (pNameWindow != NULL)
	{
		if (g_pBCustomMenuInfo->RenderCheckBox(x + 15, y + 15, 0xFFCC00C8, pNameWindow->IsShowItemName(), "Show ItemNames"))
		{
			pNameWindow->SetShowItemName(!pNameWindow->IsShowItemName());
		}
	}
}
void SEASON3B::CNewUIOptionWindow::RenderContents()
{
	float x, y;
	x = m_Pos.x + 20.f;
	y = m_Pos.y + 46.f;
	RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
	y += 22.f;
	RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
	// The Volume row's own bullet point used to go here (y += 22) - removed
	// along with the "Volume" label below, since Music ON/Off and Sound
	// ON/Off are now self-labeling checkboxes (RenderButtons()) like the
	// On/Off Custom section, not a labeled row of their own.
	y += 22.f + 40.f;
	RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
	y += 22.f;
	RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->RenderText(m_Pos.x+40, m_Pos.y+48, GlobalText[386]);
	g_pRenderText->RenderText(m_Pos.x+40, m_Pos.y+70, GlobalText[387]);
	g_pRenderText->RenderText(m_Pos.x+40, m_Pos.y+132, GlobalText[919]);
	g_pRenderText->RenderText(m_Pos.x+40, m_Pos.y+154, GlobalText[1840]);


}

void SEASON3B::CNewUIOptionWindow::RenderButtons()
{
	if(m_bAutoAttack)
	{
		RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x+150, m_Pos.y+43, 15, 15, 0, 0);
	}
	else 
	{
		RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x+150, m_Pos.y+43, 15, 15, 0, 15.f);
	}

	if(m_bWhisperSound)
	{
		RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x+150, m_Pos.y+65, 15, 15, 0, 0);
	}
	else
	{
		RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x+150, m_Pos.y+65, 15, 15, 0, 15.f);
	}

	if(m_bSlideHelp)
	{
		RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x+150, m_Pos.y+127, 15, 15, 0, 0);
	}
	else
	{
		RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x+150, m_Pos.y+127, 15, 15, 0, 15.f);
	}

	// Was a draggable Volume level bar (0-10, m_iVolumeLevel/SetEffectVolumeLevel)
	// here - replaced with two on/off checkboxes per request. m_iVolumeLevel
	// itself is left alone: other code (android_main.cpp) still reads it via
	// GetVolumeLevel() to size actual playback volume once audio is on: this
	// only removes the slider that used to adjust it from this screen.
	//
	// Y positions: this whole row sits in the 40px gap RenderFrame() leaves
	// between its 2nd and 3rd divider lines (m_Pos.y+82 to m_Pos.y+122) -
	// the old Volume bar's own slot. +88/+105 (17px apart, each box 15px
	// tall) keeps both checkboxes inside that band with margin on both ends,
	// instead of running past the lower divider into the Slide Help row.
	if (g_pBCustomMenuInfo->RenderCheckBox(m_Pos.x + 15, m_Pos.y + 88, 0xFFCC00C8, m_MusicOnOff != 0, "Music ON/Off"))
	{
		m_MusicOnOff ^= 1;

		// SaveConfigDword (Winmain.h/.cpp) only exists on PC - Winmain.cpp is
		// excluded from the Android build entirely (CMakeLists.txt), so
		// calling it unconditionally here is an Android link failure, not
		// just a no-op. Android already has its own persisted audio settings
		// (GameConfig, read at startup - android_main.cpp) and simply had no
		// path back into it from this checkbox before now.
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
			// wzAudioCreate is only ever called at startup, gated on
			// m_MusicOnOff already being true then (Winmain.cpp) - which it
			// never is, because nothing in this codebase writes the
			// "MusicOnOff" registry value that startup read checks, so that
			// read always takes its missing-key branch and defaults to
			// false. wzAudio was therefore never actually created before
			// this checkbox existed, and calling wzAudioPlay (via PlayMp3,
			// from ordinary per-map music triggers) into an instance that
			// was never set up crashed inside the wzAudio DLL. Create it
			// here instead, once, the first time this checkbox turns music
			// on - Android doesn't use wzAudio at all so this is PC/iOS only.
			if (wzAudioCreate(g_hWnd) == 0)
			{
				g_bWzAudioCreated = true;
				wzAudioOption(WZAOPT_STOPBEFOREPLAY, 1);
			}
		}
#endif
	}
	if (g_pBCustomMenuInfo->RenderCheckBox(m_Pos.x + 15, m_Pos.y + 105, 0xFFCC00C8, m_SoundOnOff != 0, "Sound ON/Off"))
	{
		m_SoundOnOff ^= 1;

#if !defined(__ANDROID__) && !defined(MU_IOS)
		SaveConfigDword("SoundOnOff", m_SoundOnOff);
#else
		GameConfig::GetInstance().SetSoundEnabled(m_SoundOnOff != 0);
		GameConfig::GetInstance().Save();
#endif

		// Order matters below: SetMasterVolume() (DSplaysound.cpp, reached via
		// SetEffectVolumeLevel) early-returns while g_EnableSound is false, so
		// the mute has to be lifted *before* the volume is applied or the
		// volume call silently does nothing.

#if !defined(__ANDROID__) && !defined(MU_IOS)
		// Same gap as Music, different subsystem: InitDirectSound (Winmain.cpp)
		// now runs unconditionally at startup, so this is only a fallback for
		// a device that failed to initialise then - without it, turning sound
		// on would set g_EnableSound true with g_lpDS still null.
		if (m_SoundOnOff != 0 && g_bDirectSoundCreated == false)
		{
			if (SUCCEEDED(InitDirectSound(g_hWnd)))
			{
				g_bDirectSoundCreated = true;
			}
		}

		SetEnableSound(g_bDirectSoundCreated && m_SoundOnOff != 0);
#else
		// Unlike m_MusicOnOff (checked live by PlayMp3/StopMp3 on every call),
		// m_SoundOnOff on its own is only consulted once, at startup. The
		// actual per-effect gate is g_androidSoundEnabled, and SetEnableSound()
		// is what updates it - mirrors what android_main.cpp's own
		// audio-settings-apply code does after setting this same flag.
		SetEnableSound(m_SoundOnOff != 0);
#endif

		// The effect volume is what actually makes sound audible, and a stale
		// VolumeLevel=0 in the registry silences every effect while music keeps
		// playing (wzAudio has its own, separate volume) - which is exactly how
		// this reported as "music works, skill effects don't". With the volume
		// bar gone this checkbox owns the level too, so turning sound on
		// restores a real one instead of leaving whatever silenced it.
		// SetMasterVolume re-applies to every already-loaded buffer, so this
		// takes effect immediately rather than only for sounds loaded later.
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

	RenderImage(IMAGE_OPTION_EFFECT_BACK, m_Pos.x+25, m_Pos.y+168, 141.f, 29.f);
	if(m_iRenderLevel >= 0)
	{
		RenderImage(IMAGE_OPTION_EFFECT_COLOR, m_Pos.x+25, m_Pos.y+168, 141.f * 0.2f * 	(m_iRenderLevel+1), 29.f);
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
