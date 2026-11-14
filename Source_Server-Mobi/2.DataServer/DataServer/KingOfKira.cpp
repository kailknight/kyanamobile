// KingOfKira.cpp - see KingOfKira.h and Working Project\MD Files\KingOfKira-DESIGN.md.
#include "stdafx.h"
#include "KingOfKira.h"
#include "DataServerProtocol.h"
#include "QueryManager.h"
#include "ServerManager.h"
#include "SocketManager.h"
#include "Util.h"

CKingOfKira gKingOfKira;

// Names come off the network and are formatted into SQL: keep plain printable ASCII, no quote, percent or backslash.
static void KokClean(const char* in,int inSize,char* out,int outSize)
{
	int o = 0;

	for(int n=0;n < inSize && in[n] != 0 && o < outSize-1;n++)
	{
		unsigned char c = (unsigned char)in[n];

		if(c >= 0x20 && c < 0x7F && c != '\'' && c != '%' && c != 0x5C)
		{
			out[o++] = (char)c;
		}
	}

	out[o] = 0;
}

void CKingOfKira::OnRelay(BYTE* lpMsg,int size,int index) // OK
{
	if(size < (int)sizeof(SDHP_KOK_RELAY))
	{
		return;
	}

	SDHP_KOK_RELAY* p = (SDHP_KOK_RELAY*)lpMsg;

	if(p->Len > KOK_RELAY_DATA)
	{
		return;
	}

	p->FromServer = gServerManager[index].m_ServerCode;

	if(p->Target == KOK_TARGET_ALL)
	{
		for(int n=0;n < MAX_SERVER;n++)
		{
			if(gServerManager[n].m_state == SERVER_ONLINE)
			{
				gSocketManager.DataSend(n,lpMsg,sizeof(SDHP_KOK_RELAY));
			}
		}
	}
	else
	{
		CServerManager* lpServer = FindServerByCode(p->Target);

		if(lpServer != 0)
		{
			gSocketManager.DataSend(lpServer->m_index,lpMsg,sizeof(SDHP_KOK_RELAY));
		}
	}
}

void CKingOfKira::OnSave(BYTE* lpMsg,int size,int index) // OK
{
	if(size < (int)sizeof(SDHP_KOK_SAVE))
	{
		return;
	}

	SDHP_KOK_SAVE* p = (SDHP_KOK_SAVE*)lpMsg;

	char winner[10],loser[10];

	KokClean(p->Winner,sizeof(p->Winner),winner,sizeof(winner));
	KokClean(p->Loser,sizeof(p->Loser),loser,sizeof(loser));

	if(winner[0] == 0)
	{
		return;
	}

	gQueryManager.ExecQuery("INSERT INTO KingOfKira (WinnerGuild,LoserGuild,WinnerPoints,LoserPoints,WinnerKey) VALUES ('%s','%s',%d,%d,%d)",winner,loser,(int)p->WinnerPoints,(int)p->LoserPoints,(int)p->WinnerKey);
	gQueryManager.Close();

	this->SendWinner(0,true);
}

void CKingOfKira::OnLoad(BYTE* lpMsg,int size,int index) // OK
{
	this->SendWinner(index,false);
}

void CKingOfKira::SendWinner(int serverIndex,bool all) // OK
{
	SDHP_KOK_WINNER pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	pMsg.type = 0xC1;
	pMsg.size = sizeof(pMsg);
	pMsg.head = KOK_HEAD;
	pMsg.subh = KOK_SUB_WINNER;

	if(gQueryManager.ExecQuery("SELECT TOP 1 WinnerGuild,WinnerKey FROM KingOfKira ORDER BY Id DESC") != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		gQueryManager.GetAsString("WinnerGuild",pMsg.Guild,sizeof(pMsg.Guild));
		pMsg.Key = gQueryManager.GetAsInteger("WinnerKey");
		gQueryManager.Close();

		pMsg.Has = 1;

		char name[10];

		KokClean(pMsg.Guild,sizeof(pMsg.Guild),name,sizeof(name));

		memset(pMsg.Mark,0xFF,sizeof(pMsg.Mark));

		if(gQueryManager.ExecQuery("SELECT G_Mark FROM Guild WHERE G_Name='%s'",name) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
		{
			gQueryManager.GetAsBinary("G_Mark",pMsg.Mark,sizeof(pMsg.Mark));
		}

		gQueryManager.Close();
	}
	else
	{
		gQueryManager.Close();
	}

	if(all == false)
	{
		gSocketManager.DataSend(serverIndex,(BYTE*)&pMsg,sizeof(pMsg));
		return;
	}

	for(int n=0;n < MAX_SERVER;n++)
	{
		if(gServerManager[n].m_state == SERVER_ONLINE)
		{
			gSocketManager.DataSend(n,(BYTE*)&pMsg,sizeof(pMsg));
		}
	}
}
