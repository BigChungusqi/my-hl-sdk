//=========== (C) Copyright 2025 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: CHudMessage implementation using ImGui 1.92.7
//          Replaces stb_truetype with ImGui for text rendering
//
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// ImGui headers
#include "imgui.h"
#include "imgui_impl_opengl2.h"
#include "imgui_impl_win32.h"

DECLARE_MESSAGE(m_Message, HudText)
DECLARE_MESSAGE(m_Message, GameTitle)

// Global client_textmessage_t for custom messages
client_textmessage_t g_pCustomMessage;
const char* g_pCustomName = "Custom";
char g_pCustomText[1024];

// ImGui font management
static ImFont* g_ImGuiFont = nullptr;
static bool g_ImGuiInitialized = false;
static float g_fontSize = 18.0f;

// Console variable for font size
static cvar_t* g_hud_fontsize = nullptr;

// Initialize ImGui font
static bool InitImGuiFont()
{
    if (g_ImGuiInitialized)
        return true;

    ImGuiIO& io = ImGui::GetIO();
    
    // Load Chinese font
    const char* fontPaths[] = {
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/SimHei.ttf",
        "C:/Windows/Fonts/SimSun.ttf",
        "C:/Windows/Fonts/msyhbd.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    
    // Read font size from cvar
    if (g_hud_fontsize)
    {
        int size = (int)g_hud_fontsize->value;
        if (size < 6) size = 6;
        if (size > 72) size = 72;
        g_fontSize = (float)size;
    }
    
    // Try to load Chinese font
    for (int i = 0; i < sizeof(fontPaths) / sizeof(fontPaths[0]); i++)
    {
        FILE* f = fopen(fontPaths[i], "rb");
        if (!f)
            continue;
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        void* fontData = malloc(size);
        if (!fontData)
        {
            fclose(f);
            continue;
        }
        
        if (fread(fontData, 1, size, f) != (size_t)size)
        {
            free(fontData);
            fclose(f);
            continue;
        }
        fclose(f);
        
        // Load font with Chinese glyph ranges
        ImFontConfig config;
        config.FontDataOwnedByAtlas = true;
        
        static const ImWchar ranges[] = {
            0x0020, 0x00FF, // Basic Latin
            0x2000, 0x206F, // General Punctuation
            0x3000, 0x30FF, // CJK Symbols and Japanese
            0x31F0, 0x31FF, // Katakana Phonetic Extensions
            0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
            0x4E00, 0x9FFF, // CJK Unified Ideographs
            0x3400, 0x4DBF, // CJK Extension A
            0
        };
        
        g_ImGuiFont = io.Fonts->AddFontFromMemoryTTF(fontData, (int)size, g_fontSize, &config, ranges);
        
        if (g_ImGuiFont)
        {
            char msg[256];
            sprintf(msg, "[ImGui] Loaded font: %s (size: %.0f)\n", fontPaths[i], g_fontSize);
            gEngfuncs.pfnConsolePrint(msg);
            g_ImGuiInitialized = true;
            return true;
        }
    }
    
    // Fallback to default font
    g_ImGuiFont = io.Fonts->AddFontDefault();
    gEngfuncs.pfnConsolePrint("[ImGui] Using default font\n");
    g_ImGuiInitialized = true;
    return true;
}

// UTF-8 string length
static int UTF8StrLen(const char* utf8)
{
    if (!utf8)
        return 0;
    
    int len = 0;
    const char* p = utf8;
    while (*p)
    {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80)
            p += 1;
        else if ((c & 0xE0) == 0xC0)
            p += 2;
        else if ((c & 0xF0) == 0xE0)
            p += 3;
        else if ((c & 0xF8) == 0xF0)
            p += 4;
        else
            p += 1;
        len++;
    }
    return len;
}

// Fade blend calculation
static int FadeBlend(float fadein, float fadeout, float holdtime, float localTime)
{
    float fadeTime = fadein + holdtime;
    
    if (localTime < fadein)
    {
        return (int)((localTime / fadein) * 255);
    }
    else if (localTime < fadeTime)
    {
        return 255;
    }
    else
    {
        if (fadeout > 0)
            return (int)(255 - ((localTime - fadeTime) / fadeout) * 255);
        else
            return 0;
    }
}

// Position calculations
static float XPosition(float x, int width, int totalWidth)
{
    if (x == -1)
        return (ScreenWidth - width) * 0.5f;
    
    if (x < 0)
        return (1.0f + x) * ScreenWidth - width;
    
    return x * ScreenWidth;
}

static float YPosition(float y, int height)
{
    if (y == -1)
        return (ScreenHeight - height) * 0.5f;
    
    if (y < 0)
        return (1.0f + y) * ScreenHeight - height;
    
    return y * ScreenHeight;
}

// CHudMessage implementation
int CHudMessage::Init(void)
{
    gEngfuncs.pfnConsolePrint("[ImGui] CHudMessage::Init\n");
    
    g_hud_fontsize = gEngfuncs.pfnRegisterVariable("hud_fontsize", "18", 0);
    
    gEngfuncs.pfnHookUserMsg("HudText", &CHudMessage::MsgFunc_HudText);
    gEngfuncs.pfnHookUserMsg("GameTitle", &CHudMessage::MsgFunc_GameTitle);
    
    m_pGameTitle = nullptr;
    m_gameTitleTime = 0;
    m_bEndAfterMessage = false;
    
    memset(m_pMessages, 0, sizeof(m_pMessages));
    memset(m_startTime, 0, sizeof(m_startTime));
    
    m_iFlags |= HUD_ACTIVE;
    
    gEngfuncs.pfnConsolePrint("[ImGui] CHudMessage initialized\n");
    return 1;
}

int CHudMessage::VidInit(void)
{
    gEngfuncs.pfnConsolePrint("[ImGui] CHudMessage::VidInit\n");
    
    // Initialize ImGui font
    InitImGuiFont();
    
    m_HUD_title_half = gHUD.GetSpriteIndex("title_half");
    m_HUD_title_life = gHUD.GetSpriteIndex("title_life");
    
    return 1;
}

void CHudMessage::MessageScanStart(void)
{
    switch (m_parms.pMessage->effect)
    {
    case 1:
    case 0:
        m_parms.fadeTime = m_parms.pMessage->fadein + m_parms.pMessage->holdtime;
        
        if (m_parms.time < m_parms.pMessage->fadein)
        {
            m_parms.fadeBlend = (int)(((m_parms.pMessage->fadein - m_parms.time) * (1.0f / m_parms.pMessage->fadein) * 255));
        }
        else if (m_parms.time > m_parms.fadeTime)
        {
            if (m_parms.pMessage->fadeout > 0)
                m_parms.fadeBlend = (int)((((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255));
            else
                m_parms.fadeBlend = 255;
        }
        else
        {
            m_parms.fadeBlend = 0;
        }
        break;
        
    case 2:
        m_parms.fadeTime = (m_parms.pMessage->fadein * UTF8StrLen(m_parms.pMessage->pMessage)) + m_parms.pMessage->fadeout + m_parms.pMessage->holdtime;
        
        if (m_parms.time < m_parms.pMessage->fadein)
        {
            m_parms.fadeBlend = (int)((m_parms.time * (1.0f / m_parms.pMessage->fadein)) * 255);
        }
        else if (m_parms.time > m_parms.fadeTime)
        {
            if (m_parms.pMessage->fadeout > 0)
                m_parms.fadeBlend = (int)((((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255));
            else
                m_parms.fadeBlend = 255;
        }
        else
        {
            m_parms.fadeBlend = 0;
        }
        break;
    }
    
    m_parms.x = XPosition(m_parms.pMessage->x, 0, 0);
    m_parms.y = YPosition(m_parms.pMessage->y, 0);
    m_parms.charTime = 0;
}

void CHudMessage::MessageScanNextChar(void)
{
    int srcRed = m_parms.pMessage->r1;
    int srcGreen = m_parms.pMessage->g1;
    int srcBlue = m_parms.pMessage->b1;
    int blend = 0;
    int destRed = 0, destGreen = 0, destBlue = 0;
    
    switch (m_parms.pMessage->effect)
    {
    case 0:
    case 1:
        blend = m_parms.fadeBlend;
        break;
        
    case 2:
        m_parms.charTime += m_parms.pMessage->fadein;
        if (m_parms.charTime > m_parms.time)
        {
            srcRed = srcGreen = srcBlue = 0;
            blend = 0;
        }
        else
        {
            float deltaTime = m_parms.time - m_parms.charTime;
            
            if (m_parms.time > m_parms.fadeTime)
            {
                blend = m_parms.fadeBlend;
            }
            else if (deltaTime > m_parms.pMessage->fxtime)
            {
                blend = 0;
            }
            else
            {
                destRed = m_parms.pMessage->r2;
                destGreen = m_parms.pMessage->g2;
                destBlue = m_parms.pMessage->b2;
                blend = (int)(255 - (deltaTime * (1.0f / m_parms.pMessage->fxtime) * 255.0f));
            }
        }
        break;
    }
    
    if (blend > 255) blend = 255;
    if (blend < 0) blend = 0;
    
    m_parms.r = ((srcRed * (255 - blend)) + (destRed * blend)) >> 8;
    m_parms.g = ((srcGreen * (255 - blend)) + (destGreen * blend)) >> 8;
    m_parms.b = ((srcBlue * (255 - blend)) + (destBlue * blend)) >> 8;
}

void CHudMessage::MessageDrawScan(client_textmessage_t* pMessage, float time)
{
    m_parms.pMessage = pMessage;
    m_parms.time = time;
    
    MessageScanStart();
    
    if (!g_ImGuiInitialized || !g_ImGuiFont)
        return;
    
    // Set ImGui font
    ImGui::PushFont(g_ImGuiFont);
    
    // Calculate text size
    ImVec2 textSize = ImGui::CalcTextSize(pMessage->pMessage);
    
    // Recalculate position with text size
    float x = XPosition(pMessage->x, (int)textSize.x, (int)textSize.x);
    float y = YPosition(pMessage->y, (int)textSize.y);
    
    // Draw text with ImGui
    ImU32 color = IM_COL32(m_parms.r, m_parms.g, m_parms.b, 255);
    
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddText(ImVec2(x, y), color, pMessage->pMessage);
    
    ImGui::PopFont();
}

bool CHudMessage::Draw(float fTime)
{
    int drawn = 0;
    
    // Initialize ImGui frame if needed
    if (!g_ImGuiInitialized)
    {
        InitImGuiFont();
    }
    
    // Draw game title
    if (m_gameTitleTime > 0)
    {
        float localTime = gHUD.m_flTime - m_gameTitleTime;
        
        if (m_gameTitleTime > gHUD.m_flTime)
            m_gameTitleTime = gHUD.m_flTime;
        
        if (localTime > (m_pGameTitle->fadein + m_pGameTitle->holdtime + m_pGameTitle->fadeout))
        {
            m_gameTitleTime = 0;
        }
        else
        {
            int brightness = FadeBlend(m_pGameTitle->fadein, m_pGameTitle->fadeout, m_pGameTitle->holdtime, localTime);
            
            int halfWidth = gHUD.GetSpriteRect(m_HUD_title_half).right - gHUD.GetSpriteRect(m_HUD_title_half).left;
            int fullWidth = halfWidth + gHUD.GetSpriteRect(m_HUD_title_life).right - gHUD.GetSpriteRect(m_HUD_title_life).left;
            int fullHeight = gHUD.GetSpriteRect(m_HUD_title_half).bottom - gHUD.GetSpriteRect(m_HUD_title_half).top;
            
            int x = (int)XPosition(m_pGameTitle->x, fullWidth, fullWidth);
            int y = (int)YPosition(m_pGameTitle->y, fullHeight);
            
            SPR_Set(gHUD.GetSprite(m_HUD_title_half), brightness, brightness, brightness);
            SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_title_half));
            
            SPR_Set(gHUD.GetSprite(m_HUD_title_life), brightness, brightness, brightness);
            SPR_DrawAdditive(0, x + halfWidth, y, &gHUD.GetSpriteRect(m_HUD_title_life));
            
            drawn = 1;
        }
    }
    
    // Update message start times
    for (int i = 0; i < maxHUDMessages; i++)
    {
        if (m_pMessages[i])
        {
            if (m_startTime[i] > gHUD.m_flTime)
            {
                m_startTime[i] = gHUD.m_flTime + m_parms.time - m_startTime[i] + 0.2f;
            }
        }
    }
    
    // Draw messages
    for (int i = 0; i < maxHUDMessages; i++)
    {
        if (m_pMessages[i])
        {
            client_textmessage_t* pMessage = m_pMessages[i];
            float endTime;
            
            switch (pMessage->effect)
            {
            default:
            case 0:
            case 1:
                endTime = m_startTime[i] + pMessage->fadein + pMessage->fadeout + pMessage->holdtime;
                break;
            case 2:
                endTime = m_startTime[i] + (pMessage->fadein * UTF8StrLen(pMessage->pMessage)) + pMessage->fadeout + pMessage->holdtime;
                break;
            }
            
            if (fTime <= endTime)
            {
                float messageTime = fTime - m_startTime[i];
                MessageDrawScan(pMessage, messageTime);
                drawn++;
            }
            else
            {
                m_pMessages[i] = nullptr;
                
                if (m_bEndAfterMessage)
                {
                    gEngfuncs.pfnClientCmd("wait\nwait\nwait\nwait\nwait\nwait\nwait\ndisconnect\n");
                }
            }
        }
    }
    
    m_parms.time = gHUD.m_flTime;
    
    if (drawn == 0)
        m_iFlags &= ~HUD_ACTIVE;
    
    return true;
}

void CHudMessage::MessageAdd(const char* pName, float time)
{
    for (int i = 0; i < maxHUDMessages; i++)
    {
        if (!m_pMessages[i])
        {
            client_textmessage_t* tempMessage;
            
            if (pName[0] == '#')
                tempMessage = TextMessageGet(pName + 1);
            else
                tempMessage = TextMessageGet(pName);
            
            if (!tempMessage)
            {
                g_pCustomMessage.effect = 2;
                g_pCustomMessage.r1 = g_pCustomMessage.g1 = g_pCustomMessage.b1 = g_pCustomMessage.a1 = 100;
                g_pCustomMessage.r2 = 240;
                g_pCustomMessage.g2 = 110;
                g_pCustomMessage.b2 = 0;
                g_pCustomMessage.a2 = 0;
                g_pCustomMessage.x = -1;
                g_pCustomMessage.y = 0.7f;
                g_pCustomMessage.fadein = 0.01f;
                g_pCustomMessage.fadeout = 1.5f;
                g_pCustomMessage.fxtime = 0.25f;
                g_pCustomMessage.holdtime = 5;
                g_pCustomMessage.pName = g_pCustomName;
                strcpy(g_pCustomText, pName);
                g_pCustomMessage.pMessage = g_pCustomText;
                
                tempMessage = &g_pCustomMessage;
            }
            
            // Check for duplicates
            for (int j = 0; j < maxHUDMessages; j++)
            {
                if (m_pMessages[j])
                {
                    if (strcmp(tempMessage->pMessage, m_pMessages[j]->pMessage) == 0)
                        return;
                    
                    if (fabs(tempMessage->y - m_pMessages[j]->y) < 0.0001f)
                    {
                        if (fabs(tempMessage->x - m_pMessages[j]->x) < 0.0001f)
                        {
                            m_pMessages[j] = nullptr;
                        }
                    }
                }
            }
            
            m_pMessages[i] = tempMessage;
            m_startTime[i] = time;
            return;
        }
    }
}

bool CHudMessage::MsgFunc_HudText(const char* pszName, int iSize, void* pbuf)
{
    BEGIN_READ(pbuf, iSize);
    char* pString = READ_STRING();
    
    const char* HL1_ENDING_STR = "END3";
    
    if (strlen(pString) == strlen(HL1_ENDING_STR) && strcmp(HL1_ENDING_STR, pString) == 0)
    {
        m_bEndAfterMessage = true;
    }
    
    MessageAdd(pString, gHUD.m_flTime);
    m_parms.time = gHUD.m_flTime;
    
    if ((m_iFlags & HUD_ACTIVE) == 0)
        m_iFlags |= HUD_ACTIVE;
    
    return true;
}

bool CHudMessage::MsgFunc_GameTitle(const char* pszName, int iSize, void* pbuf)
{
    m_pGameTitle = TextMessageGet("GAMETITLE");
    if (m_pGameTitle != nullptr)
    {
        m_gameTitleTime = gHUD.m_flTime;
        
        if ((m_iFlags & HUD_ACTIVE) == 0)
            m_iFlags |= HUD_ACTIVE;
    }
    
    return true;
}

void CHudMessage::MessageAdd(client_textmessage_t* newMessage)
{
    m_parms.time = gHUD.m_flTime;
    
    if ((m_iFlags & HUD_ACTIVE) == 0)
        m_iFlags |= HUD_ACTIVE;
    
    for (int i = 0; i < maxHUDMessages; i++)
    {
        if (!m_pMessages[i])
        {
            m_pMessages[i] = newMessage;
            m_startTime[i] = gHUD.m_flTime;
            return;
        }
    }
}
