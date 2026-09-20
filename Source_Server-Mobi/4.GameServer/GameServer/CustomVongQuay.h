// CustomHealthBar.h: interface for the CCustomHealthBar class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "Protocol.h"
#include "ItemManager.h"	// MAX_ITEM_INFO (the 12-byte client item payload), CItem

// Spin wheel claim box. Prizes that cannot be placed in the inventory at the
// moment they are won land here instead of being discarded, and are claimed from
// the wheel window later. Mirrors EventInventory.h:10-12's shape.
//
// Use SPIN_CLAIM_RANGE on every slot index that arrives from the client - the
// claim packet is player-supplied and an out-of-range slot would index straight
// past the array.
#define SPIN_CLAIM_SIZE 8
#define SPIN_CLAIM_RANGE(x) (((x)<0)?0:((x)>=SPIN_CLAIM_SIZE)?0:1)

struct MESSAGE_INFO_VONGQUAY
{
	int Index;
	char Message[256];
};

struct DATA_VONGQUAYITEM
{
	float SizeBMD;
	float PosX;
	float PosY;
	int IndexItem;
	int LvItem;
	int Dur;
	int Skill;
	int Luck;
	int Opt;
	int Exc;
	int Anc;
	int SK[MAX_SOCKET_OPTION];
	int SKBonus;
	int HSD;
	int Rate;
};

struct DATA_VONGQUAY
{
	int IndexVongQuay;
	int IndexItemYC;
	int WC;
	int WP;
	int GP;
	int Count;
	char NameVongQuay[90];
	std::vector<DATA_VONGQUAYITEM> ListItemNhan;
};

//===List VONG QUAY
struct PMSG_VONGQUAY_SEND
{
	PSWMSG_HEAD header; // C2:F3:E2
	BYTE count;
};

struct ListVongQuaySend
{
	int IndexVongQuay;
	char Name[30];
};


//===List THuowng
struct PMSG_YCVONGQUAY_SEND
{
	PSWMSG_HEAD header; // C2:F3:E2
	BYTE count;
	int IndexYC;
	int CountItem;
	int WCYC;
	int WPYC;
	int GPYC;
};

struct LISTITEMVONGQUAY_SENDINFO
{
	float SizeBMD;
	float PosX;
	float PosY;
	short Index;
	BYTE Dur;
	BYTE Item[12];
	int  PeriodTime;
};

struct XULY_CGPACKET_VONGQUAY
{
	PSBMSG_HEAD header; // C3:F3:03
	DWORD StartRoll;
	DWORD IndexWin;

};

struct XULY_CGPACKET_SOLAN
{
	PSBMSG_HEAD header; // C3:F3:03
	DWORD ThaoTac;
	DWORD SoLan;
};

// ---- Claim box wire -------------------------------------------------------
// Packed explicitly. The existing wheel structs are unpacked and carry hidden
// padding; they only work because both sides happen to declare them
// identically. These are new, so pin every offset rather than relying on that.
// The 12-byte item payload is the same convention the wheel's prize rows use
// (VongQuay_ItemByteConvert), so the client decodes it with the code it has.
#pragma pack(push,1)

struct SPINCLAIM_ROW
{
	BYTE slot;
	BYTE Item[MAX_ITEM_INFO];
};

// GameServer -> Client, C2:D3:8D. Followed by `count` SPINCLAIM_ROW.
struct PMSG_SPINCLAIM_LIST_SEND
{
	PSWMSG_HEAD header;
	BYTE count;
	BYTE free;		// free slots left, so the client can grey out / warn
};

// Client -> GameServer, C1:D3:8E. slot 0xFF means "claim everything".
struct PMSG_SPINCLAIM_CLAIM_RECV
{
	PSBMSG_HEAD header;
	BYTE slot;
};

// ---- Claim box persistence (GameServer <-> DataServer) --------------------
// BYTE-FOR-BYTE duplicates of the same structs in the DataServer's SpinClaim.h.
// The two projects deliberately keep their wire structs duplicated rather than
// shared, so any field change must be mirrored on BOTH sides in the same commit
// and both binaries deployed together - BUILD.bat does not collect
// DataServer.exe, and a one-sided deploy misaligns the wire silently, surfacing
// as corrupted items rather than a clean failure.

// GameServer -> DataServer, C1:D9:32 - load request
struct SDHP_SPINCLAIM_RECV
{
	PSBMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
};

// GameServer -> DataServer, C2:D9:33 - save
struct SDHP_SPINCLAIM_SAVE_RECV
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SpinClaim[SPIN_CLAIM_SIZE][16];
};

// DataServer -> GameServer, C2:D9:32 - load reply
struct SDHP_SPINCLAIM_SEND
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SpinClaim[SPIN_CLAIM_SIZE][16];
};

#pragma pack(pop)

// Stride pins, mirrored in the DataServer's SpinClaim.h. If one of these fires,
// the wire has drifted between the two projects - fix the struct, never the
// number.
static_assert(sizeof(SDHP_SPINCLAIM_RECV) == 28, "SDHP_SPINCLAIM_RECV must stay 28 bytes");
static_assert(sizeof(SDHP_SPINCLAIM_SAVE_RECV) == 157, "SDHP_SPINCLAIM_SAVE_RECV must stay 157 bytes");
static_assert(sizeof(SDHP_SPINCLAIM_SEND) == 157, "SDHP_SPINCLAIM_SEND must stay 157 bytes");


class CCustomVongQuay
{
public:
	CCustomVongQuay();
	virtual ~CCustomVongQuay();
	void Init();
	void LoadFileXML(char* FilePath);
	void CCustomVongQuay::UserSendClientInfo(int aIndex);
	void CCustomVongQuay::SendListNhanThuong(int aIndex, int VongQuaySo);
	void CCustomVongQuay::ActionVongQuay(int aIndex, int MocNap,int solan);
	void CCustomVongQuay::MakeItem(int aIndex,int type);
	int DrawWheelPrize(DATA_VONGQUAY& wheel);

	// ---- Claim box -------------------------------------------------------
	// Parks a prize the player has no room for. Stores a TEMPLATE, not a live
	// item: no DataServer serial is issued until the prize is actually claimed,
	// so a crash can never orphan a serial for an item nobody holds.
	// Returns the slot used, or -1 when the box is full.
	int PushClaimItem(int aIndex,CItem& item);
	// Free slot count, for the "box is full" refusal.
	int GetClaimFreeCount(int aIndex);
	// Moves one slot (or every slot when slot is 0xFF) into the inventory.
	void ClaimItem(int aIndex,int slot);
	// Sends the whole box to the client (0xD3/0x8D).
	void SendClaimList(int aIndex);
	// Client asked to claim (0xD3/0x8E).
	void CGClaimRecv(int aIndex,BYTE* lpRecv);

	// Persistence. Load is requested once per character login and guarded by
	// LoadSpinClaim so it cannot run twice; save bails when the load never
	// happened, so an unloaded box can never overwrite a real one with blanks.
	void GDSpinClaimSend(int aIndex);
	void DGSpinClaimRecv(SDHP_SPINCLAIM_SEND* lpMsg);
	void GDSpinClaimSaveSend(int aIndex);

	int SoVongQuay;
private:
	int Enable;
	int Firework;
	int Notice;
	std::map<int, MESSAGE_INFO_VONGQUAY> m_MessageInfoBP;
	std::map<int, DATA_VONGQUAY> m_DataVongQuay;
	// Backing store for GetMessage's not-found text. Must not be a local: the
	// caller formats through the returned pointer.
	char m_DefaultMessage[256];
	char* GetMessage(int index);
};

extern CCustomVongQuay gCustomVongQuay;