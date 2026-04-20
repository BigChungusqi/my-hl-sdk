//=========== (C) Copyright 2025 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: ImGui Renderer Integration for Half-Life GoldSrc Engine
//          Handles ImGui initialization, rendering, and input for HL
//
//=============================================================================

#pragma once

#include "imgui.h"

// Half-Life engine integration for ImGui
class ImGuiHLRenderer
{
public:
    ImGuiHLRenderer();
    ~ImGuiHLRenderer();

    // Lifecycle
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    // Frame management
    void NewFrame();
    void Render();
    void EndFrame();

    // Input handling
    void ProcessInput();
    bool HandleKeyEvent(int key, bool down);
    bool HandleMouseMove(int x, int y);
    bool HandleMouseButton(int button, bool down);
    void HandleMouseWheel(int delta);

    // Screen size
    void UpdateDisplaySize();

    // Font management
    bool LoadFonts();
    void ReloadFonts();

    // Configuration
    void SetInputEnabled(bool enabled) { m_inputEnabled = enabled; }
    bool IsInputEnabled() const { return m_inputEnabled; }

    // Style configuration
    void ApplyHalfLifeStyle();
    void ApplyDarkStyle();
    void ApplyClassicStyle();

    // Global instance accessor
    static ImGuiHLRenderer* GetInstance() { return s_instance; }

private:
    bool m_initialized;
    bool m_inputEnabled;
    
    // Mouse state
    int m_mouseX, m_mouseY;
    bool m_mouseButtons[5];
    
    // Key mapping
    ImGuiKey MapKey(int hlKey);
    
    // Font atlas texture
    unsigned int m_fontTexture;
    bool m_fontsLoaded;

    static ImGuiHLRenderer* s_instance;
};

// Global helper functions
bool ImGuiHL_Init();
void ImGuiHL_Shutdown();
void ImGuiHL_NewFrame();
void ImGuiHL_Render();
void ImGuiHL_UpdateDisplaySize();

// Input handling
bool ImGuiHL_HandleKeyEvent(int key, bool down);
bool ImGuiHL_HandleMouseMove(int x, int y);
bool ImGuiHL_HandleMouseButton(int button, bool down);
void ImGuiHL_HandleMouseWheel(int delta);

// Enable/disable ImGui input processing
void ImGuiHL_SetInputEnabled(bool enabled);
bool ImGuiHL_IsInputEnabled();

// Check if ImGui wants to capture input
bool ImGuiHL_WantsCaptureMouse();
bool ImGuiHL_WantsCaptureKeyboard();