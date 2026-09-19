// VoiceSettings.cpp - where the player's voice preferences live.
//
// The client has two settings stores and they are not the same one:
//
//   PC       HKCU\SOFTWARE\Webzen\Mu\Config, through leaf::CRegKey. This is
//            what SaveConfigDword and the options window already use.
//   Android  GameConfig, an ini wrapper. GameConfig exists ONLY on Android -
//            it is not in Main.vcxproj at all, and its Windows branch pulls in
//            DPAPI, which would mean linking crypt32 into the PC client for
//            the sake of four integers.
//
// Rather than teach the rest of the voice code about that split, it is dealt
// with once, here. Everything else calls these eight functions.
//
// NOTE: on Android nothing persists. GameConfig::Load and Save are no-ops
// there, so these values reset every launch. That is a pre-existing gap across
// every setting in the client, not something voice introduced, but it does
// mean a mobile player who mutes has to mute again next time - which is why
// the defaults are the common case rather than the cautious one.

#include "stdafx.h"
#include "VoiceClient.h"

#if defined(__ANDROID__) || defined(MU_IOS)
#include "GameConfig/GameConfig.h"
#else
#include "Winmain.h"
#include "./ExternalObject/leaf/regkey.h"
#endif

#include "GameConfig/GameConfigConstants.h"

// The stops the options row cycles through, shared by both platforms. 100 is
// auto-levelling alone; the rest are for microphones quiet enough that the
// levelling hits its own ceiling before reaching a usable level.
namespace
{
	const int kMicBoostSteps[] = { 100, 150, 200, 300, 400, 600 };
	const int kMicBoostStepCount = (int)(sizeof(kMicBoostSteps) / sizeof(kMicBoostSteps[0]));
}

int VoiceSettingNextMicBoost(int iCurrentPercent)
{
	for (int n = 0; n < kMicBoostStepCount; n++)
	{
		if (kMicBoostSteps[n] == iCurrentPercent)
		{
			return kMicBoostSteps[(n + 1) % kMicBoostStepCount];
		}
	}

	// A hand-edited value that is not one of the stops rejoins the cycle at the
	// start rather than being stuck outside it.
	return kMicBoostSteps[0];
}

#if !defined(__ANDROID__) && !defined(MU_IOS)

namespace
{
	// The same key the rest of the client's config already lives under, so
	// voice settings travel with everything else rather than inventing a
	// second home for themselves.
	const char* kVoiceRegPath = "SOFTWARE\\Webzen\\Mu\\Config";

	int ReadVoiceDword(const char* szName, int iDefault)
	{
		leaf::CRegKey regkey;
		regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, kVoiceRegPath);

		DWORD value = 0;

		if (regkey.ReadDword(szName, value) == false)
		{
			return iDefault;
		}

		return (int)value;
	}

	void WriteVoiceDword(const char* szName, int iValue)
	{
		leaf::CRegKey regkey;
		regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, kVoiceRegPath);
		regkey.WriteDword(szName, (DWORD)iValue);
	}
}

bool VoiceSettingGetEnabled()
{
	return (ReadVoiceDword("VoiceEnabled", CfgDefaults::CfgDefaultVoiceEnabled ? 1 : 0) != 0);
}

void VoiceSettingSetEnabled(bool bEnabled)
{
	WriteVoiceDword("VoiceEnabled", bEnabled ? 1 : 0);
}

bool VoiceSettingGetMuteOthers()
{
	return (ReadVoiceDword("VoiceMuteOthers", CfgDefaults::CfgDefaultVoiceMuteOthers ? 1 : 0) != 0);
}

void VoiceSettingSetMuteOthers(bool bMute)
{
	WriteVoiceDword("VoiceMuteOthers", bMute ? 1 : 0);
}

int VoiceSettingGetVolume()
{
	int volume = ReadVoiceDword("VoiceVolume", CfgDefaults::CfgDefaultVoiceVolume);

	// Clamped on the way out, not just on the way in: the registry is editable
	// by hand, and this number scales audio in an integer mix.
	if (volume < 0)
	{
		volume = 0;
	}

	if (volume > 100)
	{
		volume = 100;
	}

	return volume;
}

void VoiceSettingSetVolume(int iVolume)
{
	if (iVolume < 0)
	{
		iVolume = 0;
	}

	if (iVolume > 100)
	{
		iVolume = 100;
	}

	WriteVoiceDword("VoiceVolume", iVolume);
}

int VoiceSettingGetPttKey()
{
	return ReadVoiceDword("VoicePttKey", CfgDefaults::CfgDefaultVoicePttKey);
}

void VoiceSettingSetPttKey(int iVirtualKey)
{
	WriteVoiceDword("VoicePttKey", iVirtualKey);
}

int VoiceSettingGetMicDevice()
{
	// Not range-checked against the live device list here. Devices come and go
	// - a headset unplugged between sessions would turn a saved id into an
	// invalid one - so the check belongs at the point of opening, where a bad
	// id falls back to the system default instead of failing.
	return ReadVoiceDword("VoiceMicDevice", CfgDefaults::CfgDefaultVoiceMicDevice);
}

void VoiceSettingSetMicDevice(int iDeviceId)
{
	WriteVoiceDword("VoiceMicDevice", iDeviceId);
}

int VoiceSettingGetMicBoost()
{
	int boost = ReadVoiceDword("VoiceMicBoost", CfgDefaults::CfgDefaultVoiceMicBoost);

	// Clamped on read as well as write: the registry is hand-editable, and this
	// multiplies audio - a wild value would either silence the microphone or
	// turn every syllable into clipping.
	if (boost < 100)
	{
		boost = 100;
	}

	if (boost > 600)
	{
		boost = 600;
	}

	return boost;
}

void VoiceSettingSetMicBoost(int iPercent)
{
	if (iPercent < 100)
	{
		iPercent = 100;
	}

	if (iPercent > 600)
	{
		iPercent = 600;
	}

	WriteVoiceDword("VoiceMicBoost", iPercent);
}

bool VoiceSettingGetNoiseGate()
{
	return (ReadVoiceDword("VoiceNoiseGate", CfgDefaults::CfgDefaultVoiceNoiseGate ? 1 : 0) != 0);
}

void VoiceSettingSetNoiseGate(bool bEnabled)
{
	WriteVoiceDword("VoiceNoiseGate", bEnabled ? 1 : 0);
}

int VoiceSettingGetTalkMode()
{
	const int mode = ReadVoiceDword("VoiceTalkMode", CfgDefaults::CfgDefaultVoiceTalkMode);

	// Anything unrecognised means hold, which is the safer of the two: a stuck
	// toggle transmits until somebody notices, a stuck hold stops the moment the
	// key comes up.
	return (mode == CfgDefaults::CfgVoiceTalkModeToggle)
		? CfgDefaults::CfgVoiceTalkModeToggle
		: CfgDefaults::CfgVoiceTalkModeHold;
}

void VoiceSettingSetTalkMode(int iMode)
{
	WriteVoiceDword("VoiceTalkMode",
		(iMode == CfgDefaults::CfgVoiceTalkModeToggle)
			? CfgDefaults::CfgVoiceTalkModeToggle
			: CfgDefaults::CfgVoiceTalkModeHold);
}

#else   // Android

bool VoiceSettingGetEnabled()
{
	return GameConfig::GetInstance().GetVoiceEnabled();
}

void VoiceSettingSetEnabled(bool bEnabled)
{
	GameConfig::GetInstance().SetVoiceEnabled(bEnabled);
	GameConfig::GetInstance().Save();
}

bool VoiceSettingGetMuteOthers()
{
	return GameConfig::GetInstance().GetVoiceMuteOthers();
}

void VoiceSettingSetMuteOthers(bool bMute)
{
	GameConfig::GetInstance().SetVoiceMuteOthers(bMute);
	GameConfig::GetInstance().Save();
}

int VoiceSettingGetVolume()
{
	return GameConfig::GetInstance().GetVoiceVolume();
}

void VoiceSettingSetVolume(int iVolume)
{
	GameConfig::GetInstance().SetVoiceVolume(iVolume);
	GameConfig::GetInstance().Save();
}

int VoiceSettingGetPttKey()
{
	return GameConfig::GetInstance().GetVoicePttKey();
}

void VoiceSettingSetPttKey(int iVirtualKey)
{
	GameConfig::GetInstance().SetVoicePttKey(iVirtualKey);
	GameConfig::GetInstance().Save();
}

// Android records through OpenSL ES, which has no notion of choosing an input -
// it takes the default and that is that. Selecting a device there needs AAudio
// (API 28+, and minSdk here is 21) or the Java AudioManager. Hardcoded rather
// than stored, so nothing can persist a value that would be silently ignored.
int VoiceSettingGetMicDevice()
{
	return -1;
}

void VoiceSettingSetMicDevice(int /*iDeviceId*/)
{
}

// Unlike the device, boost IS meaningful on Android - the gain is applied in
// shared code before encoding, so it works regardless of which backend
// captured the samples. It does not survive a restart, because GameConfig's
// Load and Save are no-ops on this platform.
int VoiceSettingGetMicBoost()
{
	return GameConfig::GetInstance().GetVoiceMicBoost();
}

void VoiceSettingSetMicBoost(int iPercent)
{
	GameConfig::GetInstance().SetVoiceMicBoost(iPercent);
	GameConfig::GetInstance().Save();
}

bool VoiceSettingGetNoiseGate()
{
	return GameConfig::GetInstance().GetVoiceNoiseGate();
}

void VoiceSettingSetNoiseGate(bool bEnabled)
{
	GameConfig::GetInstance().SetVoiceNoiseGate(bEnabled);
	GameConfig::GetInstance().Save();
}

int VoiceSettingGetTalkMode()
{
	return GameConfig::GetInstance().GetVoiceTalkMode();
}

void VoiceSettingSetTalkMode(int iMode)
{
	GameConfig::GetInstance().SetVoiceTalkMode(iMode);
	GameConfig::GetInstance().Save();
}

#endif
