#include "stdafx.h"
#include "DSProtocol.h"
#include "GameMain.h"
#include "ServerDisplayer.h"
#include "Util.h"
#include "Message.h"
#include "User.h"
#include "Path.h"
#include "ServerInfo.h"
#include "CustomEventTime.h"
#include "InvasionManager.h"
#include "MemScript.h"
#include "Notice.h"
#include "InvasionManager.h"
#include "CustomArena.h"
CCustomEventTime gCustomEventTime;

CCustomEventTime::CCustomEventTime() // OK
{
	//gCustomEventTime.CountTime = 0;
	gCustomEventTime.DataEventTime.clear();
	this->LoadDataTime = 0;
	this->m_SendClientDataEventTime.clear();
	this->m_Enable = 0;
}


CCustomEventTime::~CCustomEventTime() // OK
{

}
void CCustomEventTime::LoadData()
{
	for (int i = 0; i < 30; i++)
	{
		//if (strlen(gInvasionManager.m_InvasionInfo[i].AlertMessage) < 1) continue;
		gCustomEventTime.AddDataEventsTime(gInvasionManager.m_InvasionInfo[i].AlertMessage, &gServerDisplayer.EventInvasion[i]);
	}
	for (int ii = 0; ii < 30; ii++)
	{
		//if (strlen(gCustomArena.GetArenaName(ii)) < 1) continue;
		gCustomEventTime.AddDataEventsTime(gCustomArena.GetArenaName(ii), &gServerDisplayer.EventCustomArena[ii]);
	}
}
void CCustomEventTime::AddDataEventsTime(char* NameEvent, int* Time)
{
	//if (this->CountTime > MAX_EVENTS_TIME) return;
	//memcpy(&this->DataEventTime[this->CountTime].NameEvent, NameEvent, sizeof(this->DataEventTime[this->CountTime].NameEvent));
	//this->DataEventTime[this->CountTime].time = Time;
	//this->CountTime++;
	BCUSTOM_EVENTTIME_DATA info;
	info.Clear();
	// bounded copy - the callers pass short string literals, so reading a full
	// sizeof(NameEvent) block off the end of one was never safe.
	if(NameEvent != 0)
	{
		strncpy_s(info.NameEvent, sizeof(info.NameEvent), NameEvent, _TRUNCATE);
	}
	info.TimeEvent = Time;
	gCustomEventTime.DataEventTime.push_back(info);
}
void CCustomEventTime::LoadFileXML(char* FilePath)
{
	pugi::xml_document file;
	pugi::xml_parse_result res = file.load_file(FilePath);
	if (res.status != pugi::status_ok) {
		ErrorMessageBox("File %s load fail. Error: %s", FilePath, res.description());
		return;
	}
	//--
	pugi::xml_node CustomEventTime = file.child("CustomEventTime");
	this->m_Enable = CustomEventTime.attribute("Enable").as_int();

	this->m_SendClientDataEventTime.clear();
	for (pugi::xml_node Event = CustomEventTime.child("Event"); Event; Event = Event.next_sibling())
	{
		DATA_CUSTOMEVENTTIME info;
		info.NumberEvent = Event.attribute("NumberEvent").as_int();
		strcpy_s(info.NameEvent, Event.attribute("NameEvent").as_string());
		strcpy_s(info.DesString, Event.attribute("DesString").as_string());
		info.NumberGate = Event.attribute("NumberGate").as_int();
		this->m_SendClientDataEventTime.push_back(info);
	}
}
int MaxPerPage = 20;
void CCustomEventTime::GCReqEventTime(int Index, PMSG_CUSTOM_EVENTTIME_RECV* lpMsg)
{

#if (GAMESERVER_CLIENTE_UPDATE >= 2)

	if (gServerInfo.m_CustomEventTimeSwitch == 0)
	{
		return;
	}

	if (gObjIsConnected(Index) == false)
	{
		return;
	}
	if (!this->m_Enable)
	{
		gNotice.GCNoticeSend(Index, eMessageBox, 0, 0, 0, 0, 0, "Chuc nang khong duoc su dung");//
		return;
	}
	int GetPage = lpMsg->Page;
	BYTE send[8192];

	PMSG_CUSTOM_EVENTTIME_SEND pMsg;

	pMsg.header.set(0xF3, 0xE8, 0);

	int size = sizeof(pMsg);

	pMsg.count = 0;
	pMsg.MaxList = gCustomEventTime.m_SendClientDataEventTime.size();

	CUSTOM_EVENTTIME_DATA info;

	for (int n = (GetPage * MaxPerPage); n < gCustomEventTime.m_SendClientDataEventTime.size(); n++)
	{
		if (pMsg.count >= MaxPerPage) break;
		info.index = n;
		if (gCustomEventTime.m_SendClientDataEventTime[n].NumberEvent >= 0 && gCustomEventTime.m_SendClientDataEventTime[n].NumberEvent < gCustomEventTime.DataEventTime.size())
		{
			info.time = *gCustomEventTime.DataEventTime[gCustomEventTime.m_SendClientDataEventTime[n].NumberEvent].TimeEvent;
		}
		else
		{
			info.time = -1;
		}

		info.NumberGate = gCustomEventTime.m_SendClientDataEventTime[n].NumberGate;
		memcpy(&info.NameEvent, gCustomEventTime.m_SendClientDataEventTime[n].NameEvent, sizeof(info.NameEvent));
		memcpy(&info.DesString, gCustomEventTime.m_SendClientDataEventTime[n].DesString, sizeof(info.DesString));
		if ((size + sizeof(info)) >= 8192) break;
		
		memcpy(&send[size], &info, sizeof(info));
		size += sizeof(info);
		pMsg.count++;
	}
	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);
	// ---
	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(Index, send, size);

#endif
	return;
}
bool CCustomEventTime::GetRowTeleport(int row,int* gate,int* map,int* x,int* y) // OK
{
	*map = -1;
	*x = -1;
	*y = -1;

	if(row < 0 || row >= (int)this->m_SendClientDataEventTime.size())
	{
		return 0;
	}

	const DATA_CUSTOMEVENTTIME& info = this->m_SendClientDataEventTime[row];

	if(info.NumberGate < 0 || info.NumberEvent < 0 || info.NumberEvent >= (int)this->DataEventTime.size())
	{
		return 0;
	}

	const int* time = this->DataEventTime[info.NumberEvent].TimeEvent;

	if(time == 0 || *time != 0)
	{
		return 0;
	}

	*gate = info.NumberGate;

	// NumberEvent 0-29 are the invasions: land on this one's own spawn, not
	// another invasion that happens to share the gate.
	if(info.NumberEvent < MAX_INVASION && gInvasionManager.GetSpawnPosition(info.NumberEvent,map,x,y) == 0)
	{
		*map = -1;
	}

	return 1;
}

bool CCustomEventTime::IsGateEventOnline(int gate) // OK
{
	for(int n=0;n < (int)this->m_SendClientDataEventTime.size();n++)
	{
		const DATA_CUSTOMEVENTTIME& info = this->m_SendClientDataEventTime[n];

		if(info.NumberGate != gate || info.NumberEvent < 0 || info.NumberEvent >= (int)this->DataEventTime.size())
		{
			continue;
		}

		const int* time = this->DataEventTime[info.NumberEvent].TimeEvent;

		if(time != 0 && *time == 0)
		{
			return 1;
		}
	}

	return 0;
}

bool CCustomEventTime::FindInvasionSpawnByGate(int gate,int* map,int* x,int* y) // OK
{
	// NumberEvent 0-29 are the invasions (LoadData adds them first, in order).
	for(int n=0;n < (int)this->m_SendClientDataEventTime.size();n++)
	{
		const DATA_CUSTOMEVENTTIME& info = this->m_SendClientDataEventTime[n];

		if(info.NumberGate != gate || info.NumberEvent < 0 || info.NumberEvent >= MAX_INVASION)
		{
			continue;
		}

		if(gInvasionManager.GetSpawnPosition(info.NumberEvent,map,x,y) != 0)
		{
			return 1;
		}
	}

	return 0;
}
