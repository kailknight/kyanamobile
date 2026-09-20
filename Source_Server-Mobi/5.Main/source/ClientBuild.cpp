// ClientBuild.cpp - the client's build stamp, in one place.
//
// Separate from the version NUMBER in ClientBuild.h, and the difference matters:
//
//   MU_CLIENT_BUILD  is a number a server compares against. Android only, since
//                    only the APK has an externally-managed one.
//   this stamp        is a human-readable identifier for "which build am I
//                    looking at", derived from compile time. Both platforms have
//                    it, and no server ever compares it.
//
// Computed in a .cpp rather than a header on purpose. __DATE__ and __TIME__
// expand per translation unit, so a header would give every file its own stamp
// and the one shown would depend on which file happened to be asked.

#include "stdafx.h"
#include "ClientBuild.h"

// The stamp has to be honest or it is worse than absent: one showing an older
// time than the build actually is reads as "the install did not take" and sends
// debugging down a false trail. The two platforms keep it honest differently.
//
//   Android  __DATE__/__TIME__, with CMakeLists.txt touching THIS FILE before
//            every build (add_custom_command PRE_BUILD) so an incremental build
//            cannot leave the stamp behind. If that touch is ever repointed at
//            another file, this goes stale silently.
//
//   PC       the timestamp the linker wrote into this module's PE header. There
//            is no equivalent pre-build touch under MSBuild, and this file would
//            otherwise compile once and keep its first stamp forever. Reading
//            the PE header sidesteps the whole problem - it is written at link
//            time, so it cannot lag the binary, and unlike a filesystem
//            modified-time it travels with the exe when it is copied.

#if !defined(__ANDROID__) && !defined(MU_IOS)

// Seconds since 1970 UTC from the PE header, or 0 if it cannot be read.
static DWORD MU_GetModuleLinkTime()
{
	BYTE* pBase = (BYTE*)GetModuleHandle(NULL);

	if (pBase == NULL)
	{
		return 0;
	}

	const IMAGE_DOS_HEADER* pDos = (const IMAGE_DOS_HEADER*)pBase;

	if (pDos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return 0;
	}

	const IMAGE_NT_HEADERS* pNt = (const IMAGE_NT_HEADERS*)(pBase + pDos->e_lfanew);

	if (pNt->Signature != IMAGE_NT_SIGNATURE)
	{
		return 0;
	}

	return pNt->FileHeader.TimeDateStamp;
}

#endif

// YYMMDDHHMM from a broken-down time. Same ordering both platforms report and
// the server compares, and the same information the displayed stamp carries -
// just rearranged so that "larger" means "newer", which DDMMYY.HHMM does not.
//
// Needs a DWORD, not an int: 2609191943 is past the signed 32-bit maximum.
static DWORD MU_PackBuildNumber(const struct tm& t)
{
	return (DWORD)(
		  (DWORD)(((t.tm_year + 1900) % 100)) * 100000000u
		+ (DWORD)(t.tm_mon + 1)               *   1000000u
		+ (DWORD)(t.tm_mday)                  *     10000u
		+ (DWORD)(t.tm_hour)                  *       100u
		+ (DWORD)(t.tm_min));
}

DWORD MU_GetClientBuildNumber()
{
	static DWORD s_Build = 0xFFFFFFFF;

	if (s_Build != 0xFFFFFFFF)
	{
		return s_Build;
	}

	s_Build = 0;

#if !defined(__ANDROID__) && !defined(MU_IOS)

	// PC: the linker's timestamp, which is also what the displayed stamp uses -
	// so the number and the stamp can never disagree.
	const DWORD linkTime = MU_GetModuleLinkTime();

	if (linkTime != 0)
	{
		const time_t t = (time_t)linkTime;
		struct tm local = { 0 };

		if (localtime_s(&local, &t) == 0)
		{
			s_Build = MU_PackBuildNumber(local);
		}
	}

	if (s_Build != 0)
	{
		return s_Build;
	}

	// Falls through to the compile-time path when the PE timestamp is
	// unreadable, which is the same fallback the stamp uses.

#endif

	// Android, and the PC fallback: compile time. Kept honest by the PRE_BUILD
	// touch of this file in CMakeLists.txt.
	{
		static const char* const s_months[12] =
		{
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};

		char szMonth[4] = { 0 };
		int nDay = 0, nYear = 0, nHour = 0, nMinute = 0, nSecond = 0;

		sscanf(__DATE__, "%3s %d %d", szMonth, &nDay, &nYear);
		sscanf(__TIME__, "%d:%d:%d", &nHour, &nMinute, &nSecond);

		int nMonth = 1;

		for (int i = 0; i < 12; ++i)
		{
			if (_stricmp(szMonth, s_months[i]) == 0)
			{
				nMonth = i + 1;
				break;
			}
		}

		struct tm t = { 0 };
		t.tm_year = nYear - 1900;
		t.tm_mon = nMonth - 1;
		t.tm_mday = nDay;
		t.tm_hour = nHour;
		t.tm_min = nMinute;

		s_Build = MU_PackBuildNumber(t);
	}

	return s_Build;
}

const char* MU_GetClientBuildStamp()
{
	static char s_szStamp[32] = { 0 };

	if (s_szStamp[0] != '\0')
	{
		return s_szStamp;
	}

#if !defined(__ANDROID__) && !defined(MU_IOS)
	{
		const DWORD linkTime = MU_GetModuleLinkTime();

		// 0 means the header could not be read, or a deterministic build wrote a
		// hash there instead of a time. Falls through to the compile-time path
		// below rather than printing something meaningless.
		if (linkTime != 0)
		{
			const time_t t = (time_t)linkTime;
			struct tm local = { 0 };

			if (localtime_s(&local, &t) == 0)
			{
				sprintf(s_szStamp, "v.1.0.%02d%02d%02d.%02d%02d",
					local.tm_mday, local.tm_mon + 1, (local.tm_year + 1900) % 100,
					local.tm_hour, local.tm_min);

				return s_szStamp;
			}
		}
	}
#endif

	static const char* s_szMonths[12] =
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	char szMonth[4] = { 0 };
	int nDay = 0, nYear = 0, nHour = 0, nMinute = 0, nSecond = 0;

	// __DATE__ is "Mmm dd yyyy" - the month is a name, which is why this has to
	// be looked up rather than parsed as a number.
	sscanf(__DATE__, "%3s %d %d", szMonth, &nDay, &nYear);
	sscanf(__TIME__, "%d:%d:%d", &nHour, &nMinute, &nSecond);

	int nMonth = 1;

	for (int i = 0; i < 12; ++i)
	{
		if (_stricmp(szMonth, s_szMonths[i]) == 0)
		{
			nMonth = i + 1;
			break;
		}
	}

	// Day-month-year then time, which sorts the way a human reads it when
	// comparing two builds from the same week.
	sprintf(s_szStamp, "v.1.0.%02d%02d%02d.%02d%02d",
		nDay, nMonth, nYear % 100, nHour, nMinute);

	return s_szStamp;
}
