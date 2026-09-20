// SlotClaim.h: interface for the CSlotClaim class.
//
// Persistence for the Slot Machine reward box - winnings a player has not
// claimed yet, held per CHARACTER until they take them.
//
// Deliberately SEPARATE from SpinClaim rather than sharing its table, because the
// two hold different things. SpinClaim holds up to 8 individual items, one per
// slot; a slot machine pays N of a single item, so a win of 40 jewels would need
// 40 of its slots. A row here is therefore {item, level, COUNT} - one win is one
// row whatever its size. Widening SPINCLAIM_ROW instead would have misread every
// prize players are already holding on the wheel.
//
// Shape follows SpinClaim in every other respect: a fixed per-character blob of
// 16-byte records, its own 0xD9 subcodes, aIndex echoed back so the GameServer can
// re-validate the account before applying a reply.
//
// IMPORTANT: these structs are a byte-for-byte duplicate of the same structs in
// the GameServer's SlotMachine.h. This codebase deliberately keeps
// GameServer<->DataServer wire structs duplicated rather than shared between the
// two projects, so ANY field change must be mirrored on both sides in the same
// commit - and both binaries must be deployed together. BUILD.bat does not
// collect DataServer.exe, so a one-sided deploy silently misaligns the wire and
// surfaces as corrupted or vanished prizes rather than a clean failure.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DataServerProtocol.h"

// Must equal SLOT_CLAIM_SIZE in the GameServer's SlotMachine.h.
#define SLOT_CLAIM_SIZE 8

#pragma pack(push,1)

// One held win. 16 bytes to match the stride every other per-character item blob
// in this codebase uses, which leaves spare room rather than needing a new
// stride if a field is ever added.
//
// ItemIndex == 0xFFFF means the row is empty. That is what a 0xFF-filled blob
// decodes to, which is both what a character with no row yet gets and what the
// GameServer writes for a cleared row - so "no data" and "empty box" are the same
// bytes and neither needs special-casing.
struct SLOTCLAIM_ROW_DATA
{
	WORD  ItemIndex;
	BYTE  Level;
	BYTE  Reserved;
	DWORD Count;
	BYTE  Padding[8];
};

//**********************************************//
//********** GameServer -> DataServer **********//
//**********************************************//

// C1:D9:34 - load request
struct SDHP_SLOTCLAIM_RECV
{
	PSBMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
};

// C2:D9:35 - save
struct SDHP_SLOTCLAIM_SAVE_RECV
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SlotClaim[SLOT_CLAIM_SIZE][16];
	BYTE State[16];
};

//**********************************************//
//********** DataServer -> GameServer **********//
//**********************************************//

// C2:D9:34 - load reply
struct SDHP_SLOTCLAIM_SEND
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SlotClaim[SLOT_CLAIM_SIZE][16];
	BYTE State[16];
};

#pragma pack(pop)

// Stride pins. If any of these ever fires, the wire has drifted between the two
// projects - fix the struct, do not raise the number.
static_assert(sizeof(SLOTCLAIM_ROW_DATA) == 16, "SLOTCLAIM_ROW_DATA must stay 16 bytes");
static_assert(sizeof(SDHP_SLOTCLAIM_RECV) == 28, "SDHP_SLOTCLAIM_RECV must stay 28 bytes");
static_assert(sizeof(SDHP_SLOTCLAIM_SAVE_RECV) == 173, "SDHP_SLOTCLAIM_SAVE_RECV must stay 173 bytes");
static_assert(sizeof(SDHP_SLOTCLAIM_SEND) == 173, "SDHP_SLOTCLAIM_SEND must stay 173 bytes");

//**********************************************//
//**********************************************//
//**********************************************//

class CSlotClaim
{
public:
	CSlotClaim();
	virtual ~CSlotClaim();
	void GDSlotClaimRecv(SDHP_SLOTCLAIM_RECV* lpMsg,int index);
	void GDSlotClaimSaveRecv(SDHP_SLOTCLAIM_SAVE_RECV* lpMsg);
};

extern CSlotClaim gSlotClaim;
