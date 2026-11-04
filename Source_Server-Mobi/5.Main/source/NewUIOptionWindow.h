// NewUIOptionWindow.h: interface for the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_)
#define AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_

#pragma once

#include "NewUIManager.h"
#include "NewUIMyInventory.h"

namespace SEASON3B
{
	
	class CNewUIOptionWindow  : public CNewUIObj 
	{
	public:
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
	public:
#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
		enum IMAGE_LIST
		{
			IMAGE_OPTION_FRAME_BACK = CNewUIMessageBoxMng::IMAGE_MSGBOX_BACK,
			IMAGE_OPTION_BTN_CLOSE = CNewUIMessageBoxMng::IMAGE_MSGBOX_BTN_CLOSE,
			IMAGE_OPTION_FRAME_DOWN = CNewUIMyInventory::IMAGE_INVENTORY_BACK_BOTTOM,

			IMAGE_OPTION_FRAME_UP = BITMAP_OPTION_BEGIN,
			IMAGE_OPTION_FRAME_LEFT,
			IMAGE_OPTION_FRAME_RIGHT,
			IMAGE_OPTION_LINE,
			IMAGE_OPTION_POINT,	
			IMAGE_OPTION_BTN_CHECK,
			IMAGE_OPTION_EFFECT_BACK,
			IMAGE_OPTION_EFFECT_COLOR,
			IMAGE_OPTION_VOLUME_BACK,
			IMAGE_OPTION_VOLUME_COLOR,
		};
		enum ONOFFGRAP_OPTION
		{
			eGlowEffect,
			eEffectDynamic,
			eEffectStatic,
			eBMDPlayer,
			eBMDWings,
			eBMDWeapons,
			eBMDImg,
			eBMDMonter,
			eRenderObjects,
			eRenderTerrain,
			eExcellentEffect,
			eBMDZen,
			eEndOnOffGrap,
		};

	public:
		CNewUIOptionWindow();
		virtual ~CNewUIOptionWindow();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();
		
		void SetPos(int x, int y);
		
		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();

		float GetLayerDepth();	//. 10.5f
		float GetKeyEventOrder();	// 10.f;
		
		void OpenningProcess();
		void ClosingProcess();

		void SetAutoAttack(bool bAuto);
		bool IsAutoAttack();
		void SetWhisperSound(bool bSound);
		bool IsWhisperSound();
		void SetSlideHelp(bool bHelp);
		bool IsSlideHelp();
		void SetVolumeLevel(int iVolume);
		int GetVolumeLevel();
		void SetRenderLevel(int iRender);
		int GetRenderLevel();
		int GetOpacityPercent() { return m_iOpacity; }
		//===
		bool OnOffGrap[eEndOnOffGrap];

	private:
		void LoadImages();
		void UnloadImages();

		void RenderModern();
		void ToggleMusic();
		void ToggleSound();
		int GetWantedLanguage();
		void ChooseLanguage(int iLang);
		bool HandleModernClick();
		void AddHit(float x, float y, float w, float h, int id, int arg);
		void DrawToggleRow(float x, float y, float w, const char* label, bool on, int id, int arg);
		void DrawDropRow(float x, float y, float w, const char* label, const char* value, int dd);
		void BuildDropOptions(int dd);
		void ApplyDropOption(int dd, int idx);
		bool IsPresetOn(int preset);
		void TogglePreset(int preset);

		// Clickable areas recorded while drawing, resolved by UpdateMouseEvent.
		enum { MODERN_MAX_HITS = 64 };
		enum { MH_CLOSE = 1, MH_AUTOATTACK, MH_WHISPER, MH_SLIDEHELP, MH_MUSIC, MH_SOUND, MH_GRAPH, MH_RENDERLEVEL, MH_HPBAR, MH_NAME,
			MH_MINIMAP, MH_TITLE, MH_ITEMNAMES, MH_VOICE, MH_MUTE, MH_NOISEGATE, MH_MIC, MH_BOOST, MH_TALK, MH_CHANNEL, MH_TEXTSIZE,
			MH_PRESET, MH_DROP, MH_DROPOPT, MH_MARKETMSG };
		enum { MODERN_MAX_DROP = 12 };
		struct ModernHit { float x, y, w, h; int id, arg; };
		ModernHit m_Hits[MODERN_MAX_HITS];
		int m_nHits;
		int m_iOpenDrop;		// id of the open drop-down, 0 = none
		float m_dropX, m_dropY, m_dropW;
		int m_nDropOpts;
		int m_dropCur;
		char m_dropLabels[MODERN_MAX_DROP][32];
		int m_dropValues[MODERN_MAX_DROP];
		int m_iFontWanted;		// PC text size bonus chosen in the window (applies at the next start)
		DWORD m_dwOpenTick;		// when the window opened, so the click that opened it cannot also close it
		int m_iOpacity;			// window opacity in percent (30..100)
		int m_iLangWanted;

	private:
		CNewUIManager*				m_pNewUIMng;
		POINT						m_Pos;

		bool m_bAutoAttack;		// �ڵ� ����
		bool m_bWhisperSound;	// �Ӹ� �˸��� 
		bool m_bSlideHelp;		// �����̵� ����
		int m_iVolumeLevel;		// ��������
		int m_iRenderLevel;		// ȿ������
	};
	
}

#endif // !defined(AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_)
