/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "d3d_font.h"
#include "../gfx_common.h"
#include "../../general.h"
#include "../../xdk/xdk_resources.h"

#define FONT_SCALE ((g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_HD)) ? 2 : 1)

typedef struct GLYPH_ATTR
{
   unsigned short tu1, tv1, tu2, tv2;   // Texture coordinates for the image
   short wOffset;                       // Pixel offset for glyph start
   short wWidth;                        // Pixel width of the glyph
   short wAdvance;                      // Pixels to advance after the glyph
   unsigned short wMask;
} GLYPH_ATTR;

typedef struct
{
   unsigned long m_dwSavedState;
   unsigned long m_cMaxGlyph;           // Number of entries in the translator table
   unsigned long m_dwNumGlyphs;         // Number of valid glyphs
   float m_fFontHeight;                 // Height of the font strike in pixels
   float m_fFontTopPadding;             // Padding above the strike zone
   float m_fFontBottomPadding;          // Padding below the strike zone
   float m_fFontYAdvance;               // Number of pixels to move the cursor for a line feed
   wchar_t * m_TranslatorTable;         // ASCII to glyph lookup table
   D3DTexture* m_pFontTexture;
   const GLYPH_ATTR* m_Glyphs;          // Array of glyphs
} xdk360_video_font_t;

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
         LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)vid->d3d_render_device;

         hr = d3dr->CreateVertexDeclaration( decl, &s_FontLocals.m_pFontVertexDecl );

         if (hr >= 0)
         {
            ID3DXBuffer* pShaderCode;

            hr = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
                  NULL, NULL, "main_vertex", "vs.2.0", 0,&pShaderCode, NULL, NULL );

            if (hr >= 0)
            {
               hr = d3dr->CreateVertexShader( ( unsigned long * )pShaderCode->GetBufferPointer(),
                     &s_FontLocals.m_pFontVertexShader );
               pShaderCode->Release();

               if (hr >= 0)
               {
                  hr = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
                        NULL, NULL, "main_fragment", "ps.2.0", 0,&pShaderCode, NULL, NULL );

                  if (hr >= 0)
                  {
                     hr = d3dr->CreatePixelShader((DWORD*)pShaderCode->GetBufferPointer(),
                           &s_FontLocals.m_pFontPixelShader );
                     pShaderCode->Release();

                     if (hr >= 0) 
                     {
                        hr = 0;
                        break;
                     }
                  }
                  s_FontLocals.m_pFontVertexShader->Release();
               }

               s_FontLocals.m_pFontVertexShader = NULL;
            }

            s_FontLocals.m_pFontVertexDecl->Release();
         }  
         s_FontLocals.m_pFontVertexDecl = NULL;
      }while (0);
   }
   else
   {
      s_FontLocals.m_pFontVertexDecl->AddRef();
      s_FontLocals.m_pFontVertexShader->AddRef();
      s_FontLocals.m_pFontPixelShader->AddRef();
      hr = 0;
   }
   return hr;
}

static bool xdk_init_font(void *data, const char *font_path, unsigned font_size)
{
   (void)font_size;

   // Create the font
   xdk360_video_font_t *font = &m_Font;

   font->m_pFontTexture = NULL;
   font->m_dwNumGlyphs = 0L;
   font->m_Glyphs = NULL;
   font->m_cMaxGlyph = 0;
   font->m_TranslatorTable = NULL;

   // Create the font
   if (FAILED( m_xprResource.Create(font_path)))
      goto error;

   D3DTexture *pFontTexture = m_xprResource.GetTexture( "FontTexture" );
   const void *pFontData    = m_xprResource.GetData( "FontData"); 

   // Save a copy of the texture
   font->m_pFontTexture = pFontTexture;

   // Check version of file (to make sure it matches up with the FontMaker tool)
   const unsigned char * pData = (const unsigned char*)pFontData;
   unsigned long dwFileVersion = ((const FontFileHeaderImage_t *)pData)->m_dwFileVersion;

   if (dwFileVersion == FONTFILEVERSION)
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
      goto error;
   }

   // Create the vertex and pixel shaders for rendering the font
   if (FAILED(xdk360_video_font_create_shaders(font)))
   {
      RARCH_ERR( "Could not create font shaders.\n" );
      goto error;
   }

   RARCH_LOG("Successfully initialized D3D9 HLSL fonts.\n");
   return true;
error:
   RARCH_ERR("Could not initialize D3D9 HLSL fonts.\n");
   return false;
}

static void xdk_deinit_font(void *data)
{
   xdk360_video_font_t *font = &m_Font;

   // Destroy the font
   font->m_pFontTexture = NULL;
   font->m_dwNumGlyphs = 0L;
   font->m_Glyphs = NULL;
   font->m_cMaxGlyph = 0;
   font->m_TranslatorTable = NULL;

   if (s_FontLocals.m_pFontPixelShader != NULL)
   {
      s_FontLocals.m_pFontPixelShader->Release();
      s_FontLocals.m_pFontPixelShader = NULL;
   }

   if (s_FontLocals.m_pFontVertexShader != NULL)
   {
      s_FontLocals.m_pFontVertexShader->Release();
      s_FontLocals.m_pFontVertexShader = NULL;
   }

   if (s_FontLocals.m_pFontVertexDecl != NULL)
   {
      s_FontLocals.m_pFontVertexDecl->Release();
      s_FontLocals.m_pFontVertexDecl = NULL;
   }

   if (m_xprResource.Initialized())
      m_xprResource.Destroy();
}

void xdk_render_msg_post(xdk360_video_font_t * font)
{
   // Cache the global pointer into a register
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;

   d3dr->SetTexture(0, NULL);
   d3dr->SetVertexDeclaration(NULL);
   D3DDevice_SetVertexShader(d3dr, NULL);
   D3DDevice_SetPixelShader(d3dr, NULL);
   d3dr->SetRenderState( D3DRS_VIEWPORTENABLE, font->m_dwSavedState );
}

static void xdk_render_msg_pre(xdk360_video_font_t * font)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;

   // Save state
   d3dr->GetRenderState( D3DRS_VIEWPORTENABLE, &font->m_dwSavedState );

   // Set the texture scaling factor as a vertex shader constant
   D3DSURFACE_DESC TextureDesc;
   D3DTexture_GetLevelDesc(font->m_pFontTexture, 0, &TextureDesc); // Get the description

   // Set render state
   d3dr->SetTexture(0, font->m_pFontTexture);

   // Read the TextureDesc here to ensure no load/hit/store from GetLevelDesc()
   float vTexScale[4];
   vTexScale[0] = 1.0f / TextureDesc.Width;		// LHS due to int->float conversion
   vTexScale[1] = 1.0f / TextureDesc.Height;
   vTexScale[2] = 0.0f;
   vTexScale[3] = 0.0f;

   d3dr->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
   d3dr->SetVertexDeclaration(s_FontLocals.m_pFontVertexDecl);
   d3dr->SetVertexShader(s_FontLocals.m_pFontVertexShader);
   d3dr->SetPixelShader(s_FontLocals.m_pFontPixelShader);

   // Set the texture scaling factor as a vertex shader constant
   // Call here to avoid load hit store from writing to vTexScale above
   d3dr->SetVertexShaderConstantF( 2, vTexScale, 1 );
}

static void xdk_video_font_draw_text(xdk360_video_font_t *font, 
      float x, float y, const wchar_t * strText)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;

   // Set the color as a vertex shader constant
   float vColor[4];
   vColor[0] = ( ( 0xffffffff & 0x00ff0000 ) >> 16L ) / 255.0f;
   vColor[1] = ( ( 0xffffffff & 0x0000ff00 ) >> 8L )  / 255.0f;
   vColor[2] = ( ( 0xffffffff & 0x000000ff ) >> 0L )  / 255.0f;
   vColor[3] = ( ( 0xffffffff & 0xff000000 ) >> 24L ) / 255.0f;

   // Perform the actual storing of the color constant here to prevent
   // a load-hit-store by inserting work between the store and the use of
   // the vColor array.
   d3dr->SetVertexShaderConstantF( 1, vColor, 1 );

   float m_fCursorX = floorf(x);
   float m_fCursorY = floorf(y);

   // Adjust for padding
   y -= font->m_fFontTopPadding;

   // Begin drawing the vertices

   // Declared as volatile to force writing in ascending
   // address order. It prevents out of sequence writing in write combined
   // memory.

   volatile float * pVertex;

   unsigned long dwNumChars = wcslen(strText);
   d3dr->BeginVertices(D3DPT_QUADLIST, 4 * dwNumChars, sizeof(XMFLOAT4), (void**)&pVertex);

   // Draw four vertices for each glyph
   while (*strText)
   {
      // Get the current letter in the string
      wchar_t letter = *strText++;

      if (letter == L'\n')
      {
         // Handle the newline character
         m_fCursorX = x;
         m_fCursorY += font->m_fFontYAdvance * FONT_SCALE;
         continue;
      }

      // Translate unprintable characters
      const GLYPH_ATTR *pGlyph;

      if (letter <= font->m_cMaxGlyph)
         pGlyph = &font->m_Glyphs[font->m_TranslatorTable[letter]];
      else
         pGlyph = &font->m_Glyphs[0];

      float fOffset  = FONT_SCALE * (float)pGlyph->wOffset;
      float fAdvance = FONT_SCALE * (float)pGlyph->wAdvance;
      float fWidth   = FONT_SCALE * (float)pGlyph->wWidth;
      float fHeight  = FONT_SCALE * font->m_fFontHeight;

      m_fCursorX += fOffset;

      // Add the vertices to draw this glyph

      unsigned long tu1 = pGlyph->tu1;        // Convert shorts to 32 bit longs for in register merging
      unsigned long tv1 = pGlyph->tv1;
      unsigned long tu2 = pGlyph->tu2;
      unsigned long tv2 = pGlyph->tv2;

      // NOTE: The vertexes are 2 floats for the screen coordinates,
      // followed by two USHORTS for the u/vs of the character,
      // terminated with the ARGB 32 bit color.
      // This makes for 16 bytes per vertex data (Easier to read)
      // Second NOTE: The uvs are merged and written using a DWORD due
      // to the write combining hardware being only able to handle 32,
      // 64 and 128 writes. Never store to write combined memory with
      // 8 or 16 bit instructions. You've been warned.

      // Setup the vertex/screen coordinates

      pVertex[0]  = m_fCursorX;
      pVertex[1]  = m_fCursorY;
      pVertex[3]  = 0;
      pVertex[4]  = m_fCursorX + fWidth;
      pVertex[5]  = m_fCursorY;
      pVertex[7]  = 0;
      pVertex[8]  = m_fCursorX + fWidth;
      pVertex[9]  = m_fCursorY + fHeight;
      pVertex[11] = 0;
      pVertex[12] = m_fCursorX;
      pVertex[13] = m_fCursorY + fHeight;
#ifndef LSB_FIRST
      ((volatile unsigned long *)pVertex)[2]  = (tu1 << 16) | tv1;         // Merged using big endian rules
      ((volatile unsigned long *)pVertex)[6]  = (tu2 << 16) | tv1;         // Merged using big endian rules
      ((volatile unsigned long *)pVertex)[10] = (tu2 << 16) | tv2;        // Merged using big endian rules
      ((volatile unsigned long *)pVertex)[14] = (tu1 << 16) | tv2;        // Merged using big endian rules
#endif
      pVertex[15] = 0;
      pVertex += 16;

      m_fCursorX += fAdvance;

      dwNumChars--;
   }

   // Since we allocated vertex data space based on the string length, we now need to
   // add some dummy verts for any skipped characters (like newlines, etc.)
   while (dwNumChars)
   {
      for (unsigned i = 0; i < 16; i++)
         pVertex[i] = 0;

      pVertex += 16;
      dwNumChars--;
   }

   d3dr->EndVertices();
}

static void xdk_render_msg(void *driver, const char *str_msg, void *parms)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver;
   xdk360_video_font_t *font = &m_Font;
   font_params_t *params = (font_params_t*)parms;
   wchar_t msg[PATH_MAX];
   float x, y;

   if (params)
   {
      x = params->x;
      y = params->y;
   }
   else
   {
      x = (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_HD)) ? 160 : 100;
      y = 120;
   }

   mbstowcs(msg, str_msg, sizeof(msg) / sizeof(wchar_t));

   if (msg != NULL || msg[0] != L'\0')
   {
      xdk_render_msg_pre(font);
      xdk_video_font_draw_text(font, x, y, msg);
      xdk_render_msg_post(font);
   }
}

const d3d_font_renderer_t d3d_xbox360_font = {
   xdk_init_font,
   xdk_deinit_font,
   xdk_render_msg,
   "Xbox 360 fonts",
};
