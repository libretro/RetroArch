/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include <xgraphics.h>
#include "../driver.h"
#include "xdk360_video.h"
#include "xdk360_video_resources.h"
#include "../gfx/shader_hlsl.h"
#include "../console/console_ext.h"
#include "../general.h"
#include "../message.h"
#include "shared.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static bool g_quitting;
static bool g_first_msg;
unsigned g_frame_count;
void *g_d3d;

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
        SSNES_ERR( "File <%s> not found.\n", strFilename );
        return E_FAIL;
    }

    // Read in and verify the XPR magic header
    XPR_HEADER xprh;
    if( !ReadFile( hFile, &xprh, sizeof( XPR_HEADER ), &dwNumBytesRead, NULL ) )
    {
        SSNES_ERR( "Error reading XPR header in file <%s>.\n", strFilename );
        CloseHandle( hFile );
        return E_FAIL;
    }

    if( xprh.dwMagic != XPR2_MAGIC_VALUE )
    {
        SSNES_ERR( "Invalid Xbox Packed Resource (.xpr) file: Magic = 0x%08lx.\n", xprh.dwMagic );
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
        SSNES_ERR( "Could not allocate system memory.\n" );
        m_dwSysMemDataSize = 0;
        return E_FAIL;
    }
    m_pVidMemData = ( BYTE* )XMemAlloc( m_dwVidMemDataSize, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
			    XALLOC_PHYSICAL_ALIGNMENT_4K, XALLOC_MEMPROTECT_WRITECOMBINE, 0, XALLOC_MEMTYPE_PHYSICAL ) );

    if( m_pVidMemData == NULL )
    {
        SSNES_ERR( "Could not allocate physical memory.\n" );
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
        SSNES_ERR( "Unable to read Xbox Packed Resource (.xpr) file.\n" );
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

static void xdk360_gfx_free(void * data)
{
   if (g_d3d)
	   return;

   xdk360_video_t *vid = (xdk360_video_t*)data;

   if (!vid)
      return;

   vid->lpTexture->Release();
   vid->vertex_buf->Release();
   vid->v_decl->Release();
   hlsl_deinit();
   vid->xdk360_render_device->Release();
   vid->xdk360_device->Release();

   free(vid);
}

static void set_viewport(bool force_full)
{
   xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
   vid->xdk360_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   0xff000000, 1.0f, 0);

   int width = vid->video_mode.fIsHiDef ? 1280 : 640;
   int height = vid->video_mode.fIsHiDef ? 720 : 480;
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
      //if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
      //{
      //	m_viewport_x_temp = g_console.custom_viewport_x;
      //	m_viewport_y_temp = g_console.custom_viewport_y;
      //	m_viewport_width_temp = g_console.custom_viewport_width;
      //	m_viewport_height_temp = g_console.custom_viewport_height;
      //}
      if (device_aspect > desired_aspect)
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
   vid->xdk360_render_device->SetViewport(&vp);

   //if(gl->overscan_enable && !force_full)
   //{
   //	m_left = -gl->overscan_amount/2;
   //	m_right = 1 + gl->overscan_amount/2;
   //	m_bottom = -gl->overscan_amount/2;
   //}
}

static void xdk360_set_orientation(void * data, uint32_t orientation)
{
   (void)data;
   xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
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

   hlsl_set_proj_matrix(XMMatrixRotationZ(angle));
}

static void xdk360_set_aspect_ratio(void * data, uint32_t aspectratio_index)
{
   (void)data;

   g_settings.video.aspect_ratio = aspectratio_lut[g_console.aspect_ratio_index].value;
   g_settings.video.force_aspect = false;
   set_viewport(false);
}

static void *xdk360_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (g_d3d)
      return g_d3d;

   xdk360_video_t *vid = (xdk360_video_t*)calloc(1, sizeof(xdk360_video_t));
   if (!vid)
      return NULL;

   vid->xdk360_device = Direct3DCreate9(D3D_SDK_VERSION);
   if (!vid->xdk360_device)
   {
      free(vid);
      return NULL;
   }

   // Get video settings

   memset(&vid->video_mode, 0, sizeof(vid->video_mode));

   XGetVideoMode(&vid->video_mode);

   memset(&vid->d3dpp, 0, sizeof(vid->d3dpp));

   // no letterboxing in 4:3 mode (if widescreen is
   // unsupported
   if(!vid->video_mode.fIsWideScreen)
      vid->d3dpp.Flags |= D3DPRESENTFLAG_NO_LETTERBOX;
   
   vid->d3dpp.BackBufferWidth         = vid->video_mode.fIsHiDef ? 1280 : 640;
   vid->d3dpp.BackBufferHeight        = vid->video_mode.fIsHiDef ? 720 : 480;
   vid->d3dpp.BackBufferFormat        = g_console.gamma_correction_enable ? (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8) : D3DFMT_A8R8G8B8;
   vid->d3dpp.FrontBufferFormat       = g_console.gamma_correction_enable ? (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8) : D3DFMT_LE_X8R8G8B8;
   vid->d3dpp.MultiSampleType         = D3DMULTISAMPLE_NONE;
   vid->d3dpp.MultiSampleQuality      = 0;
   vid->d3dpp.BackBufferCount         = 2;
   vid->d3dpp.EnableAutoDepthStencil  = FALSE;
   vid->d3dpp.SwapEffect              = D3DSWAPEFFECT_DISCARD;
   vid->d3dpp.PresentationInterval    = video->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

   // D3DCREATE_HARDWARE_VERTEXPROCESSING is ignored on 360
   vid->xdk360_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
	   &vid->d3dpp, &vid->xdk360_render_device);

   hlsl_init(g_settings.video.cg_shader_path, vid->xdk360_render_device);

   vid->xdk360_render_device->CreateTexture(512, 512, 1, 0, D3DFMT_LIN_X1R5G5B5,
      0, &vid->lpTexture, NULL);

   D3DLOCKED_RECT d3dlr;
   vid->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
   vid->lpTexture->UnlockRect(0);

   vid->last_width = 512;
   vid->last_height = 512;

   vid->xdk360_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
	   0, 0, 0, &vid->vertex_buf, NULL);

   static const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 0.0f },
   };
   
   void *verts_ptr;
   vid->vertex_buf->Lock(0, 0, &verts_ptr, 0);
   memcpy(verts_ptr, init_verts, sizeof(init_verts));
   vid->vertex_buf->Unlock();

   static const D3DVERTEXELEMENT9 VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   vid->xdk360_render_device->CreateVertexDeclaration(VertexElements, &vid->v_decl);
   
   vid->xdk360_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   0xff000000, 1.0f, 0);

   vid->xdk360_render_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   vid->xdk360_render_device->SetRenderState(D3DRS_ZENABLE, FALSE);

   D3DVIEWPORT9 vp = {0};
   vp.Width  = vid->video_mode.fIsHiDef ? 1280 : 640;
   vp.Height = vid->video_mode.fIsHiDef ? 720 : 480;
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   vid->xdk360_render_device->SetViewport(&vp);

   xdk360_set_orientation(NULL, g_console.screen_orientation);

   return vid;
}

static bool xdk360_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   xdk360_video_t *vid = (xdk360_video_t*)data;

   vid->xdk360_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   0xff000000, 1.0f, 0);
   g_frame_count++;

   if (vid->last_width != width || vid->last_height != height)
   {
      D3DLOCKED_RECT d3dlr;

      vid->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
      vid->lpTexture->UnlockRect(0);

      float tex_w = width / 512.0f;
      float tex_h = height / 512.0f;
	  
      const DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 0.0f,  tex_h },
	 {  1.0f, -1.0f, tex_w, tex_h },
	 { -1.0f,  1.0f, 0.0f,  0.0f },
	 {  1.0f,  1.0f, tex_w, 0.0f },
      };

      void *verts_ptr;
	  vid->vertex_buf->Lock(0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
	  vid->vertex_buf->Unlock();

      vid->last_width = width;
      vid->last_height = height;
   }

   hlsl_use(0);
   hlsl_set_params(width, height, 512, 512, vid->d3dpp.BackBufferWidth,
      vid->d3dpp.BackBufferHeight, g_frame_count);

   vid->xdk360_render_device->SetTexture(0, NULL);

   D3DLOCKED_RECT d3dlr;
   vid->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   for (unsigned y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
      memcpy(out, in, width * sizeof(uint16_t));
   }
   vid->lpTexture->UnlockRect(0);

   vid->xdk360_render_device->SetTexture(0, vid->lpTexture);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

   vid->xdk360_render_device->SetVertexDeclaration(vid->v_decl);
   vid->xdk360_render_device->SetStreamSource(0, vid->vertex_buf, 0, sizeof(DrawVerticeFormats));

   vid->xdk360_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   if (msg)
   {
      if(IS_TIMER_EXPIRED() || g_first_msg)
      {
         xdk360_console_format(msg);
         g_first_msg = 0;
         SET_TIMER_EXPIRATION(30);
      }
	   
      xdk360_console_draw();
   }

   if(!vid->block_swap)
      vid->xdk360_render_device->Present(NULL, NULL, NULL, NULL);

   return true;
}

static void xdk360_set_swap_block_swap (void * data, bool toggle)
{
   (void)data;
   xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
   vid->block_swap = toggle;

   if(toggle)
      SSNES_LOG("Swap is set to blocked.\n");
   else
      SSNES_LOG("Swap is set to non-blocked.\n");
}

static void xdk360_swap (void * data)
{
   (void)data;
   xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
   vid->xdk360_render_device->Present(NULL, NULL, NULL, NULL);
}

static void xdk360_gfx_set_nonblock_state(void *data, bool state)
{
   xdk360_video_t *vid = (xdk360_video_t*)data;
   SSNES_LOG("D3D Vsync => %s\n", state ? "off" : "on");
   if(state)
      vid->xdk360_render_device->SetRenderState(D3DRS_PRESENTINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
   else
      vid->xdk360_render_device->SetRenderState(D3DRS_PRESENTINTERVAL, D3DPRESENT_INTERVAL_ONE);
}

static bool xdk360_gfx_alive(void *data)
{
   (void)data;
   return !g_quitting;
}

static bool xdk360_gfx_focus(void *data)
{
   (void)data;
   return true;
}

void xdk360_video_set_vsync(bool vsync)
{
   xdk360_gfx_set_nonblock_state(g_d3d, vsync);
}

// 360 needs a working graphics stack before SSNESeven starts.
// To deal with this main.c,
// the top level module owns the instance, and is created beforehand.
// When SSNES gets around to init it, it is already allocated.
// When SSNES wants to free it, it is ignored.
void xdk360_video_init(void)
{
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;

   g_d3d = xdk360_gfx_init(&video_info, NULL, NULL);

   g_first_msg = true;

   HRESULT hr = xdk360_console_init("game:\\media\\Arial_12.xpr",
      0xff000000, 0xffffffff );

   if(FAILED(hr))
   {
      SSNES_ERR("Couldn't create debug console.\n");
   }
}

void xdk360_video_deinit(void)
{
   void *data = g_d3d;
   g_d3d = NULL;
   xdk360_console_deinit();
   xdk360_gfx_free(data);
}

const video_driver_t video_xdk360 = {
   xdk360_gfx_init,
   xdk360_gfx_frame,
   xdk360_gfx_set_nonblock_state,
   xdk360_gfx_alive,
   xdk360_gfx_focus,
   NULL,
   xdk360_gfx_free,
   "xdk360",
   xdk360_set_swap_block_swap,
   xdk360_swap,
   xdk360_set_aspect_ratio,
   xdk360_set_orientation,
};
