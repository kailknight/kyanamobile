// ItemOptionManager.cpp: implementation of the CItemOptionManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ItemOptionRate.h"
#include "CustomWing.h"
#include "ItemManager.h"
#include "MemScript.h"
#include "PentagramSystem.h"
#include "RandomManager.h"
#include "LuckyItem.h"
#include "ServerInfo.h"
#include "SetItemType.h"
#include "SocketItemType.h"
#include "Util.h"

CItemOptionRate gItemOptionRate;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CItemOptionRate::CItemOptionRate() // OK
{
	this->m_ItemOption0RateInfo.clear();

	this->m_ItemOption1RateInfo.clear();

	this->m_ItemOption2RateInfo.clear();

	this->m_ItemOption3RateInfo.clear();

	this->m_ItemOption4RateInfo.clear();

	this->m_ItemOption5RateInfo.clear();

	this->m_ItemOption6RateInfo.clear();
}

CItemOptionRate::~CItemOptionRate() // OK
{

}

void CItemOptionRate::Load(char* path) // OK
{
	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	this->m_ItemOption0RateInfo.clear();

	this->m_ItemOption1RateInfo.clear();

	this->m_ItemOption2RateInfo.clear();

	this->m_ItemOption3RateInfo.clear();

	this->m_ItemOption4RateInfo.clear();

	this->m_ItemOption5RateInfo.clear();

	this->m_ItemOption6RateInfo.clear();

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			int section = lpMemScript->GetNumber();

			while(true)
			{
				if(section == 0)
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					ITEM_OPTION0_RATE_INFO info;

					info.Index = lpMemScript->GetNumber();

					for(int n=0;n < MAX_ITEM_OPTION0_RATE;n++)
					{
						info.Rate[n] = lpMemScript->GetAsNumber();
					}

					this->m_ItemOption0RateInfo.insert(std::pair<int,ITEM_OPTION0_RATE_INFO>(info.Index,info));
				}
				else if(section == 1)
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					ITEM_OPTION1_RATE_INFO info;

					info.Index = lpMemScript->GetNumber();

					for(int n=0;n < MAX_ITEM_OPTION1_RATE;n++)
					{
						info.Rate[n] = lpMemScript->GetAsNumber();
					}

					this->m_ItemOption1RateInfo.insert(std::pair<int,ITEM_OPTION1_RATE_INFO>(info.Index,info));
				}
				else if(section == 2)
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					ITEM_OPTION2_RATE_INFO info;

					info.Index = lpMemScript->GetNumber();

					for(int n=0;n < MAX_ITEM_OPTION2_RATE;n++)
					{
						info.Rate[n] = lpMemScript->GetAsNumber();
					}

					this->m_ItemOption2RateInfo.insert(std::pair<int,ITEM_OPTION2_RATE_INFO>(info.Index,info));
				}
				else if(section == 3)
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					ITEM_OPTION3_RATE_INFO info;

					info.Index = lpMemScript->GetNumber();

					for(int n=0;n < MAX_ITEM_OPTION3_RATE;n++)
					{
						info.Rate[n] = lpMemScript->GetAsNumber();
					}

					this->m_ItemOption3RateInfo.insert(std::pair<int,ITEM_OPTION3_RATE_INFO>(info.Index,info));
				}
				else if(section == 4)
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					ITEM_OPTION4_RATE_INFO info;

					info.Index = lpMemScript->GetNumber();

					for(int n=0;n < MAX_ITEM_OPTION4_RATE;n++)
					{
						info.Rate[n] = lpMemScript->GetAsNumber();
					}

					this->m_ItemOption4RateInfo.insert(std::pair<int,ITEM_OPTION4_RATE_INFO>(info.Index,info));
				}
				else if(section == 5)
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					ITEM_OPTION5_RATE_INFO info;

					info.Index = lpMemScript->GetNumber();

					for(int n=0;n < MAX_ITEM_OPTION5_RATE;n++)
					{
						info.Rate[n] = lpMemScript->GetAsNumber();
					}

					this->m_ItemOption5RateInfo.insert(std::pair<int,ITEM_OPTION5_RATE_INFO>(info.Index,info));
				}
				else if(section == 6)
				{
					if(strcmp("end",lpMemScript->GetAsString()) == 0)
					{
						break;
					}

					ITEM_OPTION6_RATE_INFO info;

					info.Index = lpMemScript->GetNumber();

					for(int n=0;n < MAX_ITEM_OPTION6_RATE;n++)
					{
						info.Rate[n] = lpMemScript->GetAsNumber();
					}

					this->m_ItemOption6RateInfo.insert(std::pair<int,ITEM_OPTION6_RATE_INFO>(info.Index,info));
				}
				else
				{
					break;
				}
			}
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}

bool CItemOptionRate::GetItemOption0(int index,BYTE* option) // OK
{
	std::map<int,ITEM_OPTION0_RATE_INFO>::iterator it = this->m_ItemOption0RateInfo.find(index);

	if(it == this->m_ItemOption0RateInfo.end())
	{
		return 0;
	}

	CRandomManager RandomManager;

	for(int n=0;n < MAX_ITEM_OPTION0_RATE;n++)
	{
		RandomManager.AddElement(n,it->second.Rate[n]);
	}

	return RandomManager.GetRandomElement(option);
}

bool CItemOptionRate::GetItemOption1(int index,BYTE* option) // OK
{
	std::map<int,ITEM_OPTION1_RATE_INFO>::iterator it = this->m_ItemOption1RateInfo.find(index);

	if(it == this->m_ItemOption1RateInfo.end())
	{
		return 0;
	}

	CRandomManager RandomManager;

	for(int n=0;n < MAX_ITEM_OPTION1_RATE;n++)
	{
		RandomManager.AddElement(n,it->second.Rate[n]);
	}

	return RandomManager.GetRandomElement(option);
}

bool CItemOptionRate::GetItemOption2(int index,BYTE* option) // OK
{
	std::map<int,ITEM_OPTION2_RATE_INFO>::iterator it = this->m_ItemOption2RateInfo.find(index);

	if(it == this->m_ItemOption2RateInfo.end())
	{
		return 0;
	}

	CRandomManager RandomManager;

	for(int n = 0; n < MAX_ITEM_OPTION2_RATE ; n++)
	{
		RandomManager.AddElement(n,it->second.Rate[n]);
	}

	return RandomManager.GetRandomElement(option);
}

bool CItemOptionRate::GetItemOption3(int index,BYTE* option) // OK
{
	std::map<int,ITEM_OPTION3_RATE_INFO>::iterator it = this->m_ItemOption3RateInfo.find(index);

	if(it == this->m_ItemOption3RateInfo.end())
	{
		return 0;
	}

	CRandomManager RandomManager;

	for(int n=0;n < MAX_ITEM_OPTION3_RATE;n++)
	{
		RandomManager.AddElement(n,it->second.Rate[n]);
	}

	return RandomManager.GetRandomElement(option);
}

bool CItemOptionRate::GetItemOption4(int index,BYTE* option) // OK
{
	std::map<int,ITEM_OPTION4_RATE_INFO>::iterator it = this->m_ItemOption4RateInfo.find(index);

	if(it == this->m_ItemOption4RateInfo.end())
	{
		return 0;
	}

	CRandomManager RandomManager;

	for(int n=0;n < MAX_ITEM_OPTION4_RATE;n++)
	{
		RandomManager.AddElement(n,it->second.Rate[n]);
	}

	return RandomManager.GetRandomElement(option);
}

bool CItemOptionRate::GetItemOption5(int index,BYTE* option) // OK
{
	std::map<int,ITEM_OPTION5_RATE_INFO>::iterator it = this->m_ItemOption5RateInfo.find(index);

	if(it == this->m_ItemOption5RateInfo.end())
	{
		return 0;
	}

	CRandomManager RandomManager;

	for(int n=0;n < MAX_ITEM_OPTION5_RATE;n++)
	{
		RandomManager.AddElement(n,it->second.Rate[n]);
	}

	return RandomManager.GetRandomElement(option);
}

bool CItemOptionRate::GetItemOption6(int index,BYTE* option) // OK
{
	std::map<int,ITEM_OPTION6_RATE_INFO>::iterator it = this->m_ItemOption6RateInfo.find(index);

	if(it == this->m_ItemOption6RateInfo.end())
	{
		return 0;
	}

	CRandomManager RandomManager;

	for(int n=0;n < MAX_ITEM_OPTION6_RATE;n++)
	{
		RandomManager.AddElement(n,it->second.Rate[n]);
	}

	return RandomManager.GetRandomElement(option);
}

bool CItemOptionRate::MakeNewOption(int ItemIndex,int value,BYTE* option) // OK
{
	(*option) = 0;

	int count = 0;

	int MaxNewOption = MAX_EXC_OPTION;

	if((ItemIndex >= GET_ITEM(12,3) && ItemIndex <= GET_ITEM(12,6)) || ItemIndex == GET_ITEM(12,42) || ItemIndex == GET_ITEM(12,49) || ItemIndex == GET_ITEM(13,30)) // 2sd Wings
	{
		MaxNewOption = ((ItemIndex==GET_ITEM(13,30))?4:3);
	}

	if((ItemIndex >= GET_ITEM(12,36) && ItemIndex <= GET_ITEM(12,40)) || ItemIndex == GET_ITEM(12,43) || ItemIndex == GET_ITEM(12,50)) // 3rd Wings
	{
		MaxNewOption = 4;
	}

	if(ItemIndex >= GET_ITEM(12,262) && ItemIndex <= GET_ITEM(12,265)) // Monster Wings
	{
		MaxNewOption = 2;
	}

	if(ItemIndex == GET_ITEM(12,266)) // Wings of Conqueror
	{
		MaxNewOption = 3;
	}

	if(ItemIndex == GET_ITEM(12,267)) // Wings of Angel and Devil
	{
		MaxNewOption = 3;
	}

	if(gCustomWing.CheckCustomWingByItem(ItemIndex) != 0)
	{
		MaxNewOption = 4;
	}

	if(ItemIndex == GET_ITEM(13,3)) // Dinorant
	{
		MaxNewOption = 3;
	}

	if(ItemIndex == GET_ITEM(13,37)) // Fenrir
	{
		MaxNewOption = 3;
	}

	while(true)
	{
		if(count >= value || count >= MaxNewOption)
		{
			break;
		}

		int value = 1 << (GetLargeRand()%MaxNewOption);

		if(((*option) & value) == 0)
		{
			(*option) |= value;
			count++;
		}
	}

	return 1;
}

// Map drops with the CustomConfig.ini exc weights (MapExcArmor_xx / MapExcWeapon_xx / MapExcAccessory_xx, one weight per
// excellent option 0-5, 0 = that option never drops), chosen by the item's slot. Anything else (wings, pets, items without a slot) keeps the plain random MakeNewOption.
bool CItemOptionRate::MakeNewOptionWeighted(int ItemIndex,int value,BYTE* option)
{
	const BYTE* weight = 0;

	int slot = gItemManager.GetItemSlotByIndex(ItemIndex);

	if(slot >= 2 && slot <= 6){weight = gServerInfo.m_MapExcArmor;}
	else if(slot >= 0 && slot <= 1){weight = gServerInfo.m_MapExcWeapon;}
	else if(slot >= 9 && slot <= 10){weight = gServerInfo.m_MapExcAccessory;}

	if(gServerInfo.m_MapExcWeightEnable == 0 || weight == 0)
	{
		return this->MakeNewOption(ItemIndex,value,option);
	}

	(*option) = 0;

	for(int count=0;count < value && count < MAX_EXC_OPTION;count++)
	{
		CRandomManager RandomManager;

		for(int n=0;n < MAX_EXC_OPTION;n++)
		{
			if(weight[n] > 0 && ((*option) & (1 << n)) == 0)
			{
				RandomManager.AddElement(1 << n,weight[n]);
			}
		}

		int bit = 0;

		if(RandomManager.GetRandomElement(&bit) == 0)
		{
			break;
		}

		(*option) |= (BYTE)bit;
	}

	return 1;
}

bool CItemOptionRate::MakeSetOption(int ItemIndex,int value,BYTE* option) // OK
{
	// A Lucky item is not an ancient: no tier, so no ancient name or set bonus. Its set option is the stat bonus alone
	// (value 1 = +5, 2 = +10, 3 = +15 of the stat the item's SetItemType row names, e.g. Stamina +10).
	if(value > 0 && gLuckyItem.IsLuckyItemIndex(ItemIndex) != 0 && gSetItemType.CheckSetItemType(ItemIndex) != 0)
	{
		(*option) = (BYTE)(4*((value > 3) ? 3 : value));
		return 1;
	}

	if(gSetItemType.CheckSetItemType(ItemIndex) == 0 || value == 0)
	{
		(*option) = 0;
		return 0;
	}

	(*option) = 0;

	for(int n=0;n < MAX_SET_ITEM_OPTION_INDEX;n++)
	{
		if(gSetItemType.GetSetItemOptionIndex(ItemIndex,n) > 0 && n == (value-1))
		{
			(*option) = (((*option)==0)?(n+1):(((GetLargeRand()%2)==0)?(n+1):(*option)));
		}
	}

	// The item has no tier for this value (e.g. value 2 = tier 2 on an item that only has tier 1): use the tier it does have, so the
	// +5 / +10 stat option asked for (4*value) still spawns instead of the item coming out with no ancient option at all.
	if((*option) == 0)
	{
		for(int n=0;n < MAX_SET_ITEM_OPTION_INDEX;n++)
		{
			if(gSetItemType.GetSetItemOptionIndex(ItemIndex,n) > 0)
			{
				(*option) = (BYTE)(n+1);
				break;
			}
		}
	}

	(*option) |= (((*option)>0)?(4*value):0);

	return 1;
}

bool CItemOptionRate::MakeSocketOption(int ItemIndex,int value,BYTE* option) // OK
{
	if((gSocketItemType.CheckSocketItemType(ItemIndex) == 0 && gPentagramSystem.IsPentagramItem(ItemIndex) == 0) || value == 0 || value == 0xFF)
	{
		memset(option,0xFF,MAX_SOCKET_OPTION);
		return 0;
	}

	if(gSocketItemType.CheckSocketItemType(ItemIndex) == 0)
	{
		for(int n=0;n < ((value>MAX_SOCKET_OPTION)?MAX_SOCKET_OPTION:value);n++)
		{
			option[n] = 0xFE;
		}
	}
	else
	{
		for(int n=0;n < ((value>gSocketItemType.GetSocketItemMaxSocket(ItemIndex))?gSocketItemType.GetSocketItemMaxSocket(ItemIndex):value);n++)
		{
			option[n] = 0xFE;
		}
	}

	return 1;
}

ITEM_OPTION0_RATE_INFO* CItemOptionRate::GetRawOption0(int index) // OK
{
	std::map<int,ITEM_OPTION0_RATE_INFO>::iterator it = this->m_ItemOption0RateInfo.find(index);

	if(it == this->m_ItemOption0RateInfo.end())
	{
		return 0;
	}

	return &it->second;
}

ITEM_OPTION1_RATE_INFO* CItemOptionRate::GetRawOption1(int index) // OK
{
	std::map<int,ITEM_OPTION1_RATE_INFO>::iterator it = this->m_ItemOption1RateInfo.find(index);

	if(it == this->m_ItemOption1RateInfo.end())
	{
		return 0;
	}

	return &it->second;
}

ITEM_OPTION2_RATE_INFO* CItemOptionRate::GetRawOption2(int index) // OK
{
	std::map<int,ITEM_OPTION2_RATE_INFO>::iterator it = this->m_ItemOption2RateInfo.find(index);

	if(it == this->m_ItemOption2RateInfo.end())
	{
		return 0;
	}

	return &it->second;
}

ITEM_OPTION3_RATE_INFO* CItemOptionRate::GetRawOption3(int index) // OK
{
	std::map<int,ITEM_OPTION3_RATE_INFO>::iterator it = this->m_ItemOption3RateInfo.find(index);

	if(it == this->m_ItemOption3RateInfo.end())
	{
		return 0;
	}

	return &it->second;
}

ITEM_OPTION4_RATE_INFO* CItemOptionRate::GetRawOption4(int index) // OK
{
	std::map<int,ITEM_OPTION4_RATE_INFO>::iterator it = this->m_ItemOption4RateInfo.find(index);

	if(it == this->m_ItemOption4RateInfo.end())
	{
		return 0;
	}

	return &it->second;
}

ITEM_OPTION5_RATE_INFO* CItemOptionRate::GetRawOption5(int index) // OK
{
	std::map<int,ITEM_OPTION5_RATE_INFO>::iterator it = this->m_ItemOption5RateInfo.find(index);

	if(it == this->m_ItemOption5RateInfo.end())
	{
		return 0;
	}

	return &it->second;
}

ITEM_OPTION6_RATE_INFO* CItemOptionRate::GetRawOption6(int index) // OK
{
	std::map<int,ITEM_OPTION6_RATE_INFO>::iterator it = this->m_ItemOption6RateInfo.find(index);

	if(it == this->m_ItemOption6RateInfo.end())
	{
		return 0;
	}

	return &it->second;
}
