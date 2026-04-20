//=========== (C) Copyright 2025 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: Localization implementation for ImGui Scoreboard
//          Uses engine's built-in TextMessageGet for titles.txt strings
//          The engine automatically loads the correct language's titles.txt
//
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "imgui_localization.h"

// Platform headers for file operations
#include "PlatformHeaders.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <stdlib.h>

ImGuiLocalization& ImGuiLocalization::GetInstance()
{
    static ImGuiLocalization instance;
    return instance;
}

void ImGuiLocalization::Initialize()
{
    if (m_initialized)
        return;

    // Scan for available languages
    ScanLanguages();
    
    // Get current language from engine (pfnGetGameDirectory returns active txt folder)
    const char* currentDir = gEngfuncs.pfnGetGameDirectory();
    if (currentDir)
    {
        // Extract folder name
        const char* folderName = strrchr(currentDir, '\\');
        if (!folderName)
            folderName = strrchr(currentDir, '/');
        if (folderName)
            folderName++;
        else
            folderName = currentDir;
        
        m_currentLanguage = folderName;
        
        char msg[256];
        sprintf(msg, "[Scoreboard] Current language: %s\n", folderName);
        gEngfuncs.pfnConsolePrint(msg);
    }
    
    m_initialized = true;
}

void ImGuiLocalization::ScanLanguages()
{
    m_languages.clear();
    
    // pfnGetGameDirectory returns relative path like "txt" or "txt_schinese"
    const char* txtDir = gEngfuncs.pfnGetGameDirectory();
    if (!txtDir)
        return;
    
    // Convert to absolute path
    char absTxtDir[256];
    _fullpath(absTxtDir, txtDir, sizeof(absTxtDir));
    
    // Get parent directory (Half-Life root)
    char parentDir[256];
    strncpy(parentDir, absTxtDir, sizeof(parentDir) - 1);
    parentDir[sizeof(parentDir) - 1] = '\0';
    
    char* lastSlash = strrchr(parentDir, '\\');
    if (!lastSlash)
        lastSlash = strrchr(parentDir, '/');
    if (lastSlash)
        *lastSlash = '\0';
    
    // Search for txt* folders in parent directory
    char searchPath[256];
    sprintf(searchPath, "%s\\txt*", parentDir);
    
    struct _finddata_t findData;
    intptr_t findHandle = _findfirst(searchPath, &findData);
    
    if (findHandle == -1)
    {
        gEngfuncs.pfnConsolePrint("[Scoreboard] No txt folders found\n");
        return;
    }
    
    do
    {
        // Skip . and ..
        if (strcmp(findData.name, ".") == 0 || strcmp(findData.name, "..") == 0)
            continue;
        
        // Check if it's a directory
        if (!(findData.attrib & _A_SUBDIR))
            continue;
        
        // Must start with "txt"
        if (strncmp(findData.name, "txt", 3) != 0)
            continue;
        
        // Check if folder contains titles.txt
        char langTitlesPath[256];
        sprintf(langTitlesPath, "%s\\%s\\titles.txt", parentDir, findData.name);
        
        FILE* langFile = fopen(langTitlesPath, "r");
        if (langFile)
        {
            fclose(langFile);
            
            // Found a valid language folder
            LanguageInfo lang;
            lang.folderName = findData.name;
            
            // Extract language code from folder name (txt_xxx -> xxx)
            const char* langCode = findData.name;
            if (strncmp(findData.name, "txt_", 4) == 0)
                langCode = findData.name + 4;
            
            // Set display name based on language code
            if (strcmp(langCode, "schinese") == 0)
            {
                lang.displayName = "Chinese (Simplified)";
                lang.flag = "CN";
            }
            else if (strcmp(langCode, "tchinese") == 0)
            {
                lang.displayName = "Chinese (Traditional)";
                lang.flag = "TW";
            }
            else if (strcmp(langCode, "russian") == 0)
            {
                lang.displayName = "Russian";
                lang.flag = "RU";
            }
            else if (strcmp(langCode, "german") == 0)
            {
                lang.displayName = "German";
                lang.flag = "DE";
            }
            else if (strcmp(langCode, "french") == 0)
            {
                lang.displayName = "French";
                lang.flag = "FR";
            }
            else if (strcmp(langCode, "spanish") == 0)
            {
                lang.displayName = "Spanish";
                lang.flag = "ES";
            }
            else if (strcmp(langCode, "japanese") == 0)
            {
                lang.displayName = "Japanese";
                lang.flag = "JP";
            }
            else if (strcmp(langCode, "korean") == 0)
            {
                lang.displayName = "Korean";
                lang.flag = "KR";
            }
            else if (strcmp(langCode, "english") == 0)
            {
                lang.displayName = "English";
                lang.flag = "EN";
            }
            else
            {
                lang.displayName = langCode;
                lang.flag = "??";
            }
            
            m_languages.push_back(lang);
        }
        
    } while (_findnext(findHandle, &findData) == 0);
    
    _findclose(findHandle);
    
    // Log found languages
    char msg[256];
    sprintf(msg, "[Scoreboard] Found %zu languages\n", m_languages.size());
    gEngfuncs.pfnConsolePrint(msg);
}

bool ImGuiLocalization::LoadLanguage(const char* languageFolder)
{
    if (!languageFolder || !languageFolder[0])
        return false;
    
    // Tell the engine to switch language via console command
    // The engine will reload titles.txt from the new txt folder
    char cmd[256];
    sprintf(cmd, "language %s\n", languageFolder);
    gEngfuncs.pfnClientCmd(cmd);
    
    m_currentLanguage = languageFolder;
    
    char msg[256];
    sprintf(msg, "[Scoreboard] Switched language to: %s\n", languageFolder);
    gEngfuncs.pfnConsolePrint(msg);
    
    return true;
}

const char* ImGuiLocalization::GetString(const char* key, const char* defaultValue)
{
    if (!key)
        return defaultValue ? defaultValue : "";
    
    // TextMessageGet expects key WITHOUT '#' prefix
    // e.g., TextMessageGet("Player") not TextMessageGet("#Player")
    const char* lookupKey = key;
    char keyBuf[256];
    if (key[0] == '#')
    {
        lookupKey = key + 1; // skip '#'
    }
    
    // Look up in engine's titles.txt
    client_textmessage_t* msg = TextMessageGet(lookupKey);
    if (msg && msg->pMessage)
    {
        m_lastLookupResult = msg->pMessage;
        return m_lastLookupResult.c_str();
    }
    
    // Not found, return default
    return defaultValue ? defaultValue : key;
}
