#pragma once
#include "Protocol.h"
#if(CB_AUTOLOGINWIN)
#define MAX_ACCOUNT_SAVE					5
class CB_AutoLogin
{
public:
	struct AUTOLOGIN_ACCOUNT
	{
		char ID[11];
		char PW[11];
		int	 index;
	};
	CB_AutoLogin();
	~CB_AutoLogin();
	void SaveData(char* szID, char* szPass);
	void DrawInfo(int XPos, int YPos);
	void ReadConfigs();
	AUTOLOGIN_ACCOUNT	saved_acc[MAX_ACCOUNT_SAVE];
	int	 totalSavedAcc;
	void SetShowListAccount(bool show);
	void SetSelectedAccount(int Index);
	void RemoveAccount(int Index);

	// Geometry only, matching DrawInfo's own layout, so CLoginWin::FocusInputAt can
	// tell whether a tap belongs to the checkbox / arrow / saved-account list before
	// deciding whether to give the ID or password box keyboard focus instead. Must
	// not call CheckMouseIn/DrawButtonGUI/RenderCheckBoxMini - those read the live
	// mouse button state and would double-fire the click DrawInfo is about to handle
	// this same frame.
	bool HitsControlArea(int XPos, int YPos, float uiX, float uiY) const;

	bool showListAccount;
	DWORD TickCount;
	int  selectedAccount;
};

extern CB_AutoLogin* gCB_AutoLogin;
#endif