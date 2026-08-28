#include "stdafx.h"
#include "CustomServerName.h"


CCustomServerName gCustomServerName;
// -------------------------------------------------------------------------------

CCustomServerName::CCustomServerName()
{
	this->Init();
}
// -------------------------------------------------------------------------------

CCustomServerName::~CCustomServerName()
{
	// ----
}
// -------------------------------------------------------------------------------

void CCustomServerName::Init() // OK
{
	strcpy_s(this->m_szDefaultName, "SeaSon 2");

	for (int n = 0;n < MAX_CUSTOM_SERVER_NAME;n++)
	{
		this->m_CustomServerName[n].Index = -1;
	}
}

void CCustomServerName::Load(char* defaultName, CUSTOM_SERVER_NAME_INFO* info) // OK
{
	strcpy_s(this->m_szDefaultName, defaultName);

	for (int n = 0;n < MAX_CUSTOM_SERVER_NAME;n++)
	{
		this->SetInfo(info[n]);
	}
}

void CCustomServerName::SetInfo(CUSTOM_SERVER_NAME_INFO info) // OK
{
	if (info.Index < 0 || info.Index >= MAX_CUSTOM_SERVER_NAME)
	{
		return;
	}
	this->m_CustomServerName[info.Index] = info;
}

CUSTOM_SERVER_NAME_INFO* CCustomServerName::GetInfo(int index) // OK
{
	if (index < 0 || index >= MAX_CUSTOM_SERVER_NAME)
	{
		return 0;
	}

	if (this->m_CustomServerName[index].Index != index)
	{
		return 0;
	}

	return &this->m_CustomServerName[index];
}
