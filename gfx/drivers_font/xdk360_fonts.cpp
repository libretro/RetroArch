/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include <string/stdstring.h>

#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"
#include "../font_driver.h"

#include "../drivers/d3d_shaders/font.hlsl.d3d9.h"

#define XPR0_MAGIC_VALUE 0x30525058
#define XPR1_MAGIC_VALUE 0x31525058
#define XPR2_MAGIC_VALUE 0x58505232

#define FONT_SCALE(d3d) ((d3d->resolution_hd_enable) ? 2 : 1)
#define CALCFONTFILEHEADERSIZE(x) ( sizeof(uint32_t) + (sizeof(float)* 4) + sizeof(uint16_t) + (sizeof(wchar_t)*(x)) )
#define FONTFILEVERSION 5

#ifdef _XBOX360
struct XPR_HEADER
{
   DWORD dwMagic;
   DWORD dwHeaderSize;
   DWORD dwDataSize;
};
#endif

/* structure member offsets matter */
struct XBRESOURCE
{
   DWORD dwType;
   DWORD dwOffset;
   DWORD dwSize;
   char *strName;
};

enum
{
   RESOURCETYPE_USERDATA       = ( ( 'U' << 24 ) | ( 'S' << 16 ) | ( 'E' << 8 ) | ( 'R' ) ),
   RESOURCETYPE_TEXTURE        = ( ( 'T' << 24 ) | ( 'X' << 16 ) | ( '2' << 8 ) | ( 'D' ) ),
   RESOURCETYPE_VERTEXBUFFER   = ( ( 'V' << 24 ) | ( 'B' << 16 ) | ( 'U' << 8 ) | ( 'F' ) ),
   RESOURCETYPE_INDEXBUFFER    = ( ( 'I' << 24 ) | ( 'B' << 16 ) | ( 'U' << 8 ) | ( 'F' ) ),
   RESOURCETYPE_EOF            = 0xffffffff
};

class PackedResource
{
   protected:
      BYTE*       m_pSysMemData;        /* Allocated memory for resource headers etc. */
      DWORD       m_dwSysMemDataSize;

      BYTE*       m_pVidMemData;        /* Allocated memory for resource data, etc. */
      DWORD       m_dwVidMemDataSize;

      XBRESOURCE* m_pResourceTags;     /* Tags to associate names with the resources */
      DWORD       m_dwNumResourceTags; /* Number of resource tags */

   public:
      /* Loads the resources out of the specified bundle */
      HRESULT Create(const char *strFilename, DWORD dwNumResourceTags,
            void *pResourceTags);

      void Destroy();

      BOOL m_bInitialized;             /* Resource is fully initialized */

      /* Functions to retrieve resources by their name */
      void *GetData( const char* strName );
      LPDIRECT3DTEXTURE9 *GetTexture(const char* strName);

      /* Constructor/destructor */
      PackedResource();
      ~PackedResource();
};

LPDIRECT3DTEXTURE9 *PackedResource::GetTexture(const char* strName)
{
   LPDIRECT3DRESOURCE9 pResource = (LPDIRECT3DRESOURCE9)GetData(strName);
   return (LPDIRECT3DTEXTURE9*)pResource;
}

PackedResource::PackedResource()
{
   m_pSysMemData       = NULL;
   m_pVidMemData       = NULL;
   m_pResourceTags     = NULL;
   m_dwSysMemDataSize  = 0L;
   m_dwVidMemDataSize  = 0L;
   m_dwNumResourceTags = 0L;
   m_bInitialized      = false;
}

PackedResource::~PackedResource()
{
   Destroy();
}

void *PackedResource::GetData(const char *strName)
{
   if (!m_pResourceTags || !strName)
      return NULL;

   for (DWORD i = 0; i < m_dwNumResourceTags; i++)
   {
      if (string_is_equal_noncase(strName, m_pResourceTags[i].strName))
         return &m_pSysMemData[m_pResourceTags[i].dwOffset];
   }

   return NULL;
}

static INLINE void* AllocateContiguousMemory(DWORD Size, DWORD Alignment)
{
   return XMemAlloc(Size, MAKE_XALLOC_ATTRIBUTES(0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
            Alignment, XALLOC_MEMPROTECT_WRITECOMBINE, 0, XALLOC_MEMTYPE_PHYSICAL));
}

static INLINE void FreeContiguousMemory(void* pData)
{
   return XMemFree(pData, MAKE_XALLOC_ATTRIBUTES(0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
            0, 0, 0, XALLOC_MEMTYPE_PHYSICAL));
}

HRESULT PackedResource::Create(const char *strFilename,
      DWORD dwNumResourceTags, void* pResourceTags)
{
   unsigned i;
   DWORD dwNumBytesRead;
   XPR_HEADER xprh;
   HANDLE hFile = CreateFile(strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
         OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);

   if (hFile == INVALID_HANDLE_VALUE)
      return E_FAIL;

   if (!ReadFile(hFile, &xprh, sizeof(XPR_HEADER), &dwNumBytesRead, NULL) ||
         xprh.dwMagic != XPR2_MAGIC_VALUE)
   {
      CloseHandle(hFile);
      return E_FAIL;
   }

   /* Compute memory requirements */
   m_dwSysMemDataSize = xprh.dwHeaderSize;
   m_dwVidMemDataSize = xprh.dwDataSize;

   /* Allocate memory */
   m_pSysMemData = (BYTE*)malloc(m_dwSysMemDataSize);

   if (!m_pSysMemData)
   {
      m_dwSysMemDataSize = 0;
      return E_FAIL;
   }

   m_pVidMemData = (BYTE*)AllocateContiguousMemory(m_dwVidMemDataSize,
         XALLOC_PHYSICAL_ALIGNMENT_4K
         );

   if(!m_pVidMemData)
   {
      m_dwSysMemDataSize = 0;
      m_dwVidMemDataSize = 0;
      free(m_pSysMemData);
      m_pSysMemData = NULL;
      return E_FAIL;
   }

   /* Read in the data from the file */
   if( !ReadFile( hFile, m_pSysMemData, m_dwSysMemDataSize, &dwNumBytesRead, NULL) ||
         !ReadFile( hFile, m_pVidMemData, m_dwVidMemDataSize, &dwNumBytesRead, NULL))
   {
      CloseHandle( hFile);
      return E_FAIL;
   }

   /* Done with the file */
   CloseHandle( hFile);

   /* Extract resource table from the header data */
   m_dwNumResourceTags = *(DWORD*)(m_pSysMemData + 0);
   m_pResourceTags     = (XBRESOURCE*)(m_pSysMemData + 4);

   /* Patch up the resources */

   for(i = 0; i < m_dwNumResourceTags; i++)
   {
      m_pResourceTags[i].strName = (char*)(m_pSysMemData + (DWORD)m_pResourceTags[i].strName);
      if((m_pResourceTags[i].dwType & 0xffff0000) == (RESOURCETYPE_TEXTURE & 0xffff0000))
      {
         D3DTexture *pTexture = (D3DTexture*)&m_pSysMemData[m_pResourceTags[i].dwOffset];
         XGOffsetBaseTextureAddress(pTexture, m_pVidMemData, m_pVidMemData);
      }
   }

   m_bInitialized = true;

   return S_OK;
}

void PackedResource::Destroy()
{
   free(m_pSysMemData);

   if (m_pVidMemData != NULL)
      FreeContiguousMemory(m_pVidMemData);

   m_pSysMemData       = NULL;
   m_pVidMemData       = NULL;
   m_pResourceTags     = NULL;
   m_dwSysMemDataSize  = 0L;
   m_dwVidMemDataSize  = 0L;
   m_dwNumResourceTags = 0L;

   m_bInitialized = false;
}

typedef struct GLYPH_ATTR
{
   uint16_t tu1, tv1, tu2, tv2;           /* Texture coordinates for the image. */
   int16_t wOffset;                       /* Pixel offset for glyph start. */
   int16_t wWidth;                        /* Pixel width of the glyph. */
   int16_t wAdvance;                      /* Pixels to advance after the glyph. */
   uint16_t wMask;
} GLYPH_ATTR;

typedef struct
{
   D3DVertexDeclaration *m_pFontVertexDecl;
   D3DVertexShader      *m_pFontVertexShader;
   D3DPixelShader       *m_pFontPixelShader;
} Font_Locals_t;

typedef struct
{
   Font_Locals_t s_FontLocals;
   d3d9_video_t *d3d;
   uint32_t m_dwSavedState;
   uint32_t m_cMaxGlyph;                /* Number of entries in the translator table. */
   uint32_t m_dwNumGlyphs;              /* Number of valid glyphs. */
   float m_fFontHeight;                 /* Height of the font strike in pixels. */
   float m_fFontTopPadding;             /* Padding above the strike zone. */
   float m_fFontBottomPadding;          /* Padding below the strike zone. */
   float m_fFontYAdvance;               /* Number of pixels to move the cursor for a line feed. */
   wchar_t * m_TranslatorTable;         /* ASCII to glyph lookup table. */
   LPDIRECT3DTEXTURE9 m_pFontTexture;
   const GLYPH_ATTR* m_Glyphs;          /* Array of glyphs. */
} xdk360_video_font_t;

typedef struct
{
   uint32_t m_dwFileVersion;            /* Version of the font file (Must match FONTFILEVERSION). */
   float m_fFontHeight;                 /* Height of the font strike in pixels. */
   float m_fFontTopPadding;             /* Padding above the strike zone. */
   float m_fFontBottomPadding;          /* Padding below the strike zone. */
   float m_fFontYAdvance;               /* Number of pixels to move the cursor for a line feed. */
   uint16_t m_cMaxGlyph;                /* Number of font characters (Should be an odd number to maintain DWORD Alignment). */
   wchar_t m_TranslatorTable[1];        /* ASCII to Glyph lookup table, NOTE: It's m_cMaxGlyph+1 in size. */
} FontFileHeaderImage_t;

typedef struct
{
   uint32_t m_dwNumGlyphs;              /* Size of font strike array (First entry is the unknown glyph). */
   GLYPH_ATTR m_Glyphs[1];              /* Array of font strike uv's etc... NOTE: It's m_dwNumGlyphs in size. */
} FontFileStrikesImage_t;

static PackedResource m_xprResource;

static bool xdk360_video_font_create_shaders(xdk360_video_font_t * font, LPDIRECT3DDEVICE9 dev)
{
   ID3DXBuffer* pShaderCode = NULL;

   static const D3DVERTEXELEMENT9 decl[] =
   {
      { 0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0,  8, D3DDECLTYPE_USHORT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
      D3DDECL_END()
   };

   if (font->s_FontLocals.m_pFontVertexDecl)
   {
      font->s_FontLocals.m_pFontVertexDecl->AddRef();
      font->s_FontLocals.m_pFontVertexShader->AddRef();
      font->s_FontLocals.m_pFontPixelShader->AddRef();
      return true;
   }

   if (!d3d9_vertex_declaration_new(dev, decl, (void**)&font->s_FontLocals.m_pFontVertexDecl))
      goto error;

   if (!d3d9x_compile_shader( font_hlsl_d3d9_program, sizeof(font_hlsl_d3d9_program)-1 ,
            NULL, NULL, "main_vertex", "vs.2.0", 0, &pShaderCode, NULL, NULL ))
      goto error;

   if (!d3d9_create_vertex_shader(dev, (const DWORD*)pShaderCode->GetBufferPointer(),
         (void**)&font->s_FontLocals.m_pFontVertexShader ))
      goto error;

   d3d9x_buffer_release(pShaderCode);

   if (!d3d9x_compile_shader(font_hlsl_d3d9_program, sizeof(font_hlsl_d3d9_program)-1 ,
            NULL, NULL, "main_fragment", "ps.2.0", 0,&pShaderCode, NULL, NULL ))
      goto error;

   if (!d3d9_create_pixel_shader(dev, (DWORD*)pShaderCode->GetBufferPointer(),
         (void**)&font->s_FontLocals.m_pFontPixelShader))
      goto error;

   d3d9x_buffer_release(pShaderCode);

   return true;

error:
   if (pShaderCode)
      d3d9x_buffer_release(pShaderCode);
   d3d9_free_pixel_shader((LPDIRECT3DDEVICE9)font->d3d->dev,  font->s_FontLocals.m_pFontPixelShader);
   d3d9_free_vertex_shader((LPDIRECT3DDEVICE9)font->d3d->dev, font->s_FontLocals.m_pFontVertexShader);
   d3d9_vertex_declaration_free(font->s_FontLocals.m_pFontVertexDecl);
   font->s_FontLocals.m_pFontPixelShader  = NULL;
   font->s_FontLocals.m_pFontVertexShader = NULL;
   font->s_FontLocals.m_pFontVertexDecl   = NULL;

   return false;
}

static void *xdk360_init_font(void *video_data,
      const char *font_path, float font_size,
       bool is_threaded)
{
   uint32_t dwFileVersion;
   const void *pFontData      = NULL;
   void *pFontTexture         = NULL;
   const uint8_t * pData      = NULL;
   xdk360_video_font_t *font  = (xdk360_video_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   (void)font_size;

   font->d3d                  = (d3d9_video_t*)video_data;

   font->m_pFontTexture       = NULL;
   font->m_dwNumGlyphs        = 0L;
   font->m_Glyphs             = NULL;
   font->m_cMaxGlyph          = 0;
   font->m_TranslatorTable    = NULL;

   /* Create the font. */
   if (FAILED( m_xprResource.Create(font_path, 0, NULL)))
      goto error;

   pFontTexture               = (LPDIRECT3DTEXTURE9)m_xprResource.GetTexture( "FontTexture" );
   pFontData                  = m_xprResource.GetData( "FontData");

   /* Save a copy of the texture. */
   font->m_pFontTexture       = (LPDIRECT3DTEXTURE9)pFontTexture;

   /* Check version of file (to make sure it matches up with the FontMaker tool). */
   pData                      = (const uint8_t*)pFontData;
   dwFileVersion              = ((const FontFileHeaderImage_t *)pData)->m_dwFileVersion;

   if (dwFileVersion != FONTFILEVERSION)
   {
      RARCH_ERR("Incorrect version number on font file.\n");
      goto error;
   }

   font->m_fFontHeight        = ((const FontFileHeaderImage_t *)pData)->m_fFontHeight;
   font->m_fFontTopPadding    = ((const FontFileHeaderImage_t *)pData)->m_fFontTopPadding;
   font->m_fFontBottomPadding = ((const FontFileHeaderImage_t *)pData)->m_fFontBottomPadding;
   font->m_fFontYAdvance      = ((const FontFileHeaderImage_t *)pData)->m_fFontYAdvance;

   /* Point to the translator string which immediately follows the 4 floats. */
   font->m_cMaxGlyph          = ((const FontFileHeaderImage_t *)pData)->m_cMaxGlyph;
   font->m_TranslatorTable    = const_cast<FontFileHeaderImage_t*>((const FontFileHeaderImage_t *)pData)->m_TranslatorTable;

   pData                     += CALCFONTFILEHEADERSIZE(font->m_cMaxGlyph + 1);

   /* Read the glyph attributes from the file. */
   font->m_dwNumGlyphs        = ((const FontFileStrikesImage_t *)pData)->m_dwNumGlyphs;
   font->m_Glyphs             = ((const FontFileStrikesImage_t *)pData)->m_Glyphs;

   /* Create the vertex and pixel shaders for rendering the font */
   if (!xdk360_video_font_create_shaders(font, (LPDIRECT3DDEVICE9)font->d3d->dev))
   {
      RARCH_ERR( "Could not create font shaders.\n" );
      goto error;
   }

   RARCH_LOG("Successfully initialized D3D9 HLSL fonts.\n");
   return font;
error:
   RARCH_ERR("Could not initialize D3D9 HLSL fonts.\n");
   if (font)
      free(font);
   return NULL;
}

static void xdk360_free_font(void *data, bool is_threaded)
{
   xdk360_video_font_t *font = (xdk360_video_font_t*)data;

   if (!font)
      return;

   /* Destroy the font */
   font->m_pFontTexture    = NULL;
   font->m_dwNumGlyphs     = 0L;
   font->m_Glyphs          = NULL;
   font->m_cMaxGlyph       = 0;
   font->m_TranslatorTable = NULL;

   d3d9_free_pixel_shader((LPDIRECT3DDEVICE9)font->d3d->dev, font->s_FontLocals.m_pFontPixelShader);
   d3d9_free_vertex_shader((LPDIRECT3DDEVICE9)font->d3d->dev, font->s_FontLocals.m_pFontVertexShader);
   d3d9_vertex_declaration_free(font->s_FontLocals.m_pFontVertexDecl);

   font->s_FontLocals.m_pFontPixelShader  = NULL;
   font->s_FontLocals.m_pFontVertexShader = NULL;
   font->s_FontLocals.m_pFontVertexDecl   = NULL;

   if (m_xprResource.m_bInitialized)
      m_xprResource.Destroy();

   free(font);
   font = NULL;
}

static void xdk360_render_msg_post(xdk360_video_font_t * font)
{
   LPDIRECT3DDEVICE9 dev;
   if (!font || !font->d3d)
      return;
   dev = (LPDIRECT3DDEVICE9)font->d3d->dev;

   if (!dev)
	   return;

   d3d9_set_texture(dev, 0, NULL);
   d3d9_set_vertex_declaration(dev, NULL);
   d3d9_set_vertex_shader(dev, NULL);
   d3d9_set_pixel_shader(dev, NULL);
   d3d9_set_render_state(dev, D3DRS_VIEWPORTENABLE, font->m_dwSavedState);
}

static void xdk360_render_msg_pre(xdk360_video_font_t * font)
{
   float vTexScale[4];
   D3DSURFACE_DESC TextureDesc;
   LPDIRECT3DDEVICE9 dev;

   if (!font || !font->d3d)
      return;

   dev = (LPDIRECT3DDEVICE9)font->d3d->dev;

   if (!dev)
	   return;

   /* Save state. */
   d3d9_get_render_state(font->d3d->dev, D3DRS_VIEWPORTENABLE,
         (DWORD*)&font->m_dwSavedState );

   /* Set the texture scaling factor as a vertex shader constant. */
   /* Get the description */
   d3d9_texture_get_level_desc(font->m_pFontTexture, 0, &TextureDesc);

   /* Set render state. */
   d3d9_set_texture(dev, 0, font->m_pFontTexture);

   vTexScale[0] = 1.0f / TextureDesc.Width;
   vTexScale[1] = 1.0f / TextureDesc.Height;
   vTexScale[2] = 0.0f;
   vTexScale[3] = 0.0f;

   d3d9_set_render_state(dev, D3DRS_VIEWPORTENABLE, FALSE);
   d3d9_set_vertex_declaration(dev, font->s_FontLocals.m_pFontVertexDecl);
   d3d9_set_vertex_shader(dev, font->s_FontLocals.m_pFontVertexShader);
   d3d9_set_pixel_shader(dev, font->s_FontLocals.m_pFontPixelShader);
   d3d9_set_vertex_shader_constantf(dev, 2, vTexScale, 1);
}

static void xdk360_draw_text(xdk360_video_font_t *font,
      float x, float y, const wchar_t * strText)
{
   uint32_t dwNumChars;
   float vColor[4], m_fCursorX, m_fCursorY;
   volatile float *pVertex = NULL;
   LPDIRECT3DDEVICE9 dev   = (LPDIRECT3DDEVICE9)font->d3d->dev;

   /* Set the color as a vertex shader constant. */
   vColor[0]   = ((0xffffffff & 0x00ff0000) >> 16L) / 255.0f;
   vColor[1]   = ((0xffffffff & 0x0000ff00) >> 8L)  / 255.0f;
   vColor[2]   = ((0xffffffff & 0x000000ff) >> 0L)  / 255.0f;
   vColor[3]   = ((0xffffffff & 0xff000000) >> 24L) / 255.0f;

   d3d9_set_vertex_shader_constantf(dev, 1, vColor, 1);

   m_fCursorX  = floorf(x);
   m_fCursorY  = floorf(y);

   /* Adjust for padding. */
   y          -= font->m_fFontTopPadding;

   /* Begin drawing the vertices
    * Declared as volatile to force writing in ascending
    * address order.
    *
    * It prevents out of sequence writing in write combined
    * memory.
    */

   dwNumChars = wcslen(strText);
#ifdef __cplusplus
   dev->BeginVertices(D3DPT_QUADLIST, 4 * dwNumChars,
         sizeof(XMFLOAT4), (void**)&pVertex);
#else
   D3DDevice_BeginVertices(dev, D3DPT_QUADLIST, 4 * dwNumChars,
         sizeof(XMFLOAT4), (void**)&pVertex);
#endif

   /* Draw four vertices for each glyph. */
   while (*strText)
   {
      float fOffset, fAdvance, fWidth, fHeight;
#ifdef MSB_FIRST
      uint32_t tu1, tu2, tv1, tv2;
#endif
      const GLYPH_ATTR *pGlyph;
      wchar_t letter = *strText++; /* Get the current letter in the string */

      /* Handle the newline character. */
      if (letter == L'\n')
      {
         m_fCursorX = x;
         m_fCursorY += font->m_fFontYAdvance * FONT_SCALE(font->d3d);
         continue;
      }

      /* Translate unprintable characters. */
      if (letter <= font->m_cMaxGlyph)
         pGlyph   = &font->m_Glyphs[font->m_TranslatorTable[letter]];
      else
         pGlyph   = &font->m_Glyphs[0];

      fOffset     = FONT_SCALE(font->d3d) * (float)pGlyph->wOffset;
      fAdvance    = FONT_SCALE(font->d3d) * (float)pGlyph->wAdvance;
      fWidth      = FONT_SCALE(font->d3d) * (float)pGlyph->wWidth;
      fHeight     = FONT_SCALE(font->d3d) * font->m_fFontHeight;

      m_fCursorX += fOffset;

      /* Add the vertices to draw this glyph. */

#ifdef MSB_FIRST
      /* Convert shorts to 32 bit longs for in register merging */
      tu1 = pGlyph->tu1;
      tv1 = pGlyph->tv1;
      tu2 = pGlyph->tu2;
      tv2 = pGlyph->tv2;
#endif

      /* NOTE: The vertexes are 2 floats for the screen coordinates,
       * followed by two USHORTS for the u/vs of the character,
       * terminated with the ARGB 32 bit color.
       *
       * This makes for 16 bytes per vertex data (Easier to read)
       *
       * Second NOTE: The U/V coordinates are merged and written
       * using a DWORD due to the write combining hardware
       * being only able to handle 32, 64 and 128 writes.
       *
       * Never store to write combined memory with 8 or 16bit
       * instructions. You've been warned.
       */

      /* Setup the vertex/screen coordinates */
      pVertex[0]                         = m_fCursorX;
      pVertex[1]                         = m_fCursorY;
      pVertex[3]                         = 0;
      pVertex[4]                         = m_fCursorX + fWidth;
      pVertex[5]                         = m_fCursorY;
      pVertex[7]                         = 0;
      pVertex[8]                         = m_fCursorX + fWidth;
      pVertex[9]                         = m_fCursorY + fHeight;
      pVertex[11]                        = 0;
      pVertex[12]                        = m_fCursorX;
      pVertex[13]                        = m_fCursorY + fHeight;
#ifdef MSB_FIRST
      ((volatile uint32_t *)pVertex)[2]  = (tu1 << 16) | tv1;       /* Merged using big endian rules */
      ((volatile uint32_t *)pVertex)[6]  = (tu2 << 16) | tv1;       /* Merged using big endian rules */
      ((volatile uint32_t*)pVertex)[10]  = (tu2 << 16) | tv2;        /* Merged using big endian rules */
      ((volatile uint32_t*)pVertex)[14]  = (tu1 << 16) | tv2;        /* Merged using big endian rules */
#endif
      pVertex[15]                        = 0;
      pVertex                           += 16;

      m_fCursorX                        += fAdvance;

      dwNumChars--;
   }

   /* Since we allocated vertex data space
    * based on the string length, we now need to
    * add some dummy verts for any skipped
    * characters (like newlines, etc.)
    */
   while (dwNumChars)
   {
      unsigned i;
      for (i = 0; i < 16; i++)
         pVertex[i] = 0;

      pVertex += 16;
      dwNumChars--;
   }

#ifdef __cplusplus
   dev->EndVertices();
#else
   D3DDevice_EndVertices(dev);
#endif
}

static void xdk360_render_msg(
      video_frame_info_t *video_info,
      void *data, const char *str_msg,
      const struct font_params *params)
{
   float x, y;
   wchar_t msg[PATH_MAX_LENGTH];
   xdk360_video_font_t *font        = (xdk360_video_font_t*)data;

   if (params)
   {
      x = params->x;
      y = params->y;
   }
   else
   {
      x = font->d3d->resolution_hd_enable ? 160 : 100;
      y = 120;
   }

   mbstowcs(msg, str_msg, sizeof(msg) / sizeof(wchar_t));

   if (msg || msg[0] != L'\0')
   {
      xdk360_render_msg_pre(font);
      xdk360_draw_text(font, x, y, msg);
      xdk360_render_msg_post(font);
   }
}

font_renderer_t d3d_xbox360_font = {
   xdk360_init_font,
   xdk360_free_font,
   xdk360_render_msg,
   "xdk360_fonts",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   NULL                       /* get_message_width */
};
