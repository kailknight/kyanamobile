// Message.h: interface for the CMessage class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

// 256 to match the notice packet's own message buffer - the old 128 truncated
// long lines well before the wire format required it.
struct MESSAGE_INFO
{
	int Index;
	char Message[256];
};

class CMessage
{
public:
	CMessage();
	virtual ~CMessage();
	void Load(char* path);
	char* GetMessage(int index);
private:
	char m_DefaultMessage[128];
	std::map<int,MESSAGE_INFO> m_MessageInfo;
};

extern CMessage gMessage;
