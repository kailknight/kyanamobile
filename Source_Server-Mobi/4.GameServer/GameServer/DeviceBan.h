// DeviceBan.h - bans a device (phone or PC), not just an account.
//
// The id is what the client reports in packet 0xD3:0x7D (see
// PMSG_CLIENT_BUILD_RECV): 32 hex characters of a SHA-256 hash.
//   Android  ANDROID_ID - survives reinstalling the app, changes on factory reset
//   iOS      a UUID kept in the Keychain - same lifetime
//   PC       MachineGuid + system volume serial - changes on a Windows reinstall
//
// The ban list lives in Data\Custom\DeviceBan.txt, one id per line, so it can be
// edited by hand and survives restarts; it is what the login check reads.
// Every login (account, IP, id, platform) and every ban is also written to the
// database through the DataServer (DeviceLog_Schema.sql) as a record.
//
// Ways to ban:
//   /bandevice <character>         GM command in game
//   /unbandevice <character|id>
//   Tools > Online Users > "Ban hardware ID" on the GameServer window
#pragma once

#include "Protocol.h"
#include <set>
#include <vector>
#include <string>

#define DEVICE_ID_SIZE 32

#pragma pack(push,1)

// GameServer -> DataServer 0xD9:0x47. One per successful login.
struct SDHP_DEVICE_LOGIN_SEND
{
	PSBMSG_HEAD header;
	char Account[11];
	char Ip[16];
	char HardwareId[33];	// "" when the client sent none (old app)
	BYTE Platform;			// 0 = PC, 1 = phone
};

// GameServer -> DataServer 0xD9:0x48. One per ban or unban.
struct SDHP_DEVICE_BAN_SEND
{
	PSBMSG_HEAD header;
	char Account[11];
	char Name[11];
	char Ip[16];
	char HardwareId[33];
	BYTE Platform;
	char BannedBy[11];
	BYTE Banned;			// 1 = ban, 0 = unban
};

#pragma pack(pop)

// Same layout the DataServer reads (DeviceLog.h). Packed, so a padded build of
// either side cannot silently disagree on the size (mu-gs-struct-packing-trap).
static_assert(sizeof(SDHP_DEVICE_LOGIN_SEND) == 4 + 11 + 16 + 33 + 1,"SDHP_DEVICE_LOGIN_SEND layout");
static_assert(sizeof(SDHP_DEVICE_BAN_SEND) == 4 + 11 + 11 + 16 + 33 + 1 + 11 + 1,"SDHP_DEVICE_BAN_SEND layout");

struct DEVICE_BAN_ENTRY
{
	std::string Key;	// hardware id, or "ip:a.b.c.d" for an IP-only ban
	std::string Ip;		// IP banned with it, "" if none
	std::string Note;	// "account / character ... by who"
};

class CDeviceBan
{
public:
	void Load(char* path);
	bool IsBanned(const char* id);
	bool IsIpBanned(const char* ip);
	bool Add(const char* id,const char* note);
	bool Remove(const char* id);

	// Bans the hardware id, the IP and the account of the player at aIndex,
	// records it in the database (which blocks the account) and kicks that
	// account and every other one on the same device. by = who did it.
	// Returns 0 with a reason in error when it cannot.
	bool BanPlayer(int aIndex,const char* by,char* error,int errorSize);
	// target = a hardware id or an IP address; lifts what was banned with it.
	bool UnbanId(const char* target,const char* by);

	void GetBans(std::vector<DEVICE_BAN_ENTRY>& out);

	void SendLoginLog(int aIndex);
private:
	void Refresh();
	bool AddKey(const std::string& key,const char* note);
	bool RemoveKey(const std::string& key,const std::string& tag);
	unsigned long long FileStamp();
	unsigned long long m_Stamp;	// zero: gDeviceBan is a global
	void SendBanRecord(int aIndex,const char* id,const char* by,BYTE banned);
	std::string m_Path;
	std::set<std::string> m_List;	// hardware ids and "ip:a.b.c.d"
};

extern CDeviceBan gDeviceBan;

// The id as reported, or "" when the client sent none.
inline bool DeviceIdValid(const char* id) { return id != 0 && id[0] != 0; }
