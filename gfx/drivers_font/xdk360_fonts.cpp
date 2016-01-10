/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../font_driver.h"
#include "../d3d/d3d.h"
#include "../../general.h"

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
#if defined(_XBOX1)
   char *strName;
   DWORD dwOffset;
#elif defined(_XBOX360)
   DWORD dwType;
   DWORD dwOffset;
   DWORD dwSize;
   char *strName;
#endif
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
      BYTE*       m_pSysMemData;        // Alloc'ed memory for resource headers etc.
      DWORD       m_dwSysMemDataSize;

      BYTE*       m_pVidMemData;        // Alloc'ed memory for resource data, etc.
      DWORD       m_dwVidMemDataSize;

      XBRESOURCE* m_pResourceTags;     // Tags to associate names with the resources
      DWORD       m_dwNumResourceTags; // Number of resource tags
      BOOL m_bInitialized;       // Resource is fully initialized

   public:
      // Loads the resources out of the specified bundle
#if defined(_XBOX1)
      HRESULT Create( const char *strFilename, DWORD dwNumResourceTags = 0L, 
            XBRESOURCE* pResourceTags = NULL );
#elif defined(_XBOX360)
      HRESULT Create( const char * strFilename );
#endif

      void Destroy();

      BOOL    Initialized() const;

#ifdef _XBOX360
      // Retrieves the resource tags
      void GetResourceTags( DWORD* pdwNumResourceTags, XBRESOURCE** ppResourceTags );
#endif

      // Helper function to make sure a resource is registered
      LPDIRECT3DRESOURCE RegisterResource( LPDIRECT3DRESOURCE pResource ) const
      {
#ifdef _XBOX1
         // Register the resource, if it has not yet been registered. We mark
         // a resource as registered by upping it's reference count.
         if( pResource && ( pResource->Common & D3DCOMMON_REFCOUNT_MASK ) == 1 )
         {
            // Special case CPU-copy push buffers (which live in system memory)
            if( ( pResource->Common & D3DCOMMON_TYPE_PUSHBUFFER ) &&
                  ( pResource->Common & D3DPUSHBUFFER_RUN_USING_CPU_COPY ) )
               pResource->Data += (DWORD)m_pSysMemData;
            else
               pResource->Register( m_pVidMemData );

            pResource->AddRef();
         }
#endif
         return pResource;
      }

      // Functions to retrieve resources by their offset
      void *GetData( DWORD dwOffset ) const
      { return &m_pSysMemData[dwOffset]; }

      LPDIRECT3DRESOURCE GetResource( DWORD dwOffset ) const
      { return RegisterResource( (LPDIRECT3DRESOURCE)GetData(dwOffset) ); }

      LPDIRECT3DTEXTURE GetTexture( DWORD dwOffset ) const
      { return (LPDIRECT3DTEXTURE)GetResource( dwOffset ); }

      LPDIRECT3DVERTEXBUFFER GetVertexBuffer( DWORD dwOffset ) const
      { return (LPDIRECT3DVERTEXBUFFER)GetResource( dwOffset ); }

      // Functions to retrieve resources by their name
      void *GetData( const char* strName ) const;

      LPDIRECT3DRESOURCE GetResource( const char* strName ) const
      { return RegisterResource( (LPDIRECT3DRESOURCE)GetData( strName ) ); }

      LPDIRECT3DTEXTURE GetTexture( const char* strName ) const
      { return (LPDIRECT3DTEXTURE)GetResource( strName ); }

      LPDIRECT3DVERTEXBUFFER GetVertexBuffer( const char* strName ) const
      { return (LPDIRECT3DVERTEXBUFFER)GetResource( strName ); }

      // Constructor/destructor
      PackedResource();
      ~PackedResource();
};

#define XPR0_MAGIC_VALUE 0x30525058
#define XPR1_MAGIC_VALUE 0x31525058
#define XPR2_MAGIC_VALUE 0x58505232

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

void *PackedResource::GetData(const char *strName) const
{
   if (m_pResourceTags == NULL || strName == NULL)
      return NULL;

#if defined(_XBOX1)
   for (DWORD i=0; m_pResourceTags[i].strName; i++)
#elif defined(_XBOX360)
      for (DWORD i = 0; i < m_dwNumResourceTags; i++)
#endif
      {
         if (!strcasecmp(strName, m_pResourceTags[i].strName))
            return &m_pSysMemData[m_pResourceTags[i].dwOffset];
      }

   return NULL;
}

static INLINE void* AllocateContiguousMemory(DWORD Size, DWORD Alignment)
{
#if defined(_XBOX1)
   return D3D_AllocContiguousMemory(Size, Alignment);
#elif defined(_XBOX360)
   return XMemAlloc(Size, MAKE_XALLOC_ATTRIBUTES(0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
            Alignment, XALLOC_MEMPROTECT_WRITECOMBINE, 0, XALLOC_MEMTYPE_PHYSICAL));
#endif
}

static INLINE void FreeContiguousMemory(void* pData)
{
#if defined(_XBOX1)
   return D3D_FreeContiguousMemory(pData);
#elif defined(_XBOX360)
   return XMemFree(pData, MAKE_XALLOC_ATTRIBUTES(0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
            0, 0, 0, XALLOC_MEMTYPE_PHYSICAL));
#endif
}

#ifdef _XBOX1
char g_strMediaPath[512] = "D:\\Media\\";

static HRESULT FindMediaFile(char *strPath, const char *strFilename, size_t strPathsize)
{
   if(strFilename == NULL || strPath == NULL)
      return E_INVALIDARG;

   strlcpy(strPath, strFilename, strPathsize);

   if(strFilename[1] != ':')
      snprintf(strPath, strPathsize, "%s%s", g_strMediaPath, strFilename);

   HANDLE hFile = CreateFile(strPath, GENERIC_READ, FILE_SHARE_READ, NULL, 
         OPEN_EXISTING, 0, NULL);

   if (hFile == INVALID_HANDLE_VALUE)
      return 0x82000004;

   CloseHandle(hFile);

   return S_OK;
}

#endif

#if defined(_XBOX1)
HRESULT PackedResource::Create(const char *strFilename,
      DWORD dwNumResourceTags, XBRESOURCE* pResourceTags)
#elif defined(_XBOX360)
HRESULT PackedResource::Create(const char *strFilename)
#endif
{
   unsigned i;
   HANDLE hFile;
   DWORD dwNumBytesRead;
   XPR_HEADER xprh;
   bool retval;
#ifdef _XBOX1
   BOOL bHasResourceOffsetsTable = FALSE;
   char strResourcePath[512];

   if (FAILED(FindMediaFile(strResourcePath, strFilename, sizeof(strResourcePath))))
      return E_FAIL;
   strFilename = strResourcePath;
#endif

   hFile = CreateFile(strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
         OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
   if (hFile == INVALID_HANDLE_VALUE)
      return E_FAIL;

   retval = ReadFile(hFile, &xprh, sizeof(XPR_HEADER), &dwNumBytesRead, NULL);

#if defined(_XBOX1)
   if(xprh.dwMagic == XPR0_MAGIC_VALUE)
      bHasResourceOffsetsTable = FALSE;
   else if(xprh.dwMagic == XPR1_MAGIC_VALUE)
      bHasResourceOffsetsTable = TRUE;
   else
#elif defined(_XBOX360)
      if(!retval)
      {
         CloseHandle(hFile);
         return E_FAIL;
      }

   if (xprh.dwMagic != XPR2_MAGIC_VALUE)
#endif
   {
      CloseHandle(hFile);
      return E_FAIL;
   }

   // Compute memory requirements
#if defined(_XBOX1)
   m_dwSysMemDataSize = xprh.dwHeaderSize - sizeof(XPR_HEADER);
   m_dwVidMemDataSize = xprh.dwTotalSize - xprh.dwHeaderSize;
#elif defined(_XBOX360)
   m_dwSysMemDataSize = xprh.dwHeaderSize;
   m_dwVidMemDataSize = xprh.dwDataSize;
#endif

   // Allocate memory
   m_pSysMemData = (BYTE*)malloc(m_dwSysMemDataSize);
   if (m_pSysMemData == NULL)
   {
      m_dwSysMemDataSize = 0;
      return E_FAIL;
   }

   m_pVidMemData = (BYTE*)AllocateContiguousMemory(m_dwVidMemDataSize,
#if defined(_XBOX1)
         D3DTEXTURE_ALIGNMENT
#elif defined(_XBOX360)
         XALLOC_PHYSICAL_ALIGNMENT_4K
#endif
     );

   if(m_pVidMemData == NULL)
   {
      m_dwSysMemDataSize = 0;
      m_dwVidMemDataSize = 0;
      free(m_pSysMemData);
      m_pSysMemData = NULL;
      return E_FAIL;
   }

   // Read in the data from the file
   if( !ReadFile( hFile, m_pSysMemData, m_dwSysMemDataSize, &dwNumBytesRead, NULL) ||
         !ReadFile( hFile, m_pVidMemData, m_dwVidMemDataSize, &dwNumBytesRead, NULL))
   {
      CloseHandle( hFile);
      return E_FAIL;
   }

   // Done with the file
   CloseHandle( hFile);

#ifdef _XBOX1
   if (bHasResourceOffsetsTable)
   {
#endif

      /* Extract resource table from the header data */
      m_dwNumResourceTags = *(DWORD*)(m_pSysMemData + 0);
      m_pResourceTags     = (XBRESOURCE*)(m_pSysMemData + 4);

      /* Patch up the resources */

      for(i = 0; i < m_dwNumResourceTags; i++)
      {
         m_pResourceTags[i].strName = (char*)(m_pSysMemData + (DWORD)m_pResourceTags[i].strName);
#ifdef _XBOX360
         if((m_pResourceTags[i].dwType & 0xffff0000) == (RESOURCETYPE_TEXTURE & 0xffff0000))
         {
            D3DTexture *pTexture = (D3DTexture*)&m_pSysMemData[m_pResourceTags[i].dwOffset];
            XGOffsetBaseTextureAddress(pTexture, m_pVidMemData, m_pVidMemData);
         }
#endif
      }

#ifdef _XBOX1
   }
#endif

#ifdef _XBOX1
   /* Use user-supplied number of resources and the resource tags */
   if(dwNumResourceTags != 0 || pResourceTags != NULL)
   {
      m_pResourceTags     = pResourceTags;
      m_dwNumResourceTags = dwNumResourceTags;
   }
#endif

   m_bInitialized = TRUE;

   return S_OK;
}

#ifdef _XBOX360
void PackedResource::GetResourceTags(DWORD* pdwNumResourceTags,
      XBRESOURCE** ppResourceTags)
{
   if (pdwNumResourceTags)
      (*pdwNumResourceTags) = m_dwNumResourceTags;

   if (ppResourceTags)
      (*ppResourceTags) = m_pResourceTags;
}
#endif

void PackedResource::Destroy()
{
   free(m_pSysMemData);
   m_pSysMemData = NULL;
   m_dwSysMemDataSize = 0L;

   if (m_pVidMemData != NULL)
      FreeContiguousMemory(m_pVidMemData);

   m_pVidMemData = NULL;
   m_dwVidMemDataSize = 0L;

   m_pResourceTags = NULL;
   m_dwNumResourceTags = 0L;

   m_bInitialized = FALSE;
}

BOOL PackedResource::Initialized() const
{
   return m_bInitialized;
}

#define FONT_SCALE(d3d) ((d3d->resolution_hd_enable) ? 2 : 1)

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
   D3DVertexShader *m_pFontVertexShader;
   D3DPixelShader *m_pFontPixelShader;
} Font_Locals_t;

typedef struct
{
   Font_Locals_t s_FontLocals;
   d3d_video_t *d3d;
   uint32_t m_dwSavedState;
   uint32_t m_cMaxGlyph;                /* Number of entries in the translator table. */
   uint32_t m_dwNumGlyphs;              /* Number of valid glyphs. */
   float m_fFontHeight;                 /* Height of the font strike in pixels. */
   float m_fFontTopPadding;             /* Padding above the strike zone. */
   float m_fFontBottomPadding;          /* Padding below the strike zone. */
   float m_fFontYAdvance;               /* Number of pixels to move the cursor for a line feed. */
   wchar_t * m_TranslatorTable;         /* ASCII to glyph lookup table. */
   D3DTexture* m_pFontTexture;
   const GLYPH_ATTR* m_Glyphs;          /* Array of glyphs. */
} xdk360_video_font_t;


#define CALCFONTFILEHEADERSIZE(x) ( sizeof(uint32_t) + (sizeof(float)* 4) + sizeof(uint16_t) + (sizeof(wchar_t)*(x)) )
#define FONTFILEVERSION 5

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

static PackedResource m_xprResource;

static HRESULT xdk360_video_font_create_shaders(xdk360_video_font_t * font)
{
   HRESULT hr;
   LPDIRECT3DDEVICE d3dr = font->d3d->dev;

   if (font->s_FontLocals.m_pFontVertexDecl)
   {
      font->s_FontLocals.m_pFontVertexDecl->AddRef();
      font->s_FontLocals.m_pFontVertexShader->AddRef();
      font->s_FontLocals.m_pFontPixelShader->AddRef();
      return 0;
   }

   do
   {
      static const D3DVERTEXELEMENT9 decl[] =
      {
         { 0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
         { 0,  8, D3DDECLTYPE_USHORT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
         { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
         D3DDECL_END()
      };


      hr = d3dr->CreateVertexDeclaration( decl, &font->s_FontLocals.m_pFontVertexDecl );

      if (hr >= 0)
      {
         ID3DXBuffer* pShaderCode;

         hr = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
               NULL, NULL, "main_vertex", "vs.2.0", 0,&pShaderCode, NULL, NULL );

         if (hr >= 0)
         {
            hr = d3dr->CreateVertexShader((const DWORD*)pShaderCode->GetBufferPointer(),
                  &font->s_FontLocals.m_pFontVertexShader );
            pShaderCode->Release();

            if (hr >= 0)
            {
               hr = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
                     NULL, NULL, "main_fragment", "ps.2.0", 0,&pShaderCode, NULL, NULL );

               if (hr >= 0)
               {
                  hr = d3dr->CreatePixelShader((DWORD*)pShaderCode->GetBufferPointer(),
                        &font->s_FontLocals.m_pFontPixelShader );
                  pShaderCode->Release();

                  if (hr >= 0) 
                  {
                     hr = 0;
                     break;
                  }
               }
               font->s_FontLocals.m_pFontVertexShader->Release();
            }

            font->s_FontLocals.m_pFontVertexShader = NULL;
         }

         font->s_FontLocals.m_pFontVertexDecl->Release();
      }  
      font->s_FontLocals.m_pFontVertexDecl = NULL;
   }while(0);

   return hr;
}

static void *xdk360_init_font(void *video_data,
      const char *font_path, float font_size)
{
   uint32_t dwFileVersion;
   const void *pFontData      = NULL;
   D3DTexture *pFontTexture   = NULL;
   const uint8_t * pData      = NULL;
   xdk360_video_font_t *font  = (xdk360_video_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   (void)font_size;

   font->d3d                  = (d3d_video_t*)video_data;

   font->m_pFontTexture       = NULL;
   font->m_dwNumGlyphs        = 0L;
   font->m_Glyphs             = NULL;
   font->m_cMaxGlyph          = 0;
   font->m_TranslatorTable    = NULL;

   /* Create the font. */
   if (FAILED( m_xprResource.Create(font_path)))
      goto error;

   pFontTexture               = m_xprResource.GetTexture( "FontTexture" );
   pFontData                  = m_xprResource.GetData( "FontData"); 

   /* Save a copy of the texture. */
   font->m_pFontTexture       = pFontTexture;

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

   pData += CALCFONTFILEHEADERSIZE( font->m_cMaxGlyph + 1 );

   /* Read the glyph attributes from the file. */
   font->m_dwNumGlyphs        = ((const FontFileStrikesImage_t *)pData)->m_dwNumGlyphs;
   font->m_Glyphs             = ((const FontFileStrikesImage_t *)pData)->m_Glyphs;

   /* Create the vertex and pixel shaders for rendering the font */
   if (FAILED(xdk360_video_font_create_shaders(font)))
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

static void xdk360_free_font(void *data)
{
   xdk360_video_font_t *font = (xdk360_video_font_t*)data;

   if (!font)
      return;

   /* Destroy the font */
   font->m_pFontTexture = NULL;
   font->m_dwNumGlyphs = 0L;
   font->m_Glyphs = NULL;
   font->m_cMaxGlyph = 0;
   font->m_TranslatorTable = NULL;

   if (font->s_FontLocals.m_pFontPixelShader)
      font->s_FontLocals.m_pFontPixelShader->Release();
   if (font->s_FontLocals.m_pFontVertexShader)
      font->s_FontLocals.m_pFontVertexShader->Release();
   if (font->s_FontLocals.m_pFontVertexDecl)
      font->s_FontLocals.m_pFontVertexDecl->Release();

   font->s_FontLocals.m_pFontPixelShader = NULL;
   font->s_FontLocals.m_pFontVertexShader = NULL;
   font->s_FontLocals.m_pFontVertexDecl = NULL;

   if (m_xprResource.Initialized())
      m_xprResource.Destroy();

   free(font);
   font = NULL;
}

static void xdk360_render_msg_post(xdk360_video_font_t * font)
{
   /* Cache the global pointer into a register */
   LPDIRECT3DDEVICE d3dr = font->d3d->dev;

   d3d_set_texture(d3dr, 0, NULL);
   d3dr->SetVertexDeclaration(NULL);
   d3d_set_vertex_shader(d3dr, 0, NULL);
   D3DDevice_SetPixelShader(d3dr, NULL);
   d3dr->SetRenderState( D3DRS_VIEWPORTENABLE, font->m_dwSavedState );
}

static void xdk360_render_msg_pre(xdk360_video_font_t * font)
{
   float vTexScale[4];
   D3DSURFACE_DESC TextureDesc;
   LPDIRECT3DDEVICE d3dr = font->d3d->dev;

   /* Save state. */
   d3dr->GetRenderState( D3DRS_VIEWPORTENABLE, (DWORD*)&font->m_dwSavedState );

   /* Set the texture scaling factor as a vertex shader constant. */
   D3DTexture_GetLevelDesc(font->m_pFontTexture, 0, &TextureDesc); // Get the description

   /* Set render state. */
   d3d_set_texture(d3dr, 0, font->m_pFontTexture);

   /* Read the TextureDesc here to ensure no load/hit/store from GetLevelDesc(). */
   vTexScale[0] = 1.0f / TextureDesc.Width;		/* LHS due to int->float conversion. */
   vTexScale[1] = 1.0f / TextureDesc.Height;
   vTexScale[2] = 0.0f;
   vTexScale[3] = 0.0f;

   d3dr->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
   d3dr->SetVertexDeclaration(font->s_FontLocals.m_pFontVertexDecl);
   d3d_set_vertex_shader(d3dr, 0, font->s_FontLocals.m_pFontVertexShader);
   d3dr->SetPixelShader(font->s_FontLocals.m_pFontPixelShader);

   /* Set the texture scaling factor as a vertex shader constant.
    * Call here to avoid load hit store from writing to vTexScale above
    */
   d3dr->SetVertexShaderConstantF( 2, vTexScale, 1 );
}

static void xdk360_draw_text(xdk360_video_font_t *font,
      float x, float y, const wchar_t * strText)
{
   uint32_t dwNumChars;
   volatile float *pVertex;
   float vColor[4], m_fCursorX, m_fCursorY;
   LPDIRECT3DDEVICE d3dr = font->d3d->dev;

   /* Set the color as a vertex shader constant. */
   vColor[0] = ((0xffffffff & 0x00ff0000) >> 16L) / 255.0f;
   vColor[1] = ((0xffffffff & 0x0000ff00) >> 8L)  / 255.0f;
   vColor[2] = ((0xffffffff & 0x000000ff) >> 0L)  / 255.0f;
   vColor[3] = ((0xffffffff & 0xff000000) >> 24L) / 255.0f;

   /* Perform the actual storing of the color constant here to prevent
    * a load-hit-store by inserting work between the store and the use of
    * the vColor array. */
   d3dr->SetVertexShaderConstantF(1, vColor, 1);

   m_fCursorX = floorf(x);
   m_fCursorY = floorf(y);

   /* Adjust for padding. */
   y -= font->m_fFontTopPadding;

   /* Begin drawing the vertices
    * Declared as volatile to force writing in ascending
    * address order.
    *
    * It prevents out of sequence writing in write combined
    * memory.
    */

   dwNumChars = wcslen(strText);
   d3dr->BeginVertices(D3DPT_QUADLIST, 4 * dwNumChars,
         sizeof(XMFLOAT4), (void**)&pVertex);

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
         pGlyph = &font->m_Glyphs[font->m_TranslatorTable[letter]];
      else
         pGlyph = &font->m_Glyphs[0];

      fOffset  = FONT_SCALE(font->d3d) * (float)pGlyph->wOffset;
      fAdvance = FONT_SCALE(font->d3d) * (float)pGlyph->wAdvance;
      fWidth   = FONT_SCALE(font->d3d) * (float)pGlyph->wWidth;
      fHeight  = FONT_SCALE(font->d3d) * font->m_fFontHeight;

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
#ifdef MSB_FIRST
      ((volatile uint32_t *)pVertex)[2]  = (tu1 << 16) | tv1;         // Merged using big endian rules
      ((volatile uint32_t *)pVertex)[6]  = (tu2 << 16) | tv1;         // Merged using big endian rules
      ((volatile uint32_t*)pVertex)[10] = (tu2 << 16) | tv2;        // Merged using big endian rules
      ((volatile uint32_t*)pVertex)[14] = (tu1 << 16) | tv2;        // Merged using big endian rules
#endif
      pVertex[15] = 0;
      pVertex += 16;

      m_fCursorX += fAdvance;

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

   d3dr->EndVertices();
}

static void xdk360_render_msg(void *data, const char *str_msg,
      const void *userdata)
{
   float x, y;
   wchar_t msg[PATH_MAX_LENGTH];
   xdk360_video_font_t *font        = (xdk360_video_font_t*)data;
   const struct font_params *params = (const struct font_params*)userdata;

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
