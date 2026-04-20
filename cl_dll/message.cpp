/***
 *
 *   Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *   This product contains software technology licensed from Id
 *   Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *   All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/
 //
 // Message.cpp
 //
 // implementation of CHudMessage class
 //

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include <locale.h>

// stb_truetype implementation
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

DECLARE_MESSAGE(m_Message, HudText)
DECLARE_MESSAGE(m_Message, GameTitle)

// 1 Global client_textmessage_t for custom messages that aren't in the titles.txt
client_textmessage_t g_pCustomMessage;
const char* g_pCustomName = "Custom";
char g_pCustomText[1024];

// stb_truetype font management
static stbtt_fontinfo g_fontInfo;
static unsigned char* g_fontData = NULL;
static float g_fontScale = 1.0f;
static int g_fontSize = 18;		 // default pixel size (can be changed via cvar)
static int g_fontAscender = 0;	 // ascender in pixels
static int g_fontDescender = 0;	 // descender in pixels (negative)
static int g_fontLineHeight = 0; // total line height in pixels
static unsigned char* g_fontTexture = NULL;
static int g_textureWidth = 512;
static int g_textureHeight = 512;
static int g_nextCharX = 4;
static int g_nextCharY = 4;
static int g_maxCharHeight = 0;
static bool g_fontInitialized = false;  // renamed from g_ftInitialized for clarity

// Character cache structure
struct FontChar
{
	wchar_t codepoint;
	short texX, texY;
	short width, height;
	short bearingX, bearingY; // bearing relative to baseline
	short advanceX;			  // advance in pixels
	bool valid;
};

#define MAX_CACHED_CHARS 1024
static FontChar g_charCache[MAX_CACHED_CHARS];
static int g_cachedCharCount = 0;

// Console variable for font size
static cvar_t* g_hud_fontsize = NULL;

// UTF-8 Decoder
static int UTF8CharToWChar(const char* utf8, wchar_t& wc)
{
	if (!utf8 || !*utf8)
		return 0;

	unsigned char c = (unsigned char)*utf8;

	// ASCII 7-bit
	if (c < 0x80)
	{
		wc = c;
		return 1;
	}
	// 2-byte UTF-8
	else if ((c & 0xE0) == 0xC0 && (utf8[1] & 0xC0) == 0x80)
	{
		wc = ((c & 0x1F) << 6) | (utf8[1] & 0x3F);
		return 2;
	}
	// 3-byte UTF-8 (most Chinese characters)
	else if ((c & 0xF0) == 0xE0 && (utf8[1] & 0xC0) == 0x80 && (utf8[2] & 0xC0) == 0x80)
	{
		wc = ((c & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
		return 3;
	}
	// 4-byte UTF-8 (rare)
	else if ((c & 0xF8) == 0xF0 && (utf8[1] & 0xC0) == 0x80 &&
		(utf8[2] & 0xC0) == 0x80 && (utf8[3] & 0xC0) == 0x80)
	{
		wc = ((c & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) |
			((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
		return 4;
	}

	// Invalid encoding
	wc = L'?';
	return 1;
}

// Convert UTF-8 to wide string
static wchar_t* UTF8ToWide(const char* utf8)
{
	if (!utf8)
		return NULL;

	int len = 0;
	const char* p = utf8;
	wchar_t dummy;
	while (*p)
	{
		p += UTF8CharToWChar(p, dummy);
		len++;
	}

	wchar_t* wide = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if (!wide)
		return NULL;

	p = utf8;
	for (int i = 0; i < len; i++)
	{
		p += UTF8CharToWChar(p, wide[i]);
	}
	wide[len] = 0;

	return wide;
}

// Get UTF-8 string length (in characters, not bytes)
static int UTF8StrLen(const char* utf8)
{
	if (!utf8)
		return 0;

	int len = 0;
	const char* p = utf8;
	wchar_t dummy;
	while (*p)
	{
		p += UTF8CharToWChar(p, dummy);
		len++;
	}
	return len;
}

// Initialize stb_truetype font
static bool InitFont()
{
	if (g_fontInitialized)
		return true;

	// Chinese font paths
	const char* fontPaths[] = {
		// Windows Chinese fonts
		"C:/Windows/Fonts/simhei.ttf",
		"C:/Windows/Fonts/simsun.ttc",
		"C:/Windows/Fonts/msyh.ttf",
		"C:/Windows/Fonts/SimHei.ttf",
		"C:/Windows/Fonts/SimSun.ttf",
		"C:/Windows/Fonts/msyhbd.ttf",
		"C:/Windows/Fonts/kaiti.ttf",
		"C:/Windows/Fonts/fangsong.ttf",

		// Linux Chinese fonts
		"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
		"/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
		"/usr/share/fonts/truetype/arphic/uming.ttc",
		"/usr/share/fonts/truetype/arphic/ukai.ttc",

		// macOS Chinese fonts
		"/System/Library/Fonts/PingFang.ttc",
		"/System/Library/Fonts/STHeiti Light.ttf",
		"/System/Library/Fonts/STSong.ttf",

		// Fallback English fonts
		"C:/Windows/Fonts/arial.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf" };

	bool fontLoaded = false;
	for (int i = 0; i < sizeof(fontPaths) / sizeof(fontPaths[0]); i++)
	{
		FILE* f = fopen(fontPaths[i], "rb");
		if (!f)
			continue;

		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);

		unsigned char* buffer = (unsigned char*)malloc(size);
		if (!buffer)
		{
			fclose(f);
			continue;
		}

		if (fread(buffer, 1, size, f) != (size_t)size)
		{
			free(buffer);
			fclose(f);
			continue;
		}
		fclose(f);

		if (stbtt_InitFont(&g_fontInfo, buffer, 0) == 0)
		{
			free(buffer);
			continue;
		}

		// Success
		g_fontData = buffer;
		fontLoaded = true;

		char msg[256];
		safe_sprintf(msg, sizeof(msg), "Loaded font: %s\n", fontPaths[i]);
		gEngfuncs.pfnConsolePrint(msg);
		break;
	}

	if (!fontLoaded)
	{
		gEngfuncs.pfnConsolePrint("Failed to load any Chinese font\n");
		return false;
	}

	// Read font size from cvar (if available)
	if (g_hud_fontsize)
	{
		int size = (int)g_hud_fontsize->value;
		if (size < 6)
			size = 6;
		if (size > 72)
			size = 72;
		g_fontSize = size;
	}
	else
	{
		g_fontSize = 18; // fallback default
	}

	// Compute scale for pixel size
	g_fontScale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)g_fontSize);

	// Get font metrics
	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
	g_fontAscender = (int)(ascent * g_fontScale + 0.5f);
	g_fontDescender = (int)(descent * g_fontScale + 0.5f); // descent is negative
	g_fontLineHeight = (int)((ascent - descent + lineGap) * g_fontScale + 0.5f);

	// Allocate texture memory
	g_fontTexture = (unsigned char*)malloc(g_textureWidth * g_textureHeight * 4);
	if (!g_fontTexture)
	{
		free(g_fontData);
		g_fontData = NULL;
		return false;
	}

	memset(g_fontTexture, 0, g_textureWidth * g_textureHeight * 4);
	g_fontInitialized = true;

	gEngfuncs.pfnConsolePrint("stb_truetype initialized successfully\n");
	return true;
}

// Shutdown font
static void ShutdownFont()
{
	if (g_fontTexture)
	{
		free(g_fontTexture);
		g_fontTexture = NULL;
	}

	if (g_fontData)
	{
		free(g_fontData);
		g_fontData = NULL;
	}

	g_cachedCharCount = 0;
	g_nextCharX = 4;
	g_nextCharY = 4;
	g_maxCharHeight = 0;
	g_fontInitialized = false;
}

// Cache a character in the texture
static FontChar* CacheCharacter(wchar_t c)
{
	// Check if already cached
	for (int i = 0; i < g_cachedCharCount; i++)
	{
		if (g_charCache[i].codepoint == c && g_charCache[i].valid)
			return &g_charCache[i];
	}

	if (g_cachedCharCount >= MAX_CACHED_CHARS)
		return NULL;

	// Find glyph index
	int glyph = stbtt_FindGlyphIndex(&g_fontInfo, c);
	if (glyph == 0)
	{
		// Character not found -> skip
		return NULL;
	}

	// Get glyph metrics
	int advanceWidth, leftSideBearing;
	stbtt_GetGlyphHMetrics(&g_fontInfo, glyph, &advanceWidth, &leftSideBearing);

	// Get glyph bitmap box (in pixels)
	int x0, y0, x1, y1;
	stbtt_GetGlyphBitmapBox(&g_fontInfo, glyph, g_fontScale, g_fontScale, &x0, &y0, &x1, &y1);

	int width = x1 - x0;
	int height = y1 - y0;

	// Handle invisible characters (space, etc.)
	if (width <= 0 || height <= 0)
	{
		FontChar* fc = &g_charCache[g_cachedCharCount++];
		fc->codepoint = c;
		fc->texX = fc->texY = 0;
		fc->width = 0;
		fc->height = 0;
		fc->bearingX = (int)(leftSideBearing * g_fontScale + 0.5f);
		fc->bearingY = 0;
		fc->advanceX = (int)(advanceWidth * g_fontScale + 0.5f);
		fc->valid = true;
		return fc;
	}

	// Check texture space
	if (g_nextCharX + width + 4 >= g_textureWidth)
	{
		g_nextCharX = 4;
		g_nextCharY += g_maxCharHeight + 4;
		g_maxCharHeight = 0;
	}

	if (g_nextCharY + height + 4 >= g_textureHeight)
	{
		// Texture full, reset
		g_nextCharX = 4;
		g_nextCharY = 4;
		g_maxCharHeight = 0;
		memset(g_fontTexture, 0, g_textureWidth * g_textureHeight * 4);
	}

	// Render glyph to a temporary buffer
	unsigned char* bitmap = (unsigned char*)malloc(width * height);
	if (!bitmap)
		return NULL;

	stbtt_MakeGlyphBitmap(&g_fontInfo, bitmap, width, height, width, g_fontScale, g_fontScale, glyph);

	// Copy bitmap to texture
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int texX = g_nextCharX + x;
			int texY = g_nextCharY + y;
			unsigned char alpha = bitmap[y * width + x];

			int offset = (texY * g_textureWidth + texX) * 4;
			g_fontTexture[offset + 0] = 255;   // R
			g_fontTexture[offset + 1] = 255;   // G
			g_fontTexture[offset + 2] = 255;   // B
			g_fontTexture[offset + 3] = alpha; // A
		}
	}

	free(bitmap);

	// Fill cache entry
	FontChar* fc = &g_charCache[g_cachedCharCount++];
	fc->codepoint = c;
	fc->texX = g_nextCharX;
	fc->texY = g_nextCharY;
	fc->width = width;
	fc->height = height;
	fc->bearingX = x0;      // left bearing (may be negative)
	fc->bearingY = -y0;      // convert to positive (distance from baseline to top)
	fc->advanceX = (int)(advanceWidth * g_fontScale + 0.5f);
	fc->valid = true;

	g_nextCharX += width + 4;
	if (height > g_maxCharHeight)
		g_maxCharHeight = height;

	return fc;
}

// Get character width (all characters use stb_truetype)
static int GetCharWidthInternal(wchar_t c)
{
	if (!g_fontInitialized && !InitFont())
	{
		// Fallback: approximate width
		return g_fontSize / 2;
	}

	// Check cache first
	for (int i = 0; i < g_cachedCharCount; i++)
	{
		if (g_charCache[i].codepoint == c && g_charCache[i].valid)
			return g_charCache[i].advanceX;
	}

	// Not cached, compute advance without rendering bitmap
	int glyph = stbtt_FindGlyphIndex(&g_fontInfo, c);
	if (glyph == 0)
		return g_fontSize / 2; // fallback

	int advanceWidth, leftSideBearing;
	stbtt_GetGlyphHMetrics(&g_fontInfo, glyph, &advanceWidth, &leftSideBearing);
	return (int)(advanceWidth * g_fontScale + 0.5f);
}

// Get UTF-8 string width
static int GetUTF8StringWidthInternal(const char* str)
{
	if (!str)
		return 0;

	int width = 0;
	const char* p = str;
	wchar_t ch;

	while (*p)
	{
		p += UTF8CharToWChar(p, ch);
		width += GetCharWidthInternal(ch);
	}
	return width;
}

// Draw character using stb_truetype (all characters)
static void DrawCharInternal(int x, int y, wchar_t c, int r, int g, int b)
{
	// y is the top of the line (as used in original HUD)
	if (!g_fontInitialized && !InitFont())
	{
		return;
	}

	FontChar* fc = CacheCharacter(c);
	if (!fc || !fc->valid)
		return; // missing character: skip drawing

	// Calculate baseline: line top + ascender
	int baselineY = y + g_fontAscender;

	// Position where the glyph's origin should be placed
	int penX = x + fc->bearingX;
	int penY = baselineY - fc->bearingY; // bearingY is offset from baseline to top

	// Draw the glyph pixel by pixel (software rendering)
	if (fc->width > 0 && fc->height > 0 && g_fontTexture)
	{
		for (int fy = 0; fy < fc->height; fy++)
		{
			for (int fx = 0; fx < fc->width; fx++)
			{
				int texX = fc->texX + fx;
				int texY = fc->texY + fy;
				int offset = (texY * g_textureWidth + texX) * 4;
				unsigned char alpha = g_fontTexture[offset + 3];

				if (alpha > 0)
				{
					// Simple alpha blending with text color
					int a = (alpha * (r + g + b) / (3 * 255)); // approximate brightness
					FillRGBA(penX + fx, penY + fy, 1, 1, r, g, b, a);
				}
			}
		}
	}
}

bool CHudMessage::Init()
{
	HOOK_MESSAGE(HudText);
	HOOK_MESSAGE(GameTitle);

	// Register font size cvar
	g_hud_fontsize = CVAR_CREATE("hud_fontsize", "18", FCVAR_ARCHIVE);

	gHUD.AddHudElem(this);

	Reset();
	InitFont();

	return true;
}

bool CHudMessage::VidInit()
{
	m_HUD_title_half = gHUD.GetSpriteIndex("title_half");
	m_HUD_title_life = gHUD.GetSpriteIndex("title_life");

	ShutdownFont();
	InitFont(); // reinitialize with current cvar value

	return true;
}

void CHudMessage::Reset()
{
	memset(m_pMessages, 0, sizeof(m_pMessages[0]) * maxHUDMessages);
	memset(m_startTime, 0, sizeof(m_startTime[0]) * maxHUDMessages);

	m_bEndAfterMessage = false;
	m_gameTitleTime = 0;
	m_pGameTitle = NULL;

	g_cachedCharCount = 0;
	g_nextCharX = 4;
	g_nextCharY = 4;
	g_maxCharHeight = 0;
	if (g_fontTexture)
		memset(g_fontTexture, 0, g_textureWidth * g_textureHeight * 4);
}

float CHudMessage::FadeBlend(float fadein, float fadeout, float hold, float localTime)
{
	float fadeTime = fadein + hold;
	float fadeBlend;

	if (localTime < 0)
		return 0;

	if (localTime < fadein)
	{
		fadeBlend = 1 - ((fadein - localTime) / fadein);
	}
	else if (localTime > fadeTime)
	{
		if (fadeout > 0)
			fadeBlend = 1 - ((localTime - fadeTime) / fadeout);
		else
			fadeBlend = 0;
	}
	else
		fadeBlend = 1;

	return fadeBlend;
}

int CHudMessage::XPosition(float x, int width, int totalWidth)
{
	int xPos;

	if (x == -1)
	{
		xPos = (ScreenWidth - width) / 2;
	}
	else
	{
		if (x < 0)
			xPos = (1.0 + x) * ScreenWidth - totalWidth;
		else
			xPos = x * ScreenWidth;
	}

	if (xPos + width > ScreenWidth)
		xPos = ScreenWidth - width;
	else if (xPos < 0)
		xPos = 0;

	return xPos;
}

int CHudMessage::YPosition(float y, int height)
{
	int yPos;

	if (y == -1)
		yPos = (ScreenHeight - height) * 0.5;
	else
	{
		if (y < 0)
			yPos = (1.0 + y) * ScreenHeight - height;
		else
			yPos = y * ScreenHeight;
	}

	if (yPos + height > ScreenHeight)
		yPos = ScreenHeight - height;
	else if (yPos < 0)
		yPos = 0;

	return yPos;
}

void CHudMessage::MessageScanNextChar()
{
	int srcRed, srcGreen, srcBlue, destRed, destGreen, destBlue;
	int blend;

	srcRed = m_parms.pMessage->r1;
	srcGreen = m_parms.pMessage->g1;
	srcBlue = m_parms.pMessage->b1;
	blend = 0;
	destRed = destGreen = destBlue = 0;

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
				blend = 0;
			else
			{
				destRed = m_parms.pMessage->r2;
				destGreen = m_parms.pMessage->g2;
				destBlue = m_parms.pMessage->b2;
				blend = (int)(255 - (deltaTime * (1.0 / m_parms.pMessage->fxtime) * 255.0 + 0.5));
			}
		}
		break;
	}

	if (blend > 255)
		blend = 255;
	else if (blend < 0)
		blend = 0;

	m_parms.r = ((srcRed * (255 - blend)) + (destRed * blend)) >> 8;
	m_parms.g = ((srcGreen * (255 - blend)) + (destGreen * blend)) >> 8;
	m_parms.b = ((srcBlue * (255 - blend)) + (destBlue * blend)) >> 8;

	// Draw the character
	if (m_parms.x >= 0 && m_parms.y >= 0)
	{
		DrawCharInternal(m_parms.x, m_parms.y, m_parms.text,
			m_parms.r, m_parms.g, m_parms.b);
	}
}

void CHudMessage::MessageScanStart()
{
	switch (m_parms.pMessage->effect)
	{
	case 1:
	case 0:
		m_parms.fadeTime = m_parms.pMessage->fadein + m_parms.pMessage->holdtime;

		if (m_parms.time < m_parms.pMessage->fadein)
		{
			m_parms.fadeBlend = (int)(((m_parms.pMessage->fadein - m_parms.time) * (1.0 / m_parms.pMessage->fadein) * 255));
		}
		else if (m_parms.time > m_parms.fadeTime)
		{
			if (m_parms.pMessage->fadeout > 0)
				m_parms.fadeBlend = (int)((((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255));
			else
				m_parms.fadeBlend = 255;
		}
		else
			m_parms.fadeBlend = 0;

		m_parms.charTime = 0;

		if (m_parms.pMessage->effect == 1 && (rand() % 100) < 10)
			m_parms.charTime = 1;
		break;

	case 2:
		m_parms.fadeTime = (m_parms.pMessage->fadein * m_parms.length) + m_parms.pMessage->holdtime;

		if (m_parms.time > m_parms.fadeTime && m_parms.pMessage->fadeout > 0)
			m_parms.fadeBlend = (int)((((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255));
		else
			m_parms.fadeBlend = 0;
		break;
	}
}

void CHudMessage::MessageDrawScan(client_textmessage_t* pMessage, float time)
{
	int i, j, length;
	const char* pText;
	wchar_t line[256];

	pText = pMessage->pMessage;

	m_parms.lines = 1;
	m_parms.time = time;
	m_parms.pMessage = pMessage;
	length = 0;
	m_parms.totalWidth = 0;

	const char* pLineStart = pText;
	int lineWidth = 0;

	while ('\0' != *pText)
	{
		if (*pText == '\n')
		{
			m_parms.lines++;
			if (lineWidth > m_parms.totalWidth)
				m_parms.totalWidth = lineWidth;
			lineWidth = 0;
			pText++;
			pLineStart = pText;
		}
		else
		{
			wchar_t ch;
			pText += UTF8CharToWChar(pText, ch);
			lineWidth += GetCharWidthInternal(ch);
			length++;
		}
	}
	if (lineWidth > m_parms.totalWidth)
		m_parms.totalWidth = lineWidth;

	m_parms.length = length;
	// Use actual font line height
	m_parms.totalHeight = m_parms.lines * g_fontLineHeight;

	m_parms.y = YPosition(pMessage->y, m_parms.totalHeight);
	pText = pMessage->pMessage;

	m_parms.charTime = 0;

	MessageScanStart();

	for (i = 0; i < m_parms.lines; i++)
	{
		m_parms.lineLength = 0;
		m_parms.width = 0;

		while ('\0' != *pText && *pText != '\n' && m_parms.lineLength < 255)
		{
			wchar_t ch;
			pText += UTF8CharToWChar(pText, ch);
			line[m_parms.lineLength] = ch;
			m_parms.width += GetCharWidthInternal(ch);
			m_parms.lineLength++;
		}

		if (*pText == '\n')
			pText++;
		line[m_parms.lineLength] = 0;

		m_parms.x = XPosition(pMessage->x, m_parms.width, m_parms.totalWidth);

		for (j = 0; j < m_parms.lineLength; j++)
		{
			m_parms.text = line[j];
			int charWidth = GetCharWidthInternal(m_parms.text);
			int next = m_parms.x + charWidth;
			MessageScanNextChar();
			m_parms.x = next;
		}

		// Move to next line using font line height
		m_parms.y += g_fontLineHeight;
	}
}

bool CHudMessage::Draw(float fTime)
{
	int i, drawn;
	client_textmessage_t* pMessage;
	float endTime;

	drawn = 0;

	if (m_gameTitleTime > 0)
	{
		float localTime = gHUD.m_flTime - m_gameTitleTime;
		float brightness;

		if (m_gameTitleTime > gHUD.m_flTime)
			m_gameTitleTime = gHUD.m_flTime;

		if (localTime > (m_pGameTitle->fadein + m_pGameTitle->holdtime + m_pGameTitle->fadeout))
		{
			m_gameTitleTime = 0;
		}
		else
		{
			brightness = FadeBlend(m_pGameTitle->fadein, m_pGameTitle->fadeout, m_pGameTitle->holdtime, localTime);

			int halfWidth = gHUD.GetSpriteRect(m_HUD_title_half).right - gHUD.GetSpriteRect(m_HUD_title_half).left;
			int fullWidth = halfWidth + gHUD.GetSpriteRect(m_HUD_title_life).right - gHUD.GetSpriteRect(m_HUD_title_life).left;
			int fullHeight = gHUD.GetSpriteRect(m_HUD_title_half).bottom - gHUD.GetSpriteRect(m_HUD_title_half).top;

			int x = XPosition(m_pGameTitle->x, fullWidth, fullWidth);
			int y = YPosition(m_pGameTitle->y, fullHeight);

			SPR_Set(gHUD.GetSprite(m_HUD_title_half), (int)(brightness * m_pGameTitle->r1),
				(int)(brightness * m_pGameTitle->g1), (int)(brightness * m_pGameTitle->b1));
			SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_title_half));

			SPR_Set(gHUD.GetSprite(m_HUD_title_life), (int)(brightness * m_pGameTitle->r1),
				(int)(brightness * m_pGameTitle->g1), (int)(brightness * m_pGameTitle->b1));
			SPR_DrawAdditive(0, x + halfWidth, y, &gHUD.GetSpriteRect(m_HUD_title_life));

			drawn = 1;
		}
	}

	for (i = 0; i < maxHUDMessages; i++)
	{
		if (m_pMessages[i])
		{
			pMessage = m_pMessages[i];
			if (m_startTime[i] > gHUD.m_flTime)
			{
				m_startTime[i] = gHUD.m_flTime + m_parms.time - m_startTime[i] + 0.2f;
			}
		}
	}

	for (i = 0; i < maxHUDMessages; i++)
	{
		if (m_pMessages[i])
		{
			pMessage = m_pMessages[i];

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
				m_pMessages[i] = NULL;

				if (m_bEndAfterMessage)
				{
					gEngfuncs.pfnClientCmd("wait\nwait\nwait\nwait\nwait\nwait\nwait\ndisconnect\n");
				}
			}
		}
	}

	m_parms.time = gHUD.m_flTime;

	if (0 == drawn)
		m_iFlags &= ~HUD_ACTIVE;

	return true;
}

void CHudMessage::MessageAdd(const char* pName, float time)
{
	int i, j;
	client_textmessage_t* tempMessage;

	for (i = 0; i < maxHUDMessages; i++)
	{
		if (!m_pMessages[i])
		{
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

			for (j = 0; j < maxHUDMessages; j++)
			{
				if (m_pMessages[j])
				{
					if (0 == strcmp(tempMessage->pMessage, m_pMessages[j]->pMessage))
					{
						return;
					}

					if (fabs(tempMessage->y - m_pMessages[j]->y) < 0.0001f)
					{
						if (fabs(tempMessage->x - m_pMessages[j]->x) < 0.0001f)
						{
							m_pMessages[j] = NULL;
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
	if (m_pGameTitle != NULL)
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