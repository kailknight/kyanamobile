#pragma once
#include "Protocol.h"
#if(CB_CUSTOMMIXINFO)

class CB_InfoCustomMix
{

public:
	struct BCUSTOM_MIX_NGUYENLIEU
	{
		int  Index;
		int  ItemIndexMin;
		int  ItemIndexMax;
		int  Level;
		int  Skill;
		int  Luck;
		int  Op;
		int  Exl;
		int  Item380;
		int  Set;
		int  Harmory;
		int  Socket;
		int  Count;
		int  FailDelItem;
		//2
		//Index	ItemIndexMin	ItemIndexMax	Level   Skill   Luck	Op	Exl	Item380	Set	Harmory	Socket	Count	FailDelItem
	};
	struct PMSG_BCUSTOM_MIX_INFO
	{
		PSWMSG_HEAD header; // C2:F3:E2
		BYTE count;
		int  Index;
		int  MixMoney;
		int  MixRate;
		int  Name1;
		int  Name2;
		int  Name3;
		int  Des1;
		int  Des2;
		int  Des3;
		int  Advice1;
		int  Advice2;
		int  Advice3;

		// Mirrors PMSG_BCUSTOM_MIX_INFO in the GameServer's CustomMix.h. Must
		// stay the LAST field and stay in step with it - the ingredient rows
		// that follow are addressed as sizeof(*this) + n * sizeof(row).
		int  MaxTalismanOfLuck;
	};

	struct LOCAL_DATA_INFOCUSTOMMIX
	{
		int  Index;
		int  MixMoney;
		int  MixRate;
		int  Name1;
		int  Name2;
		int  Name3;
		int  Des1;
		int  Des2;
		int  Des3;
		int  Advice1;
		int  Advice2;
		int  Advice3;
		std::vector< BCUSTOM_MIX_NGUYENLIEU> m_DataItemCheck;
	};
	CB_InfoCustomMix();
	~CB_InfoCustomMix();
	void RecvInfo(PMSG_BCUSTOM_MIX_INFO* lpMsg);
	std::vector<LOCAL_DATA_INFOCUSTOMMIX> m_DataInfoCustomMix;

	std::vector<ITEM*> m_DataListItemMixInv;
	void AddItemToMixItemInventory(ITEM* pItem);
	int GetSourceName(int iItemNum, unicode::t_char* pszNameOut);

	int CheckCountMix;

	// The server's MaxTalismanOfLuck (CustomConfig.ini). Held on the class
	// rather than in m_DataInfoCustomMix because RecvInfo clears that vector
	// whenever no recipe matches, and the mix window states the cap either
	// way. -1 until the first packet arrives, which suppresses the notice
	// rather than printing a made-up number.
	int MaxTalismanOfLuck;
	bool CheckCustomMixOK();
	BOOL GetCurRecipeDesc(unicode::t_char* pszDescOut, int iDescLine);
	BOOL GetRecipeAdvice(unicode::t_char* pszDescOut, int iDescLine);
};

extern CB_InfoCustomMix* gCB_InfoCustomMix;

#endif