/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RARCH_360_FONTS_H
#define RARCH_360_FONTS_H

#include "../../xdk/xdk_resources.h"

#define PAGE_UP               (255)
#define PAGE_DOWN             (-255)

#define SCREEN_SIZE_X_DEFAULT 640
#define SCREEN_SIZE_Y_DEFAULT 480

#define SAFE_AREA_PCT_4x3     85
#define SAFE_AREA_PCT_HDTV    90

typedef struct
{
   float m_fLineHeight;                 // height of a single line in pixels
   unsigned int m_nScrollOffset;        // offset to display text (in lines)
   unsigned int m_cxSafeArea;
   unsigned int m_cySafeArea;
   unsigned int m_cxSafeAreaOffset;
   unsigned int m_cySafeAreaOffset;
   unsigned int m_nCurLine;             // index of current line being written to
   unsigned int m_cCurLineLength;       // length of the current line
   unsigned int m_cScreenHeight;        // height in lines of screen area
   unsigned int m_cScreenHeightVirtual; // height in lines of text storage buffer
   unsigned int m_cScreenWidth;         // width in characters
   wchar_t * m_Buffer;			// buffer big enough to hold a full screen
   wchar_t ** m_Lines;			// pointers to individual lines
} video_console_t;

typedef struct GLYPH_ATTR
{
   unsigned short tu1, tv1, tu2, tv2;   // Texture coordinates for the image
   short wOffset;                       // Pixel offset for glyph start
   short wWidth;                        // Pixel width of the glyph
   short wAdvance;                      // Pixels to advance after the glyph
   unsigned short wMask;                // Channel mask
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
   unsigned long m_cMaxGlyph;           // Number of entries in the translator table
   unsigned long m_dwNumGlyphs;         // Number of valid glyphs
   float m_fFontHeight;                 // Height of the font strike in pixels
   float m_fFontTopPadding;             // Padding above the strike zone
   float m_fFontBottomPadding;          // Padding below the strike zone
   float m_fFontYAdvance;               // Number of pixels to move the cursor for a line feed
   float m_fXScaleFactor;               // Scaling constants
   float m_fYScaleFactor;
   float m_fCursorX;                    // Current text cursor
   float m_fCursorY;
   D3DRECT m_rcWindow;                  // Bounds rect of the text window, modify via accessors only!
   wchar_t * m_TranslatorTable;         // ASCII to glyph lookup table
   D3DTexture* m_pFontTexture;
   const GLYPH_ATTR* m_Glyphs;          // Array of glyphs
} xdk360_video_font_t;

HRESULT d3d9_init_font(const char *font_path);
void d3d9_deinit_font(void);
void xdk360_console_format(const char *strFormat);
void xdk360_console_draw(void);

void d3d9_render_msg_pre(xdk360_video_font_t * font);
void d3d9_render_msg_post(xdk360_video_font_t * font);

#endif
