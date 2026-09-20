// SpinClaim.h: interface for the CSpinClaim class.
//
// Persistence for the Spin Wheel claim box - prizes a player had no inventory
// room for when they won them, held per CHARACTER until claimed.
//
// Shape follows EventInventory (a fixed per-character blob of 16-byte item
// records) and the wire/correlation shape follows RedeemCode (its own 0xD9
// subcodes, aIndex echoed back so the GameServer can re-validate the account
// before applying a reply).
//
// IMPORTANT: these structs are a byte-for-byte duplicate of the same structs in
// the GameServer's CustomVongQuay.h. This codebase deliberately keeps
// GameServer<->DataServer wire structs duplicated rather than shared between the
// two projects, so ANY field change must be mirrored on both sides in the same
// commit - and both binaries must be deployed together. BUILD.bat does not
// collect DataServer.exe, so a one-sided deploy silently misaligns the wire and
// surfaces as corrupted items rather than a clean failure.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DataServerProtocol.h"

// Must equal SPIN_CLAIM_SIZE in the GameServer's CustomVongQuay.h.
#define SPIN_CLAIM_SIZE 8

#pragma pack(push,1)

//**********************************************//
//********** GameServer -> DataServer **********//
//**********************************************//

// C1:D9:32 - load request
struct SDHP_SPINCLAIM_RECV
{
	PSBMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
};

// C2:D9:33 - save
struct SDHP_SPINCLAIM_SAVE_RECV
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SpinClaim[SPIN_CLAIM_SIZE][16];
};

//**********************************************//
//********** DataServer -> GameServer **********//
//**********************************************//

// C2:D9:32 - load reply
struct SDHP_SPINCLAIM_SEND
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SpinClaim[SPIN_CLAIM_SIZE][16];
};

#pragma pack(pop)

// Stride pins. If either of these ever fires, the wire has drifted between the
// two projects - fix the struct, do not raise the number.
static_assert(sizeof(SDHP_SPINCLAIM_RECV) == 28, "SDHP_SPINCLAIM_RECV must stay 28 bytes");
static_assert(sizeof(SDHP_SPINCLAIM_SAVE_RECV) == 157, "SDHP_SPINCLAIM_SAVE_RECV must stay 157 bytes");
static_assert(sizeof(SDHP_SPINCLAIM_SEND) == 157, "SDHP_SPINCLAIM_SEND must stay 157 bytes");

//**********************************************//
//**********************************************//
//**********************************************//

class CSpinClaim
{
public:
	CSpinClaim();
	virtual ~CSpinClaim();
	void GDSpinClaimRecv(SDHP_SPINCLAIM_RECV* lpMsg,int index);
	void GDSpinClaimSaveRecv(SDHP_SPINCLAIM_SAVE_RECV* lpMsg);
};

extern CSpinClaim gSpinClaim;
