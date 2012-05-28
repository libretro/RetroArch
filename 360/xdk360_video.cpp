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

// Xbox 360-specific headers
#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include "../driver.h"
#include "xdk360_video.h"
#include "xdk360_video_resources.h"

#ifdef HAVE_HLSL
#include "../gfx/shader_hlsl.h"
#endif

#include "./../gfx/gfx_context.h"
#include "../console/console_ext.h"
#include "../general.h"
#include "../message.h"
#include "shared.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static bool g_first_msg;

/* Xbox 360 specific code */

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

struct XPR_HEADER
{
   unsigned long dwMagic;
   unsigned long dwHeaderSize;
   unsigned long dwDataSize;
};

#define XPR2_MAGIC_VALUE (0x58505232)

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
   {
      if( !_stricmp( strName, m_pResourceTags[i].strName ) )
         return &m_pSysMemData[m_pResourceTags[i].dwOffset];
   }

   return NULL;
}

HRESULT PackedResource::Create( const char * strFilename )
{
    unsigned long dwNumBytesRead;
    HANDLE hFile = CreateFile( strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
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
    m_pSysMemData = (BYTE*)malloc(m_dwSysMemDataSize);
    if( m_pSysMemData == NULL )
    {
        RARCH_ERR( "Could not allocate system memory.\n" );
        m_dwSysMemDataSize = 0;
        return E_FAIL;
    }
    m_pVidMemData = ( BYTE* )XMemAlloc( m_dwVidMemDataSize, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
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

    return S_OK;
}

void PackedResource::Destroy()
{
    delete[] m_pSysMemData;
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

/* end of Xbox 360 specific code */

static void check_window(xdk360_video_t *d3d9)
{
   bool quit, resize;

   gfx_ctx_check_window(&quit,
         &resize, NULL, NULL,
         d3d9->frame_count);

   if (quit)
      d3d9->quitting = true;
   else if (resize)
      d3d9->should_resize = true;
}

static void xdk360_free(void * data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
	   return;
#endif

   xdk360_video_t *d3d9 = (xdk360_video_t*)data;

   if (!d3d9)
      return;

#ifdef HAVE_HLSL
   hlsl_deinit();
#endif
   d3d9->d3d_render_device->Release();
   d3d9->d3d_device->Release();

   free(d3d9);
}

static void xdk360_set_viewport(bool force_full)
{
   xdk360_video_t *d3d9 = (xdk360_video_t*)driver.video_data;

   d3d9->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   0xff000000, 1.0f, 0);

   int width = d3d9->video_mode.fIsHiDef ? 1280 : 640;
   int height = d3d9->video_mode.fIsHiDef ? 720 : 480;
   int m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   float m_zNear, m_zFar;

   m_viewport_x_temp = 0;
   m_viewport_y_temp = 0;
   m_viewport_width_temp = width;
   m_viewport_height_temp = height;

   m_zNear = 0.0f;
   m_zFar = 1.0f;

   if (!force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;
      float delta;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
      {
		 delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
      	 m_viewport_x_temp = g_console.viewports.custom_vp.x;
      	 m_viewport_y_temp = g_console.viewports.custom_vp.y;
      	 m_viewport_width_temp = g_console.viewports.custom_vp.width;
      	 m_viewport_height_temp = g_console.viewports.custom_vp.height;
      }
      else if (device_aspect > desired_aspect)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_x_temp = (int)(width * (0.5 - delta));
         m_viewport_width_temp = (int)(2.0 * width * delta);
         width = (unsigned)(2.0 * width * delta);
      }
      else
      {
         delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_y_temp = (int)(height * (0.5 - delta));
         m_viewport_height_temp = (int)(2.0 * height * delta);
         height = (unsigned)(2.0 * height * delta);
      }
   }

   D3DVIEWPORT9 vp = {0};
   vp.Width  = m_viewport_width_temp;
   vp.Height = m_viewport_height_temp;
   vp.X      = m_viewport_x_temp;
   vp.Y      = m_viewport_y_temp;
   vp.MinZ   = m_zNear;
   vp.MaxZ   = m_zFar;
   d3d9->d3d_render_device->SetViewport(&vp);

   //if(gl->overscan_enable && !force_full)
   //{
   //	m_left = -gl->overscan_amount/2;
   //	m_right = 1 + gl->overscan_amount/2;
   //	m_bottom = -gl->overscan_amount/2;
   //}
}

static void xdk360_set_rotation(void * data, unsigned orientation)
{
   (void)data;
   xdk360_video_t *d3d9 = (xdk360_video_t*)data;
   FLOAT angle;

   switch(orientation)
   {
      case ORIENTATION_NORMAL:
         angle = M_PI * 0 / 180;
	     break;
      case ORIENTATION_VERTICAL:
         angle = M_PI * 270 / 180;
         break;
      case ORIENTATION_FLIPPED:
         angle = M_PI * 180 / 180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         angle = M_PI * 90 / 180;
         break;
   }

   /* TODO: Move to D3DXMATRIX here */
   hlsl_set_proj_matrix(XMMatrixRotationZ(angle));

   d3d9->should_resize = TRUE;
}

static void xdk360_convert_texture_to_as16_srgb( D3DTexture *pTexture )
{
    pTexture->Format.SignX = GPUSIGN_GAMMA;
    pTexture->Format.SignY = GPUSIGN_GAMMA;
    pTexture->Format.SignZ = GPUSIGN_GAMMA;

    XGTEXTURE_DESC desc;
    XGGetTextureDesc( pTexture, 0, &desc );

    //convert to AS_16_16_16_16 format
    pTexture->Format.DataFormat = g_MapLinearToSrgbGpuFormat[ (desc.Format & D3DFORMAT_TEXTUREFORMAT_MASK) >> D3DFORMAT_TEXTUREFORMAT_SHIFT ];
}

void xdk360_init_fbo(xdk360_video_t *d3d9)
{
   if (d3d9->lpTexture_ot)
   {
      d3d9->lpTexture_ot->Release();
      d3d9->lpTexture_ot = NULL;
   }

   if (d3d9->lpSurface)
   {
      d3d9->lpSurface->Release();
      d3d9->lpSurface = NULL;
   }

   d3d9->d3d_render_device->CreateTexture(512 * g_settings.video.fbo_scale_x, 512 * g_settings.video.fbo_scale_y,
         1, 0, g_console.gamma_correction_enable ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8,
         0, &d3d9->lpTexture_ot, NULL);

   d3d9->d3d_render_device->CreateRenderTarget(512 * g_settings.video.fbo_scale_x, 512 * g_settings.video.fbo_scale_y,
         g_console.gamma_correction_enable ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 
	      0, 0, &d3d9->lpSurface, NULL);

   d3d9->lpTexture_ot_as16srgb = *d3d9->lpTexture_ot;
   xdk360_convert_texture_to_as16_srgb(d3d9->lpTexture);
   xdk360_convert_texture_to_as16_srgb(&d3d9->lpTexture_ot_as16srgb);
}

static void *xdk360_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (driver.video_data)
      return driver.video_data;

   xdk360_video_t *d3d9 = (xdk360_video_t*)calloc(1, sizeof(xdk360_video_t));
   if (!d3d9)
      return NULL;

   d3d9->d3d_device = Direct3DCreate9(D3D_SDK_VERSION);
   if (!d3d9->d3d_device)
   {
      free(d3d9);
      return NULL;
   }

   // Get video settings

   memset(&d3d9->video_mode, 0, sizeof(d3d9->video_mode));

   XGetVideoMode(&d3d9->video_mode);

   memset(&d3d9->d3dpp, 0, sizeof(d3d9->d3dpp));

   // no letterboxing in 4:3 mode (if widescreen is
   // unsupported
   if(!d3d9->video_mode.fIsWideScreen)
      d3d9->d3dpp.Flags |= D3DPRESENTFLAG_NO_LETTERBOX;
   
   d3d9->d3dpp.BackBufferWidth         = d3d9->video_mode.fIsHiDef ? 1280 : 640;
   d3d9->d3dpp.BackBufferHeight        = d3d9->video_mode.fIsHiDef ? 720 : 480;
   if(g_console.gamma_correction_enable)
   {
      d3d9->d3dpp.BackBufferFormat        = g_console.color_format ? (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8) : (D3DFORMAT)MAKESRGBFMT(D3DFMT_LIN_A1R5G5B5);
      d3d9->d3dpp.FrontBufferFormat       = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
   }
   else
   {
      d3d9->d3dpp.BackBufferFormat        = g_console.color_format ? D3DFMT_A8R8G8B8 : D3DFMT_LIN_A1R5G5B5;
      d3d9->d3dpp.FrontBufferFormat       = D3DFMT_LE_X8R8G8B8;
   }

   d3d9->d3dpp.MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3d9->d3dpp.MultiSampleQuality      = 0;
   d3d9->d3dpp.BackBufferCount         = 2;
   d3d9->d3dpp.EnableAutoDepthStencil  = FALSE;
   d3d9->d3dpp.SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3d9->d3dpp.PresentationInterval    = video->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

   // D3DCREATE_HARDWARE_VERTEXPROCESSING is ignored on 360
   d3d9->d3d_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
	   &d3d9->d3dpp, &d3d9->d3d_render_device);

   hlsl_init(g_settings.video.cg_shader_path, d3d9->d3d_render_device);

   d3d9->d3d_render_device->CreateTexture(512, 512, 1, 0, D3DFMT_LIN_X1R5G5B5,
      0, &d3d9->lpTexture, NULL);

   xdk360_init_fbo(d3d9);

   D3DLOCKED_RECT d3dlr;
   d3d9->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
   d3d9->lpTexture->UnlockRect(0);

   d3d9->last_width = 512;
   d3d9->last_height = 512;

   d3d9->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
	   0, 0, 0, &d3d9->vertex_buf, NULL);

   static const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 0.0f },
   };
   
   void *verts_ptr;
   d3d9->vertex_buf->Lock(0, 0, &verts_ptr, 0);
   memcpy(verts_ptr, init_verts, sizeof(init_verts));
   d3d9->vertex_buf->Unlock();

   static const D3DVERTEXELEMENT9 VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   d3d9->d3d_render_device->CreateVertexDeclaration(VertexElements, &d3d9->v_decl);
   
   d3d9->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   0xff000000, 1.0f, 0);

   d3d9->d3d_render_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   d3d9->d3d_render_device->SetRenderState(D3DRS_ZENABLE, FALSE);

   D3DVIEWPORT9 vp = {0};
   vp.Width  = d3d9->video_mode.fIsHiDef ? 1280 : 640;
   vp.Height = d3d9->video_mode.fIsHiDef ? 720 : 480;
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   d3d9->d3d_render_device->SetViewport(&vp);

   if(g_console.viewports.custom_vp.width == 0)
      g_console.viewports.custom_vp.width = vp.Width;

   if(g_console.viewports.custom_vp.height == 0)
      g_console.viewports.custom_vp.height = vp.Height;

   xdk360_set_rotation(d3d9, g_console.screen_orientation);

   d3d9->fbo_enabled = 1;
   d3d9->vsync = video->vsync;

   return d3d9;
}

static bool xdk360_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   xdk360_video_t *d3d9 = (xdk360_video_t*)data;
   D3DSurface* pRenderTarget0;
   bool menu_enabled = g_console.menu_enable;

   if (d3d9->last_width != width || d3d9->last_height != height)
   {
      D3DLOCKED_RECT d3dlr;

      d3d9->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
      d3d9->lpTexture->UnlockRect(0);

      float tex_w = width / 512.0f;
      float tex_h = height / 512.0f;

      DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, tex_w, 0.0f },
      };

      // Align texels and vertices (D3D9 quirk).
      for (unsigned i = 0; i < 4; i++)
      {
         verts[i].x -= 0.5f / 512.0f;
         verts[i].y += 0.5f / 512.0f;
      }

      void *verts_ptr;
      d3d9->vertex_buf->Lock(0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
      d3d9->vertex_buf->Unlock();

      d3d9->last_width = width;
      d3d9->last_height = height;
   }

   if (d3d9->fbo_enabled)
   {
      d3d9->d3d_render_device->GetRenderTarget(0, &pRenderTarget0);
      d3d9->d3d_render_device->SetRenderTarget(0, d3d9->lpSurface);
   }

   if (d3d9->should_resize)
      xdk360_set_viewport(false);

   d3d9->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         0xff000000, 1.0f, 0);
   d3d9->frame_count++;

   d3d9->d3d_render_device->SetTexture(0, d3d9->lpTexture);

   hlsl_use(1);
   if(d3d9->fbo_enabled)
   {
      hlsl_set_params(width, height, 512, 512, g_settings.video.fbo_scale_x * width,
            g_settings.video.fbo_scale_y * height, d3d9->frame_count);
      D3DVIEWPORT9 vp = {0};
      vp.Width  = g_settings.video.fbo_scale_x * width;
      vp.Height = g_settings.video.fbo_scale_y * height;
      vp.X      = 0;
      vp.Y      = 0;
      vp.MinZ   = 0.0f;
      vp.MaxZ   = 1.0f;
      d3d9->d3d_render_device->SetViewport(&vp);
   }
   else
   {
      hlsl_set_params(width, height, 512, 512, d3d9->d3dpp.BackBufferWidth,
            d3d9->d3dpp.BackBufferHeight, d3d9->frame_count);
   }

   D3DLOCKED_RECT d3dlr;
   d3d9->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   for (unsigned y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
      memcpy(out, in, width * sizeof(uint16_t));
   }
   d3d9->lpTexture->UnlockRect(0);

   d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

   d3d9->d3d_render_device->SetVertexDeclaration(d3d9->v_decl);
   d3d9->d3d_render_device->SetStreamSource(0, d3d9->vertex_buf, 0, sizeof(DrawVerticeFormats));

   d3d9->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

   if(d3d9->fbo_enabled)
   {
      d3d9->d3d_render_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, d3d9->lpTexture_ot,
            NULL, 0, 0, NULL, 0, 0, NULL);

      d3d9->d3d_render_device->SetRenderTarget(0, pRenderTarget0);
      pRenderTarget0->Release();
      d3d9->d3d_render_device->SetTexture(0, &d3d9->lpTexture_ot_as16srgb);

      hlsl_use(2);
      hlsl_set_params(g_settings.video.fbo_scale_x * width, g_settings.video.fbo_scale_y * height, g_settings.video.fbo_scale_x * 512, g_settings.video.fbo_scale_y * 512, d3d9->d3dpp.BackBufferWidth,
            d3d9->d3dpp.BackBufferHeight, d3d9->frame_count);
      xdk360_set_viewport(false);

      d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      d3d9->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      d3d9->d3d_render_device->SetVertexDeclaration(d3d9->v_decl);
      d3d9->d3d_render_device->SetStreamSource(0, d3d9->vertex_buf, 0, sizeof(DrawVerticeFormats));
      d3d9->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   }

   /* XBox 360 specific font code */
   if (msg && !menu_enabled)
   {
      if(IS_TIMER_EXPIRED() || g_first_msg)
      {
         xdk360_console_format(msg);
         g_first_msg = 0;
         SET_TIMER_EXPIRATION(30);
      }

      xdk360_console_draw();
   }

   if(!d3d9->block_swap)
      gfx_ctx_swap_buffers();

   return true;
}

static void xdk360_set_nonblock_state(void *data, bool state)
{
   xdk360_video_t *d3d9 = (xdk360_video_t*)data;

   if(d3d9->vsync)
   {
      RARCH_LOG("D3D Vsync => %s\n", state ? "off" : "on");
      gfx_ctx_set_swap_interval(state ? 0 : 1, TRUE);
   }
}

static bool xdk360_alive(void *data)
{
   xdk360_video_t *d3d9 = (xdk360_video_t*)data;
   check_window(d3d9);
   return !d3d9->quitting;
}

static bool xdk360_focus(void *data)
{
   (void)data;
   return gfx_ctx_window_has_focus();
}

// 360 needs a working graphics stack before RetroArch even starts.
// To deal with this main.c,
// the top level module owns the instance, and is created beforehand.
// When RetroArch gets around to init it, it is already allocated.
// When RetroArch wants to free it, it is ignored.
static void xdk360_start(void)
{
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.fullscreen = true;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;

   driver.video_data = xdk360_init(&video_info, NULL, NULL);

   xdk360_video_t *d3d9 = (xdk360_video_t*)driver.video_data;

   gfx_ctx_set_swap_interval(d3d9->vsync ? 1 : 0, false);

   g_first_msg = true;

   /* XBox 360 specific font code */
   HRESULT hr = xdk360_console_init("game:\\media\\Arial_12.xpr",
      0xff000000, 0xffffffff );

   if(FAILED(hr))
   {
      RARCH_ERR("Couldn't create debug console.\n");
   }
}

static void xdk360_restart(void)
{
}

static void xdk360_stop(void)
{
   void *data = driver.video_data;
   driver.video_data = NULL;
   xdk360_console_deinit();
   xdk360_free(data);
}

const video_driver_t video_xdk360 = {
   xdk360_init,
   xdk360_frame,
   xdk360_set_nonblock_state,
   xdk360_alive,
   xdk360_focus,
   NULL,
   xdk360_free,
   "xdk360",
   xdk360_start,
   xdk360_stop,
   xdk360_restart,
   xdk360_set_rotation,
};
