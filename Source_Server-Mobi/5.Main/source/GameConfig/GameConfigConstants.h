#pragma once

namespace CfgSections
{
    inline constexpr wchar_t CfgSectionWindow[]     = L"Window";
    inline constexpr wchar_t CfgSectionGraphics[]   = L"Graphics";
    inline constexpr wchar_t CfgSectionAudio[]      = L"Audio";
    inline constexpr wchar_t CfgSectionVoice[]      = L"Voice";
    inline constexpr wchar_t CfgSectionLogin[]      = L"LOGIN";
    inline constexpr wchar_t CfgSectionConnectionSettings[] = L"CONNECTION SETTINGS";
}

namespace CfgKeys
{
    // Window
    inline constexpr wchar_t CfgKeyWidth[]      = L"Width";
    inline constexpr wchar_t CfgKeyHeight[]     = L"Height";
    inline constexpr wchar_t CfgKeyWindowed[]   = L"Windowed";

    // Graphics
    inline constexpr wchar_t CfgKeyColorDepth[]     = L"ColorDepth";
    inline constexpr wchar_t CfgKeyRenderTextType[] = L"RenderTextType";

    // Audio
    inline constexpr wchar_t CfgKeySoundEnabled[] = L"SoundEnabled";
    inline constexpr wchar_t CfgKeyMusicEnabled[] = L"MusicEnabled";
    inline constexpr wchar_t CfgKeyVolumeLevel[]  = L"VolumeLevel";

    // Voice chat
    inline constexpr wchar_t CfgKeyVoiceEnabled[]    = L"Enabled";
    inline constexpr wchar_t CfgKeyVoiceMuteOthers[] = L"MuteOthers";
    inline constexpr wchar_t CfgKeyVoiceVolume[]     = L"Volume";
    inline constexpr wchar_t CfgKeyVoicePttKey[]     = L"PushToTalkKey";
    inline constexpr wchar_t CfgKeyVoiceMicBoost[]   = L"MicBoost";
    inline constexpr wchar_t CfgKeyVoiceNoiseGate[]  = L"NoiseGate";
    inline constexpr wchar_t CfgKeyVoiceTalkMode[]   = L"TalkMode";

    // Login
    inline constexpr wchar_t CfgKeyRememberMe[]        = L"RememberMe";
    inline constexpr wchar_t CfgKeyLanguage[]          = L"Language";
    inline constexpr wchar_t CfgKeyEncryptedUsername[] = L"EncryptedUsername";
    inline constexpr wchar_t CfgKeyEncryptedPassword[] = L"EncryptedPassword";

    // Connection
    inline constexpr wchar_t CfgKeyServerIP[]   = L"ServerIP";
    inline constexpr wchar_t CfgKeyServerPort[] = L"ServerPort";
}

namespace CfgDefaults
{
    inline constexpr int  CfgDefaultWindowWidth  = 1024;
    inline constexpr int  CfgDefaultWindowHeight = 768;
    inline constexpr bool CfgDefaultWindowed     = true;

    inline constexpr int  CfgDefaultColorDepth = 0;

    inline constexpr bool CfgDefaultSoundEnabled = true;
    // Was false, which on Android is not a default but a hard setting: ReadBool
    // goes through GetPrivateProfileIntW, and that is a stub returning whatever
    // default it is handed, so config.ini can never switch music on. Music was
    // silent for that reason alone - and with sound the only thing left, the
    // whole mixer init hung off one flag.
    inline constexpr bool CfgDefaultMusicEnabled = true;
    inline constexpr int  CfgDefaultVolumeLevel  = 5;

    inline constexpr int CfgDefaultRenderTextType = 0;

    // Voice chat. On by default because the SERVER already decides whether
    // voice exists at all (CustomConfig.ini VoiceChatEnable, off by default),
    // and push-to-talk means nothing is transmitted unless the player holds a
    // key. This setting is the per-player opt-OUT on a server that has it on.
    inline constexpr bool CfgDefaultVoiceEnabled    = true;
    inline constexpr bool CfgDefaultVoiceMuteOthers = false;
    inline constexpr int  CfgDefaultVoiceVolume     = 100;   // 0-100

    // 0x78 is VK_F9. Spelled as a number because this header is included by
    // code that does not pull in windows.h.
    inline constexpr int  CfgDefaultVoicePttKey     = 0x78;

    // Which microphone to record from. -1 means "whatever the system default
    // is" (WAVE_MAPPER on PC), which is the right default: it follows the
    // player's own OS choice and keeps working when they unplug a headset.
    // Anything else is a waveIn device id.
    //
    // PC only. Android records through OpenSL ES, which offers no device
    // selection at all - picking an input there needs AAudio (API 28+) or the
    // Java AudioManager, neither of which this client has.
    inline constexpr int  CfgDefaultVoiceMicDevice  = -1;

    // Extra gain on top of the automatic levelling, as a percentage. 100 means
    // "auto-levelling alone", which is enough for most microphones; the control
    // exists because phone microphones vary wildly and a player with a quiet
    // one should not have to shout.
    inline constexpr int  CfgDefaultVoiceMicBoost   = 100;

    // Noise gate: stops steady background - a fan, traffic, a room's hum - from
    // being transmitted between words, and stops anything at all going out while
    // nobody is speaking. On by default; it is nearly always wanted, and the
    // readout shows when it closes so a player can see it if it is too tight.
    inline constexpr bool CfgDefaultVoiceNoiseGate  = true;

    // How the talk control behaves. 0 = hold to talk, 1 = tap to toggle.
    //
    // The defaults differ per platform on purpose and that is not an
    // inconsistency: a keyboard has a key that can be held without costing
    // anything, while on a phone holding one means parking a thumb on the HUD
    // that is also needed for steering. Both are selectable either way.
    inline constexpr int  CfgVoiceTalkModeHold      = 0;
    inline constexpr int  CfgVoiceTalkModeToggle    = 1;

#if defined(__ANDROID__) || defined(MU_IOS)
    inline constexpr int  CfgDefaultVoiceTalkMode   = CfgVoiceTalkModeToggle;
#else
    inline constexpr int  CfgDefaultVoiceTalkMode   = CfgVoiceTalkModeHold;
#endif

    inline constexpr bool CfgDefaultRememberMe = false;
    inline constexpr wchar_t CfgDefaultLanguage[] = L"Eng";
    inline constexpr wchar_t CfgDefaultEncryptedUsername[] = L"";
    inline constexpr wchar_t CfgDefaultEncryptedPassword[] = L"";

    inline constexpr wchar_t CfgDefaultServerIP[] = L"139.99.24.220";
    inline constexpr int CfgDefaultServerPort = 63000;
}
