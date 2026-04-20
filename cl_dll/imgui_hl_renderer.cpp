//=========== (C) Copyright 2025 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: ImGui Renderer Integration for Half-Life GoldSrc Engine
//
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "imgui_hl_renderer.h"
#include "imgui_scoreboard.h"

// ImGui headers
#include "imgui.h"
#include "imgui_internal.h"

// Engine interface
#include "r_studioint.h"
#include "ref_params.h"

// Global instance
ImGuiHLRenderer* ImGuiHLRenderer::s_instance = nullptr;

// External engine functions
extern "C" {
    typedef struct cl_enginefuncs_s cl_enginefuncs_t;
    extern cl_enginefuncs_t gEngfuncs;
}

// Half-Life key codes (from in_defs.h)
#define K_TAB           9
#define K_ENTER         13
#define K_ESCAPE        27
#define K_SPACE         32
#define K_BACKSPACE     127
#define K_UPARROW       128
#define K_DOWNARROW     129
#define K_LEFTARROW     130
#define K_RIGHTARROW    131
#define K_ALT           132
#define K_CTRL          133
#define K_SHIFT         134
#define K_F1            135
#define K_F2            136
#define K_F3            137
#define K_F4            138
#define K_F5            139
#define K_F6            140
#define K_F7            141
#define K_F8            142
#define K_F9            143
#define K_F10           144
#define K_F11           145
#define K_F12           146
#define K_INS           147
#define K_DEL           148
#define K_PGDN          149
#define K_PGUP          150
#define K_HOME          151
#define K_END           152
#define K_MOUSE1        200
#define K_MOUSE2        201
#define K_MOUSE3        202
#define K_MWHEELUP      203
#define K_MWHEELDOWN    204

// Forward declaration
void RenderImGuiDrawData(ImDrawData* drawData);

//=============================================================================
// Constructor / Destructor
//=============================================================================
ImGuiHLRenderer::ImGuiHLRenderer()
    : m_initialized(false)
    , m_inputEnabled(false)
    , m_mouseX(0)
    , m_mouseY(0)
    , m_fontTexture(0)
    , m_fontsLoaded(false)
{
    memset(m_mouseButtons, 0, sizeof(m_mouseButtons));
}

ImGuiHLRenderer::~ImGuiHLRenderer()
{
    Shutdown();
}

//=============================================================================
// Lifecycle
//=============================================================================
bool ImGuiHLRenderer::Initialize()
{
    if (m_initialized)
        return true;

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Disable ImGui ini file
    io.IniFilename = nullptr;
    
    // Set up display size
    UpdateDisplaySize();
    
    // Load fonts
    if (!LoadFonts())
    {
        gEngfuncs.pfnConsolePrint("ImGui: Failed to load fonts\n");
    }
    
    // Apply default style
    ApplyDarkStyle();
    
    // Initialize scoreboard
    ImGuiScoreboard_Init();
    
    m_initialized = true;
    s_instance = this;
    
    gEngfuncs.pfnConsolePrint("ImGui renderer initialized\n");
    return true;
}

void ImGuiHLRenderer::Shutdown()
{
    if (!m_initialized)
        return;

    // Shutdown scoreboard
    ImGuiScoreboard_Shutdown();
    
    // Destroy ImGui context
    ImGui::DestroyContext();
    
    m_initialized = false;
    s_instance = nullptr;
    
    gEngfuncs.pfnConsolePrint("ImGui renderer shutdown\n");
}

//=============================================================================
// Frame Management
//=============================================================================
void ImGuiHLRenderer::NewFrame()
{
    if (!m_initialized)
        return;

    ImGuiIO& io = ImGui::GetIO();
    
    // Update display size
    UpdateDisplaySize();
    
    // Update delta time
    io.DeltaTime = gHUD.m_flTimeDelta > 0.0f ? gHUD.m_flTimeDelta : 1.0f / 60.0f;
    
    // Process input
    ProcessInput();
    
    // Start new frame
    ImGui::NewFrame();
}

void ImGuiHLRenderer::Render()
{
    if (!m_initialized)
        return;

    // Render all ImGui windows
    ImGui::Render();
    
    // Get draw data
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->CmdListsCount == 0)
        return;

    // Render using Half-Life's 2D drawing functions
    RenderImGuiDrawData(drawData);
}

void ImGuiHLRenderer::EndFrame()
{
    // Frame is complete
}

//=============================================================================
// Rendering Implementation
//=============================================================================
// Helper function to draw a triangle using FillRGBA (scanline rasterization)
static void DrawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int r, int g, int b, int a)
{
    // Simple triangle rasterization using scanline approach
    // Sort vertices by Y
    if (y0 > y1) { int tmp = y0; y0 = y1; y1 = tmp; tmp = x0; x0 = x1; x1 = tmp; }
    if (y0 > y2) { int tmp = y0; y0 = y2; y2 = tmp; tmp = x0; x0 = x2; x2 = tmp; }
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; tmp = x1; x1 = x2; x2 = tmp; }
    
    if (y2 == y0) return; // Degenerate triangle
    
    // Draw horizontal spans
    for (int y = y0; y <= y2; y++)
    {
        // Calculate x coordinates at this y
        float t0 = (y2 == y0) ? 0.0f : (float)(y - y0) / (y2 - y0);
        float t1 = (y1 == y0) ? 0.0f : (float)(y - y0) / (y1 - y0);
        float t2 = (y2 == y1) ? 0.0f : (float)(y - y1) / (y2 - y1);
        
        int x_start, x_end;
        
        if (y < y1)
        {
            // Top half
            x_start = (int)(x0 + (x2 - x0) * t0);
            x_end = (int)(x0 + (x1 - x0) * t1);
        }
        else
        {
            // Bottom half
            x_start = (int)(x0 + (x2 - x0) * t0);
            x_end = (int)(x1 + (x2 - x1) * t2);
        }
        
        if (x_start > x_end) { int tmp = x_start; x_start = x_end; x_end = tmp; }
        
        if (x_end > x_start)
        {
            FillRGBA(x_start, y, x_end - x_start + 1, 1, r, g, b, a);
        }
    }
}

void RenderImGuiDrawData(ImDrawData* drawData)
{
    // Avoid rendering when minimized
    if (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
        return;

    // Scale coordinates for screen resolution
    int screenWidth = (ScreenWidth > 0) ? ScreenWidth : 640;
    int screenHeight = (ScreenHeight > 0) ? ScreenHeight : 480;
    float scaleX = screenWidth / drawData->DisplaySize.x;
    float scaleY = screenHeight / drawData->DisplaySize.y;

    // Iterate through all command lists
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        const ImDrawVert* vtxBuffer = cmdList->VtxBuffer.Data;
        const ImDrawIdx* idxBuffer = cmdList->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < cmdList->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmd_i];
            
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmdList, pcmd);
            }
            else
            {
                // Get clip rectangle (for manual clipping in triangle rasterization)
                int clipMinX = (int)((pcmd->ClipRect.x - drawData->DisplayPos.x) * scaleX);
                int clipMinY = (int)((pcmd->ClipRect.y - drawData->DisplayPos.y) * scaleY);
                int clipMaxX = (int)((pcmd->ClipRect.z - drawData->DisplayPos.x) * scaleX);
                int clipMaxY = (int)((pcmd->ClipRect.w - drawData->DisplayPos.y) * scaleY);

                // Render triangles
                for (unsigned int i = 0; i < pcmd->ElemCount; i += 3)
                {
                    const ImDrawIdx idx0 = idxBuffer[pcmd->IdxOffset + i];
                    const ImDrawIdx idx1 = idxBuffer[pcmd->IdxOffset + i + 1];
                    const ImDrawIdx idx2 = idxBuffer[pcmd->IdxOffset + i + 2];

                    const ImDrawVert& v0 = vtxBuffer[pcmd->VtxOffset + idx0];
                    const ImDrawVert& v1 = vtxBuffer[pcmd->VtxOffset + idx1];
                    const ImDrawVert& v2 = vtxBuffer[pcmd->VtxOffset + idx2];

                    // Scale vertices to screen coordinates
                    int x0 = (int)(v0.pos.x * scaleX);
                    int y0 = (int)(v0.pos.y * scaleY);
                    int x1 = (int)(v1.pos.x * scaleX);
                    int y1 = (int)(v1.pos.y * scaleY);
                    int x2 = (int)(v2.pos.x * scaleX);
                    int y2 = (int)(v2.pos.y * scaleY);

                    // Simple clip check - skip triangles completely outside clip rect
                    int triMinX = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
                    int triMaxX = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
                    int triMinY = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
                    int triMaxY = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);
                    
                    if (triMaxX < clipMinX || triMinX > clipMaxX ||
                        triMaxY < clipMinY || triMinY > clipMaxY)
                        continue;

                    // Get color (average of three vertices for smoother look)
                    ImU32 col0 = v0.col;
                    ImU32 col1 = v1.col;
                    ImU32 col2 = v2.col;
                    
                    int r = ((col0 & 0xFF) + (col1 & 0xFF) + (col2 & 0xFF)) / 3;
                    int green = (((col0 >> 8) & 0xFF) + ((col1 >> 8) & 0xFF) + ((col2 >> 8) & 0xFF)) / 3;
                    int b = (((col0 >> 16) & 0xFF) + ((col1 >> 16) & 0xFF) + ((col2 >> 16) & 0xFF)) / 3;
                    int a = (((col0 >> 24) & 0xFF) + ((col1 >> 24) & 0xFF) + ((col2 >> 24) & 0xFF)) / 3;

                    // Draw the triangle
                    DrawTriangle(x0, y0, x1, y1, x2, y2, r, green, b, a);
                }
            }
        }
    }
}

//=============================================================================
// Input Handling
//=============================================================================
void ImGuiHLRenderer::ProcessInput()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // Update mouse position
    io.AddMousePosEvent((float)m_mouseX, (float)m_mouseY);
    
    // Update mouse buttons
    for (int i = 0; i < 5; i++)
    {
        io.AddMouseButtonEvent(i, m_mouseButtons[i]);
    }
}

bool ImGuiHLRenderer::HandleKeyEvent(int key, bool down)
{
    if (!m_initialized || !m_inputEnabled)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    
    ImGuiKey imguiKey = MapKey(key);
    if (imguiKey != ImGuiKey_None)
    {
        io.AddKeyEvent(imguiKey, down);
        return io.WantCaptureKeyboard;
    }
    
    // Handle character input
    if (down && key >= 32 && key < 127)
    {
        io.AddInputCharacter((unsigned int)key);
        return io.WantCaptureKeyboard;
    }
    
    return false;
}

bool ImGuiHLRenderer::HandleMouseMove(int x, int y)
{
    m_mouseX = x;
    m_mouseY = y;
    
    if (!m_initialized || !m_inputEnabled)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;
}

bool ImGuiHLRenderer::HandleMouseButton(int button, bool down)
{
    if (button >= 0 && button < 5)
    {
        m_mouseButtons[button] = down;
    }
    
    if (!m_initialized || !m_inputEnabled)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;
}

void ImGuiHLRenderer::HandleMouseWheel(int delta)
{
    if (!m_initialized || !m_inputEnabled)
        return;

    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent(0.0f, delta > 0 ? 1.0f : -1.0f);
}

ImGuiKey ImGuiHLRenderer::MapKey(int hlKey)
{
    switch (hlKey)
    {
        case K_TAB: return ImGuiKey_Tab;
        case K_ENTER: return ImGuiKey_Enter;
        case K_ESCAPE: return ImGuiKey_Escape;
        case K_SPACE: return ImGuiKey_Space;
        case K_BACKSPACE: return ImGuiKey_Backspace;
        case K_UPARROW: return ImGuiKey_UpArrow;
        case K_DOWNARROW: return ImGuiKey_DownArrow;
        case K_LEFTARROW: return ImGuiKey_LeftArrow;
        case K_RIGHTARROW: return ImGuiKey_RightArrow;
        case K_ALT: return ImGuiKey_LeftAlt;
        case K_CTRL: return ImGuiKey_LeftCtrl;
        case K_SHIFT: return ImGuiKey_LeftShift;
        case K_F1: return ImGuiKey_F1;
        case K_F2: return ImGuiKey_F2;
        case K_F3: return ImGuiKey_F3;
        case K_F4: return ImGuiKey_F4;
        case K_F5: return ImGuiKey_F5;
        case K_F6: return ImGuiKey_F6;
        case K_F7: return ImGuiKey_F7;
        case K_F8: return ImGuiKey_F8;
        case K_F9: return ImGuiKey_F9;
        case K_F10: return ImGuiKey_F10;
        case K_F11: return ImGuiKey_F11;
        case K_F12: return ImGuiKey_F12;
        case K_INS: return ImGuiKey_Insert;
        case K_DEL: return ImGuiKey_Delete;
        case K_PGDN: return ImGuiKey_PageDown;
        case K_PGUP: return ImGuiKey_PageUp;
        case K_HOME: return ImGuiKey_Home;
        case K_END: return ImGuiKey_End;
        case K_MOUSE1: return ImGuiKey_MouseLeft;
        case K_MOUSE2: return ImGuiKey_MouseRight;
        case K_MOUSE3: return ImGuiKey_MouseMiddle;
        default:
            if (hlKey >= 'A' && hlKey <= 'Z')
                return (ImGuiKey)(ImGuiKey_A + (hlKey - 'A'));
            if (hlKey >= '0' && hlKey <= '9')
                return (ImGuiKey)(ImGuiKey_0 + (hlKey - '0'));
            return ImGuiKey_None;
    }
}

//=============================================================================
// Screen Size
//=============================================================================
void ImGuiHLRenderer::UpdateDisplaySize()
{
    ImGuiIO& io = ImGui::GetIO();
    // Use safe defaults if ScreenWidth/ScreenHeight not yet initialized
    int width = (ScreenWidth > 0) ? ScreenWidth : 640;
    int height = (ScreenHeight > 0) ? ScreenHeight : 480;
    io.DisplaySize = ImVec2((float)width, (float)height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

//=============================================================================
// Font Management
//=============================================================================
bool ImGuiHLRenderer::LoadFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // Clear existing fonts
    io.Fonts->Clear();
    
    // Try to load system fonts
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/msyh.ttf",  // Chinese
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/System/Library/Fonts/Helvetica.ttc",
    };
    
    ImFont* defaultFont = nullptr;
    
    for (const char* path : fontPaths)
    {
        FILE* file = fopen(path, "rb");
        if (file)
        {
            fclose(file);
            
            // Load at different sizes
            ImFontConfig config;
            config.SizePixels = 16.0f;
            defaultFont = io.Fonts->AddFontFromFileTTF(path, 16.0f, &config);
            
            if (defaultFont)
            {
                // Add larger font for headers
                config.SizePixels = 20.0f;
                io.Fonts->AddFontFromFileTTF(path, 20.0f, &config);
                
                // Add smaller font for details
                config.SizePixels = 14.0f;
                io.Fonts->AddFontFromFileTTF(path, 14.0f, &config);
                
                gEngfuncs.pfnConsolePrint("ImGui: Loaded font\n");
                break;
            }
        }
    }
    
    // Use default font if no system font loaded
    if (!defaultFont)
    {
        ImFontConfig config;
        config.SizePixels = 16.0f;
        defaultFont = io.Fonts->AddFontDefault(&config);
    }
    
    // Build font atlas
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    
    m_fontsLoaded = true;
    return true;
}

void ImGuiHLRenderer::ReloadFonts()
{
    LoadFonts();
}

//=============================================================================
// Style Configuration
//=============================================================================
void ImGuiHLRenderer::ApplyHalfLifeStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Half-Life orange theme
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.20f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.67f, 0.0f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.25f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.67f, 0.0f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.53f, 0.0f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.25f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.67f, 0.0f, 0.5f);
    colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.67f, 0.0f, 1.0f);
    colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
}

void ImGuiHLRenderer::ApplyDarkStyle()
{
    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
}

void ImGuiHLRenderer::ApplyClassicStyle()
{
    ImGui::StyleColorsClassic();
}

//=============================================================================
// Global Helper Functions
//=============================================================================
bool ImGuiHL_Init()
{
    if (!ImGuiHLRenderer::GetInstance())
    {
        static ImGuiHLRenderer renderer;
        return renderer.Initialize();
    }
    return ImGuiHLRenderer::GetInstance()->IsInitialized();
}

void ImGuiHL_Shutdown()
{
    if (ImGuiHLRenderer::GetInstance())
    {
        ImGuiHLRenderer::GetInstance()->Shutdown();
    }
}

void ImGuiHL_NewFrame()
{
    if (ImGuiHLRenderer::GetInstance())
    {
        ImGuiHLRenderer::GetInstance()->NewFrame();
    }
}

void ImGuiHL_Render()
{
    if (ImGuiHLRenderer::GetInstance())
    {
        ImGuiHLRenderer::GetInstance()->Render();
    }
}

void ImGuiHL_UpdateDisplaySize()
{
    if (ImGuiHLRenderer::GetInstance())
    {
        ImGuiHLRenderer::GetInstance()->UpdateDisplaySize();
    }
}

bool ImGuiHL_HandleKeyEvent(int key, bool down)
{
    if (ImGuiHLRenderer::GetInstance())
    {
        return ImGuiHLRenderer::GetInstance()->HandleKeyEvent(key, down);
    }
    return false;
}

bool ImGuiHL_HandleMouseMove(int x, int y)
{
    if (ImGuiHLRenderer::GetInstance())
    {
        return ImGuiHLRenderer::GetInstance()->HandleMouseMove(x, y);
    }
    return false;
}

bool ImGuiHL_HandleMouseButton(int button, bool down)
{
    if (ImGuiHLRenderer::GetInstance())
    {
        return ImGuiHLRenderer::GetInstance()->HandleMouseButton(button, down);
    }
    return false;
}

void ImGuiHL_HandleMouseWheel(int delta)
{
    if (ImGuiHLRenderer::GetInstance())
    {
        ImGuiHLRenderer::GetInstance()->HandleMouseWheel(delta);
    }
}

void ImGuiHL_SetInputEnabled(bool enabled)
{
    if (ImGuiHLRenderer::GetInstance())
    {
        ImGuiHLRenderer::GetInstance()->SetInputEnabled(enabled);
    }
}

bool ImGuiHL_IsInputEnabled()
{
    if (ImGuiHLRenderer::GetInstance())
    {
        return ImGuiHLRenderer::GetInstance()->IsInputEnabled();
    }
    return false;
}

bool ImGuiHL_WantsCaptureMouse()
{
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;
}

bool ImGuiHL_WantsCaptureKeyboard()
{
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard;
}
