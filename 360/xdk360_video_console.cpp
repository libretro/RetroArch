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

#include <xtl.h>
#include <malloc.h>
#include "xdk360_video.h"
#include "xdk360_video_console.h"
#include "xdk360_video_debugfonts.h"
#include "../general.h"

Console::Console()
{
	first_message = true;
    m_Buffer = NULL;
    m_Lines = NULL;
    m_nScrollOffset = 0;
}

Console::~Console()
{
    Destroy();
}

HRESULT Console::Create( LPCSTR strFontFileName, unsigned long colBackColor,
	unsigned long colTextColor)
{
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	D3DDevice *m_pd3dDevice = vid->xdk360_render_device;

    // Calculate the safe area
    unsigned int uiSafeAreaPct = vid->video_mode.fIsHiDef ? SAFE_AREA_PCT_HDTV
	: SAFE_AREA_PCT_4x3;

    m_cxSafeArea = ( vid->d3dpp.BackBufferWidth * uiSafeAreaPct ) / 100;
    m_cySafeArea = ( vid->d3dpp.BackBufferHeight * uiSafeAreaPct ) / 100;

    m_cxSafeAreaOffset = ( vid->d3dpp.BackBufferWidth - m_cxSafeArea ) / 2;
    m_cySafeAreaOffset = ( vid->d3dpp.BackBufferHeight - m_cySafeArea ) / 2;

    // Create the font
    HRESULT hr = m_Font.Create( strFontFileName );
    if( FAILED( hr ) )
    {
        SSNES_ERR( "Could not create font.\n" );
		return -1;
    }

    // Save the colors
    m_colBackColor = colBackColor;
    m_colTextColor = colTextColor;

    // Calculate the number of lines on the screen
    float fCharWidth, fCharHeight;
    m_Font.GetTextExtent( L"i", &fCharWidth, &fCharHeight, FALSE );

    m_cScreenHeight = (unsigned int)( m_cySafeArea / fCharHeight );
    m_cScreenWidth = (unsigned int)( m_cxSafeArea / fCharWidth );

    m_cScreenHeightVirtual = m_cScreenHeight;

    m_fLineHeight = fCharHeight;

    // Allocate memory to hold the lines
    m_Buffer = new wchar_t[ m_cScreenHeightVirtual * ( m_cScreenWidth + 1 ) ];
    m_Lines = new wchar_t *[ m_cScreenHeightVirtual ];

    // Set the line pointers as indexes into the buffer
    for( unsigned int i = 0; i < m_cScreenHeightVirtual; i++ )
        m_Lines[ i ] = m_Buffer + ( m_cScreenWidth + 1 ) * i;

	m_nCurLine = 0;
    m_cCurLineLength = 0;
    memset( m_Buffer, 0, m_cScreenHeightVirtual * ( m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
    Render();

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Destroy()
// Desc: Tear everything down
//--------------------------------------------------------------------------------------
void Console::Destroy()
{
    // Delete the memory we've allocated
    if( m_Lines )
    {
        delete[] m_Lines;
        m_Lines = NULL;
    }

    if( m_Buffer )
    {
        delete[] m_Buffer;
        m_Buffer = NULL;
    }

    // Destroy the font
    m_Font.Destroy();
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the console to the screen
//--------------------------------------------------------------------------------------
void Console::Render (void)
{
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	D3DDevice *m_pd3dDevice = vid->xdk360_render_device;

    // The top line
    unsigned int nTextLine = ( m_nCurLine - m_cScreenHeight + m_cScreenHeightVirtual - m_nScrollOffset + 1 )
        % m_cScreenHeightVirtual;

    m_Font.Begin();

    for( unsigned int nScreenLine = 0; nScreenLine < m_cScreenHeight; nScreenLine++ )
    {
        m_Font.DrawText( (float)( m_cxSafeAreaOffset ),
                         (float)( m_cySafeAreaOffset + m_fLineHeight * nScreenLine ),
                         m_colTextColor, m_Lines[nTextLine] );

        nTextLine = ( nTextLine + 1 ) % m_cScreenHeightVirtual;
    }

    m_Font.End();
}


//--------------------------------------------------------------------------------------
// Name: Add( CHAR )
// Desc: Convert ANSI to WCHAR and add to the current line
//--------------------------------------------------------------------------------------
void Console::Add( char ch )
{
    wchar_t wch;

    int ret = MultiByteToWideChar( CP_ACP,        // ANSI code page
                                   0,             // No flags
                                   &ch,           // Character to convert
                                   1,             // Convert one byte
                                   &wch,          // Target wide character buffer
                                   1 );           // One wide character

    Add( wch );
}



//--------------------------------------------------------------------------------------
// Name: Add( WCHAR )
// Desc: Add a wide character to the current line
//--------------------------------------------------------------------------------------
void Console::Add( wchar_t wch )
{
    // If this is a newline, just increment lines and move on
    if( wch == L'\n' )
    {
		m_nCurLine = ( m_nCurLine + 1 ) % m_cScreenHeightVirtual;
		m_cCurLineLength = 0;
		memset( m_Lines[m_nCurLine], 0, ( m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
        return;
    }

    int bIncrementLine = FALSE;  // Whether to wrap to the next line

    if( m_cCurLineLength == m_cScreenWidth )
        bIncrementLine = TRUE;
    else
    {
        // Try to append the character to the line
        m_Lines[ m_nCurLine ][ m_cCurLineLength ] = wch;

        if( m_Font.GetTextWidth( m_Lines[ m_nCurLine ] ) > m_cxSafeArea )
        {
            // The line is too long, we need to wrap the character to the next line
            m_Lines[ m_nCurLine][ m_cCurLineLength ] = L'\0';
            bIncrementLine = TRUE;
        }
    }

    // If we need to skip to the next line, do so
    if( bIncrementLine )
    {
		m_nCurLine = ( m_nCurLine + 1 ) % m_cScreenHeightVirtual;
		m_cCurLineLength = 0;
		memset( m_Lines[m_nCurLine], 0, ( m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
        m_Lines[ m_nCurLine ][0] = wch;
    }

	if(IS_TIMER_EXPIRED())
		m_cCurLineLength++;
}


//--------------------------------------------------------------------------------------
// Name: Format()
// Desc: Output a variable argument list using a format string
//--------------------------------------------------------------------------------------
void Console::Format(int clear_screen, _In_z_ _Printf_format_string_ LPCSTR strFormat, ... )
{
	if(clear_screen)
	{
		m_nCurLine = 0;
		m_cCurLineLength = 0;
		memset( m_Buffer, 0, m_cScreenHeightVirtual * ( m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
	}

	va_list pArgList;
	va_start( pArgList, strFormat );
	FormatV( strFormat, pArgList );
	va_end( pArgList );

	// Render the output
	Render();
}

void Console::Format(int clear_screen, _In_z_ _Printf_format_string_ LPCWSTR wstrFormat, ... )
{
	if(clear_screen)
	{
		m_nCurLine = 0;
		m_cCurLineLength = 0;
		memset( m_Buffer, 0, m_cScreenHeightVirtual * ( m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
	}

	va_list pArgList;
	va_start( pArgList, wstrFormat );
	FormatV( wstrFormat, pArgList );
	va_end( pArgList );

	// Render the output
	Render();
}


//--------------------------------------------------------------------------------------
// Name: FormatV()
// Desc: Output a va_list using a format string
//--------------------------------------------------------------------------------------
void Console::FormatV( _In_z_ _Printf_format_string_ LPCSTR strFormat, va_list pArgList )
{
    // Count the required length of the string
    unsigned long dwStrLen = _vscprintf( strFormat, pArgList ) + 1;    // +1 = null terminator
    char * strMessage = ( char * )_malloca( dwStrLen );
    vsprintf_s( strMessage, dwStrLen, strFormat, pArgList );

    // Output the string to the console
	unsigned long uStringLength = strlen( strMessage );
    for( unsigned long i = 0; i < uStringLength; i++ )
        Add( strMessage[i] );

    _freea( strMessage );
}

void Console::FormatV( _In_z_ _Printf_format_string_ LPCWSTR wstrFormat, va_list pArgList )
{
    // Count the required length of the string
    unsigned long dwStrLen = _vscwprintf( wstrFormat, pArgList ) + 1;    // +1 = null terminator
    wchar_t * strMessage = ( wchar_t * )_malloca( dwStrLen * sizeof( wchar_t ) );
    vswprintf_s( strMessage, dwStrLen, wstrFormat, pArgList );

    // Output the string to the console
	unsigned long uStringLength = wcslen( strMessage );
    for( unsigned long i = 0; i < uStringLength; i++ )
        Add( strMessage[i] );

    _freea( strMessage );
}
