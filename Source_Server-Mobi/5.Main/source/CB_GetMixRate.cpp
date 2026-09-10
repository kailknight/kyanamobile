#include "StdAfx.h"
#include "CB_GetMixRate.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"
#include "MixMgr.h"

#if(CB_GETMIXRATE)
CB_GetMixRate* gCB_GetMixRate;

CB_GetMixRate::CB_GetMixRate()
{
	this->RealMix = -1;
	this->RealMixItemTotal = -1;
	g_MixRecipeMgr.RealRate = -1;
	g_MixRecipeMgr.RealZen = -1;
}

CB_GetMixRate::~CB_GetMixRate()
{
}

void CB_GetMixRate::SetInfoMixID(int MixID, int ItemTotal)
{
	if (this->RealMix != -1 && MixID == 0)
	{
		this->RealMix = -1;
		this->RealMixItemTotal = -1;
		g_MixRecipeMgr.RealRate = -1;
		g_MixRecipeMgr.RealZen = -1;
	}

	// A quantity-only change (e.g. adding a second Jewel of Chaos to a slot
	// the recipe already recognised) keeps the same MixID, so the MixID
	// check alone never re-asks the server - the rate just sits stale until
	// something forces a fresh request (closing/reopening the box, or an
	// admin's Reload ChaosMix pushing every open box's remembered mix).
	// Re-ask on either the recipe or the item count changing.
	if (MixID != 0 && (MixID != this->RealMix || ItemTotal != this->RealMixItemTotal))
	{
		//gInterface.DrawMessage(1, "SetInfoMixID() %d", MixID);
		this->RealMix = MixID;
		this->RealMixItemTotal = ItemTotal;
		this->CGSendMixInfo();
		g_MixRecipeMgr.RealRate = -1;
		g_MixRecipeMgr.RealZen = -1;
	}

	//gInterface.DrawMessage(1, "SetInfoMixID %d %d", MixID, g_MixRecipeMgr.GetMixInventoryType());


}
void CB_GetMixRate::CGSendMixInfo()
{
	PMSG_CHAOX_MIX_GET_RATE_SEND pRequest;

	pRequest.header.set(0x88, sizeof(pRequest));
	pRequest.MixID = this->RealMix;
	pRequest.MixInfo = g_MixRecipeMgr.GetMixSubType();
	DataSend((LPBYTE)& pRequest, pRequest.header.size);
}
void CB_GetMixRate::GCRecvMixInfo(BYTE* lpMsg, int size)
{
	if (sizeof(PMSG_CHAOX_MIX_GET_RATE_RECV) != size)
	{
		return;
	}
	PMSG_CHAOX_MIX_GET_RATE_RECV* Data = (PMSG_CHAOX_MIX_GET_RATE_RECV*)lpMsg;
	g_MixRecipeMgr.RealRate = Data->Rate;
	g_MixRecipeMgr.RealZen = Data->Zen;
}

#endif