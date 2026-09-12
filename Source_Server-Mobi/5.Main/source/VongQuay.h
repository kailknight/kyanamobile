#pragma once
// CustomHealthBar.h: interface for the CCustomHealthBar class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "Protocol.h"




class CVongQuay
{
public:


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

	//==Struct Client
	struct INFO_VONGQUAY_LOCAL_ITEM
	{
		float SizeBMD;
		float PosX;
		float PosY;
		short Index;
		ITEM* Item;
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

	// ---- Rewards box ------------------------------------------------------
	// Every prize the wheel awards is parked server-side and claimed by hand
	// from this window - nothing goes straight to the inventory. Packed to match
	// the GameServer's PMSG_SPINCLAIM_* in CustomVongQuay.h byte for byte; keep
	// any field change mirrored on both sides.
#pragma pack(push,1)

	struct SPINCLAIM_ROW
	{
		BYTE slot;
		BYTE Item[12];
	};

	// Server -> client, C2:D3:8D. Followed by `count` SPINCLAIM_ROW.
	struct PMSG_SPINCLAIM_LIST_RECV
	{
		PSWMSG_HEAD header;
		BYTE count;
		BYTE free;
	};

	// Client -> server, C1:D3:8E. slot 0xFF claims everything.
	struct PMSG_SPINCLAIM_CLAIM_SEND
	{
		PSBMSG_HEAD header;
		BYTE slot;
	};

#pragma pack(pop)

	// One held prize, decoded ready to render.
	struct INFO_SPINCLAIM_LOCAL
	{
		int slot;
		short Index;
		ITEM* Item;
	};

	CVongQuay();
	virtual ~CVongQuay();
	void Init();
	void DrawWindowVQ();
	void OpenVongQuay();
	int StartRollSau;
	int IndexItemSau;
	int IndexYC;
	int CountItem;
	int WCYC;
	int WPYC;
	int GPYC;
	std::vector<INFO_VONGQUAY_LOCAL_ITEM> ListItemVongQuay;
	std::vector<ListVongQuaySend> DanhSachVongQuay;
	void GetListVQ(BYTE* Recv);
	void RecvListItemVQ(BYTE* Recv);
	void GetInfoVQ(BYTE* Recv);

	// Rewards box: prizes held server-side, waiting to be claimed.
	std::vector<INFO_SPINCLAIM_LOCAL> ListClaim;
	int ClaimFree;
	void RecvClaimList(BYTE* Recv);
	void SendClaim(int slot);
	void ClearClaimList();
private:
	
};

extern CVongQuay gVongQuay;