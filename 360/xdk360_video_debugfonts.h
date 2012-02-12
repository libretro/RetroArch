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

#ifndef _XDK360_DEBUG_FONTS_H
#define _XDK360_DEBUG_FONTS_H

#include "xdk360_video_resources.h"

typedef struct GLYPH_ATTR
{
    unsigned short tu1, tv1, tu2, tv2;    // Texture coordinates for the image
    short wOffset;              // Pixel offset for glyph start
    short wWidth;               // Pixel width of the glyph
    short wAdvance;             // Pixels to advance after the glyph
    unsigned short wMask;                 // Channel mask
} GLYPH_ATTR;

#define ATGFONT_LEFT       0x00000000
#define ATGFONT_RIGHT      0x00000001
#define ATGFONT_CENTER_X   0x00000002
#define ATGFONT_CENTER_Y   0x00000004
#define ATGFONT_TRUNCATED  0x00000008

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

class XdkFont
{
public:
    PackedResource m_xprResource;

    // Font vertical dimensions taken from the font file
    float m_fFontHeight;        // Height of the font strike in pixels
    float m_fFontTopPadding;    // Padding above the strike zone
    float m_fFontBottomPadding; // Padding below the strike zone
    float m_fFontYAdvance;      // Number of pixels to move the cursor for a line feed

    float m_fXScaleFactor;      // Scaling constants
    float m_fYScaleFactor;
    float m_fSlantFactor;       // For italics
    double m_dRotCos;           // Precalculated sine and cosine for italic like rotation
    double m_dRotSin;

    D3DRECT m_rcWindow;         // Bounds rect if the text window, modify via accessors only!
    float m_fCursorX;           // Current text cursor
    float m_fCursorY;

    // Translator table for supporting unicode ranges
    unsigned long m_cMaxGlyph;          // Number of entries in the translator table
    wchar_t * m_TranslatorTable;   // ASCII to glyph lookup table

    // Glyph data for the font
    unsigned long m_dwNumGlyphs;        // Number of valid glyphs
    const GLYPH_ATTR* m_Glyphs; // Array of glyphs

    // D3D rendering objects
    D3DTexture* m_pFontTexture;

    // Saved state for rendering (if not using a pure device)
    unsigned long m_dwSavedState[ SAVEDSTATE_COUNT ];
    unsigned long m_dwNestedBeginCount;
    int m_bSaveState;

    int m_bRotate;
public:
    XdkFont();
    ~XdkFont();

    // Functions to create and destroy the internal objects
    HRESULT Create( const char * strFontFileName );
    void    Destroy();

    // Returns the dimensions of a text string
    void    GetTextExtent( const wchar_t * strText, float * pWidth,
                           float * pHeight, int bFirstLineOnly=FALSE ) const;
    float   GetTextWidth( const wchar_t * strText ) const;

    void    SetWindow(const D3DRECT &rcWindow );
    void    SetWindow( long x1, long y1, long x2, long y2 );
    void    GetWindow(D3DRECT &rcWindow) const;
    void    SetCursorPosition( float fCursorX, float fCursorY );

    // Public calls to render text. Callers can simply call DrawText(), but for
    // performance, they should batch multiple calls together, bracketed by calls to
    // Begin() and End().
    void    Begin();
    void    DrawText( unsigned long dwColor, const wchar_t * strText, unsigned long dwFlags=0L,
                      float fMaxPixelWidth = 0.0f );
    void    DrawText( float sx, float sy, unsigned long dwColor, const wchar_t * strText,
                      unsigned long dwFlags=0L, float fMaxPixelWidth = 0.0f );
    void    End();

private:
    // Internal helper functions
    HRESULT CreateFontShaders();
    void ReleaseFontShaders();
    void RotatePoint( float * X, float * Y, double OriginX, double OriginY ) const;
};

#endif