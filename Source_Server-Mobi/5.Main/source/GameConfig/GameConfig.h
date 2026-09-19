#pragma once

#include <filesystem>
#include <string>
#include <vector>
#if !defined(__ANDROID__) && !defined(MU_IOS)
#include <windows.h>
#endif

class GameConfig
{
public:
    static GameConfig& GetInstance();

    void Load();
    void Save();

    // Window
    int  GetWindowWidth()  const { return m_windowWidth; }
    int  GetWindowHeight() const { return m_windowHeight; }
    bool GetWindowMode()   const { return m_windowMode; }

    void SetWindowSize(int width, int height);
    void SetWindowMode(bool windowed);

    // Graphics
    int GetColorDepth() const { return m_colorDepth; }
    void SetColorDepth(int depth);

    // Audio
    bool GetSoundEnabled() const { return m_soundEnabled; }
    bool GetMusicEnabled() const { return m_musicEnabled; }
    int  GetVolumeLevel()  const { return m_volumeLevel; }

    void SetSoundEnabled(bool enabled);
    void SetMusicEnabled(bool enabled);
    void SetVolumeLevel(int level);

    // Proximity voice chat.
    //
    // NOTE: these do not persist on Android. GameConfig::Load and Save are
    // no-ops there, so every setting in this class resets on each launch. That
    // is a pre-existing gap and not specific to voice, but it does mean a
    // mobile player who mutes has to mute again next time - which is why the
    // defaults are the common case rather than the cautious one.
    bool GetVoiceEnabled()    const { return m_voiceEnabled; }
    bool GetVoiceMuteOthers() const { return m_voiceMuteOthers; }
    int  GetVoiceVolume()     const { return m_voiceVolume; }
    int  GetVoicePttKey()     const { return m_voicePttKey; }
    int  GetVoiceMicBoost()   const { return m_voiceMicBoost; }
    bool GetVoiceNoiseGate()  const { return m_voiceNoiseGate; }
    int  GetVoiceTalkMode()   const { return m_voiceTalkMode; }

    void SetVoiceEnabled(bool enabled);
    void SetVoiceMuteOthers(bool mute);
    void SetVoiceVolume(int volume);
    void SetVoicePttKey(int virtualKey);
    void SetVoiceMicBoost(int percent);
    void SetVoiceNoiseGate(bool enabled);
    void SetVoiceTalkMode(int mode);

    // Text rendering
    int GetRenderTextType() const { return m_renderTextType; }
    void SetRenderTextType(int type);

    // Login
    bool GetRememberMe() const { return m_rememberMe; }
    void SetRememberMe(bool remember);

    std::wstring GetLanguageSelection() const { return m_languageSelection; }
    void SetLanguageSelection(const std::wstring& lang);

    void SetEncryptedUsername(const std::wstring& encryptedUsername);
    std::wstring GetEncryptedUsername() const { return m_encryptedUsername; }

    void SetEncryptedPassword(const std::wstring& encryptedPassword);
    std::wstring GetEncryptedPassword() const { return m_encryptedPassword; }

    // Connection
    std::wstring GetServerIP() const { return m_serverIP; }
    int GetServerPort() const { return m_serverPort; }

    void SetServerIP(const std::wstring& ip);
    void SetServerPort(int port);

    // Helpers
    static std::wstring BinaryToHex(const BYTE* data, DWORD size);
    static std::vector<BYTE> HexToBinary(const std::wstring& hex);

    void DecryptCredentials(wchar_t* outUser, wchar_t* outPass, size_t userBufSize, size_t passBufSize);
    void EncryptAndSaveCredentials(const wchar_t* user, const wchar_t* pass);

private:
    GameConfig();
    GameConfig(const GameConfig&) = delete;
    GameConfig& operator=(const GameConfig&) = delete;

    std::filesystem::path m_configPath;

    int  m_windowWidth;
    int  m_windowHeight;
    bool m_windowMode;

    int m_colorDepth;

    bool m_soundEnabled;
    bool m_musicEnabled;
    int  m_volumeLevel;

    int m_renderTextType;

    bool m_voiceEnabled;
    bool m_voiceMuteOthers;
    int  m_voiceVolume;
    int  m_voicePttKey;
    int  m_voiceMicBoost;
    bool m_voiceNoiseGate;
    int  m_voiceTalkMode;

    bool m_rememberMe;
    std::wstring m_languageSelection;
    std::wstring m_encryptedUsername;
    std::wstring m_encryptedPassword;

    std::wstring m_serverIP;
    int m_serverPort;

    int ReadInt(const wchar_t* section, const wchar_t* key, int defaultValue);
    void WriteInt(const wchar_t* section, const wchar_t* key, int value);

    bool ReadBool(const wchar_t* section, const wchar_t* key, bool defaultValue);
    void WriteBool(const wchar_t* section, const wchar_t* key, bool value);

    std::wstring ReadString(const wchar_t* section, const wchar_t* key, const std::wstring& defaultValue);
    void WriteString(const wchar_t* section, const wchar_t* key, const std::wstring& value);

    std::wstring DecryptSetting(const std::wstring& hexInput);
    std::wstring EncryptSetting(const wchar_t* input);
};
