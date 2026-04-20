//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: VGUI scoreboard - Optimized for VC6
//
//=============================================================================

#include <VGUI_LineBorder.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_ScorePanel.h"
#include "vgui_helpers.h"
#include "vgui_loadtga.h"
#include "voice_status.h"
#include "vgui_SpectatorPanel.h"
#include <stdio.h>
#include <string.h>

// External globals
extern hud_player_info_t g_PlayerInfoList[MAX_PLAYERS_HUD + 1];
extern extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS_HUD + 1];
team_info_t g_TeamInfo[MAX_TEAMS + 1];
int g_IsSpectator[MAX_PLAYERS_HUD + 1];

bool HUD_IsGame(const char* game);
bool EV_TFC_IsAllyTeam(int iTeam1, int iTeam2);

// Scoreboard dimensions
const int SBOARD_TITLE_SIZE_Y = 22;  // will be scaled by YRES
const int X_BORDER = 4;              // will be scaled by XRES

// Column info structure
struct SBColumnInfo
{
    const char* m_pTitle;   // If null, ignore; if starts with '#', localized
    int m_Width;            // Base width at 640 width, scaled
    Label::Alignment m_Alignment;
};

// Column definitions
static const SBColumnInfo g_ColumnInfo[NUM_COLUMNS] =
{
    {NULL, 24,  Label::a_east},   // tracker
    {NULL, 140, Label::a_east},   // name
    {NULL, 56,  Label::a_east},   // class
    {"#SCORE", 40, Label::a_east},
    {"#DEATHS", 46, Label::a_east},
    {"#LATENCY", 46, Label::a_east},
    {"#VOICE", 40, Label::a_east},
    {NULL, 2,   Label::a_east}    // blank
};

// Team display types
const int TEAM_NO = 0;
const int TEAM_YES = 1;
const int TEAM_SPECTATORS = 2;
const int TEAM_BLANK = 3;

//-----------------------------------------------------------------------------
// CTextImage2 implementation
//-----------------------------------------------------------------------------
CTextImage2::CTextImage2()
{
    _image[0] = new TextImage("");
    _image[1] = new TextImage("");
}

CTextImage2::~CTextImage2()
{
    delete _image[0];
    delete _image[1];
}

void CTextImage2::getSize(int& wide, int& tall)
{
    int w1, t1, w2, t2;
    _image[0]->getTextSize(w1, t1);
    _image[1]->getTextSize(w2, t2);
    wide = w1 + w2;
    tall = V_max(t1, t2);
}

void CTextImage2::doPaint(Panel* panel)
{
    _image[0]->doPaint(panel);
    _image[1]->doPaint(panel);
}

void CTextImage2::setPos(int x, int y)
{
    _image[0]->setPos(x, y);
    int w1, h1, w2, h2;
    _image[0]->getSize(w1, h1);
    _image[1]->getSize(w2, h2);
    _image[1]->setPos(x + w1, y + (int)(h1 * 0.9f) - h2);
}

void CTextImage2::setColor(Color color)
{
    _image[0]->setColor(color);
}

void CTextImage2::setColor2(Color color)
{
    _image[1]->setColor(color);
}

//-----------------------------------------------------------------------------
// HitTestPanel implementation
//-----------------------------------------------------------------------------
void HitTestPanel::internalMousePressed(MouseCode code)
{
    for (int i = 0; i < _inputSignalDar.getCount(); ++i)
        _inputSignalDar[i]->mousePressed(code, this);
}

//-----------------------------------------------------------------------------
// CLabelHeader implementation
//-----------------------------------------------------------------------------
CLabelHeader::CLabelHeader() : Label("")
{
    _dualImage = new CTextImage2();
    _dualImage->setColor2(Color(255, 170, 0, 0));
    _row = -2;
    _useFgColorAsImageColor = true;
    _offset[0] = _offset[1] = 0;
    _gap = 0;
}

CLabelHeader::~CLabelHeader()
{
    delete _dualImage;
}

void CLabelHeader::setText(int, const char* text)
{
    _dualImage->GetImage(0)->setText(text);
    // Calculate gap for second text placement
    Font* font = _dualImage->GetImage(0)->getFont();
    _gap = 0;
    for (const char* ch = text; *ch != 0; ++ch)
    {
        int a, b, c;
        font->getCharABCwide(*ch, a, b, c);
        _gap += (a + b + c);
    }
    _gap += XRES(5);
}

void CLabelHeader::setText(const char* text)
{
    // Remove trailing whitespace
    char cleanText[512];
    strcpy(cleanText, text);
    int len = strlen(cleanText);
    while (len > 0 && isspace(cleanText[len - 1]))
        cleanText[--len] = 0;
    setText(0, cleanText);
}

void CLabelHeader::setText2(const char* text)
{
    _dualImage->GetImage(1)->setText(text);
}

void CLabelHeader::getTextSize(int& wide, int& tall)
{
    _dualImage->getSize(wide, tall);
}

void CLabelHeader::setFgColor(int r, int g, int b, int a)
{
    Label::setFgColor(r, g, b, a);
    Color color(r, g, b, a);
    _dualImage->setColor(color);
    _dualImage->setColor2(color);
    repaint();
}

void CLabelHeader::setFgColor(Scheme::SchemeColor sc)
{
    int r, g, b, a;
    Label::setFgColor(sc);
    Label::getFgColor(r, g, b, a);
    setFgColor(r, g, b, a);
}

void CLabelHeader::setFont(Font* font)
{
    _dualImage->GetImage(0)->setFont(font);
}

void CLabelHeader::paintBackground()
{
    Color oldBg;
    getBgColor(oldBg);

    ScorePanel* pPanel = gViewPort->GetScoreBoard();
    if (pPanel && pPanel->m_iHighlightRow == _row)
        setBgColor(134, 91, 19, 0);

    Panel::paintBackground();
    setBgColor(oldBg);
}

void CLabelHeader::paint()
{
    Color oldFg;
    getFgColor(oldFg);

    ScorePanel* pPanel = gViewPort->GetScoreBoard();
    if (pPanel && pPanel->m_iHighlightRow == _row)
        setFgColor(255, 255, 255, 0);

    int x, y, iwide, itall;
    getTextSize(iwide, itall);
    calcAlignment(iwide, itall, x, y);
    _dualImage->setPos(x, y);

    int x1, y1;
    _dualImage->GetImage(1)->getPos(x1, y1);
    _dualImage->GetImage(1)->setPos(_gap, y1);
    _dualImage->doPaint(this);

    if (_image)
    {
        Color imgColor;
        getFgColor(imgColor);
        if (_useFgColorAsImageColor)
            _image->setColor(imgColor);

        _image->getSize(iwide, itall);
        calcAlignment(iwide, itall, x, y);
        _image->setPos(x, y);
        _image->doPaint(this);
    }

    setFgColor(oldFg[0], oldFg[1], oldFg[2], oldFg[3]);
}

void CLabelHeader::calcAlignment(int iwide, int itall, int& x, int& y)
{
    int wide, tall;
    getSize(wide, tall);
    x = y = 0;

    // Horizontal alignment
    switch (_contentAlignment)
    {
    case Label::a_northwest:
    case Label::a_west:
    case Label::a_southwest:
        x = 0;
        break;
    case Label::a_north:
    case Label::a_center:
    case Label::a_south:
        x = (wide - iwide) / 2;
        break;
    case Label::a_northeast:
    case Label::a_east:
    case Label::a_southeast:
        x = wide - iwide;
        break;
    default:
        x = 0;
    }

    // Vertical alignment
    switch (_contentAlignment)
    {
    case Label::a_northwest:
    case Label::a_north:
    case Label::a_northeast:
        y = 0;
        break;
    case Label::a_west:
    case Label::a_center:
    case Label::a_east:
        y = (tall - itall) / 2;
        break;
    case Label::a_southwest:
    case Label::a_south:
    case Label::a_southeast:
        y = tall - itall;
        break;
    default:
        y = 0;
    }

    if (x < 0) x = 0;
    x += _offset[0];
    y += _offset[1];
}

//-----------------------------------------------------------------------------
// ScorePanel implementation
//-----------------------------------------------------------------------------
ScorePanel::ScorePanel(int x, int y, int wide, int tall) : Panel(x, y, wide, tall)
{
    CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
    SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Scoreboard Title Text");
    SchemeHandle_t hSmallScheme = pSchemes->getSchemeHandle("Scoreboard Small Text");
    Font* tfont = pSchemes->getFont(hTitleScheme);
    Font* smallfont = pSchemes->getFont(hSmallScheme);

    setBgColor(0, 0, 0, 96);
    m_pCurrentHighlightLabel = NULL;
    m_iHighlightRow = -1;

    // Title label
    m_TitleLabel.setFont(tfont);
    m_TitleLabel.setText("");
    m_TitleLabel.setBgColor(0, 0, 0, 255);
    m_TitleLabel.setFgColor(Scheme::sc_primary1);
    m_TitleLabel.setContentAlignment(vgui::Label::a_west);

    LineBorder* border = new LineBorder(Color(60, 60, 60, 128));
    setBorder(border);
    setPaintBorderEnabled(true);

    int titleX = g_ColumnInfo[0].m_Width + 3;
    if (ScreenWidth >= 640)
        titleX = XRES(titleX);
    m_TitleLabel.setBounds(titleX, 4, wide, YRES(SBOARD_TITLE_SIZE_Y));
    m_TitleLabel.setContentFitted(false);
    m_TitleLabel.setParent(this);

    // Header grid setup
    m_HeaderGrid.SetDimensions(NUM_COLUMNS, 1);
    m_HeaderGrid.SetSpacing(0, 0);

    for (int i = 0; i < NUM_COLUMNS; ++i)
    {
        const SBColumnInfo& colInfo = g_ColumnInfo[i];
        if (colInfo.m_pTitle)
        {
            if (colInfo.m_pTitle[0] == '#')
                m_HeaderLabels[i].setText(CHudTextMessage::BufferedLocaliseTextString(colInfo.m_pTitle));
            else
                m_HeaderLabels[i].setText(colInfo.m_pTitle);
        }

        int colWidth = colInfo.m_Width;
        if (ScreenWidth >= 640)
            colWidth = XRES(colWidth);
        else if (ScreenWidth == 400) // Hack for 400x300
        {
            if (i == COLUMN_NAME)
                colWidth -= 28;
            else if (i == COLUMN_TRACKER)
                colWidth -= 8;
        }
        m_HeaderGrid.SetColumnWidth(i, colWidth);
        m_HeaderGrid.SetEntry(i, 0, &m_HeaderLabels[i]);

        m_HeaderLabels[i].setBgColor(0, 0, 0, 255);
        m_HeaderLabels[i].setFgColor(Scheme::sc_primary1);
        m_HeaderLabels[i].setFont(smallfont);
        m_HeaderLabels[i].setContentAlignment(colInfo.m_Alignment);

        int labelHeight = (ScreenHeight >= 480) ? YRES(12) : 12;
        m_HeaderLabels[i].setSize(50, labelHeight);
    }

    // Adjust last column to fill remaining space
    int ex, ey, ew, eh;
    m_HeaderGrid.GetEntryBox(NUM_COLUMNS - 2, 0, ex, ey, ew, eh);
    m_HeaderGrid.SetColumnWidth(NUM_COLUMNS - 1, (wide - XRES(X_BORDER)) - (ex + ew));

    m_HeaderGrid.AutoSetRowHeights();
    m_HeaderGrid.setBounds(XRES(X_BORDER), YRES(SBOARD_TITLE_SIZE_Y),
        wide - XRES(X_BORDER) * 2, m_HeaderGrid.GetRowHeight(0));
    m_HeaderGrid.setParent(this);
    m_HeaderGrid.setBgColor(0, 0, 0, 255);

    // Player list setup
    int headerX, headerY, headerWidth, headerHeight;
    m_HeaderGrid.getBounds(headerX, headerY, headerWidth, headerHeight);
    m_PlayerList.setBounds(headerX, headerY + headerHeight, headerWidth,
        tall - headerY - headerHeight - 6);
    m_PlayerList.setBgColor(0, 0, 0, 255);
    m_PlayerList.setParent(this);

    for (int row = 0; row < NUM_ROWS; ++row)
    {
        CGrid& gridRow = m_PlayerGrids[row];
        gridRow.SetDimensions(NUM_COLUMNS, 1);
        for (int col = 0; col < NUM_COLUMNS; ++col)
        {
            CLabelHeader& entry = m_PlayerEntries[col][row];
            entry.setContentFitted(false);
            entry.setRow(row);
            entry.addInputSignal(this);
            gridRow.SetEntry(col, 0, &entry);
        }
        gridRow.setBgColor(0, 0, 0, 255);
        gridRow.SetSpacing(0, 0);
        gridRow.CopyColumnWidths(&m_HeaderGrid);
        gridRow.AutoSetRowHeights();
        gridRow.setSize(PanelWidth(&gridRow), gridRow.CalcDrawHeight());
        gridRow.RepositionContents();
        m_PlayerList.AddItem(&gridRow);
    }

    // Hit test panel
    m_HitTestPanel.setBgColor(0, 0, 0, 255);
    m_HitTestPanel.setParent(this);
    m_HitTestPanel.setBounds(0, 0, wide, tall);
    m_HitTestPanel.addInputSignal(this);

    // Close button
    m_pCloseButton = new CommandButton("x", wide - XRES(12 + 4), YRES(2), XRES(12), YRES(12));
    m_pCloseButton->setParent(this);
    m_pCloseButton->addActionSignal(new CMenuHandler_StringCommandWatch("-showscores", true));
    m_pCloseButton->setBgColor(0, 0, 0, 255);
    m_pCloseButton->setFgColor(255, 255, 255, 0);
    m_pCloseButton->setFont(tfont);
    m_pCloseButton->setBoundKey((char)255);
    m_pCloseButton->setContentAlignment(Label::a_center);

    Initialize();
}

void ScorePanel::Initialize()
{
    m_iLastKilledBy = 0;
    m_fLastKillTime = 0;
    m_iPlayerNum = 0;
    m_iNumTeams = 0;
    memset(g_PlayerExtraInfo, 0, sizeof(g_PlayerExtraInfo));
    memset(g_TeamInfo, 0, sizeof(g_TeamInfo));
}

void ScorePanel::Update()
{
    // Set title from server name
    if (gViewPort->m_szServerName && gViewPort->m_szServerName[0])
    {
        char title[256];
        sprintf(title, "%s", gViewPort->m_szServerName);
        m_TitleLabel.setText(title);
    }

    m_iRows = 0;
    gViewPort->GetAllPlayersInfo();

    // Reset sorting arrays
    for (int i = 0; i < NUM_ROWS; ++i) m_iSortedRows[i] = 0;
    for (int i = 0; i < NUM_ROWS; ++i) m_iIsATeam[i] = TEAM_NO;
    for (int i = 0; i < MAX_PLAYERS_HUD; ++i) m_bHasBeenSorted[i] = false;

    if (!gHUD.m_Teamplay)
        SortPlayers(TEAM_NO, NULL);
    else
        SortTeams();

    m_PlayerList.SetScrollRange(m_iRows);
    FillGrid();

    m_pCloseButton->setVisible(gViewPort->m_pSpectatorPanel->m_menuVisible);
}

void ScorePanel::SortTeams()
{
    // Reset team scores if not overridden
    for (int i = 1; i <= m_iNumTeams; ++i)
    {
        if (!g_TeamInfo[i].scores_overriden)
            g_TeamInfo[i].frags = g_TeamInfo[i].deaths = 0;
        g_TeamInfo[i].ping = g_TeamInfo[i].packetloss = 0;
    }

    // Accumulate team stats
    for (int i = 1; i < MAX_PLAYERS_HUD; ++i)
    {
        if (!g_PlayerInfoList[i].name || g_PlayerExtraInfo[i].teamname[0] == 0)
            continue;

        // Find player's team
        int teamIdx = 0;
        for (int j = 1; j <= m_iNumTeams; ++j)
        {
            if (!stricmp(g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name))
            {
                teamIdx = j;
                break;
            }
        }
        if (teamIdx == 0)
            continue;

        if (!g_TeamInfo[teamIdx].scores_overriden)
        {
            g_TeamInfo[teamIdx].frags += g_PlayerExtraInfo[i].frags;
            g_TeamInfo[teamIdx].deaths += g_PlayerExtraInfo[i].deaths;
        }
        g_TeamInfo[teamIdx].ping += g_PlayerInfoList[i].ping;
        g_TeamInfo[teamIdx].packetloss += g_PlayerInfoList[i].packetloss;
        g_TeamInfo[teamIdx].ownteam = (g_PlayerInfoList[i].thisplayer != 0);
        g_TeamInfo[teamIdx].teamnumber = g_PlayerExtraInfo[i].teamnumber;
    }

    // Compute averages
    for (int i = 1; i <= m_iNumTeams; ++i)
    {
        g_TeamInfo[i].already_drawn = false;
        if (g_TeamInfo[i].players > 0)
        {
            g_TeamInfo[i].ping /= g_TeamInfo[i].players;
            g_TeamInfo[i].packetloss /= g_TeamInfo[i].players;
        }
    }

    // Sort teams by frags (desc) and deaths (asc)
    while (true)
    {
        int bestTeam = 0;
        int bestFrags = -99999;
        int bestDeaths = 99999;

        for (int i = 1; i <= m_iNumTeams; ++i)
        {
            if (g_TeamInfo[i].players < 1 || g_TeamInfo[i].already_drawn)
                continue;
            if (g_TeamInfo[i].frags > bestFrags ||
                (g_TeamInfo[i].frags == bestFrags && g_TeamInfo[i].deaths < bestDeaths))
            {
                bestTeam = i;
                bestFrags = g_TeamInfo[i].frags;
                bestDeaths = g_TeamInfo[i].deaths;
            }
        }
        if (bestTeam == 0)
            break;

        m_iSortedRows[m_iRows] = bestTeam;
        m_iIsATeam[m_iRows] = TEAM_YES;
        g_TeamInfo[bestTeam].already_drawn = true;
        ++m_iRows;

        // Sort players within this team
        SortPlayers(TEAM_NO, g_TeamInfo[bestTeam].name);
    }

    // Remaining players go to spectators
    SortPlayers(TEAM_SPECTATORS, NULL);
}

void ScorePanel::SortPlayers(int iTeam, char* team)
{
    bool teamHeaderAdded = (iTeam != TEAM_NO);

    while (true)
    {
        int bestPlayer = 0;
        int bestFrags = -99999;
        int bestDeaths = 99999;

        for (int i = 1; i < MAX_PLAYERS_HUD; ++i)
        {
            if (m_bHasBeenSorted[i] || !g_PlayerInfoList[i].name)
                continue;
            if (team && stricmp(g_PlayerExtraInfo[i].teamname, team) != 0)
                continue;

            const extra_player_info_t& pl = g_PlayerExtraInfo[i];
            if (pl.frags > bestFrags || (pl.frags == bestFrags && pl.deaths < bestDeaths))
            {
                bestPlayer = i;
                bestFrags = pl.frags;
                bestDeaths = pl.deaths;
            }
        }
        if (bestPlayer == 0)
            break;

        if (!teamHeaderAdded)
        {
            m_iIsATeam[m_iRows] = iTeam;
            ++m_iRows;
            teamHeaderAdded = true;
        }

        m_iSortedRows[m_iRows] = bestPlayer;
        m_bHasBeenSorted[bestPlayer] = true;
        ++m_iRows;
    }

    if (team)
        m_iIsATeam[m_iRows++] = TEAM_BLANK;
}

void ScorePanel::RebuildTeams()
{
    // Reset player counts
    for (int i = 1; i <= m_iNumTeams; ++i)
        g_TeamInfo[i].players = 0;

    gViewPort->GetAllPlayersInfo();
    m_iNumTeams = 0;

    for (int i = 1; i < MAX_PLAYERS_HUD; ++i)
    {
        if (!g_PlayerInfoList[i].name || g_PlayerExtraInfo[i].teamname[0] == 0)
            continue;

        // Find or create team
        int teamIdx = 0;
        for (int j = 1; j <= m_iNumTeams; ++j)
        {
            if (!stricmp(g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name))
            {
                teamIdx = j;
                break;
            }
        }
        if (teamIdx == 0)
        {
            for (int j = 1; j <= MAX_TEAMS; ++j)
            {
                if (g_TeamInfo[j].name[0] == '\0')
                {
                    teamIdx = j;
                    break;
                }
            }
            if (teamIdx == 0) continue; // No free slot
            if (teamIdx > m_iNumTeams) m_iNumTeams = teamIdx;
            strncpy(g_TeamInfo[teamIdx].name, g_PlayerExtraInfo[i].teamname, MAX_TEAM_NAME);
            g_TeamInfo[teamIdx].players = 0;
        }
        g_TeamInfo[teamIdx].players++;
    }

    // Clean empty teams
    for (int i = 1; i <= m_iNumTeams; ++i)
    {
        if (g_TeamInfo[i].players < 1)
            memset(&g_TeamInfo[i], 0, sizeof(team_info_t));
    }

    Update();
}

//-----------------------------------------------------------------------------
// FillGrid helper methods
//-----------------------------------------------------------------------------
void ScorePanel::UpdateHighlightPosition()
{
    int x, y;
    getApp()->getCursorPos(x, y);
    cursorMoved(x, y, this);
    if (!GetClientVoiceMgr()->IsInSquelchMode())
        m_iHighlightRow = -1;
}

void ScorePanel::HideRow(int row)
{
    for (int col = 0; col < NUM_COLUMNS; ++col)
        m_PlayerEntries[col][row].setVisible(false);
}

RowType ScorePanel::GetRowType(int row) const
{
    if (m_iIsATeam[row] == TEAM_BLANK)
        return ROWTYPE_BLANK;
    if (m_iIsATeam[row] == TEAM_YES)
        return ROWTYPE_TEAM;
    if (m_iIsATeam[row] == TEAM_SPECTATORS)
        return ROWTYPE_SPECTATOR;
    return ROWTYPE_PLAYER;
}

void ScorePanel::SetupRowAppearance(int row, CGrid* pGridRow, RowType type)
{
    pGridRow->SetRowUnderline(0, false, 0, 0, 0, 0, 0);
    int rowHeight = 13;
    if (ScreenHeight > 480)
        rowHeight = YRES(rowHeight);
    else
        rowHeight = 15; // Low-res tweak

    if (type == ROWTYPE_TEAM || type == ROWTYPE_SPECTATOR)
        rowHeight = YRES(20);

    for (int col = 0; col < NUM_COLUMNS; ++col)
        m_PlayerEntries[col][row].setSize(m_PlayerEntries[col][row].getWide(), rowHeight);
}

void ScorePanel::ConfigureTeamRow(int row, CGrid* pGridRow)
{
    int teamIdx = m_iSortedRows[row];
    team_info_t& teamInfo = g_TeamInfo[teamIdx];
    int colorIdx = teamInfo.teamnumber % iNumberOfTeamColors;
    int r = iTeamColors[colorIdx][0];
    int g = iTeamColors[colorIdx][1];
    int b = iTeamColors[colorIdx][2];

    pGridRow->SetRowUnderline(0, true, YRES(3), r, g, b, 0);
    for (int col = 0; col < NUM_COLUMNS; ++col)
        m_PlayerEntries[col][row].setFgColor(r, g, b, 0);
}

void ScorePanel::ConfigureSpectatorRow(int row, CGrid* pGridRow)
{
    pGridRow->SetRowUnderline(0, true, YRES(3), 100, 100, 100, 0);
    for (int col = 0; col < NUM_COLUMNS; ++col)
        m_PlayerEntries[col][row].setFgColor(100, 100, 100, 0);
}

void ScorePanel::ConfigurePlayerRow(int row, CGrid* pGridRow)
{
    int playerIdx = m_iSortedRows[row];
    extra_player_info_t& playerExtra = g_PlayerExtraInfo[playerIdx];
    hud_player_info_t& playerInfo = g_PlayerInfoList[playerIdx];
    int colorIdx = playerExtra.teamnumber % iNumberOfTeamColors;
    int r = iTeamColors[colorIdx][0];
    int g = iTeamColors[colorIdx][1];
    int b = iTeamColors[colorIdx][2];

    for (int col = 0; col < NUM_COLUMNS; ++col)
        m_PlayerEntries[col][row].setFgColor(r, g, b, 0);

    if (playerInfo.thisplayer)
    {
        // Highlight own player
        for (int col = 0; col < NUM_COLUMNS; ++col)
        {
            m_PlayerEntries[col][row].setFgColor(255, 255, 255, 0);
            m_PlayerEntries[col][row].setBgColor(r, g, b, 196);
        }
    }
    else if (playerIdx == m_iLastKilledBy && m_fLastKillTime > gHUD.m_flTime)
    {
        int alpha = 255 - (int)(15.0f * (m_fLastKillTime - gHUD.m_flTime));
        for (int col = 0; col < NUM_COLUMNS; ++col)
            m_PlayerEntries[col][row].setBgColor(255, 0, 0, alpha);
    }
}

void ScorePanel::ConfigureBlankRow(int, CGrid*)
{
    // Nothing needed, already hidden
}

void ScorePanel::SetupLabelAppearance(CLabelHeader* pLabel, int row, int col, RowType type)
{
    // Set alignment
    if (col == COLUMN_NAME || col == COLUMN_CLASS)
        pLabel->setContentAlignment(vgui::Label::a_west);
    else if (col == COLUMN_TRACKER)
        pLabel->setContentAlignment(vgui::Label::a_center);
    else
        pLabel->setContentAlignment(vgui::Label::a_east);

    // Font
    CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
    if (type == ROWTYPE_TEAM || type == ROWTYPE_SPECTATOR)
        pLabel->setFont(pSchemes->getFont(pSchemes->getSchemeHandle("Scoreboard Title Text")));
    else
        pLabel->setFont(pSchemes->getFont(pSchemes->getSchemeHandle("Scoreboard Text")));
}

void ScorePanel::GetCellText(int row, int col, RowType type, char* buffer, int bufsize) const
{
    buffer[0] = '\0';

    if (type == ROWTYPE_TEAM || type == ROWTYPE_SPECTATOR)
    {
        int teamIdx = m_iSortedRows[row];
        const team_info_t& team = g_TeamInfo[teamIdx];
        switch (col)
        {
        case COLUMN_NAME:
            if (type == ROWTYPE_SPECTATOR)
                strcpy(buffer, CHudTextMessage::BufferedLocaliseTextString("#Spectators"));
            else
                _snprintf(buffer, bufsize, "%s", gViewPort->GetTeamName(team.teamnumber));
            break;
        case COLUMN_KILLS:
            _snprintf(buffer, bufsize, "%d", team.frags);
            break;
        case COLUMN_DEATHS:
            _snprintf(buffer, bufsize, "%d", team.deaths);
            break;
        case COLUMN_LATENCY:
            _snprintf(buffer, bufsize, "%d", team.ping);
            break;
        default:
            buffer[0] = '\0';
        }
    }
    else if (type == ROWTYPE_PLAYER)
    {
        int playerIdx = m_iSortedRows[row];
        const hud_player_info_t& plInfo = g_PlayerInfoList[playerIdx];
        const extra_player_info_t& plExtra = g_PlayerExtraInfo[playerIdx];
        switch (col)
        {
        case COLUMN_NAME:
            _snprintf(buffer, bufsize, "%s  ", plInfo.name ? plInfo.name : "");
            break;
        case COLUMN_KILLS:
            _snprintf(buffer, bufsize, "%d", plExtra.frags);
            break;
        case COLUMN_DEATHS:
            _snprintf(buffer, bufsize, "%d", plExtra.deaths);
            break;
        case COLUMN_LATENCY:
            _snprintf(buffer, bufsize, "%d", plInfo.ping);
            break;
        case COLUMN_CLASS:
            if (gViewPort && EV_TFC_IsAllyTeam(g_iTeamNumber, plExtra.teamnumber) && g_iTeamNumber != 0)
            {
                if (plExtra.playerclass != 0)
                    strcpy(buffer, CHudTextMessage::BufferedLocaliseTextString(sLocalisedClasses[plExtra.playerclass]));
            }
            break;
        case COLUMN_VOICE:
            // Handled via image later
            buffer[0] = '\0';
            break;
        default:
            buffer[0] = '\0';
        }
    }
}

void ScorePanel::AdjustRowHeights()
{
    for (int row = 0; row < NUM_ROWS; ++row)
    {
        CGrid& gridRow = m_PlayerGrids[row];
        gridRow.AutoSetRowHeights();
        gridRow.setSize(PanelWidth(&gridRow), gridRow.CalcDrawHeight());
        gridRow.RepositionContents();
    }
    // Force listbox to recalc
    int w, h;
    m_PlayerList.getSize(w, h);
    m_PlayerList.setSize(w, h);
}

void ScorePanel::FillGrid()
{
    CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
    Font* smallfont = pSchemes->getFont(pSchemes->getSchemeHandle("Scoreboard Small Text"));

    UpdateHighlightPosition();

    for (int row = 0; row < NUM_ROWS; ++row)
    {
        if (row >= m_iRows)
        {
            HideRow(row);
            continue;
        }

        CGrid* pGridRow = &m_PlayerGrids[row];
        RowType type = GetRowType(row);
        SetupRowAppearance(row, pGridRow, type);

        // Apply team/spectator specific formatting
        if (type == ROWTYPE_TEAM)
            ConfigureTeamRow(row, pGridRow);
        else if (type == ROWTYPE_SPECTATOR)
            ConfigureSpectatorRow(row, pGridRow);
        else if (type == ROWTYPE_PLAYER)
            ConfigurePlayerRow(row, pGridRow);
        else // ROWTYPE_BLANK
        {
            ConfigureBlankRow(row, pGridRow);
            continue;
        }

        for (int col = 0; col < NUM_COLUMNS; ++col)
        {
            CLabelHeader* pLabel = &m_PlayerEntries[col][row];
            pLabel->setVisible(true);
            pLabel->setText2("");
            pLabel->setImage(NULL);
            pLabel->setTextOffset(0, 0);
            pLabel->setBgColor(0, 0, 0, 255);

            SetupLabelAppearance(pLabel, row, col, type);

            char textBuffer[128];
            GetCellText(row, col, type, textBuffer, sizeof(textBuffer));
            pLabel->setText(textBuffer);

            // Handle voice image separately
            if (col == COLUMN_VOICE && type == ROWTYPE_PLAYER)
                GetClientVoiceMgr()->UpdateSpeakerImage(pLabel, m_iSortedRows[row]);

            // Handle tracker image (commented out in original)
            // if (col == COLUMN_TRACKER && g_pTrackerUser) ...
        }
    }

    AdjustRowHeights();
}

void ScorePanel::DeathMsg(int killer, int victim)
{
    if (victim == m_iPlayerNum || killer == 0)
    {
        m_iLastKilledBy = (killer != 0) ? killer : m_iPlayerNum;
        m_fLastKillTime = gHUD.m_flTime + 10.0f;
        if (killer == m_iPlayerNum)
            m_iLastKilledBy = m_iPlayerNum;
    }
}

void ScorePanel::Open()
{
    RebuildTeams();
    setVisible(true);
    m_HitTestPanel.setVisible(true);
}

void ScorePanel::mousePressed(MouseCode code, Panel* panel)
{
    if (gHUD.m_iIntermission)
        return;

    if (!GetClientVoiceMgr()->IsInSquelchMode())
    {
        GetClientVoiceMgr()->StartSquelchMode();
        m_HitTestPanel.setVisible(false);
    }
    else if (m_iHighlightRow >= 0)
    {
        int playerIdx = m_iSortedRows[m_iHighlightRow];
        if (playerIdx > 0 && g_PlayerInfoList[playerIdx].name &&
            g_PlayerInfoList[playerIdx].name[0] != '\0')
        {
            const char* playerName = g_PlayerInfoList[playerIdx].name;
            char msg[256];
            if (GetClientVoiceMgr()->IsPlayerBlocked(playerIdx))
            {
                GetClientVoiceMgr()->SetPlayerBlockedState(playerIdx, false);
                _snprintf(msg, sizeof(msg), "%c** %s %s\n", HUD_PRINTTALK,
                    CHudTextMessage::BufferedLocaliseTextString("#Unmuted"),
                    playerName);
            }
            else
            {
                GetClientVoiceMgr()->SetPlayerBlockedState(playerIdx, true);
                _snprintf(msg, sizeof(msg), "%c** %s %s %s\n", HUD_PRINTTALK,
                    CHudTextMessage::BufferedLocaliseTextString("#Muted"),
                    playerName,
                    CHudTextMessage::BufferedLocaliseTextString("#No_longer_hear_that_player"));
            }
            gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, strlen(msg) + 1, msg);
        }
    }
}

void ScorePanel::cursorMoved(int x, int y, Panel* panel)
{
    if (!GetClientVoiceMgr()->IsInSquelchMode())
        return;

    for (int i = 0; i < NUM_ROWS; ++i)
    {
        int row, col;
        if (m_PlayerGrids[i].getCellAtPoint(x, y, row, col))
        {
            MouseOverCell(i, col);
            return;
        }
    }
}

void ScorePanel::MouseOverCell(int row, int col)
{
    CLabelHeader* label = &m_PlayerEntries[col][row];
    if (m_pCurrentHighlightLabel != label)
    {
        m_pCurrentHighlightLabel = NULL;
        m_iHighlightRow = -1;
    }
    if (!label) return;

    RowType type = GetRowType(row);
    if (type != ROWTYPE_PLAYER) return;

    int playerIdx = m_iSortedRows[row];
    if (!g_PlayerInfoList[playerIdx].name ||
        g_PlayerInfoList[playerIdx].name[0] == '\0')
        return;

    if (g_PlayerInfoList[playerIdx].thisplayer && !gEngfuncs.IsSpectateOnly())
        return;

    m_pCurrentHighlightLabel = label;
    m_iHighlightRow = row;
}
bool HACK_GetPlayerUniqueID(int iPlayer, char playerID[16])
{
    return 0 != gEngfuncs.GetPlayerUniqueID(iPlayer, playerID);
}