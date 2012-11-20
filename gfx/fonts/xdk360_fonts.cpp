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

#include <xtl.h>
#include "../../general.h"
#include "../../xdk/xdk_resources.h"

#define PAGE_UP               (255)
#define PAGE_DOWN             (-255)

#define SCREEN_SIZE_X_DEFAULT 640
#define SCREEN_SIZE_Y_DEFAULT 480

#define SAFE_AREA_PCT_4x3     85
#define SAFE_AREA_PCT_HDTV    70

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
   unsigned long m_dwSavedState[ SAVEDSTATE_COUNT ];
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
   D3DVertexDeclaration* m_pFontVertexDecl;
   D3DVertexShader* m_pFontVertexShader;
   D3DPixelShader* m_pFontPixelShader;
} Font_Locals_t;

static Font_Locals_t s_FontLocals;

static void xdk360_video_font_get_text_width(xdk360_video_font_t * font, const wchar_t * strText, float * pWidth, float * pHeight)
{
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

         xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
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

   // Create the font
   if( FAILED( m_xprResource.Create( strFontFileName ) ) )
      return E_FAIL;

   D3DTexture * pFontTexture = m_xprResource.GetTexture( "FontTexture" );
   const void * pFontData = m_xprResource.GetData( "FontData"); 

   // Save a copy of the texture
   font->m_pFontTexture = pFontTexture;

   // Check version of file (to make sure it matches up with the FontMaker tool)
   const unsigned char * pData = (const unsigned char*)pFontData;
   unsigned long dwFileVersion = ((const FontFileHeaderImage_t *)pData)->m_dwFileVersion;

   if( dwFileVersion == FONTFILEVERSION )
   {
      font->m_fFontHeight = ((const FontFileHeaderImage_t *)pData)->m_fFontHeight;
      font->m_fFontTopPadding = ((const FontFileHeaderImage_t *)pData)->m_fFontTopPadding;
      font->m_fFontBottomPadding = ((const FontFileHeaderImage_t *)pData)->m_fFontBottomPadding;
      font->m_fFontYAdvance = ((const FontFileHeaderImage_t *)pData)->m_fFontYAdvance;

      // Point to the translator string which immediately follows the 4 floats
      font->m_cMaxGlyph = ((const FontFileHeaderImage_t *)pData)->m_cMaxGlyph;

      font->m_TranslatorTable = const_cast<FontFileHeaderImage_t*>((const FontFileHeaderImage_t *)pData)->m_TranslatorTable;

      pData += CALCFONTFILEHEADERSIZE( font->m_cMaxGlyph + 1 );

      // Read the glyph attributes from the file
      font->m_dwNumGlyphs = ((const FontFileStrikesImage_t *)pData)->m_dwNumGlyphs;
      font->m_Glyphs = ((const FontFileStrikesImage_t *)pData)->m_Glyphs;        // Pointer
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

   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   D3DDevice *pd3dDevice = vid->d3d_render_device;

   // Initialize the window
   D3DDISPLAYMODE DisplayMode;
   pd3dDevice->GetDisplayMode( 0, &DisplayMode );
   font->m_rcWindow.x1 = 0;
   font->m_rcWindow.y1 = 0;
   font->m_rcWindow.x2 = DisplayMode.Width;
   font->m_rcWindow.y2 = DisplayMode.Height;

   return 0;
}

HRESULT d3d9_init_font(const char *font_path)
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   D3DDevice *m_pd3dDevice = vid->d3d_render_device;

   video_console.m_Buffer = NULL;
   video_console.m_Lines = NULL;
   video_console.m_nScrollOffset = 0;

   // Calculate the safe area
   unsigned int uiSafeAreaPct = g_extern.console.rmenu.state.rmenu_hd.enable 
	   ? SAFE_AREA_PCT_HDTV : SAFE_AREA_PCT_4x3;

   video_console.m_cxSafeArea = ( vid->win_width * uiSafeAreaPct ) / 100;
   video_console.m_cySafeArea = ( vid->win_height * uiSafeAreaPct ) / 100;

   video_console.m_cxSafeAreaOffset = ( vid->win_width - video_console.m_cxSafeArea ) / 2;
   video_console.m_cySafeAreaOffset = ( vid->win_height - video_console.m_cySafeArea ) / 2;

   // Create the font
   HRESULT hr = xdk360_video_font_init(&m_Font, font_path);
   if (hr < 0)
   {
      RARCH_ERR( "Could not create font.\n" );
      return -1;
   }

   // Calculate the number of lines on the screen
   float fCharWidth, fCharHeight;
   xdk360_video_font_get_text_width(&m_Font, L"i", &fCharWidth, &fCharHeight);

   video_console.m_cScreenHeight = (unsigned int)( video_console.m_cySafeArea / fCharHeight );
   video_console.m_cScreenWidth = (unsigned int)( video_console.m_cxSafeArea / fCharWidth );

   video_console.m_cScreenHeightVirtual = video_console.m_cScreenHeight;

   video_console.m_fLineHeight = fCharHeight;

   // Allocate memory to hold the lines
   video_console.m_Buffer = (wchar_t*)malloc(sizeof(wchar_t*) * video_console.m_cScreenHeightVirtual * ( video_console.m_cScreenWidth + 1 ));
   video_console.m_Lines = (wchar_t**)malloc( video_console.m_cScreenHeightVirtual * sizeof(wchar_t*));

   // Set the line pointers as indexes into the buffer
   for( unsigned int i = 0; i < video_console.m_cScreenHeightVirtual; i++ )
      video_console.m_Lines[ i ] = video_console.m_Buffer + ( video_console.m_cScreenWidth + 1 ) * i;

   video_console.m_nCurLine = 0;
   video_console.m_cCurLineLength = 0;
   memset( video_console.m_Buffer, 0, video_console.m_cScreenHeightVirtual * ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );

   return hr;
}

void d3d9_deinit_font(void)
{
   xdk360_video_font_t *font = &m_Font;

   // Delete the memory we've allocated
   if(video_console.m_Lines)
   {
      free(video_console.m_Lines);
      video_console.m_Lines = NULL;
   }

   if(video_console.m_Buffer)
   {
      free(video_console.m_Buffer);
      video_console.m_Buffer = NULL;
   }

   // Destroy the font
   font->m_pFontTexture = NULL;
   font->m_dwNumGlyphs = 0L;
   font->m_Glyphs = NULL;
   font->m_cMaxGlyph = 0;
   font->m_TranslatorTable = NULL;

   if( ( s_FontLocals.m_pFontPixelShader != NULL ) && ( s_FontLocals.m_pFontPixelShader->Release() == 0 ) )
      s_FontLocals.m_pFontPixelShader = NULL;
   if( ( s_FontLocals.m_pFontVertexShader != NULL ) && ( s_FontLocals.m_pFontVertexShader->Release() == 0 ) )
      s_FontLocals.m_pFontVertexShader = NULL;
   if( ( s_FontLocals.m_pFontVertexDecl != NULL ) && ( s_FontLocals.m_pFontVertexDecl->Release() == 0 ) )
      s_FontLocals.m_pFontVertexDecl = NULL;
   if( m_xprResource.Initialized())
      m_xprResource.Destroy();
}

void xdk_render_msg_post(xdk360_video_font_t * font)
{
   // Restore state
   {
      // Cache the global pointer into a register
      xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
      D3DDevice *pD3dDevice = vid->d3d_render_device;

      pD3dDevice->SetTexture(0, NULL);
      pD3dDevice->SetVertexDeclaration(NULL);
      D3DDevice_SetVertexShader(pD3dDevice, NULL );
      D3DDevice_SetPixelShader(pD3dDevice, NULL );
      D3DDevice_SetRenderState_AlphaBlendEnable(pD3dDevice, font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHABLENDENABLE ]);
      D3DDevice_SetRenderState_SrcBlend(pD3dDevice, font->m_dwSavedState[ SAVEDSTATE_D3DRS_SRCBLEND ] );
      D3DDevice_SetRenderState_DestBlend( pD3dDevice, font->m_dwSavedState[ SAVEDSTATE_D3DRS_DESTBLEND ] );
      pD3dDevice->SetRenderState( D3DRS_ALPHAREF, font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAREF ] );
      pD3dDevice->SetRenderState( D3DRS_ALPHAFUNC, font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAFUNC ] );
      pD3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, font->m_dwSavedState[ SAVEDSTATE_D3DRS_VIEWPORTENABLE ] );
      pD3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MINFILTER ]);
      pD3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MAGFILTER ]);
   }
}

static void xdk_render_msg_pre(xdk360_video_font_t * font)
{
   // Set state on the first call
      // Cache the global pointer into a register
      xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
      D3DDevice *pD3dDevice = vid->d3d_render_device;

      // Save state
      {
         pD3dDevice->GetRenderState( D3DRS_ALPHABLENDENABLE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHABLENDENABLE ] );
         pD3dDevice->GetRenderState( D3DRS_SRCBLEND, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_SRCBLEND ] );
         pD3dDevice->GetRenderState( D3DRS_DESTBLEND, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_DESTBLEND ] );
         pD3dDevice->GetRenderState( D3DRS_ALPHAREF, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAREF ] );
         pD3dDevice->GetRenderState( D3DRS_ALPHAFUNC, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAFUNC ] );
         pD3dDevice->GetRenderState( D3DRS_FILLMODE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_FILLMODE ] );
         pD3dDevice->GetRenderState( D3DRS_VIEWPORTENABLE, &font->m_dwSavedState[ SAVEDSTATE_D3DRS_VIEWPORTENABLE ] );
         font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MINFILTER ] = D3DDevice_GetSamplerState_MinFilter( pD3dDevice, 0 );
         font->m_dwSavedState[ SAVEDSTATE_D3DSAMP_MAGFILTER ] = D3DDevice_GetSamplerState_MagFilter( pD3dDevice, 0 );
      }

      // Set the texture scaling factor as a vertex shader constant
      D3DSURFACE_DESC TextureDesc;
      D3DTexture_GetLevelDesc(font->m_pFontTexture, 0, &TextureDesc); // Get the description

      // Set render state
      pD3dDevice->SetTexture(0, font->m_pFontTexture);

      // Read the TextureDesc here to ensure no load/hit/store from GetLevelDesc()
      float vTexScale[4];
      vTexScale[0] = 1.0f / TextureDesc.Width;		// LHS due to int->float conversion
      vTexScale[1] = 1.0f / TextureDesc.Height;
      vTexScale[2] = 0.0f;
      vTexScale[3] = 0.0f;

      D3DDevice_SetRenderState_AlphaBlendEnable( pD3dDevice, TRUE );
      D3DDevice_SetRenderState_SrcBlend(pD3dDevice, D3DBLEND_SRCALPHA );
      D3DDevice_SetRenderState_DestBlend( pD3dDevice, D3DBLEND_INVSRCALPHA );
      pD3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x08 );
      pD3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
      pD3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
      pD3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
      pD3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
      pD3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

      pD3dDevice->SetVertexDeclaration(s_FontLocals.m_pFontVertexDecl);
      D3DDevice_SetVertexShader(pD3dDevice, s_FontLocals.m_pFontVertexShader );
      D3DDevice_SetPixelShader(pD3dDevice, s_FontLocals.m_pFontPixelShader );

      // Set the texture scaling factor as a vertex shader constant
      // Call here to avoid load hit store from writing to vTexScale above
      pD3dDevice->SetVertexShaderConstantF( 2, vTexScale, 1 );
}

static void xdk_video_font_draw_text(xdk360_video_font_t *font, 
      float fOriginX, float fOriginY, const wchar_t * strText, float fMaxPixelWidth )
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   D3DDevice *pd3dDevice = vid->d3d_render_device;

   // Set the color as a vertex shader constant
   float vColor[4];
   vColor[0] = ( ( 0xffffffff & 0x00ff0000 ) >> 16L ) / 255.0F;
   vColor[1] = ( ( 0xffffffff & 0x0000ff00 ) >> 8L ) / 255.0F;
   vColor[2] = ( ( 0xffffffff & 0x000000ff ) >> 0L ) / 255.0F;
   vColor[3] = ( ( 0xffffffff & 0xff000000 ) >> 24L ) / 255.0F;

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
}

void xdk_render_msg(void *driver, const char * strFormat)
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver;

   video_console.m_nCurLine = 0;
   video_console.m_cCurLineLength = 0;

   memset( video_console.m_Buffer, 0, 
         video_console.m_cScreenHeightVirtual * 
         ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );

   // Output the string to the console
   unsigned long uStringLength = strlen(strFormat);

   for( unsigned long i = 0; i < uStringLength; i++ )
   {
      wchar_t wch;
      convert_char_to_wchar(&wch, &strFormat[i], sizeof(wch));

      // If this is a newline, just increment lines and move on
      if( wch == L'\n' )
      {
         video_console.m_nCurLine = ( video_console.m_nCurLine + 1 ) 
            % video_console.m_cScreenHeightVirtual;
         video_console.m_cCurLineLength = 0;
         memset(video_console.m_Lines[video_console.m_nCurLine], 0, 
               ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
         continue;
      }

      int bIncrementLine = FALSE;  // Whether to wrap to the next line

      if( video_console.m_cCurLineLength == video_console.m_cScreenWidth )
         bIncrementLine = TRUE;
      else
      {
         float fTextWidth, fTextHeight;
         // Try to append the character to the line
         video_console.m_Lines[ video_console.m_nCurLine ][ video_console.m_cCurLineLength ] = wch;
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
         memset( video_console.m_Lines[video_console.m_nCurLine], 0,
               ( video_console.m_cScreenWidth + 1 ) * sizeof( wchar_t ) );
         video_console.m_Lines[video_console.m_nCurLine ][0] = wch;
      }

      video_console.m_cCurLineLength++;
   }

   // The top line
   unsigned int nTextLine = ( video_console.m_nCurLine - 
         video_console.m_cScreenHeight + video_console.m_cScreenHeightVirtual - 
         video_console.m_nScrollOffset + 1 )
      % video_console.m_cScreenHeightVirtual;

   for( unsigned int nScreenLine = 0; nScreenLine < video_console.m_cScreenHeight; nScreenLine++ )
   {
	   const wchar_t *msg = video_console.m_Lines[nTextLine];
	   if (msg != NULL || msg[0] != L'\0')
	   {
		   xdk_render_msg_pre(&m_Font);
		   xdk_video_font_draw_text(&m_Font, (float)( video_console.m_cxSafeAreaOffset ),
            (float)( video_console.m_cySafeAreaOffset + video_console.m_fLineHeight * nScreenLine ), 
            msg, 0.0f );
		   xdk_render_msg_post(&m_Font);
	   }

      nTextLine = ( nTextLine + 1 ) % video_console.m_cScreenHeightVirtual;
   }
}


