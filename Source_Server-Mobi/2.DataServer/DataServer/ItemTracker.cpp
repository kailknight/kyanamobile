// ItemTracker.cpp - see ItemTracker.h.
#include "StdAfx.h"
#include "ItemTracker.h"
#include "QueryManager.h"

CItemTracker gItemTracker;

CItemTracker::CItemTracker() // OK
{
	this->m_enabled = 1;
	this->m_retentionDays = 30;
	this->m_lastPurge = 0;
}

void CItemTracker::Load() // OK
{
	this->m_enabled = GetPrivateProfileInt("DataServerInfo","ItemTrackerEnabled",1,".\DataServer.ini");
	this->m_retentionDays = GetPrivateProfileInt("DataServerInfo","ItemTrackRetentionDays",30,".\DataServer.ini");

	if(this->m_retentionDays < 0){this->m_retentionDays = 0;}
}

// Everything here came over the network and ExecQuery formats it straight into
// SQL. Names and item names are plain text; keep printable ASCII and drop the
// quote (the only character that can end a SQL string) and the percent sign
// (a format specifier to ExecQuery).
static void Clean(const char* in,int inSize,char* out,int outSize)
{
	int o = 0;

	for(int n=0;n < inSize && in[n] != 0 && o < outSize - 1;n++)
	{
		unsigned char c = (unsigned char)in[n];

		if(c >= 0x20 && c < 0x7F && c != '\'' && c != '%' && c != 0x5C)
		{
			out[o++] = (char)c;
		}
	}

	out[o] = 0;
}

void CItemTracker::OnItem(SDHP_ITEMTRACK_RECV* lpMsg,int size) // OK
{
	if(this->m_enabled == 0 || size < (int)sizeof(SDHP_ITEMTRACK_RECV) || lpMsg->header.size != sizeof(SDHP_ITEMTRACK_RECV))
	{
		return;
	}

	char action[17],name[12],source[25],map[25],item[41],details[57];

	Clean(lpMsg->Action,sizeof(lpMsg->Action),action,sizeof(action));
	Clean(lpMsg->Character,sizeof(lpMsg->Character),name,sizeof(name));
	Clean(lpMsg->Source,sizeof(lpMsg->Source),source,sizeof(source));
	Clean(lpMsg->Map,sizeof(lpMsg->Map),map,sizeof(map));
	Clean(lpMsg->ItemName,sizeof(lpMsg->ItemName),item,sizeof(item));
	Clean(lpMsg->Details,sizeof(lpMsg->Details),details,sizeof(details));

	if(action[0] == 0 || name[0] == 0)
	{
		return;
	}

	gQueryManager.ExecQuery("INSERT INTO ItemActionLog (Action,CharacterName,ItemIndex,ItemName,ItemSerial,Level,Skill,Luck,Opt,Excellent,SetOption,Harmony,OptionEx,Qty,Source,Details,MapName,ServerCode) VALUES ('%s','%s',%d,'%s',%I64d,%d,%d,%d,%d,%d,%d,%d,%d,%d,'%s','%s','%s',%d)",
		action,name,(int)lpMsg->ItemIndex,item,(__int64)lpMsg->Serial,(int)lpMsg->Level,(int)lpMsg->Skill,(int)lpMsg->Luck,(int)lpMsg->Option,(int)lpMsg->Excellent,(int)lpMsg->SetOption,(int)lpMsg->Harmony,(int)lpMsg->OptionEx,(int)lpMsg->Count,source,details,map,(int)lpMsg->ServerCode);
	gQueryManager.Close();

	this->Purge();
}

void CItemTracker::Purge() // OK
{
	if(this->m_retentionDays == 0 || (this->m_lastPurge != 0 && (GetTickCount() - this->m_lastPurge) < 3600000))
	{
		return;
	}

	this->m_lastPurge = GetTickCount();

	// 5000 rows a pass keeps the lock short; the next hour continues if a
	// backlog is left (first run after turning retention on).
	gQueryManager.ExecQuery("DELETE TOP (5000) FROM ItemActionLog WHERE ActionTime < DATEADD(day,-%d,GETDATE())",this->m_retentionDays);
	gQueryManager.Close();
}
