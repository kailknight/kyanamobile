#pragma once
#include "WSclient.h"
#define SET_NUMBERHB(x) ((BYTE)((DWORD)(x)>>(DWORD)8))
#define SET_NUMBERLB(x) ((BYTE)((DWORD)(x)&0xFF))
#define SET_NUMBERHW(x) ((WORD)((DWORD)(x)>>(DWORD)16))
#define SET_NUMBERLW(x) ((WORD)((DWORD)(x)&0xFFFF))
#define SET_NUMBERHDW(x) ((DWORD)((QWORD)(x)>>(QWORD)32))
#define SET_NUMBERLDW(x) ((DWORD)((QWORD)(x)&0xFFFFFFFF))

#define MAKE_NUMBERW(x,y) ((WORD)(((BYTE)((y)&0xFF))|((BYTE)((x)&0xFF)<<8)))
#define MAKE_NUMBERDW(x,y) ((DWORD)(((WORD)((y)&0xFFFF))|((WORD)((x)&0xFFFF)<<16)))
#define MAKE_NUMBERQW(x,y) ((QWORD)(((DWORD)((y)&0xFFFFFFFF))|((DWORD)((x)&0xFFFFFFFF)<<32)))

struct PMSG_CHARACTER_REGEN_RECV
{
	PSBMSG_HEAD header; // C3:F3:04
	BYTE X;
	BYTE Y;
	BYTE Map;
	BYTE Dir;
	WORD Life;
	WORD Mana;
	WORD Shield;
	WORD BP;
	BYTE Experience[8];
	DWORD Money;
	//EXTRA
	DWORD ViewCurHP;
	DWORD ViewCurMP;
	DWORD ViewCurBP;
	DWORD ViewCurSD;
};

struct PMSG_LEVEL_UP_RECV
{
	PSBMSG_HEAD header; // C1:F3:05
	WORD Level;
	WORD LevelUpPoint;
	WORD MaxLife;
	WORD MaxMana;
	WORD MaxShield;
	WORD MaxBP;
	WORD FruitAddPoint;
	WORD MaxFruitAddPoint;
	WORD FruitSubPoint;
	WORD MaxFruitSubPoint;
	//EXTRA
	DWORD ViewPoint;
	DWORD ViewMaxHP;
	DWORD ViewMaxMP;
	DWORD ViewMaxBP;
	DWORD ViewMaxSD;
	DWORD ViewExperience;
	DWORD ViewNextExperience;
};

struct PMSG_LEVEL_UP_POINT_RECV
{
	PSBMSG_HEAD header; // C1:F3:06
	BYTE result;
	WORD MaxLifeAndMana;
	WORD MaxShield;
	WORD MaxBP;
	//EXTRA
	DWORD ViewPoint;
	DWORD ViewMaxHP;
	DWORD ViewMaxMP;
	DWORD ViewMaxBP;
	DWORD ViewMaxSD;
	DWORD ViewStrength;
	DWORD ViewDexterity;
	DWORD ViewVitality;
	DWORD ViewEnergy;
	DWORD ViewLeadership;
};

struct PMSG_MONSTER_DAMAGE_RECV
{
	PSBMSG_HEAD header; // C1:F3:07
	BYTE damage[2];
	BYTE ShieldDamage[2];
	//EXTRA
	DWORD ViewCurHP;
	DWORD ViewCurSD;
	DWORD ViewDamageHP;
	DWORD ViewDamageSD;
};

struct PMSG_MASTER_INFO_RECV
{
	PSBMSG_HEAD header; // C1:F3:50
	WORD MasterLevel;
	BYTE Experience[8];
	BYTE NextExperience[8];
	WORD MasterPoint;
	WORD MaxLife;
	WORD MaxMana;
	WORD MaxShield;
	WORD MaxBP;
	//EXTRA
	DWORD ViewMaxHP;
	DWORD ViewMaxMP;
	DWORD ViewMaxBP;
	DWORD ViewMaxSD;
};

struct PMSG_MASTER_LEVEL_UP_RECV
{
	PSBMSG_HEAD header; // C1:F3:51
	WORD MasterLevel;
	WORD MinMasterLevel;
	WORD MasterPoint;
	WORD MaxMasterLevel;
	WORD MaxLife;
	WORD MaxMana;
	WORD MaxShield;
	WORD MaxBP;
	//EXTRA
	DWORD ViewMaxHP;
	DWORD ViewMaxMP;
	DWORD ViewMaxBP;
	DWORD ViewMaxSD;
	QWORD ViewMasterExperience;
	QWORD ViewMasterNextExperience;
};

//==Helper
struct PMSG_HELPER_START_SEND //Client Send GS On/Offf
{
	PSBMSG_HEAD header; // C1:BF:51
	BYTE type;
};
struct PMSG_HELPER_START_RECV //GS Send Client
{
	PSBMSG_HEAD header; // C1:BF:51
	DWORD time;
	DWORD money;
	BYTE result;
};

struct PMSG_DAMAGE_RECV
{
	PBMSG_HEAD header; // C1:11
	BYTE index[2];
	BYTE damage[2];
	BYTE type;
	BYTE ShieldDamage[2];
	//EXTRA
	DWORD ViewCurHP;
	DWORD ViewCurSD;
	DWORD ViewDamageHP;
	DWORD ViewDamageSD;
};

struct PMSG_VIEWPORT_DESTROY_RECV
{
	PBMSG_HEAD header; // C1:14
	BYTE count;
};

struct PMSG_VIEWPORT_DESTROY
{
	BYTE index[2];
};

struct PMSG_MONSTER_DIE_RECV
{
	PBMSG_HEAD header; // C1:16
	BYTE index[2];
	BYTE experience[2];
	BYTE damage[2];
	//EXTRA
	DWORD ViewDamageHP;
};

struct PMSG_USER_DIE_RECV
{
	PBMSG_HEAD header; // C1:17
	BYTE index[2];
	BYTE skill[2];
	BYTE killer[2];
};

struct PMSG_LIFE_RECV
{
	PBMSG_HEAD header; // C1:26
	BYTE type;
	BYTE life[2];
	BYTE flag;
	BYTE shield[2];
	//EXTRA
	DWORD ViewHP;
	DWORD ViewSD;
};

struct PMSG_MANA_RECV
{
	PBMSG_HEAD header; // C1:27
	BYTE type;
	BYTE mana[2];
	BYTE bp[2];
	//EXTRA
	DWORD ViewMP;
	DWORD ViewBP;
};

struct PMSG_FRUIT_RESULT_RECV
{
	PBMSG_HEAD header; // C1:2C
	BYTE result;
	WORD value;
	BYTE type;
	//EXTRA
	DWORD ViewValue;
	DWORD ViewPoint;
	DWORD ViewStrength;
	DWORD ViewDexterity;
	DWORD ViewVitality;
	DWORD ViewEnergy;
	DWORD ViewLeadership;
};
struct PMSG_CONNECT_CLIENT_RECV
{
	PSBMSG_HEAD header; // C1:F1:00
	BYTE result;
	BYTE index[2];
	BYTE ClientVersion[5];
};
struct PMSG_PET_ITEM_COMMAND_SEND
{
	PBMSG_HEAD header; // C1:A7
	BYTE type;
	BYTE command;
	BYTE index[2];
};

struct PMSG_COIN_RECV
{
	PSBMSG_HEAD header; // 
	int  Coin1;
	int  Coin2;
	int  Coin3;
};
void GCRecvCoin(PMSG_COIN_RECV* lpMsg);

struct XULY_CGPACKET
{
	PSBMSG_HEAD header; // C3:F3:03
	DWORD ThaoTac;
};
struct PMSG_NOTICE_SEND
{
	PBMSG_HEAD header; // C1:0D
	BYTE type;
	BYTE count;
	BYTE opacity;
	WORD delay;
	DWORD color;
	BYTE speed;
	char message[256];
};
struct PMSG_SET_CHAOSBOX_STATE
{
	PSBMSG_HEAD header; // C1:32
	BYTE state;
};
void SetChaosBoxState(PMSG_SET_CHAOSBOX_STATE* Data);
BOOL ProtocolCoreEx(BYTE head, BYTE* lpMsg, int size, int key);
void DataSend(BYTE* lpMsg, DWORD size);
void GCDamageRecv(PMSG_DAMAGE_RECV* lpMsg);
void GCMonsterDieRecv(PMSG_MONSTER_DIE_RECV* lpMsg);
void GCUserDieRecv(PMSG_USER_DIE_RECV* lpMsg);
void GCLifeRecv(PMSG_LIFE_RECV* lpMsg);
void GCManaRecv(PMSG_MANA_RECV* lpMsg);
void GCFruitResultRecv(PMSG_FRUIT_RESULT_RECV* lpMsg);
void GCConnectClientRecv(PMSG_CONNECT_CLIENT_RECV* lpMsg);

void RecvPostItem(BYTE* ReceiveBuffer);
void CGLoadDataMuHelper(BYTE* ReceiveBuffer);
void ReceiveNewHealthBar(BYTE* ReceiveBuffer);
void GCLevelUpPointRecv(PMSG_LEVEL_UP_POINT_RECV* lpMsg);
void GCMonsterDamageRecv(PMSG_MONSTER_DAMAGE_RECV* lpMsg);
void GCCharacterRegenRecv(PMSG_CHARACTER_REGEN_RECV* lpMsg);
void GCLevelUpRecv(PMSG_LEVEL_UP_RECV* lpMsg);
void GCMasterInfoRecv(PMSG_MASTER_INFO_RECV* lpMsg);
void GCMasterLevelUpRecv(PMSG_MASTER_LEVEL_UP_RECV* lpMsg);
void GCNewCharacterInfoRecv(BYTE* ReceiveBuffer);
void GCNewCharacterCalcRecv(BYTE* ReceiveBuffer);
void GCCharacterInfoRecv(BYTE* ReceiveBuffer);
void CGSaveDataMuHelper(BYTE* ReceiveBuffer);
void StartMuHelper(int Type = 0);
void RecvStartMuHelper(BYTE* ReceiveBuffer);
struct SEND_COUNTLIST
{
	PSWMSG_HEAD header;
	int Count;
};
#if(HAISLOTRING)
typedef struct //-- Durabilidad
{
	PSBMSG_HEAD  Header;
	BYTE         Key;
	BYTE         Value;
} PHEADER_DEFAULT_VALUE_DUR, * LPPHEADER_DEFAULT_VALUE_DUR;

typedef struct {
	PSBMSG_HEAD   Header;
	BYTE          KeyH;
	BYTE          KeyL;
	BYTE          Item[12];
} PSCHANGE_CHARACTER, * LPPSCHANGE_CHARACTER;

void BReceiveInventory(BYTE* lpMsg);
void BReceiveDurability(BYTE* lpMsg);
void BReceiveDeleteInventory(BYTE* lpMsg);
void BReceiveItemChange(BYTE* lpMsg);
#endif

bool CheckHidenPetiSClass(BYTE Class);

#if(SAUDOIITEM)
struct INFOITEM_DOIITEM_CLIENT
{
	PSWMSG_HEAD header;
	BYTE ActiveMix;
	BYTE ItemChinh[16];
	BYTE ItemPhu[3][16];
	BYTE ItemKetQua[6][16];
	int Rate;
};
#endif

#if(REDEEMCODE)
//-----------------------------------
// Redemption code system. Byte-for-byte duplicate of the same structs in
// GameServer's Protocol.h - this codebase keeps client<->GameServer wire
// structs duplicated rather than shared between the two projects. Two full
// request/reply round trips: 0xA0/0xA1 for the non-binding "Check Code"
// preview, 0xA2/0xA3 for the actual "Redeem" - see NewUIRedeemCodeWindow for
// how the window uses them.
//-----------------------------------
struct PMSG_REDEEM_CODE_SEND
{
	PSBMSG_HEAD header;
	char Code[33];
};

enum eRedeemCodeResult
{
	REDEEM_SUCCESS = 0,
	REDEEM_OK_TO_REDEEM,
	REDEEM_NOT_FOUND,
	REDEEM_INACTIVE,
	REDEEM_EXPIRED,
	REDEEM_CAP_REACHED,
	REDEEM_ALREADY_REDEEMED,
	REDEEM_INVENTORY_FULL,
	REDEEM_BUSY,
	REDEEM_SERVER_ERROR,
};

struct REDEEM_ITEM_DETAIL
{
	WORD ItemIndex;
	BYTE ItemLevel;
	BYTE ItemSkill;
	BYTE ItemLuck;
	BYTE ItemOption;
	BYTE ItemExcellent;
	BYTE Quantity;
	BYTE SocketOption[5]; // literal 5, not MAX_SOCKETS - keeps this struct self-contained regardless of include order
	BYTE SocketOptionBonus;
	BYTE Item380;           // 0/1 - ITEM::option_380
	BYTE HarmonyOption;     // category-relative option index (0-10 depending on weapon/staff/defense), 0 = none - ITEM::Jewel_Of_Harmony_Option
	BYTE HarmonyOptionLevel; // 0-15 - ITEM::Jewel_Of_Harmony_OptionLevel
};

#define REDEEM_CODE_BUNDLE_MAX 8

struct PMSG_REDEEM_CODE_RECV
{
	PSBMSG_HEAD header;
	BYTE Result;
	BYTE ItemCount;
	REDEEM_ITEM_DETAIL Items[REDEEM_CODE_BUNDLE_MAX];
	// Per-code currency rewards (W Coin C / W Coin P / Goblin Point / Ruud).
	// After Items[] on purpose - see the assert below.
	DWORD RewardWCoinC;
	DWORD RewardWCoinP;
	DWORD RewardGoblinPoint;
	DWORD RewardRuud;
};

// This struct is hand-duplicated in GameServer's own Protocol.h (this codebase
// shares no headers between the two projects), so nothing but this assert
// stops the two copies from silently drifting apart. They must agree on the
// STRIDE, not just the field list: Items[] is an array, so a stride that
// differs by even one byte leaves Items[0] readable while every later entry
// lands at the wrong offset - which reads as a plausible-but-wrong item
// (index 0, level 0, five zeroed sockets) rather than as an obvious error.
// If this fires, fix the field list/padding on whichever side changed; never
// just update the number.
static_assert(sizeof(REDEEM_ITEM_DETAIL) == 18, "REDEEM_ITEM_DETAIL must stay 18 bytes and byte-identical to GameServer's copy in its own Protocol.h");
static_assert(sizeof(PMSG_REDEEM_CODE_RECV) <= 255, "PSBMSG_HEAD stores the packet size in a single BYTE - this packet must never exceed 255 bytes or header.set() silently truncates it");
#endif

void CGAutoMove(int Type);
