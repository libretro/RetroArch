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

#include <xgraphics.h>
#include "xdk360_video_resources.h"

struct XPR_HEADER
{
   unsigned long dwMagic;
   unsigned long dwHeaderSize;
   unsigned long dwDataSize;
};

#define XPR2_MAGIC_VALUE (0x58505232)

const DWORD g_MapLinearToSrgbGpuFormat[] = 
{
   GPUTEXTUREFORMAT_1_REVERSE,
   GPUTEXTUREFORMAT_1,
   GPUTEXTUREFORMAT_8,
   GPUTEXTUREFORMAT_1_5_5_5,
   GPUTEXTUREFORMAT_5_6_5,
   GPUTEXTUREFORMAT_6_5_5,
   GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
   GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_8_A,
   GPUTEXTUREFORMAT_8_B,
   GPUTEXTUREFORMAT_8_8,
   GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP,     
   GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP,      
   GPUTEXTUREFORMAT_16_16_EDRAM,          
   GPUTEXTUREFORMAT_8_8_8_8_A,
   GPUTEXTUREFORMAT_4_4_4_4,
   GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
   GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,  
   GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
   GPUTEXTUREFORMAT_16_16_16_16_EDRAM,
   GPUTEXTUREFORMAT_24_8,
   GPUTEXTUREFORMAT_24_8_FLOAT,
   GPUTEXTUREFORMAT_16,
   GPUTEXTUREFORMAT_16_16,
   GPUTEXTUREFORMAT_16_16_16_16,
   GPUTEXTUREFORMAT_16_EXPAND,
   GPUTEXTUREFORMAT_16_16_EXPAND,
   GPUTEXTUREFORMAT_16_16_16_16_EXPAND,
   GPUTEXTUREFORMAT_16_FLOAT,
   GPUTEXTUREFORMAT_16_16_FLOAT,
   GPUTEXTUREFORMAT_16_16_16_16_FLOAT,
   GPUTEXTUREFORMAT_32,
   GPUTEXTUREFORMAT_32_32,
   GPUTEXTUREFORMAT_32_32_32_32,
   GPUTEXTUREFORMAT_32_FLOAT,
   GPUTEXTUREFORMAT_32_32_FLOAT,
   GPUTEXTUREFORMAT_32_32_32_32_FLOAT,
   GPUTEXTUREFORMAT_32_AS_8,
   GPUTEXTUREFORMAT_32_AS_8_8,
   GPUTEXTUREFORMAT_16_MPEG,
   GPUTEXTUREFORMAT_16_16_MPEG,
   GPUTEXTUREFORMAT_8_INTERLACED,
   GPUTEXTUREFORMAT_32_AS_8_INTERLACED,
   GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED,
   GPUTEXTUREFORMAT_16_INTERLACED,
   GPUTEXTUREFORMAT_16_MPEG_INTERLACED,
   GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED,
   GPUTEXTUREFORMAT_DXN,
   GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
   GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
   GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_32_32_32_FLOAT,
   GPUTEXTUREFORMAT_DXT3A,
   GPUTEXTUREFORMAT_DXT5A,
   GPUTEXTUREFORMAT_CTX1,
   GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1,
   GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM,
   GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM,
};

PackedResource::PackedResource()
{
   m_pSysMemData = NULL;
   m_dwSysMemDataSize = 0L;
   m_pVidMemData = NULL;
   m_dwVidMemDataSize = 0L;
   m_pResourceTags = NULL;
   m_dwNumResourceTags = 0L;
   m_bInitialized = FALSE;
}

PackedResource::~PackedResource()
{
   Destroy();
}

void * PackedResource::GetData( const char * strName ) const
{
   if( m_pResourceTags == NULL || strName == NULL )
      return NULL;

   for( unsigned long i = 0; i < m_dwNumResourceTags; i++ )
   { if( !_stricmp( strName, m_pResourceTags[i].strName ) )
         return &m_pSysMemData[m_pResourceTags[i].dwOffset];
   }

   return NULL;
}

HRESULT PackedResource::Create( const char * strFilename )
{
   unsigned long dwNumBytesRead;
   void * hFile = CreateFile( strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL );

   if( hFile == INVALID_HANDLE_VALUE )
   {
      RARCH_ERR( "File <%s> not found.\n", strFilename );
      return E_FAIL;
   }

   // Read in and verify the XPR magic header
   XPR_HEADER xprh;
   if( !ReadFile( hFile, &xprh, sizeof( XPR_HEADER ), &dwNumBytesRead, NULL ) )
   {
      RARCH_ERR( "Error reading XPR header in file <%s>.\n", strFilename );
      CloseHandle( hFile );
      return E_FAIL;
   }

   if( xprh.dwMagic != XPR2_MAGIC_VALUE )
   {
      RARCH_ERR( "Invalid Xbox Packed Resource (.xpr) file: Magic = 0x%08lx.\n", xprh.dwMagic );
      CloseHandle( hFile );
      return E_FAIL;
   }

   // Compute memory requirements
   m_dwSysMemDataSize = xprh.dwHeaderSize;
   m_dwVidMemDataSize = xprh.dwDataSize;

   // Allocate memory
   m_pSysMemData = (unsigned char*)malloc(m_dwSysMemDataSize);
   if( m_pSysMemData == NULL )
   {
      RARCH_ERR( "Could not allocate system memory.\n" );
      m_dwSysMemDataSize = 0;
      return E_FAIL;
   }
   m_pVidMemData = ( unsigned char* )XMemAlloc( m_dwVidMemDataSize, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
      XALLOC_PHYSICAL_ALIGNMENT_4K, XALLOC_MEMPROTECT_WRITECOMBINE, 0, XALLOC_MEMTYPE_PHYSICAL ) );

   if( m_pVidMemData == NULL )
   {
      RARCH_ERR( "Could not allocate physical memory.\n" );
      m_dwSysMemDataSize = 0;
      m_dwVidMemDataSize = 0;
      free(m_pSysMemData);
      m_pSysMemData = NULL;
      return E_FAIL;
   }

   // Read in the data from the file
   if( !ReadFile( hFile, m_pSysMemData, m_dwSysMemDataSize, &dwNumBytesRead, NULL ) ||
      !ReadFile( hFile, m_pVidMemData, m_dwVidMemDataSize, &dwNumBytesRead, NULL ) )
   {
      RARCH_ERR( "Unable to read Xbox Packed Resource (.xpr) file.\n" );
      CloseHandle( hFile );
      return E_FAIL;
   }

   // Done with the file
   CloseHandle( hFile );

   // Extract resource table from the header data
   m_dwNumResourceTags = *( unsigned long * )( m_pSysMemData + 0 );
   m_pResourceTags = ( RESOURCE* )( m_pSysMemData + 4 );

   // Patch up the resources
   for( unsigned long i = 0; i < m_dwNumResourceTags; i++ )
   {
      m_pResourceTags[i].strName = ( char * )( m_pSysMemData + ( unsigned long )m_pResourceTags[i].strName );

      // Fixup the texture memory
      if( ( m_pResourceTags[i].dwType & 0xffff0000 ) == ( RESOURCETYPE_TEXTURE & 0xffff0000 ) )
      {
         D3DTexture* pTexture = ( D3DTexture* )&m_pSysMemData[m_pResourceTags[i].dwOffset];
	 // Adjust Base address according to where memory was allocated
	 XGOffsetBaseTextureAddress( pTexture, m_pVidMemData, m_pVidMemData );
      }
   }

   m_bInitialized = TRUE;

   return 0;
}

void PackedResource::Destroy()
{
    free(m_pSysMemData);
    m_pSysMemData = NULL;
    m_dwSysMemDataSize = 0L;

    if( m_pVidMemData != NULL )
       XMemFree( m_pVidMemData, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
       0, 0, 0, XALLOC_MEMTYPE_PHYSICAL ) );

    m_pVidMemData = NULL;
    m_dwVidMemDataSize = 0L;

    m_pResourceTags = NULL;
    m_dwNumResourceTags = 0L;

    m_bInitialized = FALSE;
}

void xdk360_convert_texture_to_as16_srgb( D3DTexture *pTexture )
{
   pTexture->Format.SignX = GPUSIGN_GAMMA;
   pTexture->Format.SignY = GPUSIGN_GAMMA;
   pTexture->Format.SignZ = GPUSIGN_GAMMA;

   XGTEXTURE_DESC desc;
   XGGetTextureDesc( pTexture, 0, &desc );

   //convert to AS_16_16_16_16 format
   pTexture->Format.DataFormat = g_MapLinearToSrgbGpuFormat[ (desc.Format & D3DFORMAT_TEXTUREFORMAT_MASK) >> D3DFORMAT_TEXTUREFORMAT_SHIFT ];
}

void xdk360_video_font_draw_text(xdk360_video_font_t * font, 
   float fOriginX, float fOriginY, const wchar_t * strText, float fMaxPixelWidth )
{
   if( strText == NULL || strText[0] == L'\0')
      return;

   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   D3DDevice *pd3dDevice = vid->d3d_render_device;

   // Set the color as a vertex shader constant
   float vColor[4];
   vColor[0] = ( ( 0xffffffff & 0x00ff0000 ) >> 16L ) / 255.0F;
   vColor[1] = ( ( 0xffffffff & 0x0000ff00 ) >> 8L ) / 255.0F;
   vColor[2] = ( ( 0xffffffff & 0x000000ff ) >> 0L ) / 255.0F;
   vColor[3] = ( ( 0xffffffff & 0xff000000 ) >> 24L ) / 255.0F;

   d3d9_render_msg_pre(font);

   // Perform the actual storing of the color constant here to prevent
   // a load-hit-store by inserting work between the store and the use of
   // the vColor array.
   pd3dDevice->SetVertexShaderConstantF( 1, vColor, 1 );

   // Set the starting screen position
   if((fOriginX < 0.0f))
      fOriginX += font->m_rcWindow.x2;
   if( fOriginY < 0.0f )
      fOriginY += font->m_rcWindow.y2;

   font->m_fCursorX = floorf( fOriginX );
   font->m_fCursorY = floorf( fOriginY );

   // Adjust for padding
   fOriginY -= font->m_fFontTopPadding;

   // Add window offsets
   float Winx = 0.0f;
   float Winy = 0.0f;
   fOriginX += Winx;
   fOriginY += Winy;
   font->m_fCursorX += Winx;
   font->m_fCursorY += Winy;

   // Begin drawing the vertices

   // Declared as volatile to force writing in ascending
   // address order. It prevents out of sequence writing in write combined
   // memory.

   volatile float * pVertex;

   unsigned long dwNumChars = wcslen(strText);
   HRESULT hr = pd3dDevice->BeginVertices( D3DPT_QUADLIST, 4 * dwNumChars, sizeof( XMFLOAT4 ) ,
      ( VOID** )&pVertex );

   // The ring buffer may run out of space when tiling, doing z-prepasses,
   // or using BeginCommandBuffer. If so, make the buffer larger.
   if( hr < 0 )
      RARCH_ERR( "Ring buffer out of memory.\n" );

   // Draw four vertices for each glyph
   while( *strText )
   {
      wchar_t letter;

      // Get the current letter in the string
      letter = *strText++;

      // Handle the newline character
      if( letter == L'\n' )
      {
         font->m_fCursorX = fOriginX;
         font->m_fCursorY += font->m_fFontYAdvance * font->m_fYScaleFactor;
         continue;
      }

      // Translate unprintable characters
      const GLYPH_ATTR * pGlyph = &font->m_Glyphs[ ( letter <= font->m_cMaxGlyph )
      ? font->m_TranslatorTable[letter] : 0 ];

      float fOffset = font->m_fXScaleFactor * (float)pGlyph->wOffset;
      float fAdvance = font->m_fXScaleFactor * (float)pGlyph->wAdvance;
      float fWidth = font->m_fXScaleFactor * (float)pGlyph->wWidth;
      float fHeight = font->m_fYScaleFactor * font->m_fFontHeight;

      // Setup the screen coordinates
      font->m_fCursorX += fOffset;
      float X4 = font->m_fCursorX;
      float X1 = X4;
      float X3 = X4 + fWidth;
      float X2 = X1 + fWidth;
      float Y1 = font->m_fCursorY;
      float Y3 = Y1 + fHeight;
      float Y2 = Y1;
      float Y4 = Y3;

      font->m_fCursorX += fAdvance;

      // Add the vertices to draw this glyph

      unsigned long tu1 = pGlyph->tu1;        // Convert shorts to 32 bit longs for in register merging
      unsigned long tv1 = pGlyph->tv1;
      unsigned long tu2 = pGlyph->tu2;
      unsigned long tv2 = pGlyph->tv2;

      // NOTE: The vertexs are 2 floats for the screen coordinates,
      // followed by two USHORTS for the u/vs of the character,
      // terminated with the ARGB 32 bit color.
      // This makes for 16 bytes per vertex data (Easier to read)
      // Second NOTE: The uvs are merged and written using a DWORD due
      // to the write combining hardware being only able to handle 32,
      // 64 and 128 writes. Never store to write combined memory with
      // 8 or 16 bit instructions. You've been warned.

      pVertex[0] = X1;
      pVertex[1] = Y1;
      ((volatile unsigned long *)pVertex)[2] = (tu1<<16)|tv1;         // Merged using big endian rules
      pVertex[3] = 0;
      pVertex[4] = X2;
      pVertex[5] = Y2;
      ((volatile unsigned long *)pVertex)[6] = (tu2<<16)|tv1;         // Merged using big endian rules
      pVertex[7] = 0;
      pVertex[8] = X3;
      pVertex[9] = Y3;
      ((volatile unsigned long *)pVertex)[10] = (tu2<<16)|tv2;        // Merged using big endian rules
      pVertex[11] = 0;
      pVertex[12] = X4;
      pVertex[13] = Y4;
      ((volatile unsigned long *)pVertex)[14] = (tu1<<16)|tv2;        // Merged using big endian rules
      pVertex[15] = 0;
      pVertex+=16;

      dwNumChars--;
   }

   // Since we allocated vertex data space based on the string length, we now need to
   // add some dummy verts for any skipped characters (like newlines, etc.)
   while( dwNumChars )
   {
      for(int i = 0; i < 16; i++)
	     pVertex[i] = 0;

      pVertex += 16;
      dwNumChars--;
   }

   // Stop drawing vertices
   D3DDevice_EndVertices(pd3dDevice);

   // Undo window offsets
   font->m_fCursorX -= Winx;
   font->m_fCursorY -= Winy;

   d3d9_render_msg_post(font);
}

void xdk360_console_draw(void)
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   D3DDevice *m_pd3dDevice = vid->d3d_render_device;

   // The top line
   unsigned int nTextLine = ( video_console.m_nCurLine - 
      video_console.m_cScreenHeight + video_console.m_cScreenHeightVirtual - 
      video_console.m_nScrollOffset + 1 )
      % video_console.m_cScreenHeightVirtual;

   d3d9_render_msg_pre(&m_Font);

   for( unsigned int nScreenLine = 0; nScreenLine < video_console.m_cScreenHeight; nScreenLine++ )
   {
      xdk360_video_font_draw_text(&m_Font, (float)( video_console.m_cxSafeAreaOffset ),
      (float)( video_console.m_cySafeAreaOffset + video_console.m_fLineHeight * nScreenLine ), 
      video_console.m_Lines[nTextLine], 0.0f );

      nTextLine = ( nTextLine + 1 ) % video_console.m_cScreenHeightVirtual;
   }

   d3d9_render_msg_post(&m_Font);
}
