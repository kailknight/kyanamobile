#include "stdafx.h"
#include "QuickToggles.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "NewUIMyInventory.h"
#include "wsclientinline.h"
#include "CBInterface.h"
#include "Protect.h"
#include "ZzzCharacter.h"
#include "ZzzInfomation.h"
#include "DSPlaySound.h"

CAutoPotion gAutoPotion;

namespace
{
	const DWORD kTripleTapGapMs = 500;
	const DWORD kHoldToOpenMs = 700;
	// Floor on the gap between two auto potions, on top of MainInfo's
	// DelayAutoHP, so a zero there cannot fire a request every frame. The
	// GameServer applies its own PotionDelayMS as well.
	const DWORD kMinDrinkGapMs = 250;
	const char* kSettingsFile = "AutoPotion.ini";

	void SystemMessage(const char* text)
	{
		if (g_pChatListBox != NULL)
		{
			g_pChatListBox->AddText("", text, SEASON3B::TYPE_SYSTEM_MESSAGE);
		}
	}

	// A key typed into chat (or any other edit box) is text, not a hotkey.
	bool IsTyping()
	{
		return GetFocus() != g_hWnd;
	}
}

bool CTripleTap::Press()
{
	const DWORD now = GetTickCount();

	this->Count = (this->Count > 0 && (now - this->LastTick) <= kTripleTapGapMs) ? (this->Count + 1) : 1;
	this->LastTick = now;

	if (this->Count >= 3)
	{
		this->Count = 0;
		return true;
	}

	return false;
}

CAutoPotion::CAutoPotion()
{
	this->m_CanConfigure = false;
	this->m_ServerThreshold = kDefaultThreshold;
	this->m_OwnThreshold = kDefaultThreshold;
	this->m_SettingsLoaded = false;
	this->m_QDownTick = 0;
	this->m_QHoldHandled = true;
	this->m_NextDrinkTick = 0;
}

void CAutoPotion::SetServerRules(int accountLevel, int canConfigure, int threshold)
{
	this->m_CanConfigure = (canConfigure >= 0) ? (canConfigure != 0) : (accountLevel > 0);
	this->m_ServerThreshold = (threshold >= 10 && threshold <= 90) ? threshold : kDefaultThreshold;

	// Lost the right (VIP ran out, or the config changed) with the settings
	// open: close them.
	if (!this->m_CanConfigure && gInterface.Data[eAutoPotionConfig].OnShow)
	{
		gInterface.Data[eAutoPotionConfig].Close();
	}
}

// The on/off state is the AutoHP flag the MainInfo "HP" button already flips,
// so that button and Q x3 can never disagree.
bool CAutoPotion::IsEnabled() const
{
	return g_pBCustomMenuInfo != NULL && g_pBCustomMenuInfo->AutoHP;
}

void CAutoPotion::SetEnabled(bool enabled)
{
	if (g_pBCustomMenuInfo == NULL)
	{
		return;
	}

	g_pBCustomMenuInfo->AutoHP = enabled;

	char text[128];
	if (enabled)
	{
		sprintf_s(text, sizeof(text), "Auto potion ON - drinks at %d%% HP.", this->GetThreshold());
	}
	else
	{
		sprintf_s(text, sizeof(text), "Auto potion OFF.");
	}
	SystemMessage(text);
}

int CAutoPotion::GetThreshold() const
{
	return this->m_CanConfigure ? this->m_OwnThreshold : this->m_ServerThreshold;
}

bool CAutoPotion::OnQPress()
{
	this->m_QDownTick = GetTickCount();
	this->m_QHoldHandled = false;

	if (IsTyping() || !this->m_QTap.Press())
	{
		return false;
	}

	this->SetEnabled(!this->IsEnabled());
	return true;
}

void CAutoPotion::OnQHeld()
{
	if (this->m_QHoldHandled || IsTyping())
	{
		return;
	}

	if ((GetTickCount() - this->m_QDownTick) >= kHoldToOpenMs)
	{
		this->m_QHoldHandled = true;
		this->OpenSettings();
	}
}

void CAutoPotion::OpenSettings()
{
	if (!this->m_CanConfigure)
	{
		char text[128];
		sprintf_s(text, sizeof(text), "Auto potion settings are not available for your account. It drinks at %d%% HP.", this->m_ServerThreshold);
		SystemMessage(text);
		return;
	}

	this->LoadSettings();

	if (!gInterface.Data[eAutoPotionConfig].OnShow)
	{
		gInterface.Data[eAutoPotionConfig].Open();
		PlayBuffer(25, 0, 0);
	}
}

void CAutoPotion::LoadSettings()
{
	if (this->m_SettingsLoaded)
	{
		return;
	}
	this->m_SettingsLoaded = true;

	// Never set by this player: start from the server's value.
	this->m_OwnThreshold = this->m_ServerThreshold;

	FILE* file = fopen(kSettingsFile, "r");
	if (file == NULL)
	{
		return;
	}

	int value = 0;
	if (fscanf(file, "Threshold=%d", &value) == 1 && value >= 10 && value <= 90)
	{
		this->m_OwnThreshold = value;
	}
	fclose(file);
}

void CAutoPotion::SaveSettings()
{
	FILE* file = fopen(kSettingsFile, "w");
	if (file == NULL)
	{
		return;
	}

	fprintf(file, "Threshold=%d\n", this->m_OwnThreshold);
	fclose(file);
}

void CAutoPotion::Update()
{
	// A saved threshold applies even if the settings were never opened
	// this session.
	if (this->m_CanConfigure)
	{
		this->LoadSettings();
	}

	if (!this->IsEnabled() || Hero == NULL || CharacterAttribute == NULL)
	{
		return;
	}

	const int maxHP = CharacterAttribute->PrintPlayer.ViewMaxHP;
	if (maxHP <= 0)
	{
		return;
	}

	const int rateHP = (int)((CharacterAttribute->PrintPlayer.ViewCurHP * 100) / maxHP);
	if (rateHP > this->GetThreshold() || GetTickCount() < this->m_NextDrinkTick)
	{
		return;
	}

	const int index = g_pMyInventory->FindHPItemIndex();
	if (index != -1)
	{
		SendRequestUse(index, 0);
	}

	DWORD gap = (DWORD)gProtect.m_MainInfo.DelayAutoHP;
	if (gap < kMinDrinkGapMs)
	{
		gap = kMinDrinkGapMs;
	}
	this->m_NextDrinkTick = GetTickCount() + gap;
}

void CAutoPotion::Render()
{
	if (!gInterface.Data[eAutoPotionConfig].OnShow || g_pBCustomMenuInfo == NULL)
	{
		return;
	}

	const float windowW = 190.0f;
	const float windowH = 150.0f;
	float startX = (MAX_WIN_WIDTH - windowW) / 2.0f;
	float startY = 120.0f;

	g_pBCustomMenuInfo->DrawWindowCustomMini(&startX, &startY, windowW, windowH, eAutoPotionConfig, "Auto Potion");

	if (!gInterface.Data[eAutoPotionConfig].OnShow || gInterface.Data[eAutoPotionConfig].BActiveHiden)
	{
		return;
	}

	gInterface.DrawBarForm(startX, startY + 25.0f, windowW, windowH - 25.0f, 0.0f, 0.0f, 0.0f, 0.8f);

	float posX = startX + 10.0f;
	float posY = startY + 32.0f;

	// On/off, the same as Q x3.
	const bool enabled = this->IsEnabled();
	if (g_pBCustomMenuInfo->DrawButton(posX, posY, 100, 12, " ", 55))
	{
		this->SetEnabled(!enabled);
	}
	TextDraw(g_hFontBold, posX, posY + 6, enabled ? 0xFFC738FF : 0xFFFFFFFF, 0x0, 55, 0, 3, enabled ? "ON" : "OFF");

	// Threshold, drawn exactly like the MU Helper's potion bar.
	posY += 30.0f;
	g_pBCustomMenuInfo->RenderGroupBox((int)posX, (int)posY, 170, 62, 90, 20);
	TextDraw(g_hFontBold, posX + 5, posY + 5, 0xffffffff, 0x0, 0, 0, 1, "HP potion below");
	posY += 26.0f;

	int steps = this->m_OwnThreshold / 10;
	g_pBCustomMenuInfo->RederBarOptionW((int)posX + 25, (int)posY, &steps);
	if (steps < 1) steps = 1;
	if (steps > 9) steps = 9;

	if (steps * 10 != this->m_OwnThreshold)
	{
		this->m_OwnThreshold = steps * 10;
		this->SaveSettings();
	}
	TextDraw(g_hFont, posX, posY + 17, 0xffffffff, 0x0, 170, 0, 3, "%d%%", this->m_OwnThreshold);

#if defined(__ANDROID__) || defined(MU_IOS)
	TextDraw(g_hFont, startX, startY + windowH - 16.0f, 0xFFB0B0B0, 0x0, windowW, 0, 3, "AutoPots: tap on/off, hold for settings");
#else
	TextDraw(g_hFont, startX, startY + windowH - 16.0f, 0xFFB0B0B0, 0x0, windowW, 0, 3, "Q x3: on/off   Hold Q: settings");
#endif
}

namespace
{
	CTripleTap g_CtrlTap;
}

void UpdatePkModeHotkey()
{
	if (g_pBCustomMenuInfo == NULL || !SEASON3B::IsPress(VK_CONTROL) || IsTyping())
	{
		return;
	}

	if (!g_CtrlTap.Press())
	{
		return;
	}

	g_pBCustomMenuInfo->AutoCtrlPK ^= 1;
	SystemMessage(g_pBCustomMenuInfo->AutoCtrlPK ? "PK mode ON." : "PK mode OFF.");
}
