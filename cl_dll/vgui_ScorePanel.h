//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: VGUI scoreboard - VC6 compatible optimized version
//
//=============================================================================

#pragma once

#include <VGUI_Panel.h>
#include <VGUI_TablePanel.h>
#include <VGUI_HeaderPanel.h>
#include <VGUI_TextGrid.h>
#include <VGUI_Label.h>
#include <VGUI_TextImage.h>
#include "../game_shared/vgui_listbox.h"
#include "../game_shared/vgui_grid.h"
#include "../game_shared/vgui_defaultinputsignal.h"
#include <ctype.h>

// Constants (VC6 compatible)
#define MAX_SCORES 10
#define MAX_SCOREBOARD_TEAMS 5
#define NUM_ROWS (MAX_PLAYERS_HUD + (MAX_SCOREBOARD_TEAMS * 2))

// Column indices (use enum with prefix)
enum ScoreColumn
{
    COLUMN_TRACKER = 0,
    COLUMN_NAME,
    COLUMN_CLASS,
    COLUMN_KILLS,
    COLUMN_DEATHS,
    COLUMN_LATENCY,
    COLUMN_VOICE,
    COLUMN_BLANK,
    NUM_COLUMNS
};

// Row types
enum RowType
{
    ROWTYPE_PLAYER,
    ROWTYPE_TEAM,
    ROWTYPE_SPECTATOR,
    ROWTYPE_BLANK
};

// Forward declarations
class ScorePanel;
class CLabelHeader;

// Dual-text image helper
class CTextImage2 : public Image
{
public:
    CTextImage2();
    ~CTextImage2();

    TextImage* GetImage(int index) { return _image[index]; }
    void getSize(int& wide, int& tall) override;
    void doPaint(Panel* panel) override;
    void setPos(int x, int y) override;
    void setColor(Color color) override;
    void setColor2(Color color);

private:
    TextImage* _image[2];
};

// Custom label for scoreboard cells
class CLabelHeader : public Label
{
public:
    CLabelHeader();
    ~CLabelHeader();

    void setRow(int row) { _row = row; }
    void setFgColorAsImageColor(bool state) { _useFgColorAsImageColor = state; }
    void setText(int textBufferLen, const char* text) override;
    void setText(const char* text);
    void setText2(const char* text);
    void getTextSize(int& wide, int& tall) override;
    void setFgColor(int r, int g, int b, int a) override;
    void setFgColor(Scheme::SchemeColor sc) override;
    void setFont(Font* font) override;
    void setFont2(Font* font) { _dualImage->GetImage(1)->setFont(font); }
    void setTextOffset(int x, int y) { _offset[0] = x; _offset[1] = y; }

    void paint() override;
    void paintBackground() override;
    void calcAlignment(int iwide, int itall, int& x, int& y);

private:
    CTextImage2* _dualImage;
    int _row;
    int _gap;
    int _offset[2];
    bool _useFgColorAsImageColor;
};

// Hit test panel
class HitTestPanel : public Panel
{
public:
    void internalMousePressed(MouseCode code) override;
};

// Main ScorePanel class
class ScorePanel : public Panel, public vgui::CDefaultInputSignal
{
public:
    ScorePanel(int x, int y, int wide, int tall);

    void Update();
    void SortTeams();
    void SortPlayers(int iTeam, char* team);
    void RebuildTeams();
    void FillGrid();
    void DeathMsg(int killer, int victim);
    void Initialize();
    void Open();
    void MouseOverCell(int row, int col);

    // InputSignal overrides
    void mousePressed(MouseCode code, Panel* panel) override;
    void cursorMoved(int x, int y, Panel* panel) override;

    // Data members (public for external access)
    int m_iNumTeams;
    int m_iPlayerNum;
    int m_iShowscoresHeld;
    int m_iRows;
    int m_iSortedRows[NUM_ROWS];
    int m_iIsATeam[NUM_ROWS];
    bool m_bHasBeenSorted[MAX_PLAYERS_HUD];
    int m_iLastKilledBy;
    float m_fLastKillTime;

private:
    // UI components
    Label m_TitleLabel;
    CGrid m_HeaderGrid;
    CLabelHeader m_HeaderLabels[NUM_COLUMNS];
    CLabelHeader* m_pCurrentHighlightLabel;
    int m_iHighlightRow;
    vgui::CListBox m_PlayerList;
    CGrid m_PlayerGrids[NUM_ROWS];
    CLabelHeader m_PlayerEntries[NUM_COLUMNS][NUM_ROWS];
    HitTestPanel m_HitTestPanel;
    CommandButton* m_pCloseButton;

    // Helper methods
    void UpdateHighlightPosition();
    void HideRow(int row);
    RowType GetRowType(int row) const;
    void SetupRowAppearance(int row, CGrid* pGridRow, RowType type);
    void ConfigureTeamRow(int row, CGrid* pGridRow);
    void ConfigureSpectatorRow(int row, CGrid* pGridRow);
    void ConfigurePlayerRow(int row, CGrid* pGridRow);
    void ConfigureBlankRow(int row, CGrid* pGridRow);
    void SetupLabelAppearance(CLabelHeader* pLabel, int row, int col, RowType type);
    void GetCellText(int row, int col, RowType type, char* buffer, int bufsize) const;
    void AdjustRowHeights();

    // Utility
    CLabelHeader* GetPlayerEntry(int col, int row) { return &m_PlayerEntries[col][row]; }
    void SetLabelTextAndImage(CLabelHeader* pLabel, int col, int row, RowType type);

    friend class CLabelHeader;  // Allow label to access highlight row
};