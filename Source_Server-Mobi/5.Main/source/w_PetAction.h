// w_PetAction.h: interface for the PetAction class.
//////////////////////////////////////////////////////////////////////

#include "ZzzBMD.h"
#pragma once

class PetAction  
{
public:
	PetAction() {}
	virtual ~PetAction() {}
	virtual bool Release( OBJECT* obj, CHARACTER *Owner ) { return false; }
	
public:
	virtual bool Model( OBJECT *obj, CHARACTER *Owner, int targetKey, DWORD tick, bool bForceRender )	{ return false; }
	virtual bool Move( OBJECT *obj, CHARACTER *Owner, int targetKey, DWORD tick, bool bForceRender )	{ return false; }
	virtual bool Effect( OBJECT *obj, CHARACTER *Owner, int targetKey, DWORD tick, bool bForceRender )	{ return false; }
	virtual bool Sound( OBJECT *obj, CHARACTER *Owner, int targetKey, DWORD tick, bool bForceRender )	{ return false; }
};

// ---------------------------------------------------------------------------
// Drops a collector pet must leave alone.
//
// A pet that targets a drop reserved for someone else asks for it about once a
// second for as long as it keeps chasing, is refused every time, and never
// moves on - so one other player's drop nearby stalls the pet completely and
// it fetches nothing it could actually take. The server has no way to tell the
// pet's request from a manual click (identical packet), so the pet has to
// notice the refusal itself and skip that drop.
//
// Entries expire: a loot reservation runs out (CMapItem::m_LootTime server
// side), and item slots get reused by later drops, so a permanent blacklist
// would keep the pet off items that have since become free.
// ---------------------------------------------------------------------------

// Called right before a pet sends a pickup request, so a refusal that comes
// back can be matched to the drop that caused it.
void PetCollectNoteRequestedItem(int itemIndex);

// Called from ReceiveGetItem when the server refuses a pickup. Blacklists the
// drop the pet last asked for - but only if that request was recent, so a
// manual click failing for its own reasons (a full inventory, say) does not
// make the pet avoid a drop nobody asked it to skip.
void PetCollectOnPickupRefused();

// True while a pet should pretend this drop is not there.
bool PetCollectIsItemRefused(int itemIndex);
