#include "stdafx.h"
#include "Other.h"
#include "Util.h"
Other	gOther;

extern std::string g_strSelectedML;

Other::Other()
{
	Other::Load();
}


Other::~Other()
{
}

void Other::Reload()
{
#if defined(__ANDROID__) || defined(MU_IOS)
	mu_ini::ForgetCachedIniFiles();
#endif
	Load();
}

void Other::Load()
{
	//==Check Local

	char NameFileLocal[MAX_PATH] = { 0 };
	wsprintf(NameFileLocal, "./Data/Custom/Text_%s.ini", g_strSelectedML.c_str());

	if (FileExist(NameFileLocal) == false)
	{
		strcpy(NameFileLocal, "./Data/Custom/Text_Eng.ini");
	}

	char GetFotText[55] = { 0 };
	for (int st = 0; st < 50; st++)
	{
		wsprintf(GetFotText, "Text%d", st);
		GetPrivateProfileStringA("TextVN_NAPGAME", GetFotText, "Null", TextVN_NAPGAME[st], sizeof(TextVN_NAPGAME[st]), NameFileLocal);
		GetPrivateProfileStringA("TextVN_CHOTROI", GetFotText, "Null", TextVN_CHOTROI[st], sizeof(TextVN_CHOTROI[st]), NameFileLocal);
		GetPrivateProfileStringA("TextVN_ArenaPvP", GetFotText, "Null", TextVN_ArenaPvP[st], sizeof(TextVN_ArenaPvP[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_VIPCHAR", GetFotText, "Null", Text_VIPCHAR[st], sizeof(Text_VIPCHAR[st]), NameFileLocal);

		GetPrivateProfileStringA("Text_Button", GetFotText, "Null", Text_Button[st], sizeof(Text_Button[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_Event", GetFotText, "Null", Text_Event[st], sizeof(Text_Event[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_InfoDW", GetFotText, "Null", Text_InfoDW[st], sizeof(Text_InfoDW[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_InfoDK", GetFotText, "Null", Text_InfoDK[st], sizeof(Text_InfoDK[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_InfoELF", GetFotText, "Null", Text_InfoELF[st], sizeof(Text_InfoELF[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_InfoMG", GetFotText, "Null", Text_InfoMG[st], sizeof(Text_InfoMG[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_InfoDL", GetFotText, "Null", Text_InfoDL[st], sizeof(Text_InfoDL[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_InfoSUM", GetFotText, "Null", Text_InfoSUM[st], sizeof(Text_InfoSUM[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_InfoRF", GetFotText, "Null", Text_InfoRF[st], sizeof(Text_InfoRF[st]), NameFileLocal);
	
		GetPrivateProfileStringA("Text_ChangeItem", GetFotText, "Null", Text_ChangeItem[st], sizeof(Text_ChangeItem[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_ChacracterInfo", GetFotText, "Null", Text_ChacracterInfo[st], sizeof(Text_ChacracterInfo[st]), NameFileLocal);
		GetPrivateProfileStringA("Text_HuyDong", GetFotText, "Null", Text_HuyDong[st], sizeof(Text_HuyDong[st]), NameFileLocal);
#if(CongHuongV2)
		GetPrivateProfileStringA("TextVN_CONGHUONG", GetFotText, "Null", TextVN_CongHuongTrangBi[st], sizeof(TextVN_CongHuongTrangBi[st]), NameFileLocal);
#endif

	}

	char GetFotText2[55] = { 0 };
	for (int st2 = 0; st2 < 150; st2++)
	{
		wsprintf(GetFotText2, "Text%d", st2);
		GetPrivateProfileStringA("Text_Chung", GetFotText2, "Null", Text_Chung[st2], sizeof(Text_Chung[st2]), NameFileLocal);
	}
#if defined(__ANDROID__) || defined(MU_IOS)
	{
		// How many flea market labels actually loaded, for mu_exit_trace.txt: a
		// window full of "Null" means this came out 0.
		int loaded = 0;
		for (int i = 0; i < 50; i++)
		{
			if (strcmp(TextVN_CHOTROI[i], "Null") != 0) loaded++;
		}
		MU_AppendExitTrace("text: %s loaded=%d market labels (of 46 defined)", NameFileLocal, loaded);
	}
#endif
	//Set Text Change Class
	//WindowClass.CreateText();
}