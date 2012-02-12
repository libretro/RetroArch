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

#pragma once
#ifndef SSNES360_CONSOLE_H
#define SSNES360_CONSOLE_H

#include <xtl.h>
#include "xdk360_video_debugfonts.h"

#define PAGE_UP		(255)
#define PAGE_DOWN	(-255)

#define SCREEN_SIZE_X_DEFAULT 640
#define SCREEN_SIZE_Y_DEFAULT 480

#define SAFE_AREA_PCT_4x3 85
#define SAFE_AREA_PCT_HDTV 90

#define INCREMENT_LINE() \
	m_nCurLine = ( m_nCurLine + 1 ) % m_cScreenHeightVirtual; \
    m_cCurLineLength = 0; \
    memset( m_Lines[m_nCurLine], 0, ( m_cScreenWidth + 1 ) * sizeof( wchar_t ) );

#define CLEAR_SCREEN() \
	m_nCurLine = 0; \
    m_cCurLineLength = 0; \
    memset( m_Buffer, 0, m_cScreenHeightVirtual * ( m_cScreenWidth + 1 ) * sizeof( wchar_t ) ); \
    Render();

//--------------------------------------------------------------------------------------
// Name: class Console
// Desc: Class to implement the console.
//--------------------------------------------------------------------------------------
class Console
{
public:
	Console();
	~Console();

    // Initialization
    HRESULT         Create( LPCSTR strFontFileName,
                            D3DCOLOR colBackColor,
                            D3DCOLOR colTextColor,
                            unsigned int nLines = 0 );

    void            Destroy();

    // Console output
    void    Format(int clear_screen, _In_z_ _Printf_format_string_ LPCSTR strFormat, ... );
    void    Format(int clear_screen, _In_z_ _Printf_format_string_ LPCWSTR wstrFormat, ... );
    void    FormatV( _In_z_ _Printf_format_string_ LPCSTR strFormat, va_list pArgList );
    void    FormatV( _In_z_ _Printf_format_string_ LPCWSTR wstrFormat, va_list pArgList );

    // method for rendering the console
    void            Render();
private:
	int			first_message;
    // Safe area dimensions
    unsigned int m_cxSafeArea;
    unsigned int m_cySafeArea;

    unsigned int m_cxSafeAreaOffset;
    unsigned int m_cySafeAreaOffset;

    // Font for rendering text
    XdkFont m_Font;

    // Colors
    D3DCOLOR m_colBackColor;
    D3DCOLOR m_colTextColor;

    // Text Buffers
    unsigned int m_cScreenHeight;        // height in lines of screen area
    unsigned int m_cScreenHeightVirtual; // height in lines of text storage buffer
    unsigned int m_cScreenWidth;         // width in characters
    float m_fLineHeight;          // height of a single line in pixels

    wchar_t * m_Buffer;               // buffer big enough to hold a full screen
    wchar_t ** m_Lines;                // pointers to individual lines
    unsigned int m_nCurLine;             // index of current line being written to
    unsigned int m_cCurLineLength;       // length of the current line
    int m_nScrollOffset;        // offset to display text (in lines)

    // Add a character to the current line
    void            Add( char ch );
    void            Add( wchar_t wch );
};

#endif
