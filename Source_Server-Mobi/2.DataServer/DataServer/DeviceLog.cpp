// DeviceLog.cpp - see DeviceLog.h.
#include "StdAfx.h"
#include "DeviceLog.h"
#include "QueryManager.h"

CDeviceLog gDeviceLog;

// Every field here came over the network, and ExecQuery formats them straight
// into SQL. Keep only what an account name, character name, IP or hex id can
// contain; anything else (a quote above all) is dropped.
static void Clean(const char* in,int inSize,char* out,int outSize)
{
	int o = 0;

	for(int n=0;n < inSize && in[n] != 0 && o < outSize - 1;n++)
	{
		char c = in[n];

		if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == ':')
		{
			out[o++] = c;
		}
	}

	out[o] = 0;
}

void CDeviceLog::OnLogin(SDHP_DEVICE_LOGIN_RECV* lpMsg) // OK
{
	char account[11],ip[16],hwid[33];

	Clean(lpMsg->Account,sizeof(lpMsg->Account),account,sizeof(account));
	Clean(lpMsg->Ip,sizeof(lpMsg->Ip),ip,sizeof(ip));
	Clean(lpMsg->HardwareId,sizeof(lpMsg->HardwareId),hwid,sizeof(hwid));

	if(account[0] == 0)
	{
		return;
	}

	gQueryManager.ExecQuery("EXEC WZ_HardwareLogin '%s','%s','%s',%d",account,ip,hwid,(int)lpMsg->Platform);
	gQueryManager.Close();
}

void CDeviceLog::OnBan(SDHP_DEVICE_BAN_RECV* lpMsg) // OK
{
	char account[11],name[11],ip[16],hwid[33],by[11];

	Clean(lpMsg->Account,sizeof(lpMsg->Account),account,sizeof(account));
	Clean(lpMsg->Name,sizeof(lpMsg->Name),name,sizeof(name));
	Clean(lpMsg->Ip,sizeof(lpMsg->Ip),ip,sizeof(ip));
	Clean(lpMsg->HardwareId,sizeof(lpMsg->HardwareId),hwid,sizeof(hwid));
	Clean(lpMsg->BannedBy,sizeof(lpMsg->BannedBy),by,sizeof(by));

	if(hwid[0] == 0)
	{
		return;
	}

	gQueryManager.ExecQuery("EXEC WZ_HardwareBan '%s','%s','%s','%s',%d,'%s',%d",hwid,account,name,ip,(int)lpMsg->Platform,by,(int)(lpMsg->Banned != 0));
	gQueryManager.Close();
}
