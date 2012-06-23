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

#define NONET
#include <xtl.h>
#include "xdk360_video.h"
#include "fonts.h"
#include "../general.h"

static video_console_t video_console;
static xdk360_video_font_t m_Font;

static PackedResource m_xprResource;

#define CALCFONTFILEHEADERSIZE(x) ( sizeof(unsigned long) + (sizeof(float)* 4) + sizeof(unsigned short) + (sizeof(wchar_t)*(x)) )
#define FONTFILEVERSION 5

typedef struct {
   unsigned long m_dwFileVersion;   // Version of the font file (Must match FONTFILEVERSION)
   float m_fFontHeight;            // Height of the font strike in pixels
   float m_fFontTopPadding;        // Padding above the strike zone
   float m_fFontBottomPadding;     // Padding below the strike zone
   float m_fFontYAdvance;          // Number of pixels to move the cursor for a line feed
   unsigned short m_cMaxGlyph;     // Number of font characters (Should be an odd number to maintain DWORD Alignment)
   wchar_t m_TranslatorTable[1];   // ASCII to Glyph lookup table, NOTE: It's m_cMaxGlyph+1 in size.
} FontFileHeaderImage_t;

// Font strike array. Immediately follows the FontFileHeaderImage_t
// structure image

typedef struct {
   unsigned long m_dwNumGlyphs;    // Size of font strike array (First entry is the unknown glyph)
   GLYPH_ATTR m_Glyphs[1];         // Array of font strike uv's etc... NOTE: It's m_dwNumGlyphs in size
} FontFileStrikesImage_t; 

static const char g_strFontShader[] =
   "struct VS_IN\n"
   "{\n"
      "float2 Pos : POSITION;\n"
      "float2 Tex : TEXCOORD0;\n"
   "};\n"
   "struct VS_OUT\n"
   "{\n"
      "float4 Position : POSITION;\n"
      "float2 TexCoord0 : TEXCOORD0;\n"
   "};\n"
   "uniform float4 Color : register(c1);\n"
   "uniform float2 TexScale : register(c2);\n"
   "sampler FontTexture : register(s0);\n"
   "VS_OUT main_vertex( VS_IN In )\n"
   "{\n"
     "VS_OUT Out;\n"
     "Out.Position.x  = (In.Pos.x-0.5);\n"
     "Out.Position.y  = (In.Pos.y-0.5);\n"
     "Out.Position.z  = ( 0.0 );\n"
     "Out.Position.w  = ( 1.0 );\n"
     "Out.TexCoord0.x = In.Tex.x * TexScale.x;\n"
     "Out.TexCoord0.y = In.Tex.y * TexScale.y;\n"
     "return Out;\n"
   "}\n"
   "float4 main_fragment( VS_OUT In ) : COLOR0\n"
   "{\n"
     "float4 FontTexel = tex2D( FontTexture, In.TexCoord0 );\n"
     "return FontTexel;\n"
   "}\n";

typedef struct {
   D3DVertexDeclaration* m_pFontVertexDecl;    // Shared vertex buffer
   D3DVertexShader* m_pFontVertexShader;       // Created vertex shader
   D3DPixelShader* m_pFontPixelShader;         // Created pixel shader
} Font_Locals_t;

// All elements are defaulted to NULL
static Font_Locals_t s_FontLocals;    // Global static instance

static void xdk360_video_font_draw_text(xdk360_video_font_t * font, 
   float fOriginX, float fOriginY, unsigned long dwColor,
   const wchar_t * strText, float fMaxPixelWidth )
{
   if( strText == NULL || strText[0] == L'\0')
      return;

   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
   D3DDevice *pd3dDevice = vid->d3d_render_device;

   // Set the color as a vertex shader constant
   float vColor[4];
   vColor[0] = ( ( dwColor & 0x00ff0000 ) >> 16L ) / 255.0F;
   vColor[1] = ( ( dwColor & 0x0000ff00 ) >> 8L ) / 255.0F;
   vColor[2] = ( ( dwColor & 0x000000ff ) >> 0L ) / 255.0F;
   vColor[3] = ( ( dwColor & 0xff000000 ) >> 24L ) / 255.0F;

   // Set up stuff to prepare for drawing text
   xdk360_video_font_begin(font);

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

      // Select the RGBA channel that the compressed glyph is stored in
      // Takes a 4 bit per pixel ARGB value and expand it to an 8 bit per pixel ARGB value

      unsigned long dwChannelSelector = pGlyph->wMask;        // Convert to 32 bit
      // Perform the conversion without branching

      // Splat the 4 bit per pixels from 0x1234 to 0x01020304
      dwChannelSelector = ((dwChannelSelector&0xF000)<<(24-12))|((dwChannelSelector&0xF00)<<(16-8))|
	      ((dwChannelSelector&0xF0)<<(8-4))|(dwChannelSelector&0xF);

      // Perform a vectorized multiply to make 0x01020304 into 0x11223344
      dwChannelSelector *= 0x11;

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
      reinterpret_cast<volatile unsigned long *>(pVertex)[2] = (tu1<<16)|tv1;         // Merged using big endian rules
      reinterpret_cast<volatile unsigned long *>(pVertex)[3] = dwChannelSelector;
      pVertex[4] = X2;
      pVertex[5] = Y2;
      reinterpret_cast<volatile unsigned long *>(pVertex)[6] = (tu2<<16)|tv1;         // Merged using big endian rules
      reinterpret_cast<volatile unsigned long *>(pVertex)[7] = dwChannelSelector;
      pVertex[8] = X3;
      pVertex[9] = Y3;
      reinterpret_cast<volatile unsigned long *>(pVertex)[10] = (tu2<<16)|tv2;        // Merged using big endian rules
      reinterpret_cast<volatile unsigned long *>(pVertex)[11] = dwChannelSelector;
      pVertex[12] = X4;
      pVertex[13] = Y4;
      reinterpret_cast<volatile unsigned long *>(pVertex)[14] = (tu1<<16)|tv2;        // Merged using big endian rules
      reinterpret_cast<volatile unsigned long *>(pVertex)[15] = dwChannelSelector;
      pVertex+=16;

      dwNumChars--;
   }

   // Since we allocated vertex data space based on the string length, we now need to
   // add some dummy verts for any skipped characters (like newlines, etc.)
   while( dwNumChars )
   {
      pVertex[0] = 0;
      pVertex[1] = 0;
      pVertex[2] = 0;
      pVertex[3] = 0;
      pVertex[4] = 0;
      pVertex[5] = 0;
      pVertex[6] = 0;
      pVertex[7] = 0;
      pVertex[8] = 0;
      pVertex[9] = 0;
      pVertex[10] = 0;
      pVertex[11] = 0;
      pVertex[12] = 0;
      pVertex[13] = 0;
      pVertex[14] = 0;
      pVertex[15] = 0;
      pVertex+=16;
      dwNumChars--;
   }

   // Stop drawing vertices
   D3DDevice_EndVertices(pd3dDevice);

   // Undo window offsets
   font->m_fCursorX -= Winx;
   font->m_fCursorY -= Winy;

   // Call End() to complete the begin/end pair for drawing text
   xdk360_video_font_end(font);
}

void xdk360_console_draw(void)
{
   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
   D3DDevice *m_pd3dDevice = vid->d3d_render_device;

   // The top line
   unsigned int nTextLine = ( video_console.m_nCurLine - 
      video_console.m_cScreenHeight + video_console.m_cScreenHeightVirtual - 
      video_console.m_nScrollOffset + 1 )
      % video_console.m_cScreenHeightVirtual;

   xdk360_video_font_begin(&m_Font);

   for( unsigned int nScreenLine = 0; nScreenLine < video_console.m_cScreenHeight; nScreenLine++ )
   {
      xdk360_video_font_draw_text(&m_Font, (float)( video_console.m_cxSafeAreaOffset ),
      (float)( video_console.m_cySafeAreaOffset + 
      video_console.m_fLineHeight * nScreenLine ),
      video_console.m_colTextColor, 
      video_console.m_Lines[nTextLine], 0.0f );

      nTextLine = ( nTextLine + 1 ) % video_console.m_cScreenHeightVirtual;
   }

   xdk360_video_font_end(&m_Font);
}

static void xdk360_video_font_get_text_width(xdk360_video_font_t * font, const wchar_t * strText, float * pWidth, float * pHeight)
{
	   // Set default text extent in output parameters
    int iWidth = 0;
    float fHeight = 0.0f;

    if( strText )
    {
        // Initialize counters that keep track of text extent
        int ix = 0;
        float fy = font->m_fFontHeight;       // One character high to start
        if( fy > fHeight )
            fHeight = fy;

        // Loop through each character and update text extent
        unsigned long letter;
        while( (letter = *strText) != 0 )
        {
            ++strText;

            // Handle newline character
            if (letter == L'\n')
               break;

            // Translate unprintable characters
            const GLYPH_ATTR* pGlyph;
            
            if( letter > font->m_cMaxGlyph )
                letter = 0;     // Out of bounds?
            else
                letter = font->m_TranslatorTable[letter];     // Remap ASCII to glyph

            pGlyph = &font->m_Glyphs[letter];                 // Get the requested glyph

            // Get text extent for this character's glyph
            ix += pGlyph->wOffset;
            ix += pGlyph->wAdvance;

            // Since the x widened, test against the x extent

            if( ix > iWidth )
                iWidth = ix;
        }
    }

    float fWidth = (float)iWidth;
    fHeight *= font->m_fYScaleFactor;     // Apply the scale factor to the result
    *pHeight = fHeight;                   // Store the final results

    fWidth *= font->m_fXScaleFactor;
    *pWidth = fWidth;
}

static HRESULT xdk360_video_font_create_shaders (xdk360_video_font_t * font)
{
    HRESULT hr;

    if (!s_FontLocals.m_pFontVertexDecl)
    {
        do
        {
            static const D3DVERTEXELEMENT9 decl[] =
            {
                { 0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                { 0,  8, D3DDECLTYPE_USHORT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
                D3DDECL_END()
            };
            
            xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
            D3DDevice *pd3dDevice = vid->d3d_render_device;

            hr = pd3dDevice->CreateVertexDeclaration( decl, &s_FontLocals.m_pFontVertexDecl );

            if (hr >= 0)
            {
                ID3DXBuffer* pShaderCode;

                hr = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
                    NULL, NULL, "main_vertex", "vs.2.0", 0,&pShaderCode, NULL, NULL );

                if (hr >= 0)
                {
                    hr = pd3dDevice->CreateVertexShader( ( unsigned long * )pShaderCode->GetBufferPointer(),
                        &s_FontLocals.m_pFontVertexShader );
                    pShaderCode->Release();
                    
                    if (hr >= 0)
                    {
                        hr = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
                            NULL, NULL, "main_fragment", "ps.2.0", 0,&pShaderCode, NULL, NULL );

                        if (hr >= 0)
                        {
                            hr = pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                &s_FontLocals.m_pFontPixelShader );
                            pShaderCode->Release();

                            if (hr >= 0) 
                            {
                                hr = 0;
                                break;
                            }
                        }
                        D3DResource_Release((D3DResource *)s_FontLocals.m_pFontVertexShader);
                    }

                    s_FontLocals.m_pFontVertexShader = NULL;
                }

                D3DResource_Release((D3DResource *)s_FontLocals.m_pFontVertexDecl);
            }  
            s_FontLocals.m_pFontVertexDecl = NULL;
        }while(0);
        return hr;
    }
    else
    {
       D3DResource_AddRef((D3DResource *)s_FontLocals.m_pFontVertexDecl);
       D3DResource_AddRef((D3DResource *)s_FontLocals.m_pFontVertexShader);
       D3DResource_AddRef((D3DResource *)s_FontLocals.m_pFontPixelShader);
       hr = 0;
    }
    return hr;
}

static HRESULT xdk360_video_font_init(xdk360_video_font_t * font, const char * strFontFileName)
{
   font->m_pFontTexture = NULL;
   font->m_dwNumGlyphs = 0L;
   font->m_Glyphs = NULL;
   font->m_fCursorX = 0.0f;
   font->m_fCursorY = 0.0f;
   font->m_fXScaleFactor = 2.0f;
   font->m_fYScaleFactor = 2.0f;
   font->m_cMaxGlyph = 0;
   font->m_TranslatorTable = NULL;
   font->m_dwNestedBeginCount = 0L;
    
   // Create the font
   if( FAILED( m_xprResource.Create( strFontFileName ) ) )
      return E_FAIL;

   D3DTexture * pFontTexture = m_xprResource.GetTexture( "FontTexture" );
   const void * pFontData = m_xprResource.GetData( "FontData"); 

   // Save a copy of the texture
   font->m_pFontTexture = pFontTexture;

   // Check version of file (to make sure it matches up with the FontMaker tool)
   const unsigned char * pData = static_cast<const unsigned char *>(pFontData);
   unsigned long dwFileVersion = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_dwFileVersion;

   if( dwFileVersion == FONTFILEVERSION )
   {
      font->m_fFontHeight = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontHeight;
      font->m_fFontTopPadding = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontTopPadding;
      font->m_fFontBottomPadding = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontBottomPadding;
      font->m_fFontYAdvance = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontYAdvance;

      // Point to the translator string which immediately follows the 4 floats
      font->m_cMaxGlyph = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_cMaxGlyph;

      font->m_TranslatorTable = const_cast<FontFileHeaderImage_t*>(reinterpret_cast<const FontFileHeaderImage_t *>(pData))->m_TranslatorTable;

      pData += CALCFONTFILEHEADERSIZE( font->m_cMaxGlyph + 1 );

      // Read the glyph attributes from the file
      font->m_dwNumGlyphs = reinterpret_cast<const FontFileStrikesImage_t *>(pData)->m_dwNumGlyphs;
      font->m_Glyphs = reinterpret_cast<const FontFileStrikesImage_t *>(pData)->m_Glyphs;        // Pointer
   }
   else
   {
      RARCH_ERR( "Incorrect version number on font file.\n" );
      return E_FAIL;
   }

   // Create the vertex and pixel shaders for rendering the font
   if( FAILED( xdk360_video_font_create_shaders(font) ) )
   {
      RARCH_ERR( "Could not create font shaders.\n" );
      return E_FAIL;
   }

   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
   D3DDevice *pd3dDevice = vid->d3d_render_device;

   // Initialize the window
   D3DDISPLAYMODE DisplayMode;
   pd3dDevice->GetDisplayMode( 0, &DisplayMode );
   font->m_rcWindow.x1 = 0;
   font->m_rcWindow.y1 = 0;
   font->m_rcWindow.x2 = DisplayMode.Width;
   font->m_rcWindow.y2 = DisplayMode.Height;

   // Determine whether we should save/restore state
   font->m_bSaveState = TRUE;

   return 0;
}

HRESULT xdk360_console_init( LPCSTR strFontFileName, unsigned long colBackColor,
   unsigned long colTextColor)
{
   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
   D3DDevice *m_pd3dDevice = vid->d3d_render_device;

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
   HRESULT hr = xdk360_video_font_init(&m_Font, strFontFileName );
   if (hr < 0)
   {
      RARCH_ERR( "Could not create font.\n" );
      return -1;
   }

   // Save the colors
   video_console.m_colBackColor = colBackColor;
   video_console.m_colTextColor = colTextColor;

   // Calculate the number of lines on the screen
   float fCharWidth, fCharHeight;
   xdk360_video_font_get_text_width(&m_Font, L"i", &fCharWidth, &fCharHeight);

   video_console.m_cScreenHeight = (unsigned int)( video_console.m_cySafeArea / fCharHeight );
   video_console.m_cScreenWidth = (unsigned int)( video_console.m_cxSafeArea / fCharWidth );

   video_console.m_cScreenHeightVirtual = video_console.m_cScreenHeight;

   video_console.m_fLineHeight = fCharHeight;

   // Allocate memory to hold the lines
   video_console.m_Buffer = (wchar_t*)malloc(sizeof(wchar_t*) * video_console.m_cScreenHeightVirtual * ( video_console.m_cScreenWidth + 1 ));
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

static void xdk360_video_font_deinit(xdk360_video_font_t * font)
{
    font->m_pFontTexture = NULL;
    font->m_dwNumGlyphs = 0L;
    font->m_Glyphs = NULL;
    font->m_cMaxGlyph = 0;
    font->m_TranslatorTable = NULL;
    font->m_dwNestedBeginCount = 0L;

    // Safely release shaders
    // NOTE: They are released in reverse order of creation
    // to make sure any interdependencies are dealt with

    if( ( s_FontLocals.m_pFontPixelShader != NULL ) && ( s_FontLocals.m_pFontPixelShader->Release() == 0 ) )
        s_FontLocals.m_pFontPixelShader = NULL;
    if( ( s_FontLocals.m_pFontVertexShader != NULL ) && ( s_FontLocals.m_pFontVertexShader->Release() == 0 ) )
        s_FontLocals.m_pFontVertexShader = NULL;
    if( ( s_FontLocals.m_pFontVertexDecl != NULL ) && ( s_FontLocals.m_pFontVertexDecl->Release() == 0 ) )
        s_FontLocals.m_pFontVertexDecl = NULL;

    if( m_xprResource.m_bInitialized)
        m_xprResource.Destroy();
}

void xdk360_console_deinit(void)
{
   // Delete the memory we've allocated
   if(video_console.m_Lines)
   {
      delete[] video_console.m_Lines;
      video_console.m_Lines = NULL;
   }

   if(video_console.m_Buffer)
   {
      free(video_console.m_Buffer);
      video_console.m_Buffer = NULL;
   }

   // Destroy the font
   xdk360_video_font_deinit(&m_Font);
}

static void xdk360_console_add( wchar_t wch )
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
      video_console.m_Lines[ video_console.m_nCurLine ][ video_console.m_cCurLineLength ] = wch;

      float fTextWidth, fTextHeight;

      xdk360_video_font_get_text_width(&m_Font, video_console.m_Lines[ video_console.m_nCurLine ], &fTextWidth,
         &fTextHeight);

      if( fTextHeight > video_console.m_cxSafeArea )
      {
         // The line is too long, we need to wrap the character to the next line
         video_console.m_Lines[video_console.m_nCurLine][ video_console.m_cCurLineLength ] = L'\0';
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

void xdk360_console_format(const char * strFormat)
{
   video_console.m_nCurLine = 0;
   video_console.m_cCurLineLength = 0;

   memset( video_console.m_Buffer, 0, 
      video_console.m_cScreenHeightVirtual * 
      ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );

   // Output the string to the console
   unsigned long uStringLength = strlen( strFormat );
   for( unsigned long i = 0; i < uStringLength; i++ )
   {
      wchar_t wch;
	  rarch_convert_char_to_wchar(&wch, &strFormat[i], sizeof(wch));
      xdk360_console_add( wch );
   }
}

void xdk360_video_font_begin (xdk360_video_font_t * font)
{
   // Set state on the first call
   if( font->m_dwNestedBeginCount == 0 )
   {
      // Cache the global pointer into a register
      xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
      D3DDevice *pD3dDevice = vid->d3d_render_device;

      // Save state
      if( font->m_bSaveState )
      {
         pD3dDevice->GetRenderState( D3DRS_ALPHABLENDENABLE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHABLENDENABLE ] );
         pD3dDevice->GetRenderState( D3DRS_SRCBLEND, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_SRCBLEND ] );
		 pD3dDevice->GetRenderState( D3DRS_DESTBLEND, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_DESTBLEND ] );
		 pD3dDevice->GetRenderState( D3DRS_BLENDOP, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_BLENDOP ] );
		 pD3dDevice->GetRenderState( D3DRS_ALPHATESTENABLE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHATESTENABLE ] );
		 pD3dDevice->GetRenderState( D3DRS_ALPHAREF, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAREF ] );
		 pD3dDevice->GetRenderState( D3DRS_ALPHAFUNC, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAFUNC ] );
		 pD3dDevice->GetRenderState( D3DRS_FILLMODE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_FILLMODE ] );
		 pD3dDevice->GetRenderState( D3DRS_CULLMODE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_CULLMODE ] );
		 pD3dDevice->GetRenderState( D3DRS_VIEWPORTENABLE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_VIEWPORTENABLE ] );
		 font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MINFILTER ] = D3DDevice_GetSamplerState_MinFilter( pD3dDevice, 0 );
		 font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MAGFILTER ] = D3DDevice_GetSamplerState_MagFilter( pD3dDevice, 0 );
		 font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSU ] = D3DDevice_GetSamplerState_AddressU( pD3dDevice, 0);
		 font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSV ]  = D3DDevice_GetSamplerState_AddressV( pD3dDevice, 0);
      }

      // Set the texture scaling factor as a vertex shader constant
      D3DSURFACE_DESC TextureDesc;
      D3DTexture_GetLevelDesc(font->m_pFontTexture, 0, &TextureDesc); // Get the description

      // Set render state
      D3DDevice_SetTexture_Inline(pD3dDevice, 0, font->m_pFontTexture);

      // Read the TextureDesc here to ensure no load/hit/store from GetLevelDesc()
      float vTexScale[4];
      vTexScale[0] = 1.0f / TextureDesc.Width;		// LHS due to int->float conversion
      vTexScale[1] = 1.0f / TextureDesc.Height;
      vTexScale[2] = 0.0f;
      vTexScale[3] = 0.0f;

      D3DDevice_SetRenderState_AlphaBlendEnable( pD3dDevice, TRUE );
      D3DDevice_SetRenderState_SrcBlend(pD3dDevice, D3DBLEND_SRCALPHA );
      D3DDevice_SetRenderState_DestBlend( pD3dDevice, D3DBLEND_INVSRCALPHA );
      D3DDevice_SetRenderState_BlendOp( pD3dDevice, D3DBLENDOP_ADD );
      pD3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
      pD3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x08 );
      pD3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
      pD3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
      pD3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
      pD3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
      D3DDevice_SetSamplerState_MinFilter(pD3dDevice, 0, D3DTEXF_LINEAR );
      D3DDevice_SetSamplerState_MagFilter(pD3dDevice, 0, D3DTEXF_LINEAR );
      D3DDevice_SetSamplerState_AddressU_Inline(pD3dDevice, 0, D3DTADDRESS_CLAMP );
      D3DDevice_SetSamplerState_AddressV_Inline(pD3dDevice, 0, D3DTADDRESS_CLAMP );

      D3DDevice_SetVertexDeclaration(pD3dDevice, s_FontLocals.m_pFontVertexDecl );
      D3DDevice_SetVertexShader(pD3dDevice, s_FontLocals.m_pFontVertexShader );
      D3DDevice_SetPixelShader(pD3dDevice, s_FontLocals.m_pFontPixelShader );

      // Set the texture scaling factor as a vertex shader constant
      // Call here to avoid load hit store from writing to vTexScale above
      pD3dDevice->SetVertexShaderConstantF( 2, vTexScale, 1 );
   }

   // Keep track of the nested begin/end calls.
   font->m_dwNestedBeginCount++;
}

void xdk360_video_font_end(xdk360_video_font_t * font)
{
   if( --font->m_dwNestedBeginCount > 0 )
      return;

   // Restore state
   if( font->m_bSaveState )
   {
      // Cache the global pointer into a register
      xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
      D3DDevice *pD3dDevice = vid->d3d_render_device;

      D3DDevice_SetTexture_Inline(pD3dDevice, 0, NULL);
      D3DDevice_SetVertexDeclaration(pD3dDevice, NULL);
      D3DDevice_SetVertexShader(pD3dDevice, NULL );
      D3DDevice_SetPixelShader(pD3dDevice, NULL );
      D3DDevice_SetRenderState_AlphaBlendEnable(pD3dDevice, font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHABLENDENABLE ]);
      D3DDevice_SetRenderState_SrcBlend(pD3dDevice, font->m_dwSavedState[ SAVEDSTATE_D3DRS_SRCBLEND ] );
      D3DDevice_SetRenderState_DestBlend( pD3dDevice, font->m_dwSavedState[ SAVEDSTATE_D3DRS_DESTBLEND ] );
      D3DDevice_SetRenderState_BlendOp( pD3dDevice, font->m_dwSavedState[ SAVEDSTATE_D3DRS_BLENDOP ] );
      pD3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHATESTENABLE ] );
      pD3dDevice->SetRenderState( D3DRS_ALPHAREF, font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAREF ] );
      pD3dDevice->SetRenderState( D3DRS_ALPHAFUNC, font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAFUNC ] );
      pD3dDevice->SetRenderState( D3DRS_FILLMODE, font->m_dwSavedState[ SAVEDSTATE_D3DRS_FILLMODE ] );
      pD3dDevice->SetRenderState( D3DRS_CULLMODE, font->m_dwSavedState[ SAVEDSTATE_D3DRS_CULLMODE ] );
      pD3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, font->m_dwSavedState[ SAVEDSTATE_D3DRS_VIEWPORTENABLE ] );
      D3DDevice_SetSamplerState_MinFilter(pD3dDevice, 0, font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MINFILTER ] );
      D3DDevice_SetSamplerState_MagFilter(pD3dDevice, 0, font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MAGFILTER ] );
      D3DDevice_SetSamplerState_AddressU_Inline(pD3dDevice, 0, font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSU ] );
      D3DDevice_SetSamplerState_AddressV_Inline(pD3dDevice, 0, font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSV ] );
   }
}
