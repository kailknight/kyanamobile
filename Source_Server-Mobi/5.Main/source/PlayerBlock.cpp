// PlayerBlock.cpp: see PlayerBlock.h.

#include "stdafx.h"
#include "PlayerBlock.h"
#include "ZzzCharacter.h"
#include "NewUISystem.h"
#include "wsclientinline.h"

#include <cctype>
#include <cstdio>
#include <cstring>

CPlayerBlock gPlayerBlock;

namespace
{
	const char* kBlockFile = "mu_blocked_players.txt";
	const int kMaxBlocked = 200;
}

std::string CPlayerBlock::Key(const char* name)
{
	std::string key;

	for (const char* p = name; p != NULL && *p != 0 && key.size() < MAX_ID_SIZE; p++)
	{
		if (*p == ' ' || *p == '\n' || *p == '\r')
		{
			break;
		}
		key.push_back((char)tolower((unsigned char)*p));
	}

	return key;
}

void CPlayerBlock::Load()
{
	if (this->m_Loaded)
	{
		return;
	}
	this->m_Loaded = true;
	this->m_List = "\n";

	FILE* file = fopen(kBlockFile, "r");
	if (file == NULL)
	{
		return;
	}

	char line[64];
	while (fgets(line, sizeof(line), file) != NULL)
	{
		std::string key = Key(line);
		if (!key.empty() && this->m_List.find("\n" + key + "\n") == std::string::npos)
		{
			this->m_List += key + "\n";
		}
	}
	fclose(file);
}

void CPlayerBlock::Save()
{
	FILE* file = fopen(kBlockFile, "w");
	if (file == NULL)
	{
		return;
	}
	fputs(this->m_List.c_str() + 1, file);
	fclose(file);
}

bool CPlayerBlock::IsBlocked(const char* name)
{
	this->Load();
	std::string key = Key(name);
	return !key.empty() && this->m_List.find("\n" + key + "\n") != std::string::npos;
}

bool CPlayerBlock::Toggle(const char* name)
{
	this->Load();
	std::string key = Key(name);

	if (key.empty())
	{
		return false;
	}

	const size_t at = this->m_List.find("\n" + key + "\n");

	if (at != std::string::npos)
	{
		this->m_List.erase(at, key.size() + 1);
		this->Save();
		this->m_SpeakerTick = 0;
		return false;
	}

	if (std::count(this->m_List.begin(), this->m_List.end(), '\n') > kMaxBlocked)
	{
		this->Say("Your block list is full. Unblock someone first (/unblock <name>).");
		return false;
	}

	this->m_List += key + "\n";
	this->Save();
	this->m_SpeakerTick = 0;
	return true;
}

void CPlayerBlock::Say(const char* text)
{
	if (g_pChatListBox != NULL)
	{
		g_pChatListBox->AddText("", text, SEASON3B::TYPE_SYSTEM_MESSAGE);
	}
}

void CPlayerBlock::Report(const char* name, const char* reason)
{
	char line[MAX_CHAT_SIZE + 1];
	sprintf_s(line, sizeof(line), "/report %s %s", name, (reason != NULL && reason[0] != 0) ? reason : "abusive behaviour");
	SendChat(line);
}

bool CPlayerBlock::HandleCommand(const char* text)
{
	if (text == NULL || text[0] != '/')
	{
		return false;
	}

	const bool block = _strnicmp(text, "/block ", 7) == 0;
	const bool unblock = _strnicmp(text, "/unblock ", 9) == 0;

	if (block || unblock)
	{
		const char* name = text + (block ? 7 : 9);
		while (*name == ' ')
		{
			name++;
		}

		if (Key(name).empty())
		{
			this->Say("Usage: /block <name>  or  /unblock <name>");
			return true;
		}

		char message[96];
		if (this->IsBlocked(name) == block)
		{
			sprintf_s(message, sizeof(message), block ? "%s is already blocked." : "%s is not blocked.", Key(name).c_str());
		}
		else
		{
			const bool now = this->Toggle(name);
			sprintf_s(message, sizeof(message), now ? "Blocked %s: you will no longer see their chat or hear their voice." : "Unblocked %s.", Key(name).c_str());
		}
		this->Say(message);
		return true;
	}

	if (_stricmp(text, "/blocklist") == 0)
	{
		this->Load();
		std::string names = this->m_List.size() > 1 ? this->m_List.substr(1) : "";
		for (char& c : names)
		{
			if (c == '\n') c = ' ';
		}
		std::string message = names.empty() ? "You have not blocked anyone." : "Blocked: " + names;
		this->Say(message.c_str());
		return true;
	}

	return false;
}

bool CPlayerBlock::IsSpeakerBlocked(int speakerIndex)
{
	this->Load();

	if (this->m_List.size() <= 1)
	{
		return false;
	}

	// Rebuilt twice a second: which visible characters are blocked.
	const unsigned long now = GetTickCount();
	if (this->m_SpeakerTick == 0 || now - this->m_SpeakerTick > 500)
	{
		this->m_SpeakerTick = (now == 0) ? 1 : now;
		this->m_BlockedSpeakerCount = 0;

		for (int i = 0; i < MAX_CHARACTERS_CLIENT && this->m_BlockedSpeakerCount < 64; i++)
		{
			CHARACTER* c = &CharactersClient[i];
			if (c->Object.Live && c->ID[0] != 0 && this->IsBlocked(c->ID))
			{
				this->m_BlockedSpeakers[this->m_BlockedSpeakerCount++] = c->Key & 0x7FFF;
			}
		}
	}

	for (int i = 0; i < this->m_BlockedSpeakerCount; i++)
	{
		if (this->m_BlockedSpeakers[i] == (speakerIndex & 0x7FFF))
		{
			return true;
		}
	}
	return false;
}
