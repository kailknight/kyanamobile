//************************************************************************
//
// Decompiled by @myheart, @synth3r
// <https://forum.ragezone.com/members/2000236254.html>
//
//
// FILE: FTPFileDownLoader.cpp
//
//

#include "stdafx.h"
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
#include "FTPFileDownLoader.h"
CFTPFileDownLoader::CFTPFileDownLoader() // OK
{
	this->m_Break = 0;
	this->m_pFileDownloader = NULL;
}

CFTPFileDownLoader::~CFTPFileDownLoader() // OK
{
#ifndef __ANDROID__
	SAFE_DELETE(this->m_pFileDownloader);
#endif // __ANDROID__ - m_pFileDownloader is never assigned on Android (see
	   // the #else DownLoadFiles() below), so there is nothing to delete,
	   // and FileDownloader's destructor is never linked in.
}

#ifndef __ANDROID__
WZResult CFTPFileDownLoader::DownLoadFiles(DownloaderType type,
										   std::string strServerIP,
										   unsigned short PortNum,
										   std::string strUserName,
										   std::string strPWD,
										   std::string strRemotepath,
										   std::string strlocalpath,
										   bool bPassiveMode,
										   CListVersionInfo Version,
										   std::vector<std::string>	vScriptFiles) // OK
{
	static WZResult result;

	result.BuildSuccessResult();

	DownloadServerInfo ServerInfo;
	DownloadFileInfo FileInfo;

	ServerInfo.SetPassiveMode(bPassiveMode);
	ServerInfo.SetOverWrite(1);
	ServerInfo.SetDownloaderType(FTP);
	ServerInfo.SetConnectTimeout(3000);
	ServerInfo.SetServerInfo((TCHAR*)strServerIP.c_str(),PortNum,(TCHAR*)strUserName.c_str(),(TCHAR*)strPWD.c_str());

	char Buffer[MAX_PATH] = {0};

	StringCchPrintfA(Buffer,sizeof(Buffer),"%03d.%04d.%03d",Version.Zone,Version.year,Version.yearId);

	strRemotepath += Buffer;
	strRemotepath += "/";
	strlocalpath += Buffer;
	strlocalpath += "\\";

	for(std::vector<std::string>::iterator it = vScriptFiles.begin();it!=vScriptFiles.end();it++)
	{
		std::string lPath = strlocalpath+(*it);
		std::string rPath = strRemotepath+(*it);

		FileInfo.SetFilePath((TCHAR*)it->c_str(),(TCHAR*)lPath.c_str(),(TCHAR*)rPath.c_str(),NULL);

		this->m_pFileDownloader = new FileDownloader(NULL,&ServerInfo,&FileInfo);

		result = this->m_pFileDownloader->DownloadFile();

		SAFE_DELETE(this->m_pFileDownloader);

		if(this->m_Break != 0)
		{
			result.SetResult(1,0,"Time Out Break");
			break;
		}

		if(!result.IsSuccess())
		{
			break;
		}
	}

	return result;
}

void	CFTPFileDownLoader::Break() // OK
{
	this->m_Break = 1;
	if(this->m_pFileDownloader!=NULL)
		m_pFileDownloader->Break();
}
#else // __ANDROID__
// The real implementation above is WinINet-based FTP/HTTP, which does not
// exist on Android. It is unreachable in practice - CListManager::LoadScriptList
// skips downloading whenever the local script files already exist, and the
// shop ships those files as Android assets - but the symbols must still
// link, since CListManager unconditionally new/deletes a CFTPFileDownLoader.
// m_pFileDownloader is never assigned here, so FileDownloader (the actual
// WinINet class, excluded from the Android build) never needs to be linked.
WZResult CFTPFileDownLoader::DownLoadFiles(DownloaderType, std::string, unsigned short,
										   std::string, std::string, std::string, std::string,
										   bool, CListVersionInfo, std::vector<std::string>)
{
	WZResult result;
	result.SetResult(ERROR_LOAD_SCRIPT, 0, "Cash shop live update is not available on this platform");
	return result;
}

void	CFTPFileDownLoader::Break()
{
	this->m_Break = 1;
}
#endif // __ANDROID__

BOOL CFTPFileDownLoader::CreateFolder(std::string strFilePath) // OK
{
	if(GetFileAttributesA(strFilePath.c_str())==INVALID_FILE_ATTRIBUTES)
	{
		return CreateDirectoryA(strFilePath.c_str(),0);
	}

	return 1;
}
#endif