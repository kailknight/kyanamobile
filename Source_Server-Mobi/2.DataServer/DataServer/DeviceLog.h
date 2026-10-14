// DeviceLog.h - records logins and hardware bans in the database.
//
// The GameServer sends one 0xD9:0x47 per successful login (account, IP,
// hardware id, platform) and one 0xD9:0x48 per ban or unban (GameServer
// DeviceBan.h). Fire and forget: nothing is answered, and the GameServer's own
// ban list (Data\Custom\DeviceBan.txt) is what login is checked against, so a
// database outage never lets a banned device in or keeps anyone out.
//
// Tables and procedures: DeviceLog_Schema.sql.
#pragma once

#include "DataServerProtocol.h"

#pragma pack(push,1)

struct SDHP_DEVICE_LOGIN_RECV
{
	PSBMSG_HEAD header; // C1:D9:47
	char Account[11];
	char Ip[16];
	char HardwareId[33];
	BYTE Platform;		// 0 = PC, 1 = phone
};

struct SDHP_DEVICE_BAN_RECV
{
	PSBMSG_HEAD header; // C1:D9:48
	char Account[11];
	char Name[11];
	char Ip[16];
	char HardwareId[33];
	BYTE Platform;
	char BannedBy[11];
	BYTE Banned;		// 1 = ban, 0 = unban
};

#pragma pack(pop)

static_assert(sizeof(SDHP_DEVICE_LOGIN_RECV) == 4 + 11 + 16 + 33 + 1,"SDHP_DEVICE_LOGIN_RECV layout");
static_assert(sizeof(SDHP_DEVICE_BAN_RECV) == 4 + 11 + 11 + 16 + 33 + 1 + 11 + 1,"SDHP_DEVICE_BAN_RECV layout");

class CDeviceLog
{
public:
	void OnLogin(SDHP_DEVICE_LOGIN_RECV* lpMsg);
	void OnBan(SDHP_DEVICE_BAN_RECV* lpMsg);
};

extern CDeviceLog gDeviceLog;
