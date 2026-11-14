// KingOfKira.h - DataServer side of King of Kira: a relay between GameServers + the winner table.
#pragma once
#include "KingOfKiraProtocol.h"

class CKingOfKira
{
public:
	void OnRelay(BYTE* lpMsg,int size,int index);
	void OnSave(BYTE* lpMsg,int size,int index);
	void OnLoad(BYTE* lpMsg,int size,int index);
private:
	void SendWinner(int serverIndex,bool all);
};

extern CKingOfKira gKingOfKira;
