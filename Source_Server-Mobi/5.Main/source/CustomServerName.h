// -------------------------------------------------------------------------------

#pragma once

struct CUSTOM_SERVER_NAME_INFO
{
	int Index;
	char Name[32];
};

class CCustomServerName
{
public:
	CCustomServerName();
	~CCustomServerName();

	void Init();
	void Load(char* defaultName, CUSTOM_SERVER_NAME_INFO* info);
	void SetInfo(CUSTOM_SERVER_NAME_INFO info);
	CUSTOM_SERVER_NAME_INFO* GetInfo(int index);
	// ----
public:
	char m_szDefaultName[32];
	CUSTOM_SERVER_NAME_INFO m_CustomServerName[MAX_CUSTOM_SERVER_NAME];
	// ----
}; extern CCustomServerName gCustomServerName;
// -------------------------------------------------------------------------------
