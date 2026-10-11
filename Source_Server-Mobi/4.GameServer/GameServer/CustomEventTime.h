
#include "Protocol.h"
#define MAX_EVENTTIME  42


struct CUSTOM_EVENTTIME_DATA
{
	int index;
	char NameEvent[30];
	char DesString[90];
	int NumberGate;
	int time;
};

//**********************************************//
//********** GameServer -> Cliente    **********//
//**********************************************//

struct PMSG_CUSTOM_EVENTTIME_SEND
{
	PSWMSG_HEAD header;
	int MaxList;
	int count;
};

//**********************************************//
//********** Cliente -> GameServer    **********//
//**********************************************//

struct PMSG_CUSTOM_EVENTTIME_RECV
{
	PSBMSG_HEAD header; // C1:BF:51
	BYTE Page;
};

struct BCUSTOM_EVENTTIME_DATA
{
	char NameEvent[60];
	int* TimeEvent;
	void Clear() // OK
	{
		memset(NameEvent, 0, sizeof(NameEvent));
		TimeEvent = 0;
	}
};
struct DATA_CUSTOMEVENTTIME
{
	int NumberEvent;
	char NameEvent[30];
	char DesString[90];
	int NumberGate;
};
// ---
class CCustomEventTime
{
public:
	CCustomEventTime();
	virtual ~CCustomEventTime();
	void GCReqEventTime(int Index, PMSG_CUSTOM_EVENTTIME_RECV* pMsg);
	// ---
	void CCustomEventTime::LoadData();
	//int CountTime;
	//BCUSTOM_EVENTTIME_DATA DataEventTime[255];
	bool LoadDataTime;
	std::vector <BCUSTOM_EVENTTIME_DATA> DataEventTime;
	void CCustomEventTime::AddDataEventsTime(char* NameEvent, int* Time);

	void LoadFileXML(char* FilePath);
	// A schedule entry using this gate that is an invasion running right now:
	// where it spawned (CInvasionManager::GetSpawnPosition).
	bool FindInvasionSpawnByGate(int gate,int* map,int* x,int* y);
	// True when an event listed with this gate is running right now (its
	// countdown reads 0, "Online" in the client's Event Schedule window).
	bool IsGateEventOnline(int gate);
	// One row of the Event Schedule window (the index the client was sent):
	// true when that event is running, with its gate and - for a running
	// invasion - where its monster spawned (map -1 = use the gate).
	bool GetRowTeleport(int row,int* gate,int* map,int* x,int* y);
	std::vector <DATA_CUSTOMEVENTTIME> m_SendClientDataEventTime;
	int m_Enable;
private:
	int m_count;
	// ---
	//CUSTOM_EVENTTIME_DATA r_Data[MAX_EVENTTIME];


};
extern CCustomEventTime gCustomEventTime;
// ---
