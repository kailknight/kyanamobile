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
#include "ZzzInterface.h"
#include "SkillManager.h"
#include "DSPlaySound.h"
#include "UIControls.h"
#include "CharacterManager.h"

CAutoPotion gAutoPotion;
CAutoCombo gAutoCombo;

#ifndef VK_OEM_3
#define VK_OEM_3 0xC0 // ~ (the key left of 1 on a US keyboard)
#endif

namespace
{
	const DWORD kTripleTapGapMs = 500;
	const DWORD kHoldToOpenMs = 700;
	// Floor on the gap between two auto potions, on top of MainInfo's
	// DelayAutoHP, so a zero there cannot fire a request every frame. The
	// GameServer applies its own PotionDelayMS as well.
	const DWORD kMinDrinkGapMs = 250;
	const char* kSettingsFile = "AutoPotion.ini";
	const char* kComboSettingsFile = "AutoCombo.ini";

	// The chain goes cold after this long without a cast attempt, and starts
	// again from the first skill.
	const DWORD kComboColdMs = 2500;
	// A step refused for this long (target gone, out of range, no mana) is not
	// going to work by being retried: start the chain over instead of freezing
	// on it.
	const DWORD kComboBlockedResetMs = 1200;

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

	// Auto combo is a Dark Knight feature (Blade Knight, Blade Master): only that
	// class line sees it in the menu, and only it casts the chain.
	bool IsAutoComboClass()
	{
		return Hero != NULL && gCharacterManager.GetBaseClass(Hero->Class) == CLASS_KNIGHT;
	}

	int ClampComboDelay(int delayMs)
	{
		if (delayMs < CAutoCombo::kMinDelayMs)
		{
			return CAutoCombo::kMinDelayMs;
		}
		if (delayMs > CAutoCombo::kMaxDelayMs)
		{
			return CAutoCombo::kMaxDelayMs;
		}
		return delayMs;
	}
}

#if !defined(__ANDROID__) && !defined(MU_IOS)
namespace
{
	// Typing a ms value in the auto menu: one box, moved onto whichever row was
	// clicked. Created on first use and kept, like the jewel bank's amount box.
	CUITextInputBox* g_ComboDelayInput = NULL;
	int g_ComboDelayEditStep = -1;

	void EndComboDelayEdit(bool apply)
	{
		if (g_ComboDelayEditStep < 0 || g_ComboDelayInput == NULL)
		{
			g_ComboDelayEditStep = -1;
			return;
		}

		if (apply)
		{
			char text[8] = { 0 };
			g_ComboDelayInput->GetText(text, sizeof(text));
			const int value = atoi(text);
			if (value > 0)
			{
				gAutoCombo.SetOwnDelayMs(g_ComboDelayEditStep, value);
			}
		}

		if (g_ComboDelayInput->HaveFocus())
		{
			SetFocus(g_hWnd);
		}
		g_ComboDelayEditStep = -1;
	}

	void BeginComboDelayEdit(int step, int x, int y)
	{
		if (g_ComboDelayInput == NULL)
		{
			g_ComboDelayInput = new CUITextInputBox;
			g_ComboDelayInput->Init(g_hWnd, 40, 14, 3);
			g_ComboDelayInput->SetBackColor(0, 0, 0, 0);
			g_ComboDelayInput->SetTextColor(255, 255, 255, 255);
			g_ComboDelayInput->SetFont(g_hFontBold);
			g_ComboDelayInput->SetState(UISTATE_NORMAL);
			g_ComboDelayInput->SetOption(UIOPTION_NUMBERONLY);
		}

		char text[8];
		sprintf_s(text, sizeof(text), "%d", gAutoCombo.GetDelayMs(step));
		g_ComboDelayInput->SetText(text);
		g_ComboDelayInput->SetPosition(x, y);
		g_ComboDelayInput->GiveFocus(TRUE);
		g_ComboDelayEditStep = step;
	}
}
#endif

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
	this->m_DownTick = 0;
	this->m_HoldHandled = true;
	this->m_NextDrinkTick = 0;
}

void CAutoPotion::SetServerRules(int accountLevel, int canConfigure, int threshold)
{
	this->m_CanConfigure = (canConfigure >= 0) ? (canConfigure != 0) : (accountLevel > 0);
	this->m_ServerThreshold = (threshold >= 10 && threshold <= 90) ? threshold : kDefaultThreshold;

#if defined(__ANDROID__) || defined(MU_IOS)
	// Lost the right (VIP ran out, or the config changed) with the settings
	// open: close them. On PC the menu stays - only its settings lock.
	if (!this->m_CanConfigure && gInterface.Data[eAutoPotionConfig].OnShow)
	{
		gInterface.Data[eAutoPotionConfig].Close();
	}
#endif
}

// The on/off state is the AutoHP flag the MainInfo "HP" button already flips,
// so that button and ~ x3 can never disagree.
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

void CAutoPotion::UpdateHotkey()
{
	if (IsTyping())
	{
		return;
	}

	if (SEASON3B::IsPress(VK_OEM_3))
	{
		this->m_DownTick = GetTickCount();
		this->m_HoldHandled = false;

		// The third quick press toggles auto potion.
		if (this->m_Tap.Press())
		{
			this->m_HoldHandled = true;
			this->SetEnabled(!this->IsEnabled());
		}
	}
	else if (SEASON3B::IsRepeat(VK_OEM_3) && !this->m_HoldHandled)
	{
		if ((GetTickCount() - this->m_DownTick) >= kHoldToOpenMs)
		{
			this->m_HoldHandled = true;
			// The press that started the hold must not count towards a later
			// triple press.
			this->m_Tap.Count = 0;
			this->OpenSettings();
		}
	}
}

void CAutoPotion::OpenSettings()
{
#if defined(__ANDROID__) || defined(MU_IOS)
	if (!this->m_CanConfigure)
	{
		char text[128];
		sprintf_s(text, sizeof(text), "Auto potion settings are not available for your account. It drinks at %d%% HP.", this->m_ServerThreshold);
		SystemMessage(text);
		return;
	}
#endif

	this->LoadSettings();
	gAutoCombo.LoadSettings();

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
#if !defined(__ANDROID__) && !defined(MU_IOS)
		// Closed while a value was being typed: keep what was typed and hand the
		// keyboard back to the game.
		EndComboDelayEdit(true);
#endif
		return;
	}

#if defined(__ANDROID__) || defined(MU_IOS)
	const float windowW = 190.0f;
	const float windowH = 150.0f;
#else
	const bool showCombo = IsAutoComboClass();
	const float windowW = 200.0f;
	const float windowH = showCombo ? 282.0f : 150.0f;
#endif
	float startX = (MAX_WIN_WIDTH - windowW) / 2.0f;
	float startY = 120.0f;

#if defined(__ANDROID__) || defined(MU_IOS)
	g_pBCustomMenuInfo->DrawWindowCustomMini(&startX, &startY, windowW, windowH, eAutoPotionConfig, "Auto Potion");
#else
	// The size is only taken on the window's first draw; keep it in step with
	// the class, which changes on a character switch.
	gInterface.Data[eAutoPotionConfig].Height = windowH;
	g_pBCustomMenuInfo->DrawWindowCustomMini(&startX, &startY, windowW, windowH, eAutoPotionConfig, "Auto Menu");
#endif

	if (!gInterface.Data[eAutoPotionConfig].OnShow || gInterface.Data[eAutoPotionConfig].BActiveHiden)
	{
		return;
	}

	gInterface.DrawBarForm(startX, startY + 25.0f, windowW, windowH - 25.0f, 0.0f, 0.0f, 0.0f, 0.8f);

#if defined(__ANDROID__) || defined(MU_IOS)
	float posX = startX + 10.0f;
	float posY = startY + 32.0f;

	// On/off, the same as the AutoPots button.
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

	TextDraw(g_hFont, startX, startY + windowH - 16.0f, 0xFFB0B0B0, 0x0, windowW, 0, 3, "AutoPots: tap on/off, hold for settings");
#else
	this->LoadSettings();
	gAutoCombo.LoadSettings();

	const float left = startX + 10.0f;
	const float rowW = windowW - 20.0f;

	// ---- Auto potion -------------------------------------------------------
	float posY = startY + 32.0f;

	TextDraw(g_hFontBold, left, posY + 6, 0xFFFFFFFF, 0x0, 0, 0, 1, "Auto Potion");
	const bool potionOn = this->IsEnabled();
	if (g_pBCustomMenuInfo->DrawButton(left + rowW - 55.0f, posY, 100, 12, " ", 55))
	{
		this->SetEnabled(!potionOn);
	}
	TextDraw(g_hFontBold, left + rowW - 55.0f, posY + 6, potionOn ? 0xFFC738FF : 0xFFFFFFFF, 0x0, 55, 0, 3, potionOn ? "ON" : "OFF");

	posY += 26.0f;
	g_pBCustomMenuInfo->RenderGroupBox((int)left, (int)posY, (int)rowW, 62, 90, 20);
	TextDraw(g_hFontBold, left + 5, posY + 5, 0xffffffff, 0x0, 0, 0, 1, "HP potion below");

	if (this->m_CanConfigure)
	{
		int steps = this->m_OwnThreshold / 10;
		g_pBCustomMenuInfo->RederBarOptionW((int)left + 25, (int)posY + 26, &steps);
		if (steps < 1) steps = 1;
		if (steps > 9) steps = 9;

		if (steps * 10 != this->m_OwnThreshold)
		{
			this->m_OwnThreshold = steps * 10;
			this->SaveSettings();
		}
		TextDraw(g_hFont, left, posY + 43, 0xffffffff, 0x0, rowW, 0, 3, "%d%%", this->m_OwnThreshold);
	}
	else
	{
		TextDraw(g_hFont, left, posY + 30, 0xFFFFFFFF, 0x0, rowW, 0, 3, "%d%%", this->m_ServerThreshold);
		TextDraw(g_hFont, left, posY + 44, 0xFF909090, 0x0, rowW, 0, 3, "set by the server for your account");
	}

	if (showCombo)
	{
		// ---- Auto combo --------------------------------------------------------
		posY += 74.0f;

		TextDraw(g_hFontBold, left, posY + 6, 0xFFFFFFFF, 0x0, 0, 0, 1, "Auto Combo");
		const bool comboOn = gAutoCombo.IsEnabled();
		if (g_pBCustomMenuInfo->DrawButton(left + rowW - 55.0f, posY, 100, 12, " ", 55))
		{
			gAutoCombo.SetEnabled(!comboOn);
		}
		TextDraw(g_hFontBold, left + rowW - 55.0f, posY + 6, comboOn ? 0xFFC738FF : 0xFFFFFFFF, 0x0, 55, 0, 3, comboOn ? "ON" : "OFF");

		posY += 26.0f;
		g_pBCustomMenuInfo->RenderGroupBox((int)left, (int)posY, (int)rowW, 90, 90, 20);
		TextDraw(g_hFontBold, left + 5, posY + 5, 0xffffffff, 0x0, 0, 0, 1, "ms per skill");

		for (int step = 0; step < CAutoCombo::kSteps; ++step)
		{
			const float rowY = posY + 24.0f + (float)step * 21.0f;
			const int delayMs = gAutoCombo.GetDelayMs(step);

			TextDraw(g_hFont, left + 10, rowY + 5, 0xFFFFFFFF, 0x0, 0, 0, 1, "Skill %d (key %d)", step + 1, step + 1);

			if (gAutoCombo.CanConfigure())
			{
				const float btnW = 22.0f;
				const float minusX = left + rowW - 98.0f;
				const float plusX = left + rowW - 10.0f - btnW;

				if (g_pBCustomMenuInfo->DrawButton(minusX, rowY, 70, 12, "-", btnW))
				{
					EndComboDelayEdit(false);
					gAutoCombo.SetOwnDelayMs(step, delayMs - CAutoCombo::kDelayStepMs);
				}

				// The number itself: click it to type a value, Enter or a click
				// anywhere else keeps it.
				const float cellX = minusX + btnW + 4.0f;
				const float cellW = plusX - cellX - 4.0f;
				const bool overCell = SEASON3B::CheckMouseIn(cellX, rowY, cellW, 14) == 1;

				if (g_ComboDelayEditStep == step && g_ComboDelayInput != NULL)
				{
					gInterface.DrawBarForm(cellX, rowY + 1.0f, cellW, 12.0f, 0.25f, 0.25f, 0.25f, 0.9f);
					g_ComboDelayInput->SetPosition((int)(cellX + cellW * 0.5f - 10.0f), (int)rowY + 2);
					g_ComboDelayInput->Render();

					if (SEASON3B::IsPress(VK_RETURN))
					{
						EndComboDelayEdit(true);
						// Used up here, so it does not also open the chat box.
						g_pNewKeyInput->SetKeyState(VK_RETURN, SEASON3B::CNewKeyInput::KEY_NONE);
					}
					else if (SEASON3B::IsPress(VK_ESCAPE))
					{
						EndComboDelayEdit(false);
					}
					else if (SEASON3B::IsPress(VK_LBUTTON) && !overCell)
					{
						EndComboDelayEdit(true);
					}
				}
				else
				{
					if (overCell)
					{
						gInterface.DrawBarForm(cellX, rowY + 1.0f, cellW, 12.0f, 1.0f, 1.0f, 1.0f, 0.15f);
					}
					TextDraw(g_hFontBold, minusX + btnW, rowY + 5, 0xFFFFFFFF, 0x0, plusX - (minusX + btnW), 0, 3, "%d", delayMs);

					if (overCell && SEASON3B::IsPress(VK_LBUTTON))
					{
						EndComboDelayEdit(true);
						BeginComboDelayEdit(step, (int)(cellX + cellW * 0.5f - 10.0f), (int)rowY + 2);
						PlayBuffer(25, 0, 0);
					}
				}

				if (g_pBCustomMenuInfo->DrawButton(plusX, rowY, 70, 12, "+", btnW))
				{
					EndComboDelayEdit(false);
					gAutoCombo.SetOwnDelayMs(step, delayMs + CAutoCombo::kDelayStepMs);
				}
			}
			else
			{
				TextDraw(g_hFontBold, left + rowW - 70.0f, rowY + 5, 0xFFFFFFFF, 0x0, 60, 0, 3, "%d", delayMs);
			}
		}

		if (!gAutoCombo.CanConfigure())
		{
			TextDraw(g_hFont, left, posY + 94, 0xFF909090, 0x0, rowW, 0, 3, "set by the server for your account");
		}
		else
		{
			TextDraw(g_hFont, left, posY + 94, 0xFF909090, 0x0, rowW, 0, 3, "click a number to type it (%d-%d)", CAutoCombo::kMinDelayMs, CAutoCombo::kMaxDelayMs);
		}

	}
	else
	{
		EndComboDelayEdit(true);
	}

	TextDraw(g_hFont, startX, startY + windowH - 16.0f, 0xFFB0B0B0, 0x0, windowW, 0, 3, "~ x3: potion on/off   Hold ~: this menu");
#endif
}

// -----------------------------------------------------------------------------
// Auto combo
// -----------------------------------------------------------------------------

CAutoCombo::CAutoCombo()
{
	this->m_Enabled = false;
	this->m_CanConfigure = false;
	this->m_ServerDelayMs = kDefaultDelayMs;
	for (int n = 0; n < kSteps; ++n)
	{
		this->m_OwnDelayMs[n] = 0;
	}
	this->m_SettingsLoaded = false;

	this->m_Step = 0;
	this->m_LastAttemptTick = 0;
	this->m_NextCastTick = 0;
	this->m_BlockedSinceTick = 0;
}

void CAutoCombo::SetServerRules(int accountLevel, int canConfigure, int delayMs)
{
	this->m_CanConfigure = (canConfigure >= 0) ? (canConfigure != 0) : (accountLevel > 0);
	this->m_ServerDelayMs = (delayMs >= kMinDelayMs && delayMs <= kMaxDelayMs) ? delayMs : kDefaultDelayMs;
}

void CAutoCombo::SetEnabled(bool enabled)
{
	if (this->m_Enabled == enabled)
	{
		return;
	}

	this->m_Enabled = enabled;
	this->m_Step = 0;
	this->m_NextCastTick = 0;
	this->m_BlockedSinceTick = 0;
	this->SaveSettings();

	char text[128];
	if (enabled)
	{
		sprintf_s(text, sizeof(text), "Auto combo ON - hold the skill button to cast skills 1, 2, 3 in turn.");
	}
	else
	{
		sprintf_s(text, sizeof(text), "Auto combo OFF.");
	}
	SystemMessage(text);
}

int CAutoCombo::GetDelayMs(int step) const
{
	if (step < 0 || step >= kSteps)
	{
		return this->m_ServerDelayMs;
	}

	// Locked accounts always follow the server, even with a file from when they
	// were allowed to choose.
	if (!this->m_CanConfigure || this->m_OwnDelayMs[step] <= 0)
	{
		return this->m_ServerDelayMs;
	}

	return this->m_OwnDelayMs[step];
}

void CAutoCombo::SetOwnDelayMs(int step, int delayMs)
{
	if (step < 0 || step >= kSteps || !this->m_CanConfigure)
	{
		return;
	}

	this->m_OwnDelayMs[step] = ClampComboDelay(delayMs);
	this->SaveSettings();
}

void CAutoCombo::LoadSettings()
{
	if (this->m_SettingsLoaded)
	{
		return;
	}
	this->m_SettingsLoaded = true;

	FILE* file = fopen(kComboSettingsFile, "r");
	if (file == NULL)
	{
		return;
	}

	char line[64];
	while (fgets(line, sizeof(line), file) != NULL)
	{
		int value = 0;

		if (sscanf(line, "Enabled=%d", &value) == 1)
		{
			this->m_Enabled = (value != 0);
		}
		else
		{
			for (int step = 0; step < kSteps; ++step)
			{
				char key[24];
				sprintf_s(key, sizeof(key), "Delay%d=%%d", step + 1);

				if (sscanf(line, key, &value) == 1)
				{
					this->m_OwnDelayMs[step] = ClampComboDelay(value);
				}
			}
		}
	}

	fclose(file);
}

void CAutoCombo::SaveSettings()
{
	FILE* file = fopen(kComboSettingsFile, "w");
	if (file == NULL)
	{
		return;
	}

	fprintf(file, "Enabled=%d\n", this->m_Enabled ? 1 : 0);
	for (int step = 0; step < kSteps; ++step)
	{
		if (this->m_OwnDelayMs[step] > 0)
		{
			fprintf(file, "Delay%d=%d\n", step + 1, this->m_OwnDelayMs[step]);
		}
	}
	fclose(file);
}

bool CAutoCombo::TryCast(CHARACTER* c, int selectedSkill, float selectedDistance)
{
#if defined(__ANDROID__) || defined(MU_IOS)
	// Mobile has its own combo on the attack wheel.
	return false;
#else
	if (!this->m_Enabled || c == NULL || Hero == NULL || g_pSkillList == NULL || CharacterAttribute == NULL || !IsAutoComboClass())
	{
		return false;
	}

	// The MU Helper runs its own skills.
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Started == 1)
	{
		return false;
	}

	// Buffs, friendly skills and the Nova charge keep their normal one-skill
	// behaviour: the player picked that skill to use it, not to start a chain.
	if (selectedSkill <= 0
		|| IsCorrectSkillType_FrendlySkill(selectedSkill)
		|| IsCorrectSkillType_Buff(selectedSkill)
		|| selectedSkill == AT_SKILL_BLAST_HELL
		|| selectedSkill == AT_SKILL_BLAST_HELL_BEGIN)
	{
		return false;
	}

	const DWORD now = GetTickCount();

	// Cold: start again from the first skill rather than halfway through.
	if ((now - this->m_LastAttemptTick) > kComboColdMs)
	{
		this->m_Step = 0;
		this->m_BlockedSinceTick = 0;
		this->m_NextCastTick = 0;
	}
	this->m_LastAttemptTick = now;

	// Still inside the wait after the last skill: nothing to cast yet. The held
	// button asks again next frame.
	if ((int)(now - this->m_NextCastTick) < 0)
	{
		return true;
	}

	if (this->m_BlockedSinceTick != 0 && (now - this->m_BlockedSinceTick) > kComboBlockedResetMs)
	{
		this->m_Step = 0;
		this->m_BlockedSinceTick = 0;
	}

	const int step = this->m_Step;

	// Hotkey 1, 2 or 3. A step with nothing bound uses the selected skill, which
	// is the plain attack the player had before the combo.
	int skill = selectedSkill;
	float distance = selectedDistance;

	const int hotKeySkillIndex = g_pSkillList->GetHotKey(step + 1);
	if (hotKeySkillIndex >= 0 && hotKeySkillIndex < MAX_MAGIC)
	{
		const int hotKeySkill = CharacterAttribute->Skill[hotKeySkillIndex];
		if (hotKeySkill > 0 && hotKeySkill < MAX_SKILLS)
		{
			skill = hotKeySkill;
			distance = gSkillManager.GetSkillDistance(skill, c);
			Hero->CurrentSkill = (BYTE)hotKeySkillIndex;
		}
	}

	// ExecuteSkill's return value describes the previous cast; a request on the
	// wire is the only honest "it went out".
	const DWORD sentBefore = g_SkillRequestSendSeq;
	ExecuteSkill(c, skill, distance);

	if (g_SkillRequestSendSeq != sentBefore)
	{
		this->m_Step = (step + 1) % kSteps;
		this->m_NextCastTick = now + (DWORD)this->GetDelayMs(step);
		this->m_BlockedSinceTick = 0;
	}
	else if (this->m_BlockedSinceTick == 0)
	{
		// Refused (cooldown, mana, out of range): hold this step and retry.
		this->m_BlockedSinceTick = now;
	}

	return true;
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
