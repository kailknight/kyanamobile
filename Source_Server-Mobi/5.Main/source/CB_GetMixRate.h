#pragma once
#include "Protocol.h"
#if(CB_GETMIXRATE)
class CB_GetMixRate
{
public:
	struct PMSG_CHAOX_MIX_GET_RATE_SEND
	{
		PBMSG_HEAD header;
		int MixID;
		int MixInfo;
	};

	struct PMSG_CHAOX_MIX_GET_RATE_RECV
	{
		PBMSG_HEAD header; // C1:86
		int Rate;
		int Zen;
	};


	CB_GetMixRate();
	~CB_GetMixRate();
	void SetInfoMixID(int MixID, int ItemTotal = 0);
	void CGSendMixInfo();

	int RealMix;
	int RealMixItemTotal; // last-requested total item count in the mix grid - lets a quantity change re-ask even when the recognized recipe (MixID) doesn't change

	void GCRecvMixInfo(BYTE* lpMsg, int size);
};


extern CB_GetMixRate* gCB_GetMixRate;

#endif