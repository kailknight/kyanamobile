// DeviceBan.cpp - see DeviceBan.h.
#include "stdafx.h"
#include "DeviceBan.h"
#include "CustomAttack.h"
#include "CustomStore.h"
#include "GameMain.h"
#include "Log.h"
#include "OfflineMode.h"
#include "User.h"
#include "Util.h"

CDeviceBan gDeviceBan;

// A ban-file key is either a hardware id (32 lower-case hex) or "ip:a.b.c.d".
// Anything else on a line is a comment, a note or a typo.
static bool NormalizeDeviceId(const char* in,std::string& out)
{
	out.clear();

	for(int n=0;in[n] != 0 && in[n] != ' ' && in[n] != '\t' && in[n] != '\r' && in[n] != '\n';n++)
	{
		char c = in[n];

		if(c >= 'A' && c <= 'F'){ c = (char)(c - 'A' + 'a'); }

		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))){ return false; }

		out += c;
	}

	return out.size() == DEVICE_ID_SIZE;
}

static bool NormalizeIp(const char* in,std::string& out)
{
	out.clear();

	int dots = 0;

	for(int n=0;in[n] != 0 && in[n] != ' ' && in[n] != '\t' && in[n] != '\r' && in[n] != '\n';n++)
	{
		char c = in[n];

		if(c == '.'){ dots++; }
		else if(c < '0' || c > '9'){ return false; }

		out += c;
	}

	return dots == 3 && out.size() >= 7 && out.size() <= 15;
}

// Line -> key, or false when the line holds no ban.
static bool ParseKey(const char* line,std::string& key)
{
	while(*line == ' ' || *line == '\t'){ line++; }

	if(_strnicmp(line,"ip:",3) == 0)
	{
		std::string ip;

		if(NormalizeIp(line + 3,ip) == 0)
		{
			return 0;
		}

		key = "ip:" + ip;
		return 1;
	}

	return NormalizeDeviceId(line,key);
}

static std::string IpKey(const char* ip)
{
	std::string out;
	return (NormalizeIp(ip,out) != 0) ? ("ip:" + out) : std::string();
}

void CDeviceBan::Load(char* path) // OK
{
	// Both GameServers (main + Sub 1) have their own Data folder. Pointing them
	// at ONE file with DeviceBanFile makes a ban on either apply to both - the
	// other picks it up on its next check (see Refresh).
	char shared[MAX_PATH] = {0};
	GetPrivateProfileString("GameServerInfo","DeviceBanFile","",shared,sizeof(shared),".\\Data\\GameServerInfo - Common.ini");

	this->m_Path = (shared[0] != 0) ? shared : path;
	this->m_List.clear();
	this->m_Stamp = this->FileStamp();

	FILE* file = fopen(this->m_Path.c_str(),"r");

	if(file == 0)
	{
		return;	// no file yet = nobody banned; Add() creates it
	}

	char line[256];

	while(fgets(line,sizeof(line),file) != 0)
	{
		std::string key;

		if(ParseKey(line,key) != 0)
		{
			this->m_List.insert(key);
		}
	}

	fclose(file);

	LogAdd(LOG_BLUE,"[DeviceBan] %d bans (hardware ID / IP) loaded",(int)this->m_List.size());
}

// The other GameServer, or a hand edit, may have changed the file since.
void CDeviceBan::Refresh() // OK
{
	if(this->FileStamp() != this->m_Stamp)
	{
		std::string path = this->m_Path;
		this->Load((char*)path.c_str());
	}
}

bool CDeviceBan::IsBanned(const char* id) // OK
{
	this->Refresh();

	std::string key;

	if(NormalizeDeviceId(id,key) == 0)
	{
		return 0;
	}

	return this->m_List.count(key) != 0;
}

bool CDeviceBan::IsIpBanned(const char* ip) // OK
{
	this->Refresh();

	std::string key = IpKey(ip);

	return key.empty() == 0 && this->m_List.count(key) != 0;
}

// key is already normalized. Appended rather than rewritten, so a hand edit
// made while the server is running is not thrown away.
bool CDeviceBan::AddKey(const std::string& key,const char* note) // OK
{
	this->Refresh();

	if(key.empty() != 0 || this->m_List.insert(key).second == 0)
	{
		return 0;
	}

	FILE* file = fopen(this->m_Path.c_str(),"a");

	if(file != 0)
	{
		fprintf(file,"%s\t// %s\n",key.c_str(),(note != 0) ? note : "");
		fclose(file);
	}

	this->m_Stamp = this->FileStamp();

	return 1;
}

bool CDeviceBan::Add(const char* id,const char* note) // OK
{
	std::string key;

	return NormalizeDeviceId(id,key) != 0 && this->AddKey(key,note) != 0;
}

// Removes key, and with it every line whose note mentions tag - that is how the
// IP banned together with a hardware id goes when the hardware id is unbanned.
bool CDeviceBan::RemoveKey(const std::string& key,const std::string& tag) // OK
{
	this->Refresh();

	if(this->m_List.count(key) == 0)
	{
		return 0;
	}

	std::string keep;

	FILE* file = fopen(this->m_Path.c_str(),"r");

	if(file != 0)
	{
		char line[256];

		while(fgets(line,sizeof(line),file) != 0)
		{
			std::string lineKey;

			if(ParseKey(line,lineKey) != 0 && (lineKey == key || (tag.empty() == 0 && strstr(line,tag.c_str()) != 0)))
			{
				this->m_List.erase(lineKey);
				continue;
			}

			keep += line;
		}

		fclose(file);
	}

	this->m_List.erase(key);

	file = fopen(this->m_Path.c_str(),"w");

	if(file == 0)
	{
		return 0;
	}

	fputs(keep.c_str(),file);
	fclose(file);

	this->m_Stamp = this->FileStamp();

	return 1;
}

bool CDeviceBan::Remove(const char* id) // OK
{
	std::string key;

	return NormalizeDeviceId(id,key) != 0 && this->RemoveKey(key,key) != 0;
}

// ---------------------------------------------------------------------------
// Database records (DataServer DeviceLog.cpp)
// ---------------------------------------------------------------------------

void CDeviceBan::SendLoginLog(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	SDHP_DEVICE_LOGIN_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x47,sizeof(pMsg));
	strncpy_s(pMsg.Account,sizeof(pMsg.Account),lpObj->Account,_TRUNCATE);
	strncpy_s(pMsg.Ip,sizeof(pMsg.Ip),lpObj->IpAddr,_TRUNCATE);
	strncpy_s(pMsg.HardwareId,sizeof(pMsg.HardwareId),lpObj->m_DeviceId,_TRUNCATE);
	pMsg.Platform = lpObj->m_ClientPlatform;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CDeviceBan::SendBanRecord(int aIndex,const char* id,const char* by,BYTE banned) // OK
{
	SDHP_DEVICE_BAN_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x48,sizeof(pMsg));

	if(aIndex >= 0)
	{
		LPOBJ lpObj = &gObj[aIndex];
		strncpy_s(pMsg.Account,sizeof(pMsg.Account),lpObj->Account,_TRUNCATE);
		strncpy_s(pMsg.Name,sizeof(pMsg.Name),lpObj->Name,_TRUNCATE);
		strncpy_s(pMsg.Ip,sizeof(pMsg.Ip),lpObj->IpAddr,_TRUNCATE);
		pMsg.Platform = lpObj->m_ClientPlatform;
	}

	strncpy_s(pMsg.HardwareId,sizeof(pMsg.HardwareId),id,_TRUNCATE);
	strncpy_s(pMsg.BannedBy,sizeof(pMsg.BannedBy),by,_TRUNCATE);
	pMsg.Banned = banned;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

// ---------------------------------------------------------------------------
// Ban / unban
// ---------------------------------------------------------------------------

// Bans all three at once:
//   hardware id  this file - refused at login on every account on that device
//   IP           this file - refused at login from that address
//   account      MEMB_INFO.BLOC_CODE, set by WZ_HardwareBan in the database
// A client too old to send a hardware id still loses its account and IP.
bool CDeviceBan::BanPlayer(int aIndex,const char* by,char* error,int errorSize) // OK
{
	LPOBJ lpTarget = &gObj[aIndex];

	// Copied: the kicks below clear the object they came from.
	char id[33] = {0};
	char ip[16] = {0};
	char account[11] = {0};
	char name[11] = {0};
	strcpy_s(id,lpTarget->m_DeviceId);
	strcpy_s(ip,lpTarget->IpAddr);
	strcpy_s(account,lpTarget->Account);
	strcpy_s(name,lpTarget->Name);

	const bool hasId = (DeviceIdValid(id) != 0);
	const std::string ipKey = IpKey(ip);

	if(hasId == 0 && ipKey.empty() != 0)
	{
		sprintf_s(error,errorSize,"%s has neither a hardware ID nor a valid IP.",account);
		return 0;
	}

	char note[96];

	if(hasId != 0)
	{
		sprintf_s(note,"%s / %s by %s",account,name,by);
		this->Add(id,note);
	}

	if(ipKey.empty() == 0)
	{
		// Tagged with the hardware id so unbanning the hardware lifts the IP too.
		sprintf_s(note,"%s / %s %s by %s",account,name,hasId ? id : "",by);
		this->AddKey(ipKey,note);
	}

	// The database record also blocks the account (WZ_HardwareBan).
	this->SendBanRecord(aIndex,hasId ? id : ipKey.c_str(),by,1);

	gLog.Output(LOG_COMMAND,"[DeviceBan] Banned (Acc: %s, Char: %s, Ip: %s, Device: %s, By: %s)",account,name,ip,hasId ? id : "-",by);

	// Kick the account, and everyone else on that device - a second account on
	// the same phone or PC is the same player. Others sharing the IP are not
	// kicked (mobile carriers put many players behind one address); they are
	// refused from it at their next login.
	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		LPOBJ lpObj = &gObj[n];

		if(lpObj->Connected < OBJECT_LOGGED || lpObj->Type != OBJECT_USER)
		{
			continue;
		}

		if(strcmp(lpObj->Account,account) != 0 && (hasId == 0 || strcmp(lpObj->m_DeviceId,id) != 0))
		{
			continue;
		}

		gObjUserKill(lpObj->Index);
		gCustomAttack.OnAttackAlreadyConnected(lpObj);
		gCustomStore.OnPShopAlreadyConnected(lpObj);
		g_OfflineMode.OnHelperpAlreadyConnected(lpObj);
		CloseClient(lpObj->Index);
	}

	return 1;
}

// target: a hardware id (also lifts the IP banned with it, and the account) or
// an IP address (lifts that IP, and the account banned with it).
bool CDeviceBan::UnbanId(const char* target,const char* by) // OK
{
	std::string key;

	if(NormalizeDeviceId(target,key) != 0)
	{
		if(this->RemoveKey(key,key) == 0)
		{
			return 0;
		}
	}
	else
	{
		key = IpKey(target);

		if(key.empty() != 0 || this->RemoveKey(key,"") == 0)
		{
			return 0;
		}
	}

	this->SendBanRecord(-1,key.c_str(),by,0);

	gLog.Output(LOG_COMMAND,"[DeviceBan] Unbanned (Target: %s, By: %s)",key.c_str(),by);

	return 1;
}

// Last-write time of the ban file, 0 when it does not exist.
unsigned long long CDeviceBan::FileStamp() // OK
{
	WIN32_FILE_ATTRIBUTE_DATA data;

	if(GetFileAttributesExA(this->m_Path.c_str(),GetFileExInfoStandard,&data) == 0)
	{
		return 0;
	}

	return ((unsigned long long)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
}

// One entry per ban for the Banned List window: every hardware id line, plus
// IP lines that were not banned together with a hardware id (old clients).
// An IP that came with a hardware id is shown on that hardware id's entry.
void CDeviceBan::GetBans(std::vector<DEVICE_BAN_ENTRY>& out) // OK
{
	out.clear();

	this->Refresh();

	FILE* file = fopen(this->m_Path.c_str(),"r");

	if(file == 0)
	{
		return;
	}

	std::vector<std::pair<std::string,std::string>> lines;	// key, whole line

	char line[256];

	while(fgets(line,sizeof(line),file) != 0)
	{
		std::string key;

		if(ParseKey(line,key) != 0)
		{
			std::string text = line;
			while(text.empty() == 0 && (text.back() == '\n' || text.back() == '\r')){ text.pop_back(); }
			lines.push_back(std::make_pair(key,text));
		}
	}

	fclose(file);

	for(size_t n=0;n < lines.size();n++)
	{
		const std::string& key = lines[n].first;
		const std::string& text = lines[n].second;

		size_t slash = text.find("//");
		std::string note = (slash == std::string::npos) ? "" : text.substr(slash + 2);
		while(note.empty() == 0 && note[0] == ' '){ note.erase(0,1); }

		DEVICE_BAN_ENTRY entry;
		entry.Key = key;
		entry.Note = note;

		if(key.compare(0,3,"ip:") == 0)
		{
			// Belongs to a hardware id ban if any hardware id is named in it.
			bool tagged = false;

			for(size_t i=0;i < lines.size();i++)
			{
				if(lines[i].first.compare(0,3,"ip:") != 0 && text.find(lines[i].first) != std::string::npos)
				{
					tagged = true;
					break;
				}
			}

			if(tagged != 0)
			{
				continue;
			}

			entry.Ip = key.substr(3);
		}
		else
		{
			for(size_t i=0;i < lines.size();i++)
			{
				if(lines[i].first.compare(0,3,"ip:") == 0 && lines[i].second.find(key) != std::string::npos)
				{
					entry.Ip = lines[i].first.substr(3);
					break;
				}
			}
		}

		out.push_back(entry);
	}
}
