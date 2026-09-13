#include "StdAfx.h"
#include "CB_AutoLogin.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"
#include "./ExternalObject/leaf/regkey.h"
#include "UIMng.h"
#include "LoginWin.h"
#if(CB_AUTOLOGINWIN)

#if defined(__ANDROID__) || defined(MU_IOS)
namespace
{
	// Saved-account persistence needs an actual place to land on Android/iOS. The
	// Reg* calls below go through leaf::CRegKey to Platform/PlatformDefs.h, whose
	// Android shim is a deliberate no-op: RegSetValueEx writes nothing anywhere,
	// and RegQueryValueEx always reports success without copying the string
	// requested. That is fine for the many callers that only ever expected a
	// registry that quietly does nothing - it is not fine for a feature whose
	// entire purpose is to write something now and read it back later, so this
	// gets its own small file instead of pretending the registry works here.
	const char* const kAutoLoginConfigPath = "mu_autologin.cfg";

	struct AutoLoginConfigEntry
	{
		char key[16];
		char value[11];
	};

	int LoadAutoLoginConfig(AutoLoginConfigEntry* entries, int maxEntries)
	{
		FILE* f = fopen(kAutoLoginConfigPath, "r");
		if (f == nullptr)
		{
			return 0;
		}

		int count = 0;
		char line[64];
		while (count < maxEntries && fgets(line, sizeof(line), f) != nullptr)
		{
			line[strcspn(line, "\r\n")] = 0;
			char* eq = strchr(line, '=');
			if (eq == nullptr)
			{
				continue;
			}
			*eq = 0;
			strncpy(entries[count].key, line, sizeof(entries[count].key) - 1);
			entries[count].key[sizeof(entries[count].key) - 1] = 0;
			strncpy(entries[count].value, eq + 1, sizeof(entries[count].value) - 1);
			entries[count].value[sizeof(entries[count].value) - 1] = 0;
			++count;
		}
		fclose(f);
		return count;
	}

	const char* FindAutoLoginConfigValue(const AutoLoginConfigEntry* entries, int count, const char* key)
	{
		for (int i = 0; i < count; ++i)
		{
			if (strcmp(entries[i].key, key) == 0)
			{
				return entries[i].value;
			}
		}
		return nullptr;
	}
}
#endif

namespace
{
	// Saved-account list layout, in the same raw design-space units as the rest of
	// CB_AutoLogin's drawing (ScaleLoginMetric is applied at each use site, exactly
	// like everything else here). The list draws in the space CLoginWin clears for
	// it - Account box, Password box, checkbox and OK/Cancel/register all hidden
	// while showListAccount is true (CLoginWin::RenderControls) - because the
	// window is only 245 units tall and there was never room to show five stacked
	// accounts at the old fixed y=111/pitch=15 alongside those controls without
	// running straight through all of them.
	constexpr int kAccountListStartY = 128;
	constexpr int kAccountListRowPitch = 20;
	constexpr int kAccountListIdX = 114;
	constexpr int kAccountListRowWidth = 105;
	constexpr int kAccountListDeleteX = 245;
	constexpr int kAccountListDeleteSize = 12;

	/*
		Row hit-box size in the 640x480 space MouseX/MouseY live in.

		Everything here builds positions as (XPos + ScaleLoginMetric(k)) / rate,
		so ScaleLoginMetric values are in the same units as XPos/YPos and only
		become comparable to MouseX/MouseY after that division. The sizes handed
		to CheckMouseIn skipped it, and the row pitch is the one place that breaks
		visibly: consecutive rows are drawn exactly (KC / rate_y) apart in the
		compared space, so a box KC tall covers about rate_y rows.

		On a phone that is ~2.9 rows per box. Two rows lit up at once, and because
		the loop selects the first i whose box matches, tapping the third account
		selected the first - so the list looked like it refused to select.

		Derived from the pitch rather than from any assumption about the units,
		because the loop's own y expression is the proof: it advances by
		(KC * i) / rate_y per row.
	*/
	inline float AccountRowHitHeight(int rowPitchScaled)
	{
		const float rate = (g_fScreenRate_y > 0.f) ? g_fScreenRate_y : 1.f;
		return static_cast<float>(rowPitchScaled) / rate;
	}

	inline float AccountRowHitWidth()
	{
		const float rate = (g_fScreenRate_x > 0.f) ? g_fScreenRate_x : 1.f;
		return static_cast<float>(ScaleLoginMetric(kAccountListRowWidth)) / rate;
	}

	inline float AccountDeleteHitSize()
	{
		const float rate = (g_fScreenRate_x > 0.f) ? g_fScreenRate_x : 1.f;
		return static_cast<float>(ScaleLoginMetric(kAccountListDeleteSize)) / rate;
	}
	constexpr int kAccountListToggleX = 245;
	constexpr int kAccountListToggleY = 110;
	constexpr int kAccountListToggleSize = 14;
}

CB_AutoLogin* gCB_AutoLogin;

CB_AutoLogin::CB_AutoLogin()
{
	this->showListAccount = false;
	this->selectedAccount = 0;
	this->totalSavedAcc = 0;
	this->TickCount = 0;
}

CB_AutoLogin::~CB_AutoLogin()
{
}
void CB_AutoLogin::DrawInfo(int XPos, int YPos)
{
	// Every offset and size below is scaled with ScaleLoginMetric because XPos/YPos
	// is the top-left of the login window, and on Android/iOS that window is itself
	// enlarged by the same factor (CLoginWin::Create). Left as raw design-space
	// constants, these drifted further from the OK/Cancel buttons and input boxes -
	// which do scale - the taller the phone.
	if (!this->showListAccount)
	{
		// Checkbox only draws while the list isn't using this same area.
		if (g_pBCustomMenuInfo->RenderCheckBoxMini(int((XPos + ScaleLoginMetric(124)) / g_fScreenRate_x),
			int((YPos + ScaleLoginMetric(158)) / g_fScreenRate_y), 0xFFFFFFFF, m_SavePassOnOff == 1 ? TRUE : FALSE, "Save password"))
		{
			m_SavePassOnOff ^= 1;
		}
	}

	if (this->showListAccount && this->totalSavedAcc == 0)
	{
		this->showListAccount = 0;
	}

	if (this->showListAccount)
	{
		int KC = ScaleLoginMetric(kAccountListRowPitch);

		for (int i = 0; i < this->totalSavedAcc; i++)
		{
			DWORD BGColor = 0x000000FF;
			// Sizes divided by the screen rate, like the positions above them -
			// see AccountRowHitHeight. Passing the raw scaled pitch here made one
			// row's box cover about three rows.
			if (SEASON3B::CheckMouseIn(int((XPos + ScaleLoginMetric(kAccountListIdX)) / g_fScreenRate_x), int((YPos + ScaleLoginMetric(kAccountListStartY) + (KC * i)) / g_fScreenRate_y), static_cast<int>(AccountRowHitWidth()), static_cast<int>(AccountRowHitHeight(KC))) == 1)
			{
				BGColor = 0xA5A100FF;
				if (GetKeyState(VK_LBUTTON) & 0x8000 && GetTickCount() > this->TickCount +500)
				{
					PlayBuffer(25, 0, 0);
					this->SetSelectedAccount(i);
					this->showListAccount = 0;
					this->TickCount = GetTickCount();
				}
			}
			TextDraw(g_hFont, int((XPos + ScaleLoginMetric(kAccountListIdX)) / g_fScreenRate_x), int((YPos + ScaleLoginMetric(kAccountListStartY) + (KC*i)) / g_fScreenRate_y), 0xFFFFFFFF, BGColor, 115, 0, 1, this->saved_acc[i].ID); //ID
			//delete
			if (g_pBCustomMenuInfo->DrawButtonGUI(CNewUIPartyInfoWindow::IMAGE_PARTY_EXIT, int((XPos + ScaleLoginMetric(kAccountListDeleteX)) / g_fScreenRate_x), int((YPos + ScaleLoginMetric(kAccountListStartY) + (KC * i)) / g_fScreenRate_y), ScaleLoginMetric(kAccountListDeleteSize), ScaleLoginMetric(kAccountListDeleteSize)))
			{
				this->RemoveAccount(i);
			}
		}
	}

	// Same toggle in both states - it opens the list, and it is also the only way
	// to back out of the list without picking an account, now that OK/Cancel are
	// hidden behind it (CLoginWin::RenderControls).
	if (g_pBCustomMenuInfo->DrawButtonGUI(CNewUICastleWindow::IMAGE_CASTLEWINDOW_SCROLL_DOWN_BTN, int((XPos + ScaleLoginMetric(kAccountListToggleX)) / g_fScreenRate_x), int((YPos + ScaleLoginMetric(kAccountListToggleY)) / g_fScreenRate_y), ScaleLoginMetric(kAccountListToggleSize), ScaleLoginMetric(kAccountListToggleSize)))
	{
		this->showListAccount ^= 1;
		this->TickCount = GetTickCount();
	}
}
bool CB_AutoLogin::HitsControlArea(int XPos, int YPos, float uiX, float uiY) const
{
	// Mirrors DrawInfo's own layout by hand, in the same units each call there
	// already uses: positions are divided by g_fScreenRate_x/y to land in the same
	// space as MouseX/MouseY, sizes are not (DrawButtonGUI and RenderCheckBoxMini
	// take their SizeW/SizeH directly, undivided).
	auto within = [](float px, float py, float bx, float by, float bw, float bh)
	{
		return px >= bx && px <= (bx + bw) && py >= by && py <= (by + bh);
	};

	// "Save password" checkbox - RenderCheckBoxMini's own hit box is a fixed 15x15
	// (NewUIBCustomMenu.cpp), not scaled with the window. Only drawn/clickable
	// while the list isn't using this same area.
	if (!this->showListAccount && within(uiX, uiY,
		(XPos + ScaleLoginMetric(124)) / g_fScreenRate_x,
		(YPos + ScaleLoginMetric(158)) / g_fScreenRate_y,
		15.0f, 15.0f))
	{
		return true;
	}

	if (this->showListAccount)
	{
		const int KC = ScaleLoginMetric(kAccountListRowPitch);
		for (int i = 0; i < this->totalSavedAcc; ++i)
		{
			const float rowY = (YPos + ScaleLoginMetric(kAccountListStartY) + (KC * i)) / g_fScreenRate_y;
			// Divided, matching DrawInfo's own hit test - these two mirror each
			// other by hand, so an undivided size here would put the touch
			// dispatcher's idea of the list out of step with the highlight.
			if (within(uiX, uiY, (XPos + ScaleLoginMetric(kAccountListIdX)) / g_fScreenRate_x, rowY,
				AccountRowHitWidth(), AccountRowHitHeight(KC)))
			{
				return true;
			}
			if (within(uiX, uiY, (XPos + ScaleLoginMetric(kAccountListDeleteX)) / g_fScreenRate_x, rowY,
				AccountDeleteHitSize(), AccountDeleteHitSize()))
			{
				return true;
			}
		}
	}

	// Scroll-down arrow, present (and hit-testable) in both states.
	if (within(uiX, uiY,
		(XPos + ScaleLoginMetric(kAccountListToggleX)) / g_fScreenRate_x,
		(YPos + ScaleLoginMetric(kAccountListToggleY)) / g_fScreenRate_y,
		static_cast<float>(ScaleLoginMetric(kAccountListToggleSize)), static_cast<float>(ScaleLoginMetric(kAccountListToggleSize))))
	{
		return true;
	}

	return false;
}

void CB_AutoLogin::SetShowListAccount(bool show)
{
	this->showListAccount = show;
}
void CB_AutoLogin::SetSelectedAccount(int Index)
{
	this->selectedAccount = Index;
	CUIMng& rUIMng = CUIMng::Instance();
	if (m_SavePassOnOff)
	{
		rUIMng.m_LoginWin.GetIDInputBox()->SetText(this->saved_acc[this->selectedAccount].ID);
		rUIMng.m_LoginWin.GetPassInputBox()->SetText(this->saved_acc[this->selectedAccount].PW);
	}
	else
	{
		rUIMng.m_LoginWin.GetIDInputBox()->SetText(this->saved_acc[this->selectedAccount].ID);
		rUIMng.m_LoginWin.GetPassInputBox()->SetText("");
	}
}
void CB_AutoLogin::RemoveAccount(int Index)
{
	if (Index < 0 && Index >= this->totalSavedAcc)
	{
		return;
	}
#if defined(__ANDROID__) || defined(MU_IOS)
	AutoLoginConfigEntry entries[2 * MAX_ACCOUNT_SAVE + 1];
	int count = LoadAutoLoginConfig(entries, sizeof(entries) / sizeof(entries[0]));

	char zKey[50];
	if (Index == 0)
	{
		sprintf(zKey, "ID");
	}
	else
	{
		sprintf(zKey, "ID_%d", Index + 1);
	}

	FILE* f = fopen(kAutoLoginConfigPath, "w");
	if (f != nullptr)
	{
		char pwKey[50];
		if (Index == 0)
		{
			sprintf(pwKey, "PW");
		}
		else
		{
			sprintf(pwKey, "PW_%d", Index + 1);
		}

		for (int i = 0; i < count; ++i)
		{
			if (strcmp(entries[i].key, zKey) == 0 || strcmp(entries[i].key, pwKey) == 0)
			{
				continue;
			}
			fprintf(f, "%s=%s\n", entries[i].key, entries[i].value);
		}
		fclose(f);
	}
#else
	leaf::CRegKey regkey;
	regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config");
	if (Index == 0)
	{
		regkey.WriteString("ID", "");
		regkey.WriteString("PW", "");
	}
	else {
		char zKey[50];

		sprintf(zKey, "ID_%d", Index + 1);
		regkey.WriteString(zKey, "");
		sprintf(zKey, "PW_%d", Index + 1);
		regkey.WriteString(zKey, "");
	}
#endif
	this->ReadConfigs();
}
void CB_AutoLogin::ReadConfigs()
{
#if defined(__ANDROID__) || defined(MU_IOS)
	this->totalSavedAcc = 0;

	AutoLoginConfigEntry entries[2 * MAX_ACCOUNT_SAVE + 1];
	int count = LoadAutoLoginConfig(entries, sizeof(entries) / sizeof(entries[0]));

	char zKey[50];
	for (int i = 0; i < MAX_ACCOUNT_SAVE; i++)
	{
		sprintf(zKey, i ? "ID_%d" : "ID", i + 1);
		const char* id = FindAutoLoginConfigValue(entries, count, zKey);
		if (id == nullptr || id[0] == 0)
		{
			ZeroMemory(this->saved_acc[this->totalSavedAcc].ID, sizeof(this->saved_acc[this->totalSavedAcc].ID));
			continue;
		}

		sprintf(zKey, i ? "PW_%d" : "PW", i + 1);
		const char* pw = FindAutoLoginConfigValue(entries, count, zKey);
		if (pw == nullptr)
		{
			ZeroMemory(this->saved_acc[this->totalSavedAcc].PW, sizeof(this->saved_acc[this->totalSavedAcc].PW));
			continue;
		}

		strncpy(this->saved_acc[this->totalSavedAcc].ID, id, sizeof(this->saved_acc[this->totalSavedAcc].ID) - 1);
		this->saved_acc[this->totalSavedAcc].ID[sizeof(this->saved_acc[this->totalSavedAcc].ID) - 1] = 0;
		strncpy(this->saved_acc[this->totalSavedAcc].PW, pw, sizeof(this->saved_acc[this->totalSavedAcc].PW) - 1);
		this->saved_acc[this->totalSavedAcc].PW[sizeof(this->saved_acc[this->totalSavedAcc].PW) - 1] = 0;
		this->saved_acc[this->totalSavedAcc].index = i;
		this->totalSavedAcc++;
	}

	const char* savePass = FindAutoLoginConfigValue(entries, count, "SavePass");
	m_SavePassOnOff = (savePass == nullptr) || (atoi(savePass) != 0);
#else
	HKEY hKey;
	DWORD dwDisp;
	DWORD dwSize;
	if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisp))
	{
		//==============
		this->totalSavedAcc = 0;
		char zKey[50];
		for (int i = 0; i < MAX_ACCOUNT_SAVE; i++) {
			if (i) {
				sprintf(zKey, "ID_%d", i + 1);
			}
			else {
				sprintf(zKey, "ID");
			}
			dwSize = 11;
			if (RegQueryValueEx(hKey, zKey, 0, NULL, (LPBYTE)this->saved_acc[this->totalSavedAcc].ID, &dwSize) != ERROR_SUCCESS ||
				dwSize < 1)
			{
				ZeroMemory(this->saved_acc[this->totalSavedAcc].ID, sizeof(this->saved_acc[this->totalSavedAcc].ID));
				continue;
			}
			if (i) {
				sprintf(zKey, "PW_%d", i + 1);
			}
			else {
				sprintf(zKey, "PW");
			}
			dwSize = 11;
			if (RegQueryValueEx(hKey, zKey, 0, NULL, (LPBYTE)this->saved_acc[this->totalSavedAcc].PW, &dwSize) != ERROR_SUCCESS ||
				dwSize < 1)
			{
				ZeroMemory(this->saved_acc[this->totalSavedAcc].PW, sizeof(this->saved_acc[this->totalSavedAcc].PW));
				continue;
			}
			this->saved_acc[this->totalSavedAcc].index = i;
			this->totalSavedAcc++;
		}
		dwSize = sizeof(int);
		if (RegQueryValueEx(hKey, "SavePass", 0, NULL, (LPBYTE)& m_SavePassOnOff, &dwSize) != ERROR_SUCCESS)
		{
			m_SavePassOnOff = false;
		}
	}

	m_SavePassOnOff = true;
#endif
}

void CB_AutoLogin::SaveData(char* szID, char* szPass)
{
#if defined(__ANDROID__) || defined(MU_IOS)
	// Rewritten whole: the account just logged in becomes slot 0, everything
	// already known shifts down behind it (skipping a duplicate of szID), capped
	// at MAX_ACCOUNT_SAVE. Simpler than patching individual keys in place, and it
	// means a stale slot can never survive by just not being touched.
	FILE* f = fopen(kAutoLoginConfigPath, "w");
	if (f != nullptr)
	{
		fprintf(f, "ID=%s\n", szID);
		fprintf(f, "PW=%s\n", szPass);

		int successCount = 1;
		for (int i = 0; i < this->totalSavedAcc && successCount < MAX_ACCOUNT_SAVE; i++)
		{
			if (strcmp(szID, this->saved_acc[i].ID) == 0) //duplicate account
			{
				continue;
			}
			fprintf(f, "ID_%d=%s\n", successCount + 1, this->saved_acc[i].ID);
			fprintf(f, "PW_%d=%s\n", successCount + 1, this->saved_acc[i].PW);
			successCount++;
		}

		fprintf(f, "SavePass=%d\n", m_SavePassOnOff ? 1 : 0);
		fclose(f);
	}

	// So the account just used shows at the top of the list immediately, in the
	// same session, instead of only after the login window is recreated.
	this->ReadConfigs();
#else
	leaf::CRegKey regkey;
	regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config");
	char zKey[50];
	int i;
	if (this->totalSavedAcc > 0) {
		//clear all for sure, infact we can clear the rest for performance
		for (i = 1; i < MAX_ACCOUNT_SAVE; i++) {
			sprintf(zKey, "ID_%d", i + 1);
			regkey.WriteString(zKey, "");
			sprintf(zKey, "PW_%d", i + 1);
			regkey.WriteString(zKey, "");
		}
		//save new data
		int successCount = 1;
		for (i = 0; i < this->totalSavedAcc; i++) {
			if (successCount >= MAX_ACCOUNT_SAVE) {
				break;
			}
			if (strcmp(szID, this->saved_acc[i].ID) == 0) { //duplicate account
				continue;
			}
			sprintf(zKey, "ID_%d", successCount + 1);
			regkey.WriteString(zKey, this->saved_acc[i].ID);
			sprintf(zKey, "PW_%d", successCount + 1);
			regkey.WriteString(zKey, this->saved_acc[i].PW);
			successCount++;
		}
	}
	regkey.WriteString("ID", szID);
	regkey.WriteString("PW", szPass);
	regkey.WriteDword("SavePass", m_SavePassOnOff ? 1 : 0);
#endif
}
#endif