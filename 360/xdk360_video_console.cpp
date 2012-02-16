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

static video_console_t video_console;
static XdkFont m_Font;

void xdk360_console_draw(void)
{
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	D3DDevice *m_pd3dDevice = vid->xdk360_render_device;

    // The top line
    unsigned int nTextLine = ( video_console.m_nCurLine - 
		video_console.m_cScreenHeight + video_console.m_cScreenHeightVirtual - 
		video_console.m_nScrollOffset + 1 )
        % video_console.m_cScreenHeightVirtual;

    m_Font.Begin();

    for( unsigned int nScreenLine = 0; nScreenLine < video_console.m_cScreenHeight; nScreenLine++ )
    {
        m_Font.DrawText( (float)( video_console.m_cxSafeAreaOffset ),
                         (float)( video_console.m_cySafeAreaOffset + 
						 video_console.m_fLineHeight * nScreenLine ),
                         video_console.m_colTextColor, 
						 video_console.m_Lines[nTextLine] );

        nTextLine = ( nTextLine + 1 ) % video_console.m_cScreenHeightVirtual;
    }

    m_Font.End();
}

HRESULT xdk360_console_init( LPCSTR strFontFileName, unsigned long colBackColor,
	unsigned long colTextColor)
{
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	D3DDevice *m_pd3dDevice = vid->xdk360_render_device;

	video_console.first_message = true;
	video_console.m_Buffer = NULL;
    video_console.m_Lines = NULL;
    video_console.m_nScrollOffset = 0;

    // Calculate the safe area
    unsigned int uiSafeAreaPct = vid->video_mode.fIsHiDef ? SAFE_AREA_PCT_HDTV
	: SAFE_AREA_PCT_4x3;

    video_console.m_cxSafeArea = ( vid->d3dpp.BackBufferWidth * uiSafeAreaPct ) / 100;
    video_console.m_cySafeArea = ( vid->d3dpp.BackBufferHeight * uiSafeAreaPct ) / 100;

    video_console.m_cxSafeAreaOffset = ( vid->d3dpp.BackBufferWidth - video_console.m_cxSafeArea ) / 2;
    video_console.m_cySafeAreaOffset = ( vid->d3dpp.BackBufferHeight - video_console.m_cySafeArea ) / 2;

    // Create the font
    HRESULT hr = m_Font.Create( strFontFileName );
    if( FAILED( hr ) )
    {
        SSNES_ERR( "Could not create font.\n" );
		return -1;
    }

    // Save the colors
    video_console.m_colBackColor = colBackColor;
    video_console.m_colTextColor = colTextColor;

    // Calculate the number of lines on the screen
    float fCharWidth, fCharHeight;
    m_Font.GetTextExtent( L"i", &fCharWidth, &fCharHeight, FALSE );

    video_console.m_cScreenHeight = (unsigned int)( video_console.m_cySafeArea / fCharHeight );
    video_console.m_cScreenWidth = (unsigned int)( video_console.m_cxSafeArea / fCharWidth );

    video_console.m_cScreenHeightVirtual = video_console.m_cScreenHeight;

    video_console.m_fLineHeight = fCharHeight;

    // Allocate memory to hold the lines
    video_console.m_Buffer = new wchar_t[ video_console.m_cScreenHeightVirtual * ( video_console.m_cScreenWidth + 1 ) ];
    video_console.m_Lines = new wchar_t *[ video_console.m_cScreenHeightVirtual ];

    // Set the line pointers as indexes into the buffer
    for( unsigned int i = 0; i < video_console.m_cScreenHeightVirtual; i++ )
        video_console.m_Lines[ i ] = video_console.m_Buffer + ( video_console.m_cScreenWidth + 1 ) * i;

	video_console.m_nCurLine = 0;
    video_console.m_cCurLineLength = 0;
    memset( video_console.m_Buffer, 0, video_console.m_cScreenHeightVirtual * ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
    xdk360_console_draw();

    return hr;
}

void xdk360_console_deinit()
{
    // Delete the memory we've allocated
    if(video_console.m_Lines)
    {
        delete[] video_console.m_Lines;
        video_console.m_Lines = NULL;
    }

    if(video_console.m_Buffer)
    {
        delete[] video_console.m_Buffer;
        video_console.m_Buffer = NULL;
    }

    // Destroy the font
    m_Font.Destroy();
}

void xdk360_console_add( wchar_t wch )
{
    // If this is a newline, just increment lines and move on
    if( wch == L'\n' )
    {
		video_console.m_nCurLine = ( video_console.m_nCurLine + 1 ) 
			% video_console.m_cScreenHeightVirtual;
		video_console.m_cCurLineLength = 0;
		memset(video_console.m_Lines[video_console.m_nCurLine], 0, 
			( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
        return;
    }

    int bIncrementLine = FALSE;  // Whether to wrap to the next line

    if( video_console.m_cCurLineLength == video_console.m_cScreenWidth )
        bIncrementLine = TRUE;
    else
    {
        // Try to append the character to the line
        video_console.m_Lines[ video_console.m_nCurLine ]
		[ video_console.m_cCurLineLength ] = wch;

        if( m_Font.GetTextWidth( video_console.m_Lines
			[ video_console.m_nCurLine ] ) > video_console.m_cxSafeArea )
        {
            // The line is too long, we need to wrap the character to the next line
            video_console.m_Lines[video_console.m_nCurLine]
			[ video_console.m_cCurLineLength ] = L'\0';
            bIncrementLine = TRUE;
        }
    }

    // If we need to skip to the next line, do so
    if( bIncrementLine )
    {
		video_console.m_nCurLine = ( video_console.m_nCurLine + 1 ) 
			% video_console.m_cScreenHeightVirtual;
		video_console.m_cCurLineLength = 0;
		memset( video_console.m_Lines[video_console.m_nCurLine], 
			0, ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
        video_console.m_Lines[video_console.m_nCurLine ][0] = wch;
    }

	video_console.m_cCurLineLength++;
}

void xdk360_console_format(_In_z_ _Printf_format_string_ LPCSTR strFormat, ... )
{
	video_console.m_nCurLine = 0;
	video_console.m_cCurLineLength = 0;
	memset( video_console.m_Buffer, 0, 
		video_console.m_cScreenHeightVirtual * 
		( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );

	va_list pArgList;
	va_start( pArgList, strFormat );
	
	// Count the required length of the string
    unsigned long dwStrLen = _vscprintf( strFormat, pArgList ) + 1;    
	// +1 = null terminator
    char * strMessage = ( char * )_malloca( dwStrLen );
    vsprintf_s( strMessage, dwStrLen, strFormat, pArgList );

    // Output the string to the console
	unsigned long uStringLength = strlen( strMessage );
    for( unsigned long i = 0; i < uStringLength; i++ )
	{
		wchar_t wch;
		int ret = MultiByteToWideChar( CP_ACP,        // ANSI code page
                                   0,             // No flags
                                   &strMessage[i],           // Character to convert
                                   1,             // Convert one byte
                                   &wch,          // Target wide character buffer
                                   1 );           // One wide character
		xdk360_console_add( wch );
	}

    _freea( strMessage );

	va_end( pArgList );
}

void xdk360_console_format_w(_In_z_ _Printf_format_string_ LPCWSTR wstrFormat, ... )
{
	video_console.m_nCurLine = 0;
	video_console.m_cCurLineLength = 0;
	memset( video_console.m_Buffer, 0, video_console.m_cScreenHeightVirtual 
		* ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );

	va_list pArgList;
	va_start( pArgList, wstrFormat );

	    // Count the required length of the string
    unsigned long dwStrLen = _vscwprintf( wstrFormat, pArgList ) + 1;    // +1 = null terminator
    wchar_t * strMessage = ( wchar_t * )_malloca( dwStrLen * sizeof( wchar_t ) );
    vswprintf_s( strMessage, dwStrLen, wstrFormat, pArgList );

    // Output the string to the console
	unsigned long uStringLength = wcslen( strMessage );
    for( unsigned long i = 0; i < uStringLength; i++ )
        xdk360_console_add( strMessage[i] );

    _freea( strMessage );

	va_end( pArgList );
}