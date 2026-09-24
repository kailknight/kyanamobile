#pragma once

// Keyboard toggles made by pressing a key three times in a row, and the auto
// potion they drive.
//
//   Ctrl x3   PK mode on/off (the same AutoCtrlPK the "HP and PK" button and
//             the mobile PK button flip)
//   Q x3      auto potion on/off
//   hold Q    auto potion settings
//
// Auto potion drinks an HP potion when HP falls to a threshold, like the MU
// Helper's potion option. Who may change the threshold, and what it is for
// everyone else, is server config (CustomConfig.ini AutoPotion_AL0..AL3 and
// AutoPotionThreshold), sent with the account level as 0xD3:0x7E. Until that
// arrives - or from an older GameServer - the rule is the old one: account
// level above 0 may change it, everyone else drinks at kDefaultThreshold.
// The gate is client-side, like the rest of this convenience feature.

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

	// From CNewUIItemHotKey::UpdateKeyEvent. OnQPress returns true when the
	// press was the third of a triple press and was used to toggle - the
	// caller then skips drinking the Q-slot potion for that press.
	bool OnQPress();
	void OnQHeld();

	// Every frame in the game scene.
	void Update();
	void Render();

	// Settings window, or a message when this account may not change them.
	// Hold Q on PC; long-press the AutoPots button on mobile.
	void OpenSettings();

private:
	void LoadSettings();
	void SaveSettings();

	bool m_CanConfigure;
	int m_ServerThreshold;  // for players who may not change it
	int m_OwnThreshold;     // the player's own choice, when they may
	bool m_SettingsLoaded;

	CTripleTap m_QTap;
	DWORD m_QDownTick;
	bool m_QHoldHandled;

	DWORD m_NextDrinkTick;
};

extern CAutoPotion gAutoPotion;

// Every frame in the game scene: Ctrl pressed three times toggles PK mode.
void UpdatePkModeHotkey();
