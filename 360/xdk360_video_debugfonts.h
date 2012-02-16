/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SSNES360_DEBUG_FONTS_H
#define _SSNES360_DEBUG_FONTS_H

#include "xdk360_video_resources.h"

typedef struct GLYPH_ATTR
{
    unsigned short tu1, tv1, tu2, tv2;    // Texture coordinates for the image
    short wOffset;              // Pixel offset for glyph start
    short wWidth;               // Pixel width of the glyph
    short wAdvance;             // Pixels to advance after the glyph
    unsigned short wMask;                 // Channel mask
} GLYPH_ATTR;

enum SavedStates
{
    SAVEDSTATE_D3DRS_ALPHABLENDENABLE,
    SAVEDSTATE_D3DRS_SRCBLEND,
    SAVEDSTATE_D3DRS_DESTBLEND,
    SAVEDSTATE_D3DRS_BLENDOP,
    SAVEDSTATE_D3DRS_ALPHATESTENABLE,
    SAVEDSTATE_D3DRS_ALPHAREF,
    SAVEDSTATE_D3DRS_ALPHAFUNC,
    SAVEDSTATE_D3DRS_FILLMODE,
    SAVEDSTATE_D3DRS_CULLMODE,
    SAVEDSTATE_D3DRS_ZENABLE,
    SAVEDSTATE_D3DRS_STENCILENABLE,
    SAVEDSTATE_D3DRS_VIEWPORTENABLE,
    SAVEDSTATE_D3DSAMP_MINFILTER,
    SAVEDSTATE_D3DSAMP_MAGFILTER,
    SAVEDSTATE_D3DSAMP_ADDRESSU,
    SAVEDSTATE_D3DSAMP_ADDRESSV,

    SAVEDSTATE_COUNT
};

typedef struct
{
	unsigned int m_bSaveState;
	unsigned long m_dwSavedState[ SAVEDSTATE_COUNT ];
    unsigned long m_dwNestedBeginCount;
	unsigned long m_cMaxGlyph;          // Number of entries in the translator table
	unsigned long m_dwNumGlyphs;        // Number of valid glyphs
    float m_fFontHeight;        // Height of the font strike in pixels
    float m_fFontTopPadding;    // Padding above the strike zone
    float m_fFontBottomPadding; // Padding below the strike zone
    float m_fFontYAdvance;      // Number of pixels to move the cursor for a line feed
    float m_fXScaleFactor;      // Scaling constants
    float m_fYScaleFactor;
	float m_fCursorX;           // Current text cursor
    float m_fCursorY;
    D3DRECT m_rcWindow;         // Bounds rect of the text window, modify via accessors only!
	wchar_t * m_TranslatorTable;		// ASCII to glyph lookup table
	D3DTexture* m_pFontTexture;
	const GLYPH_ATTR* m_Glyphs;			// Array of glyphs
} xdk360_video_font_t;

HRESULT xdk360_video_font_init(xdk360_video_font_t * font, const char * strFontFileName);
void xdk360_video_font_get_text_width(xdk360_video_font_t * font, const wchar_t * strText, float * pWidth, float * pHeight, int bFirstLineOnly);
void xdk360_video_font_deinit(xdk360_video_font_t * font);
void xdk360_video_font_set_cursor_position(xdk360_video_font_t *font, float fCursorX, float fCursorY );
void xdk360_video_font_begin (xdk360_video_font_t * font);
void xdk360_video_font_end (xdk360_video_font_t * font);
void xdk360_video_font_set_size(float x, float y);
void xdk360_video_font_draw_text(xdk360_video_font_t * font, float fOriginX, float fOriginY, unsigned long dwColor,
	const wchar_t * strText, float fMaxPixelWidth );

#endif