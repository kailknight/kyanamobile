// PlayBilling.cpp: Google Play WCoin (C) recharge, DataServer side.
// See PlayBilling.h for the flow.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PlayBilling.h"
#include "QueryManager.h"
#include "SocketManager.h"
#include "ServerDisplayer.h"
#include "Util.h"

#include <wincrypt.h>
#include <winhttp.h>
#include <time.h>
#include <deque>
#include <string>

// _WIN32_WINNT is XP (StdAfx.h), which hides the SHA-256 id; the provider
// itself has had it since XP SP3.
#ifndef CALG_SHA_256
#define CALG_SHA_256 (ALG_CLASS_HASH | ALG_TYPE_ANY | 12)
#endif

#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"crypt32.lib")
#pragma comment(lib,"advapi32.lib")

CPlayBilling gPlayBilling;

namespace
{
	const char* kConfigFile = ".\\PlayBilling.ini";

	// Token, product id and order id go into URLs and SQL as-is, so only the
	// characters Google actually uses get through.
	bool IsSafeToken(const char* text,size_t maxLen,bool allowEmpty)
	{
		if(text == NULL)
			return false;

		size_t len = strnlen(text,maxLen + 1);

		if(len > maxLen || (len == 0 && !allowEmpty))
			return false;

		for(size_t n = 0; n < len; n++)
		{
			char c = text[n];
			bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';

			if(!ok)
				return false;
		}

		return true;
	}

	void CopyField(char* dst,size_t dstSize,const char* src)
	{
		memset(dst,0,dstSize);

		if(src != NULL)
			strncpy_s(dst,dstSize,src,_TRUNCATE);
	}

	// ---- tiny JSON reader ---------------------------------------------------
	// Only ever reads Google's own responses and the service account key, and
	// only top-level scalar fields from them.

	bool JsonFind(const std::string& json,const char* key,std::string* out)
	{
		std::string needle = std::string("\"") + key + "\"";
		size_t pos = json.find(needle);

		if(pos == std::string::npos)
			return false;

		pos = json.find(':',pos + needle.size());

		if(pos == std::string::npos)
			return false;

		pos++;

		while(pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
			pos++;

		if(pos >= json.size())
			return false;

		out->clear();

		if(json[pos] == '"')
		{
			for(pos++; pos < json.size() && json[pos] != '"'; pos++)
			{
				char c = json[pos];

				if(c == '\\' && pos + 1 < json.size())
				{
					char e = json[++pos];

					switch(e)
					{
					case 'n': out->push_back('\n'); break;
					case 'r': out->push_back('\r'); break;
					case 't': out->push_back('\t'); break;
					case 'u': pos += 4; out->push_back('?'); break;	// not in anything we read
					default: out->push_back(e); break;
					}
				}
				else
				{
					out->push_back(c);
				}
			}

			return true;
		}

		while(pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ' ' && json[pos] != '\r' && json[pos] != '\n')
		{
			out->push_back(json[pos++]);
		}

		return !out->empty();
	}

	int JsonInt(const std::string& json,const char* key,int fallback)
	{
		std::string value;

		if(!JsonFind(json,key,&value))
			return fallback;

		return atoi(value.c_str());
	}

	// ---- base64url ----------------------------------------------------------

	std::string Base64Url(const BYTE* data,size_t len)
	{
		static const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		std::string out;
		out.reserve((len + 2) / 3 * 4);

		for(size_t i = 0; i < len; i += 3)
		{
			DWORD v = data[i] << 16;

			if(i + 1 < len) v |= data[i + 1] << 8;
			if(i + 2 < len) v |= data[i + 2];

			out.push_back(kAlphabet[(v >> 18) & 63]);
			out.push_back(kAlphabet[(v >> 12) & 63]);

			if(i + 1 < len) out.push_back(kAlphabet[(v >> 6) & 63]);
			if(i + 2 < len) out.push_back(kAlphabet[v & 63]);
		}

		return out;
	}

	std::string Base64Url(const std::string& text)
	{
		return Base64Url((const BYTE*)text.data(),text.size());
	}

	// ---- RS256 with the service account key ---------------------------------

	bool SignRs256(const std::string& pemKey,const std::string& input,std::string* signatureB64)
	{
		bool ok = false;
		BYTE* der = NULL;
		DWORD derLen = 0;
		CRYPT_PRIVATE_KEY_INFO* pki = NULL;
		DWORD pkiLen = 0;
		BYTE* keyBlob = NULL;
		DWORD keyBlobLen = 0;
		HCRYPTPROV prov = 0;
		HCRYPTKEY key = 0;
		HCRYPTHASH hash = 0;

		if(!CryptStringToBinaryA(pemKey.c_str(),0,CRYPT_STRING_BASE64HEADER,NULL,&derLen,NULL,NULL))
			goto done;

		der = new BYTE[derLen];

		if(!CryptStringToBinaryA(pemKey.c_str(),0,CRYPT_STRING_BASE64HEADER,der,&derLen,NULL,NULL))
			goto done;

		// Google's key is PKCS#8 ("BEGIN PRIVATE KEY"): unwrap it to the
		// PKCS#1 RSA key inside, then to a CryptoAPI PRIVATEKEYBLOB.
		if(!CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,PKCS_PRIVATE_KEY_INFO,der,derLen,CRYPT_DECODE_ALLOC_FLAG,NULL,&pki,&pkiLen))
			goto done;

		if(!CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,PKCS_RSA_PRIVATE_KEY,pki->PrivateKey.pbData,pki->PrivateKey.cbData,CRYPT_DECODE_ALLOC_FLAG,NULL,&keyBlob,&keyBlobLen))
			goto done;

		if(!CryptAcquireContextA(&prov,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))
			goto done;

		if(!CryptImportKey(prov,keyBlob,keyBlobLen,0,0,&key))
			goto done;

		if(!CryptCreateHash(prov,CALG_SHA_256,0,0,&hash))
			goto done;

		if(!CryptHashData(hash,(const BYTE*)input.data(),(DWORD)input.size(),0))
			goto done;

		{
			DWORD sigLen = 0;

			if(!CryptSignHashA(hash,AT_KEYEXCHANGE,NULL,0,NULL,&sigLen))
				goto done;

			BYTE* sig = new BYTE[sigLen];

			if(CryptSignHashA(hash,AT_KEYEXCHANGE,NULL,0,sig,&sigLen))
			{
				// CryptoAPI hands the signature back little-endian; JWT wants
				// it big-endian.
				for(DWORD i = 0; i < sigLen / 2; i++)
				{
					BYTE t = sig[i];
					sig[i] = sig[sigLen - 1 - i];
					sig[sigLen - 1 - i] = t;
				}

				*signatureB64 = Base64Url(sig,sigLen);
				ok = true;
			}

			delete[] sig;
		}

	done:
		if(hash) CryptDestroyHash(hash);
		if(key) CryptDestroyKey(key);
		if(prov) CryptReleaseContext(prov,0);
		if(keyBlob) LocalFree(keyBlob);
		if(pki) LocalFree(pki);
		delete[] der;
		return ok;
	}

	// ---- HTTPS --------------------------------------------------------------

	std::wstring Widen(const std::string& s)
	{
		return std::wstring(s.begin(),s.end());
	}

	bool HttpsRequest(const char* host,const char* method,const std::string& path,const std::string& headers,const std::string& body,DWORD* status,std::string* response)
	{
		*status = 0;
		response->clear();

		bool ok = false;
		HINTERNET session = WinHttpOpen(L"KiraDataServer/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
		HINTERNET connect = NULL;
		HINTERNET request = NULL;

		if(session == NULL)
			return false;

		WinHttpSetTimeouts(session,10000,10000,15000,15000);

		connect = WinHttpConnect(session,Widen(host).c_str(),INTERNET_DEFAULT_HTTPS_PORT,0);

		if(connect != NULL)
		{
			request = WinHttpOpenRequest(connect,Widen(method).c_str(),Widen(path).c_str(),NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
		}

		if(request != NULL)
		{
			std::wstring wHeaders = Widen(headers);

			if(WinHttpSendRequest(request,wHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wHeaders.c_str(),(DWORD)-1L,
				body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),(DWORD)body.size(),(DWORD)body.size(),0)
				&& WinHttpReceiveResponse(request,NULL))
			{
				DWORD code = 0;
				DWORD codeSize = sizeof(code);

				WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&codeSize,WINHTTP_NO_HEADER_INDEX);
				*status = code;

				for(;;)
				{
					DWORD avail = 0;

					if(!WinHttpQueryDataAvailable(request,&avail) || avail == 0)
						break;

					std::string chunk(avail,'\0');
					DWORD read = 0;

					if(!WinHttpReadData(request,&chunk[0],avail,&read) || read == 0)
						break;

					response->append(chunk.data(),read);

					if(response->size() > 64 * 1024)
						break;
				}

				ok = true;
			}
		}

		if(request) WinHttpCloseHandle(request);
		if(connect) WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return ok;
	}

	// ---- worker -------------------------------------------------------------

	enum { JOB_VERIFY = 0, JOB_CONSUME = 1 };

	struct PlayJob
	{
		int Type;
		int Index;						// GameServer connection
		SDHP_PLAY_PURCHASE_RECV Req;
	};

	CRITICAL_SECTION g_JobLock;
	HANDLE g_JobSemaphore = NULL;
	std::deque<PlayJob> g_Jobs;

	// Worker-thread only.
	std::string g_ClientEmail;
	std::string g_PrivateKey;
	std::string g_AccessToken;
	__time64_t g_AccessTokenExpiry = 0;
	char g_WorkerKeyFile[MAX_PATH];
	char g_WorkerPackage[128];

	bool LoadKeyFile()
	{
		if(!g_PrivateKey.empty())
			return true;

		FILE* f = NULL;

		if(fopen_s(&f,g_WorkerKeyFile,"rb") != 0 || f == NULL)
		{
			LogAdd(LOG_RED,"[PlayBilling] Cannot open key file %s",g_WorkerKeyFile);
			return false;
		}

		std::string json;
		char buf[1024];
		size_t n;

		while((n = fread(buf,1,sizeof(buf),f)) > 0)
			json.append(buf,n);

		fclose(f);

		if(!JsonFind(json,"client_email",&g_ClientEmail) || !JsonFind(json,"private_key",&g_PrivateKey))
		{
			LogAdd(LOG_RED,"[PlayBilling] Key file %s has no client_email/private_key",g_WorkerKeyFile);
			g_PrivateKey.clear();
			return false;
		}

		return true;
	}

	// OAuth2 service-account flow: a JWT signed with the key, swapped for an
	// access token that is good for an hour.
	bool GetAccessToken(std::string* token)
	{
		__time64_t now = _time64(NULL);

		if(!g_AccessToken.empty() && now < g_AccessTokenExpiry - 60)
		{
			*token = g_AccessToken;
			return true;
		}

		if(!LoadKeyFile())
			return false;

		char claims[512];
		sprintf_s(claims,sizeof(claims),
			"{\"iss\":\"%s\",\"scope\":\"https://www.googleapis.com/auth/androidpublisher\",\"aud\":\"https://oauth2.googleapis.com/token\",\"iat\":%lld,\"exp\":%lld}",
			g_ClientEmail.c_str(),(long long)now,(long long)(now + 3600));

		std::string unsignedJwt = Base64Url(std::string("{\"alg\":\"RS256\",\"typ\":\"JWT\"}")) + "." + Base64Url(std::string(claims));
		std::string signature;

		if(!SignRs256(g_PrivateKey,unsignedJwt,&signature))
		{
			LogAdd(LOG_RED,"[PlayBilling] Signing the token request failed (error %d) - is the key file intact?",GetLastError());
			return false;
		}

		std::string body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + unsignedJwt + "." + signature;
		DWORD status = 0;
		std::string response;

		if(!HttpsRequest("oauth2.googleapis.com","POST","/token","Content-Type: application/x-www-form-urlencoded\r\n",body,&status,&response) || status != 200)
		{
			LogAdd(LOG_RED,"[PlayBilling] Google token request failed (HTTP %d)",status);
			return false;
		}

		std::string accessToken;

		if(!JsonFind(response,"access_token",&accessToken))
			return false;

		g_AccessToken = accessToken;
		g_AccessTokenExpiry = now + JsonInt(response,"expires_in",3600);
		*token = g_AccessToken;
		return true;
	}

	std::string PurchasePath(const char* productId,const char* purchaseToken)
	{
		return std::string("/androidpublisher/v3/applications/") + g_WorkerPackage + "/purchases/products/" + productId + "/tokens/" + purchaseToken;
	}

	void RunVerify(const PlayJob& job)
	{
		SDHP_PLAY_PURCHASE_VERIFIED msg;
		memset(&msg,0,sizeof(msg));
		msg.header.set(0xD9,0x38,sizeof(msg));
		msg.Magic = CPlayBilling::kVerifiedMagic;
		msg.aIndex = job.Req.aIndex;
		memcpy(msg.AccountID,job.Req.AccountID,sizeof(msg.AccountID));
		memcpy(msg.ProductId,job.Req.ProductId,sizeof(msg.ProductId));
		memcpy(msg.Token,job.Req.Token,sizeof(msg.Token));
		msg.Result = PLAY_RESULT_RETRY;

		std::string access;

		if(GetAccessToken(&access))
		{
			DWORD status = 0;
			std::string response;

			if(HttpsRequest("androidpublisher.googleapis.com","GET",PurchasePath(job.Req.ProductId,job.Req.Token),"Authorization: Bearer " + access + "\r\n","",&status,&response))
			{
				if(status == 200)
				{
					// purchaseState: 0 purchased, 1 cancelled, 2 pending.
					int state = JsonInt(response,"purchaseState",-1);
					std::string orderId;
					JsonFind(response,"orderId",&orderId);

					if(IsSafeToken(orderId.c_str(),PLAY_ORDER_ID_SIZE - 1,true))
						CopyField(msg.OrderId,sizeof(msg.OrderId),orderId.c_str());

					msg.Consumed = (JsonInt(response,"consumptionState",0) == 1) ? 1 : 0;
					msg.Result = (state == 0) ? PLAY_RESULT_CREDITED : ((state == 2) ? PLAY_RESULT_PENDING : PLAY_RESULT_INVALID);
				}
				else if(status == 400 || status == 404 || status == 410)
				{
					msg.Result = PLAY_RESULT_INVALID;
				}
				else
				{
					// 401/403 here is nearly always the service account's Play
					// Console permission not having propagated yet (up to 24h).
					LogAdd(LOG_RED,"[PlayBilling] Purchase lookup failed (HTTP %d) for %s",status,job.Req.AccountID);

					if(status == 401)
						g_AccessToken.clear();
				}
			}
		}

		gSocketManager.PostInternal(job.Index,0xD9,(BYTE*)&msg,sizeof(msg));
	}

	void RunConsume(const PlayJob& job)
	{
		std::string access;

		if(!GetAccessToken(&access))
			return;

		DWORD status = 0;
		std::string response;

		HttpsRequest("androidpublisher.googleapis.com","POST",PurchasePath(job.Req.ProductId,job.Req.Token) + ":consume",
			"Authorization: Bearer " + access + "\r\nContent-Length: 0\r\n","",&status,&response);

		if(status != 200 && status != 204)
		{
			// Not fatal: the purchase stays unconsumed, the phone reports it
			// again next shop open, the credit proc answers "already" and this
			// runs again.
			LogAdd(LOG_RED,"[PlayBilling] Consume failed (HTTP %d) for %s %s",status,job.Req.AccountID,job.Req.ProductId);
		}
	}

	DWORD WINAPI PlayBillingWorker(LPVOID)
	{
		for(;;)
		{
			WaitForSingleObject(g_JobSemaphore,INFINITE);

			PlayJob job;
			bool have = false;

			EnterCriticalSection(&g_JobLock);

			if(!g_Jobs.empty())
			{
				job = g_Jobs.front();
				g_Jobs.pop_front();
				have = true;
			}

			LeaveCriticalSection(&g_JobLock);

			if(!have)
				continue;

			if(job.Type == JOB_VERIFY)
				RunVerify(job);
			else
				RunConsume(job);
		}

		return 0;
	}

	bool PushJob(const PlayJob& job)
	{
		if(g_JobSemaphore == NULL)
			return false;

		bool pushed = false;

		EnterCriticalSection(&g_JobLock);

		if(g_Jobs.size() < 256)
		{
			g_Jobs.push_back(job);
			pushed = true;
		}

		LeaveCriticalSection(&g_JobLock);

		if(pushed)
			ReleaseSemaphore(g_JobSemaphore,1,NULL);

		return pushed;
	}
}

CPlayBilling::CPlayBilling()
{
	this->m_Enabled = false;
	this->m_KeyFile[0] = 0;
	this->m_PackageName[0] = 0;
}

void CPlayBilling::LoadConfig()
{
	this->m_Enabled = GetPrivateProfileIntA("PlayBilling","Enabled",0,kConfigFile) != 0;
	GetPrivateProfileStringA("PlayBilling","KeyFile",".\\PlayBilling.json",this->m_KeyFile,sizeof(this->m_KeyFile),kConfigFile);
	GetPrivateProfileStringA("PlayBilling","PackageName","com.worldofkira",this->m_PackageName,sizeof(this->m_PackageName),kConfigFile);

	if(!IsSafeToken(this->m_PackageName,sizeof(this->m_PackageName) - 1,false))
	{
		LogAdd(LOG_RED,"[PlayBilling] PackageName '%s' is not valid - disabled",this->m_PackageName);
		this->m_Enabled = false;
	}

	LogAdd(LOG_BLACK,"[PlayBilling] %s (package %s)",this->m_Enabled ? "Enabled" : "Disabled",this->m_PackageName);
}

void CPlayBilling::StartWorker()
{
	if(g_JobSemaphore != NULL)
		return;

	// The worker reads these without a lock, so they are copied once, here,
	// before it starts. A changed key file or package needs a restart.
	strcpy_s(g_WorkerKeyFile,sizeof(g_WorkerKeyFile),this->m_KeyFile);
	strcpy_s(g_WorkerPackage,sizeof(g_WorkerPackage),this->m_PackageName);

	InitializeCriticalSection(&g_JobLock);
	g_JobSemaphore = CreateSemaphore(NULL,0,0x7FFFFFFF,NULL);
	CloseHandle(CreateThread(NULL,0,PlayBillingWorker,NULL,0,NULL));
}

void CPlayBilling::SendResult(int index,WORD aIndex,const char* account,const char* productId,BYTE result,DWORD wcoin)
{
	SDHP_PLAY_PURCHASE_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x37,sizeof(pMsg));
	pMsg.aIndex = aIndex;
	memcpy(pMsg.AccountID,account,sizeof(pMsg.AccountID));
	memcpy(pMsg.ProductId,productId,sizeof(pMsg.ProductId));
	pMsg.Result = result;
	pMsg.WCoinC = wcoin;
	gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
}

void CPlayBilling::OnPurchaseRequest(SDHP_PLAY_PURCHASE_RECV* lpMsg,int index)
{
	lpMsg->AccountID[sizeof(lpMsg->AccountID) - 1] = 0;
	lpMsg->ProductId[sizeof(lpMsg->ProductId) - 1] = 0;
	lpMsg->Token[sizeof(lpMsg->Token) - 1] = 0;

	if(!this->m_Enabled)
	{
		this->SendResult(index,lpMsg->aIndex,lpMsg->AccountID,lpMsg->ProductId,PLAY_RESULT_DISABLED,0);
		return;
	}

	if(!IsSafeToken(lpMsg->AccountID,10,false) || !IsSafeToken(lpMsg->ProductId,PLAY_PRODUCT_ID_SIZE - 1,false) || !IsSafeToken(lpMsg->Token,PLAY_TOKEN_SIZE - 1,false))
	{
		this->SendResult(index,lpMsg->aIndex,lpMsg->AccountID,lpMsg->ProductId,PLAY_RESULT_INVALID,0);
		return;
	}

	if(GetPrivateProfileIntA("Products",lpMsg->ProductId,0,kConfigFile) <= 0)
	{
		this->SendResult(index,lpMsg->aIndex,lpMsg->AccountID,lpMsg->ProductId,PLAY_RESULT_UNKNOWN_PRODUCT,0);
		return;
	}

	PlayJob job;
	job.Type = JOB_VERIFY;
	job.Index = index;
	job.Req = *lpMsg;

	if(!PushJob(job))
	{
		this->SendResult(index,lpMsg->aIndex,lpMsg->AccountID,lpMsg->ProductId,PLAY_RESULT_RETRY,0);
	}
}

void CPlayBilling::OnPurchaseVerified(SDHP_PLAY_PURCHASE_VERIFIED* lpMsg,int index)
{
	if(lpMsg->Magic != kVerifiedMagic)
		return;

	if(lpMsg->Result != PLAY_RESULT_CREDITED)
	{
		this->SendResult(index,lpMsg->aIndex,lpMsg->AccountID,lpMsg->ProductId,lpMsg->Result,0);
		return;
	}

	const int coins = GetPrivateProfileIntA("Products",lpMsg->ProductId,0,kConfigFile);

	if(coins <= 0)
	{
		this->SendResult(index,lpMsg->aIndex,lpMsg->AccountID,lpMsg->ProductId,PLAY_RESULT_UNKNOWN_PRODUCT,0);
		return;
	}

	// One transaction keyed on the token: a replayed token can never be
	// credited twice (PlayBilling_Schema.sql).
	BYTE result = PLAY_RESULT_RETRY;

	if(gQueryManager.ExecQuery("EXEC WZ_PlayStoreCredit '%s','%s','%s','%s',%d,%d",
		lpMsg->Token,lpMsg->OrderId,lpMsg->AccountID,lpMsg->ProductId,coins,lpMsg->Consumed) != 0
		&& gQueryManager.Fetch() != SQL_NO_DATA)
	{
		int r = gQueryManager.GetAsInteger("Result");
		result = (r == 0) ? PLAY_RESULT_CREDITED : ((r == 1) ? PLAY_RESULT_ALREADY : PLAY_RESULT_INVALID);
	}

	gQueryManager.Close();

	if(result == PLAY_RESULT_CREDITED)
	{
		LogAdd(LOG_GREEN,"[PlayBilling] %s bought %s: +%d WCoin (C) (order %s)",lpMsg->AccountID,lpMsg->ProductId,coins,lpMsg->OrderId);
	}

	this->SendResult(index,lpMsg->aIndex,lpMsg->AccountID,lpMsg->ProductId,result,(result == PLAY_RESULT_CREDITED) ? (DWORD)coins : 0);

	// Credited now or before: finish it with Google so the pack can be bought
	// again and the purchase is not auto-refunded.
	if((result == PLAY_RESULT_CREDITED || result == PLAY_RESULT_ALREADY) && !lpMsg->Consumed)
	{
		PlayJob job;
		memset(&job,0,sizeof(job));
		job.Type = JOB_CONSUME;
		job.Index = index;
		memcpy(job.Req.AccountID,lpMsg->AccountID,sizeof(job.Req.AccountID));
		memcpy(job.Req.ProductId,lpMsg->ProductId,sizeof(job.Req.ProductId));
		memcpy(job.Req.Token,lpMsg->Token,sizeof(job.Req.Token));
		PushJob(job);
	}
}
