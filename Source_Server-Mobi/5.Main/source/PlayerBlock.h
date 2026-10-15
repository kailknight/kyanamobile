// PlayerBlock.h: blocking and reporting other players, on every platform.
//
// Blocking is local to this device: a blocked player's chat, whispers and
// proximity voice are no longer shown or played. The list survives restarts
// (mu_blocked_players.txt in the data folder). Reporting goes to the server
// as "/report <name> [reason]", which logs it for the GMs (GameServer
// CommandManager::CommandReport).
//
// App Store guideline 1.2 asks for both in any app where players can talk to
// each other; the player menu (tap/click a player) carries Block and Report.

#pragma once

#include <string>

class CPlayerBlock
{
public:
	bool IsBlocked(const char* name);

	// Returns true when the name is now blocked, false when now unblocked.
	bool Toggle(const char* name);

	// /block <name>, /unblock <name>, /blocklist. True when handled (the line
	// is not sent to the server). /report is left to the server.
	bool HandleCommand(const char* text);

	// Proximity voice: true when the speaker (the server's player index) is a
	// visible character on the block list. Game thread only.
	bool IsSpeakerBlocked(int speakerIndex);

	void Report(const char* name, const char* reason);

private:
	void Load();
	void Save();
	static std::string Key(const char* name);
	void Say(const char* text);

	bool m_Loaded = false;
	std::string m_List;	// "\n" + lowercased names, each followed by "\n"
	unsigned long m_SpeakerTick = 0;
	int m_BlockedSpeakers[64] = {};
	int m_BlockedSpeakerCount = 0;
};

extern CPlayerBlock gPlayerBlock;
