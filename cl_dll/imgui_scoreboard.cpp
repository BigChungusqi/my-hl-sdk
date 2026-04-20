//=========== (C) Copyright 2025 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: Modern Scoreboard Implementation using Half-Life HUD rendering
//
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "vgui_TeamFortressViewport.h"
#include "imgui_scoreboard.h"
#include "imgui_localization.h"
#include "global_consts.h"

// Platform headers (includes Windows.h with conflict fixes)
#include "PlatformHeaders.h"

// Define RGB macros if not available (NOGDI disables them)
#ifndef RGB
#define RGB(r,g,b) ((DWORD)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#endif
#ifndef GetRValue
#define GetRValue(rgb) (LOBYTE(rgb))
#endif
#ifndef GetGValue
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb)) >> 8))
#endif
#ifndef GetBValue
#define GetBValue(rgb) (LOBYTE((rgb)>>16))
#endif

#include <cmath>
#include <algorithm>

// External globals from game
extern hud_player_info_t g_PlayerInfoList[MAX_PLAYERS_HUD + 1];
extern extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS_HUD + 1];
extern team_info_t g_TeamInfo[MAX_TEAMS + 1];
extern int g_IsSpectator[MAX_PLAYERS_HUD + 1];

// Team colors from game
extern int iNumberOfTeamColors;
extern int iTeamColors[5][3];

// Global instance (non-static for external access)
ImGuiScoreboard* g_ImGuiScoreboard = nullptr;

//=============================================================================
// Helper Functions
//=============================================================================
// Helper to draw text with RGB color
static void DrawText(int x, int y, int maxX, const char* text, int color)
{
    gHUD.DrawHudString(x, y, maxX, text, 
        GetRValue(color), GetGValue(color), GetBValue(color));
}

static int GetTeamColor(int teamNumber)
{
    if (teamNumber < 0 || teamNumber >= iNumberOfTeamColors)
        teamNumber = 0;
    
    int r = iTeamColors[teamNumber][0];
    int g = iTeamColors[teamNumber][1];
    int b = iTeamColors[teamNumber][2];
    
    return (r << 16) | (g << 8) | b;
}

//=============================================================================
// Constructor / Destructor
//=============================================================================
ImGuiScoreboard::ImGuiScoreboard()
    : m_visible(false)
    , m_initialized(false)
    , m_teamplay(false)
    , m_maxPlayers(32)
    , m_localPlayerIndex(0)
    , m_sortColumn(SORT_SCORE)
    , m_sortAscending(false)
    , m_currentLanguageIndex(0)
{
    memset(&m_style, 0, sizeof(m_style));
}

ImGuiScoreboard::~ImGuiScoreboard()
{
}

//=============================================================================
// Initialization
//=============================================================================
void ImGuiScoreboard::Initialize()
{
    if (m_initialized)
        return;

    // Initialize localization
    ImGuiLocalization::GetInstance().Initialize();
    
    // Set default style colors (RGB format for engine)
    m_style.backgroundColor = RGB(15, 15, 20);
    m_style.borderColor = RGB(80, 80, 90);
    m_style.headerColor = RGB(25, 25, 35);
    m_style.rowEvenColor = RGB(20, 20, 25);
    m_style.rowOddColor = RGB(30, 30, 35);
    m_style.highlightColor = RGB(255, 140, 0);
    m_style.textColor = RGB(220, 220, 220);
    m_style.textDimColor = RGB(150, 150, 150);
    m_style.headerHeight = 40;
    m_style.rowHeight = 20;
    
    m_initialized = true;
}

//=============================================================================
// Visibility
//=============================================================================
void ImGuiScoreboard::Show()
{
    if (!m_initialized)
        Initialize();
    
    m_visible = true;
    RefreshData();
}

void ImGuiScoreboard::Hide()
{
    m_visible = false;
}

void ImGuiScoreboard::Toggle()
{
    if (m_visible)
        Hide();
    else
        Show();
}

bool ImGuiScoreboard::IsVisible() const
{
    return m_visible;
}

//=============================================================================
// Data Management
//=============================================================================
void ImGuiScoreboard::RefreshData()
{
    m_players.clear();
    m_teams.clear();
    
    // Get game state
    m_teamplay = gHUD.m_Teamplay ? true : false;
    
    // Get local player
    cl_entity_t* localPlayer = gEngfuncs.GetLocalPlayer();
    if (localPlayer)
        m_localPlayerIndex = localPlayer->index;
    
    // Get server name
    const char* serverName = gEngfuncs.pfnGetCvarString("hostname");
    if (serverName && serverName[0])
        m_serverName = serverName;
    else
        m_serverName = ImGuiLocalization::GetInstance().GetString("ServerName", "Half-Life Server");
    
    // Get map name
    const char* levelName = gEngfuncs.pfnGetLevelName();
    if (levelName && levelName[0])
    {
        const char* mapStart = strstr(levelName, "maps/");
        if (mapStart)
            levelName = mapStart + 5;
        m_mapName = levelName;
        size_t dotPos = m_mapName.find(".bsp");
        if (dotPos != std::string::npos)
            m_mapName = m_mapName.substr(0, dotPos);
    }
    else
    {
        m_mapName = ImGuiLocalization::GetInstance().GetString("UnknownMap", "Unknown");
    }
    
    // Collect player data
    for (int i = 1; i <= MAX_PLAYERS_HUD; i++)
    {
        // Update player info from engine
        gEngfuncs.pfnGetPlayerInfo(i, &g_PlayerInfoList[i]);
        
        if (!g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0])
            continue;
        
        ImGuiPlayerData player;
        player.index = i;
        player.name = g_PlayerInfoList[i].name;
        player.frags = g_PlayerExtraInfo[i].frags;
        player.deaths = g_PlayerExtraInfo[i].deaths;
        player.ping = g_PlayerInfoList[i].ping;
        player.teamNumber = g_PlayerExtraInfo[i].teamnumber;
        player.teamName = g_TeamInfo[player.teamNumber].name ? g_TeamInfo[player.teamNumber].name : "";
        player.isBot = (g_PlayerInfoList[i].thisplayer == 0 && g_PlayerInfoList[i].ping == 0);
        player.isLocalPlayer = (i == m_localPlayerIndex);
        player.isSpeaking = false;
        player.voiceLevel = 0.0f;
        
        m_players.push_back(player);
    }
    
    // Sort players
    SortPlayers();
    
    // Collect team data if teamplay
    if (m_teamplay)
    {
        for (int i = 1; i <= iNumberOfTeamColors; i++)
        {
            if (g_TeamInfo[i].players > 0)
            {
                ImGuiTeamData team;
                team.teamNumber = i;
                team.name = g_TeamInfo[i].name ? g_TeamInfo[i].name : ImGuiLocalization::GetInstance().GetString("Team", "Team");
                team.frags = g_TeamInfo[i].frags;
                team.deaths = g_TeamInfo[i].deaths;
                team.ping = g_TeamInfo[i].ping;
                team.playerCount = g_TeamInfo[i].players;
                team.color = GetTeamColor(i);
                
                m_teams.push_back(team);
            }
        }
    }
}

void ImGuiScoreboard::SortPlayers()
{
    auto compare = [this](const ImGuiPlayerData& a, const ImGuiPlayerData& b)
    {
        switch (m_sortColumn)
        {
            case SORT_NAME:
                return m_sortAscending ? (a.name < b.name) : (a.name > b.name);
            case SORT_SCORE:
                return m_sortAscending ? 
                    ((a.frags - a.deaths) < (b.frags - b.deaths)) :
                    ((a.frags - a.deaths) > (b.frags - b.deaths));
            case SORT_KILLS:
                return m_sortAscending ? (a.frags < b.frags) : (a.frags > b.frags);
            case SORT_DEATHS:
                return m_sortAscending ? (a.deaths < b.deaths) : (a.deaths > b.deaths);
            case SORT_PING:
                return m_sortAscending ? (a.ping < b.ping) : (a.ping > b.ping);
            default:
                return false;
        }
    };
    
    std::sort(m_players.begin(), m_players.end(), compare);
}

//=============================================================================
// Rendering
//=============================================================================
void ImGuiScoreboard::Render()
{
    if (!m_visible || !m_initialized)
        return;
    
    // Only render in multiplayer
    if (gEngfuncs.GetMaxClients() <= 1)
    {
        m_visible = false;
        return;
    }

    int screenWidth = (ScreenWidth > 0) ? ScreenWidth : 640;
    int screenHeight = (ScreenHeight > 0) ? ScreenHeight : 480;
    
    // Calculate window dimensions
    float scale = screenWidth / 1280.0f;
    if (scale < 0.7f) scale = 0.7f;
    if (scale > 1.5f) scale = 1.5f;
    
    int windowWidth = (int)(700 * scale);
    int windowHeight = (int)(400 * scale);
    int windowX = (screenWidth - windowWidth) / 2;
    int windowY = (screenHeight - windowHeight) / 2;
    
    // Draw background
    FillRGBA(windowX, windowY, windowWidth, windowHeight, 
        GetRValue(m_style.backgroundColor), 
        GetGValue(m_style.backgroundColor), 
        GetBValue(m_style.backgroundColor), 
        220);
    
    // Draw border
    FillRGBA(windowX, windowY, windowWidth, 2, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    FillRGBA(windowX, windowY + windowHeight - 2, windowWidth, 2, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    FillRGBA(windowX, windowY, 2, windowHeight, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    FillRGBA(windowX + windowWidth - 2, windowY, 2, windowHeight, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    
    // Draw header background
    int headerHeight = (int)(m_style.headerHeight * scale);
    FillRGBA(windowX + 2, windowY + 2, windowWidth - 4, headerHeight, 
        GetRValue(m_style.headerColor), 
        GetGValue(m_style.headerColor), 
        GetBValue(m_style.headerColor), 240);
    
    // Draw title
    int textY = windowY + (int)(10 * scale);
    char title[128];
    sprintf(title, "%s - %s", m_serverName.c_str(), m_mapName.c_str());
    DrawText(windowX + (int)(10 * scale), textY, windowX + windowWidth, title, m_style.textColor);
    
    // Draw column headers (localized)
    int colY = windowY + headerHeight + (int)(5 * scale);
    int colX = windowX + (int)(10 * scale);
    int colWidths[] = { (int)(30 * scale), (int)(200 * scale), (int)(80 * scale), 
                        (int)(80 * scale), (int)(60 * scale) };
    const char* colKeys[] = { "", "Player", "Score", "Deaths", "Ping" };
    
    for (int i = 0; i < 5; i++)
    {
        int textColor = (m_sortColumn == i) ? m_style.highlightColor : m_style.textDimColor;
        const char* text = ImGuiLocalization::GetInstance().GetString(colKeys[i]);
        DrawText(colX, colY, colX + colWidths[i], text, textColor);
        colX += colWidths[i];
    }
    
    // Draw separator line
    int sepY = colY + (int)(15 * scale);
    FillRGBA(windowX + (int)(10 * scale), sepY, windowWidth - (int)(20 * scale), 1, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 150);
    
    // Draw players
    int rowY = sepY + (int)(5 * scale);
    int rowHeight = (int)(m_style.rowHeight * scale);
    
    for (size_t i = 0; i < m_players.size() && rowY < windowY + windowHeight - rowHeight; i++)
    {
        const ImGuiPlayerData& player = m_players[i];
        
        // Draw row background
        int bgColor = (i % 2 == 0) ? m_style.rowEvenColor : m_style.rowOddColor;
        
        // Highlight local player
        if (player.isLocalPlayer)
        {
            FillRGBA(windowX + 2, rowY - 2, windowWidth - 4, rowHeight, 
                GetRValue(m_style.highlightColor), 
                GetGValue(m_style.highlightColor), 
                GetBValue(m_style.highlightColor), 60);
        }
        else
        {
            FillRGBA(windowX + 2, rowY - 2, windowWidth - 4, rowHeight, 
                GetRValue(bgColor), 
                GetGValue(bgColor), 
                GetBValue(bgColor), 180);
        }
        
        // Draw team color bar
        if (m_teamplay && player.teamNumber >= 0)
        {
            int teamColor = GetTeamColor(player.teamNumber);
            FillRGBA(windowX + 2, rowY - 2, 4, rowHeight, 
                (teamColor >> 16) & 0xFF, 
                (teamColor >> 8) & 0xFF, 
                teamColor & 0xFF, 255);
        }
        
        // Draw player data
        colX = windowX + (int)(10 * scale);
        
        // Rank
        char rankText[8];
        sprintf(rankText, "%d", (int)i + 1);
        DrawText(colX, rowY, colX + colWidths[0], rankText, m_style.textDimColor);
        colX += colWidths[0];
        
        // Name
        char nameText[64];
        if (player.isBot)
        {
            const char* botTag = ImGuiLocalization::GetInstance().GetString("BOT", "[BOT]");
            sprintf(nameText, "%s %s", player.name.c_str(), botTag);
        }
        else if (player.isLocalPlayer)
            sprintf(nameText, "> %s", player.name.c_str());
        else
            strncpy(nameText, player.name.c_str(), sizeof(nameText) - 1);
        
        DrawText(colX, rowY, colX + colWidths[1], nameText, m_style.textColor);
        colX += colWidths[1];
        
        // Score
        char scoreText[16];
        sprintf(scoreText, "%d", player.frags - player.deaths);
        DrawText(colX, rowY, colX + colWidths[2], scoreText, m_style.textColor);
        colX += colWidths[2];
        
        // Deaths
        sprintf(scoreText, "%d", player.deaths);
        DrawText(colX, rowY, colX + colWidths[3], scoreText, m_style.textColor);
        colX += colWidths[3];
        
        // Ping with color coding
        sprintf(scoreText, "%d", player.ping);
        int pingColor = m_style.textColor;
        if (player.ping < 50)
            pingColor = RGB(0, 255, 0);
        else if (player.ping < 100)
            pingColor = RGB(255, 255, 0);
        else
            pingColor = RGB(255, 128, 0);
        
        DrawText(colX, rowY, colX + colWidths[4], scoreText, pingColor);
        
        rowY += rowHeight;
    }
    
    // Draw footer (localized)
    int footerY = windowY + windowHeight - (int)(25 * scale);
    char footerText[64];
    sprintf(footerText, "%d / %d %s", (int)m_players.size(), m_maxPlayers,
            ImGuiLocalization::GetInstance().GetString("Players"));
    DrawText(windowX + (int)(10 * scale), footerY, windowX + windowWidth, footerText, m_style.textDimColor);
    
    // Draw game mode (localized)
    const char* modeText = m_teamplay 
        ? ImGuiLocalization::GetInstance().GetString("TeamDeathmatch")
        : ImGuiLocalization::GetInstance().GetString("Deathmatch");
    int modeWidth = strlen(modeText) * 8;
    DrawText(windowX + windowWidth - modeWidth - (int)(10 * scale), footerY, windowX + windowWidth, modeText, m_style.textDimColor);
    
    // Draw language selector
    DrawLanguageSelector(windowX + windowWidth - (int)(100 * scale), windowY + (int)(10 * scale), (int)scale);
}

//=============================================================================
// Language Selection
//=============================================================================
void ImGuiScoreboard::DrawLanguageSelector(int x, int y, int scale)
{
    const auto& languages = ImGuiLocalization::GetInstance().GetAvailableLanguages();
    if (languages.empty())
        return;
    
    // Ensure current index is valid
    if (m_currentLanguageIndex < 0 || m_currentLanguageIndex >= (int)languages.size())
        m_currentLanguageIndex = 0;
    
    // Draw language button background
    int btnWidth = (int)(80 * scale);
    int btnHeight = (int)(20 * scale);
    
    // Highlight on hover (simple implementation)
    FillRGBA(x, y, btnWidth, btnHeight, 
        GetRValue(m_style.headerColor), 
        GetGValue(m_style.headerColor), 
        GetBValue(m_style.headerColor), 200);
    
    // Draw border
    FillRGBA(x, y, btnWidth, 1, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    FillRGBA(x, y + btnHeight - 1, btnWidth, 1, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    FillRGBA(x, y, 1, btnHeight, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    FillRGBA(x + btnWidth - 1, y, 1, btnHeight, 
        GetRValue(m_style.borderColor), 
        GetGValue(m_style.borderColor), 
        GetBValue(m_style.borderColor), 255);
    
    // Draw current language name
    const char* langName = languages[m_currentLanguageIndex].displayName.c_str();
    int textX = x + (btnWidth - strlen(langName) * 8) / 2;
    int textY = y + (btnHeight - 8) / 2;
    DrawText(textX, textY, x + btnWidth, langName, m_style.textColor);
}

void ImGuiScoreboard::CycleLanguage()
{
    const auto& languages = ImGuiLocalization::GetInstance().GetAvailableLanguages();
    if (languages.empty())
        return;
    
    // Move to next language
    m_currentLanguageIndex++;
    if (m_currentLanguageIndex >= (int)languages.size())
        m_currentLanguageIndex = 0;
    
    // Load the new language
    const char* langFolder = languages[m_currentLanguageIndex].folderName.c_str();
    ImGuiLocalization::GetInstance().LoadLanguage(langFolder);
}

//=============================================================================
// Global Functions
//=============================================================================
void ImGuiScoreboard_Init()
{
    if (!g_ImGuiScoreboard)
    {
        g_ImGuiScoreboard = new ImGuiScoreboard();
        g_ImGuiScoreboard->Initialize();
    }
}

void ImGuiScoreboard_Shutdown()
{
    if (g_ImGuiScoreboard)
    {
        delete g_ImGuiScoreboard;
        g_ImGuiScoreboard = nullptr;
    }
}

void ImGuiScoreboard_Toggle()
{
    if (g_ImGuiScoreboard)
    {
        g_ImGuiScoreboard->Toggle();
    }
}

void ImGuiScoreboard_Show()
{
    if (g_ImGuiScoreboard)
    {
        g_ImGuiScoreboard->Show();
    }
}

void ImGuiScoreboard_Hide()
{
    if (g_ImGuiScoreboard)
    {
        g_ImGuiScoreboard->Hide();
    }
}

bool ImGuiScoreboard_IsVisible()
{
    return g_ImGuiScoreboard ? g_ImGuiScoreboard->IsVisible() : false;
}

void ImGuiScoreboard_Render()
{
    if (g_ImGuiScoreboard)
    {
        g_ImGuiScoreboard->RefreshData();
        g_ImGuiScoreboard->Render();
    }
}
