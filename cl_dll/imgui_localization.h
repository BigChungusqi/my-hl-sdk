//=========== (C) Copyright 2025 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: Localization helper for ImGui Scoreboard
//          Uses engine's built-in TextMessageGet for titles.txt strings
//          The engine automatically loads the correct language's titles.txt
//
//=============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// Language info structure
struct LanguageInfo
{
    std::string folderName;     // e.g., "txt", "txt_schinese"
    std::string displayName;    // e.g., "Default", "Chinese (Simplified)"
    std::string flag;           // e.g., "DF", "CN"
};

// Localization manager for ImGui UI
class ImGuiLocalization
{
public:
    static ImGuiLocalization& GetInstance();
    
    // Initialize - scans available languages, loads current
    void Initialize();
    
    // Scan for available language folders
    void ScanLanguages();
    
    // Load a specific language (changes game's txt folder)
    bool LoadLanguage(const char* languageFolder);
    
    // Get current language folder name
    const char* GetCurrentLanguage() const { return m_currentLanguage.c_str(); }
    
    // Get localized string from engine's titles.txt
    // Usage: GetString("#Player") or GetString("#Player", "Player")
    const char* GetString(const char* key, const char* defaultValue = nullptr);
    
    // Get list of available languages
    const std::vector<LanguageInfo>& GetAvailableLanguages() const { return m_languages; }
    
    // Check if initialized
    bool IsInitialized() const { return m_initialized; }
    
    // Get default language folder name
    static const char* GetDefaultLanguage() { return "txt"; }

private:
    ImGuiLocalization() : m_initialized(false) {}
    ~ImGuiLocalization() {}
    
    // Buffer for localized strings (engine returns pointer, we need to cache)
    std::string m_lastLookupResult;
    
    bool m_initialized;
    std::string m_currentLanguage;
    std::vector<LanguageInfo> m_languages;
};

// Helper macro for easy string lookup
#define L(key, defaultVal) ImGuiLocalization::GetInstance().GetString(key, defaultVal)
