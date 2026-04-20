//=========== (C) Copyright 2025 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: Modern Scoreboard Header - Half-Life HUD based rendering
//              with multi-language support
//
//=============================================================================

#pragma once

#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>

#include "imgui_localization.h"

//=============================================================================
// Data Structures
//=============================================================================

// Player data for scoreboard display
struct ImGuiPlayerData
{
    int index;
    std::string name;
    int frags;
    int deaths;
    int ping;
    int teamNumber;
    std::string teamName;
    bool isBot;
    bool isLocalPlayer;
    bool isSpeaking;
    float voiceLevel;
    
    ImGuiPlayerData()
        : index(0), frags(0), deaths(0), ping(0), teamNumber(0)
        , isBot(false), isLocalPlayer(false), isSpeaking(false), voiceLevel(0.0f)
    {}
};

// Team data for team-based games
struct ImGuiTeamData
{
    int teamNumber;
    std::string name;
    int frags;
    int deaths;
    int ping;
    int playerCount;
    int color;
    
    ImGuiTeamData()
        : teamNumber(0), frags(0), deaths(0), ping(0), playerCount(0), color(0)
    {}
};

// Style configuration
struct ImGuiScoreboardStyle
{
    int backgroundColor;    // RGB color
    int borderColor;
    int headerColor;
    int rowEvenColor;
    int rowOddColor;
    int highlightColor;
    int textColor;
    int textDimColor;
    int headerHeight;
    int rowHeight;
    
    ImGuiScoreboardStyle()
        : backgroundColor(0), borderColor(0), headerColor(0)
        , rowEvenColor(0), rowOddColor(0), highlightColor(0)
        , textColor(0), textDimColor(0), headerHeight(40), rowHeight(20)
    {}
};

// Sort column enumeration
enum SortColumn
{
    SORT_RANK = 0,
    SORT_NAME,
    SORT_SCORE,
    SORT_KILLS,
    SORT_DEATHS,
    SORT_PING
};

//=============================================================================
// Scoreboard Class
//=============================================================================
class ImGuiScoreboard
{
public:
    ImGuiScoreboard();
    ~ImGuiScoreboard();
    
    // Lifecycle
    void Initialize();
    
    // Visibility
    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const;
    
    // Rendering
    void Render();
    
    // Data
    void RefreshData();
    
    // Language selection
    void DrawLanguageSelector(int x, int y, int scale);
    void CycleLanguage();
    
private:
    void SortPlayers();
    
    // State
    bool m_visible;
    bool m_initialized;
    bool m_teamplay;
    int m_maxPlayers;
    int m_localPlayerIndex;
    
    // Sorting
    SortColumn m_sortColumn;
    bool m_sortAscending;
    
    // Style
    ImGuiScoreboardStyle m_style;
    
    // Data
    std::vector<ImGuiPlayerData> m_players;
    std::vector<ImGuiTeamData> m_teams;
    std::string m_serverName;
    std::string m_mapName;
    
    // Language
    int m_currentLanguageIndex;
};

//=============================================================================
// Global Interface Functions
//=============================================================================
void ImGuiScoreboard_Init();
void ImGuiScoreboard_Shutdown();
void ImGuiScoreboard_Toggle();
void ImGuiScoreboard_Show();
void ImGuiScoreboard_Hide();
bool ImGuiScoreboard_IsVisible();
void ImGuiScoreboard_Render();
