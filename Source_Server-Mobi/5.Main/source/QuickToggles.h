#pragma once

// Keyboard toggles made by pressing a key three times in a row, and the auto
// potion / auto combo they drive.
//
//   Ctrl x3   PK mode on/off (the same AutoCtrlPK the "HP and PK" button and
//             the mobile PK button flip)
//   ~ x3      auto potion on/off
//   hold ~    the auto menu: switches for auto potion and auto combo, and
//             their settings (HP threshold, ms per combo skill)
//
// Auto potion drinks an HP potion when HP falls to a threshold, like the MU
// Helper's potion option. Who may change the threshold, and what it is for
// everyone else, is server config (CustomConfig.ini AutoPotion_AL0..AL3 and
// AutoPotionThreshold), sent with the account level as 0xD3:0x7E. Until that
// arrives - or from an older GameServer - the rule is the old one: account
// level above 0 may change it, everyone else drinks at kDefaultThreshold.
//
// Auto combo (PC): while it is on, holding the skill button (right mouse)
// casts the skills on hotkeys 1, 2 and 3 in turn instead of the selected one
// over and over, waiting a set number of milliseconds after each. The ms per
// skill follow the same rule as the potion threshold (CustomConfig.ini
// AutoCombo_AL0..AL3 and AutoComboDelayMS, same packet).
// The gate is client-side, like the rest of these convenience features.

// Three presses of one key, each within kTripleTapGapMs of the last.
struct CTripleTap
{
	CTripleTap() : LastTick(0), Count(0) {}

	// Call on each press. True on the third.
	bool Press();

	DWORD LastTick;
	int Count;
};

class CAutoPotion
{
public:
	CAutoPotion();

	static const int kDefaultThreshold = 30;

	// From 0xD3:0x7E. canConfigure / threshold are -1 from a GameServer that
	// only sends the account level.
	void SetServerRules(int accountLevel, int canConfigure, int threshold);
	bool CanConfigure() const { return m_CanConfigure; }

	bool IsEnabled() const;
	void SetEnabled(bool enabled);

	// Percent of max HP at or below which a potion is used.
	int GetThreshold() const;

	// Every frame in the game scene, from the custom menu: the ~ key. Three
	// quick presses toggle auto potion, holding it opens the menu.
	void UpdateHotkey();

	// Every frame in the game scene.
	void Update();
	void Render();

	// The auto menu. On PC it always opens (the switches work for everyone;
	// the settings in it are locked for accounts that may not change them). On
	// mobile this is the AutoPots button's potion settings, which only open for
	// accounts that may change the threshold.
	void OpenSettings();

private:
	void LoadSettings();
	void SaveSettings();

	bool m_CanConfigure;
	int m_ServerThreshold;  // for players who may not change it
	int m_OwnThreshold;     // the player's own choice, when they may
	bool m_SettingsLoaded;

	CTripleTap m_Tap;
	DWORD m_DownTick;
	bool m_HoldHandled;

	DWORD m_NextDrinkTick;
};

extern CAutoPotion gAutoPotion;

class CHARACTER;

class CAutoCombo
{
public:
	CAutoCombo();

	static const int kSteps = 3;            // hotkeys 1, 2 and 3
	static const int kDefaultDelayMs = 400;
	static const int kMinDelayMs = 200;
	static const int kMaxDelayMs = 600; // same range as the mobile combo
	static const int kDelayStepMs = 50;

	// From 0xD3:0x7E. Both are -1 from a GameServer that does not send them:
	// the potion rule (account level above 0) decides, at the default pace.
	void SetServerRules(int accountLevel, int canConfigure, int delayMs);
	bool CanConfigure() const { return m_CanConfigure; }

	bool IsEnabled() const { return m_Enabled; }
	void SetEnabled(bool enabled);

	// Milliseconds waited after the skill of that step (0-2).
	int GetDelayMs(int step) const;
	void SetOwnDelayMs(int step, int delayMs);
	int GetServerDelayMs() const { return m_ServerDelayMs; }

	// The skill button is being held and the selected skill is an attack: casts
	// the next skill of the chain, or waits out the delay of the last. True when
	// it took over the cast - the caller then does not cast the selected skill.
	// Called from Attack() in ZzzInterface.cpp.
	bool TryCast(CHARACTER* c, int selectedSkill, float selectedDistance);

	void LoadSettings();

private:
	void SaveSettings();

	bool m_Enabled;
	bool m_CanConfigure;
	int m_ServerDelayMs;
	int m_OwnDelayMs[kSteps];   // 0 = never set, follows the server's value
	bool m_SettingsLoaded;

	int m_Step;
	DWORD m_LastAttemptTick;
	DWORD m_NextCastTick;
	DWORD m_BlockedSinceTick;
};

extern CAutoCombo gAutoCombo;

// Every frame in the game scene: Ctrl pressed three times toggles PK mode.
void UpdatePkModeHotkey();
