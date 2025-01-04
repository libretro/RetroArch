/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - OV2
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

/* Direct3D 8 driver.
 *
 * Minimum version : Direct3D 8.0 (2000)
 * Minimum OS      : Windows 95 (8.0a), Windows 98 and XP after 8.0a
 * Recommended OS  : Windows 98/ME and/or Windows XP
 */

#define CINTERFACE

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include <formats/image.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_math.h>

#include <d3d8.h>

#include <defines/d3d_defines.h>
#include "../common/d3d8_defines.h"
#include "../common/d3d_common.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../dynamic.h"
#include "../../frontend/frontend_driver.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../common/win32_common.h"

#ifdef _XBOX
#define D3D8_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../core.h"
#include "../../verbosity.h"

#ifdef __WINRT__
#error "UWP does not support D3D8"
#endif

#ifdef _XBOX
#define D3D8_RGB565_FORMAT D3DFMT_LIN_R5G6B5
#define D3D8_XRGB8888_FORMAT D3DFMT_LIN_X8R8G8B8
#define D3D8_ARGB8888_FORMAT D3DFMT_LIN_A8R8G8B8
#else
#define D3D8_RGB565_FORMAT D3DFMT_R5G6B5
#define D3D8_XRGB8888_FORMAT D3DFMT_X8R8G8B8
#define D3D8_ARGB8888_FORMAT D3DFMT_A8R8G8B8
#endif

typedef struct d3d8_renderchain
{
   unsigned pixel_size;
   LPDIRECT3DDEVICE8 dev;
   const video_info_t *video_info;
   LPDIRECT3DTEXTURE8 tex;
   LPDIRECT3DVERTEXBUFFER8 vertex_buf;
   unsigned last_width;
   unsigned last_height;
   void *vertex_decl;
   unsigned tex_w;
   unsigned tex_h;
   uint64_t frame_count;
} d3d8_renderchain_t;

struct d3d8_texture_info
{
   void *userdata;
   void *data;
   enum texture_filter_type type;
};

static const float d3d8_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float d3d8_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static LPDIRECT3D8 g_pD3D8;

void *dinput;

/*
 * D3D8 COMMON
 */

/* For Xbox we will just link statically
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
#endif

/* TODO/FIXME - static globals */
#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d8_dll;
static bool dylib_initialized = false;
#endif

typedef IDirect3D8 *(__stdcall *D3DCreate_t)(UINT);

static D3DCreate_t D3DCreate;

#ifdef HAVE_DYNAMIC_D3D
static void d3d8_deinitialize_symbols(void)
{
   if (g_d3d8_dll)
      dylib_close(g_d3d8_dll);
   g_d3d8_dll         = NULL;
   dylib_initialized = false;
}
#endif

static bool d3d8_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (dylib_initialized)
      return true;

#if defined(DEBUG) || defined(_DEBUG)
   if (!(g_d3d8_dll = dylib_load("d3d8d.dll")))
#endif
      g_d3d8_dll            = dylib_load("d3d8.dll");

   if (!g_d3d8_dll)
      return false;
   D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d8_dll, "Direct3DCreate8");
#else
   D3DCreate                = Direct3DCreate8;
#endif

   if (!D3DCreate)
      goto error;

#ifdef HAVE_DYNAMIC_D3D
   dylib_initialized        = true;
#endif

   return true;

error:
#ifdef HAVE_DYNAMIC_D3D
   d3d8_deinitialize_symbols();
#endif
   return false;
}


static INLINE void *
d3d8_vertex_buffer_lock(LPDIRECT3DVERTEXBUFFER8 vertbuf)
{
   void *buf = NULL;
   IDirect3DVertexBuffer8_Lock(vertbuf, 0, 0, (BYTE**)&buf, 0);
   return buf;
}

bool d3d8_reset(void *data, void *d3dpp)
{
   const char       *err = NULL;
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
   if (dev && IDirect3DDevice8_Reset(dev, (D3DPRESENT_PARAMETERS*)d3dpp) ==
         D3D_OK)
      return true;
#ifndef _XBOX
   RARCH_WARN("[D3D]: Attempting to recover from dead state...\n");
   /* Try to recreate the device completely. */
   switch (IDirect3DDevice8_TestCooperativeLevel(dev))
   {
      case D3DERR_DEVICELOST:
         err = "DEVICELOST";
         break;

      case D3DERR_DEVICENOTRESET:
         err = "DEVICENOTRESET";
         break;

      case D3DERR_DRIVERINTERNALERROR:
         err = "DRIVERINTERNALERROR";
         break;

      default:
         err = "Unknown";
   }
   RARCH_WARN("[D3D]: recovering from dead state: (%s).\n", err);
#endif
   return false;
}

static bool d3d8_create_device_internal(
      LPDIRECT3DDEVICE8 dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      LPDIRECT3D8 d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
   if (dev &&
         SUCCEEDED(IDirect3D8_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice8**)dev)))
      return true;

   return false;
}

static bool d3d8_create_device(void *dev,
      void *d3dpp,
      LPDIRECT3D8 d3d,
      HWND focus_window,
      unsigned cur_mon_id)
{
   if (!d3d8_create_device_internal(dev,
            (D3DPRESENT_PARAMETERS*)d3dpp,
            d3d,
            focus_window,
            cur_mon_id,
            D3DCREATE_HARDWARE_VERTEXPROCESSING))
      if (!d3d8_create_device_internal(
               dev,
               (D3DPRESENT_PARAMETERS*)d3dpp, d3d, focus_window,
               cur_mon_id,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING))
         return false;
   return true;
}

static void *d3d8_texture_new(LPDIRECT3DDEVICE8 dev,
      unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   void *buf             = NULL;
   if (SUCCEEDED(IDirect3DDevice8_CreateTexture(dev,
               width, height, miplevels, usage,
               (D3DFORMAT)format, (D3DPOOL)pool,
               (struct IDirect3DTexture8**)&buf)))
      return buf;
   return NULL;
}

static void d3d8_set_mvp(void *data, const void *mat_data)
{
   struct d3d_matrix matrix;
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)data;

   d3d_matrix_identity(&matrix);

   IDirect3DDevice8_SetTransform(d3dr,
         D3DTS_PROJECTION, (D3DMATRIX*)&matrix);
   IDirect3DDevice8_SetTransform(d3dr,
         D3DTS_VIEW, (D3DMATRIX*)&matrix);
   d3d_matrix_transpose(&matrix, mat_data);
   IDirect3DDevice8_SetTransform(d3dr, D3DTS_WORLD, (D3DMATRIX*)&matrix);
}

static void d3d8_set_vertices(
      d3d8_video_t *d3d,
      d3d8_renderchain_t *chain,
      unsigned pass,
      unsigned vert_width, unsigned vert_height, uint64_t frame_count)
{
   unsigned width, height;

   video_driver_get_size(&width, &height);

   if (chain->last_width != vert_width || chain->last_height != vert_height)
   {
      Vertex vert[4];
      void *verts        = NULL;
      float tex_w        = vert_width;
      float tex_h        = vert_height;

      chain->last_width  = vert_width;
      chain->last_height = vert_height;

      if (chain->vertex_buf)
      {
         LPDIRECT3DVERTEXBUFFER8 vbo;
         vert[0].x        =  0.0f;
         vert[0].y        =  1.0f;
         vert[0].z        =  1.0f;

         vert[1].x        =  1.0f;
         vert[1].y        =  1.0f;
         vert[1].z        =  1.0f;

         vert[2].x        =  0.0f;
         vert[2].y        =  0.0f;
         vert[2].z        =  1.0f;

         vert[3].x        =  1.0f;
         vert[3].y        =  0.0f;
         vert[3].z        =  1.0f;

         vert[0].u        = 0.0f;
         vert[0].v        = 0.0f;
         vert[1].v        = 0.0f;
         vert[2].u        = 0.0f;
         vert[1].u        = tex_w;
         vert[2].v        = tex_h;
         vert[3].u        = tex_w;
         vert[3].v        = tex_h;
#ifndef _XBOX
         vert[1].u       /= chain->tex_w;
         vert[2].v       /= chain->tex_h;
         vert[3].u       /= chain->tex_w;
         vert[3].v       /= chain->tex_h;
#endif

         vert[0].color    = 0xFFFFFFFF;
         vert[1].color    = 0xFFFFFFFF;
         vert[2].color    = 0xFFFFFFFF;
         vert[3].color    = 0xFFFFFFFF;

         vbo              = (LPDIRECT3DVERTEXBUFFER8)chain->vertex_buf;
         verts            = d3d8_vertex_buffer_lock(vbo);
         memcpy(verts, vert, sizeof(vert));
         IDirect3DVertexBuffer8_Unlock(vbo);
      }
   }
}

static void d3d8_blit_to_texture(
      d3d8_renderchain_t *chain,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   D3DLOCKED_RECT d3dlr;
   D3DLOCKED_RECT *lr         = &d3dlr;
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)chain->dev;
   LPDIRECT3DTEXTURE8 tex     = (LPDIRECT3DTEXTURE8)chain->tex;
#ifdef _XBOX
   global_t        *global    = global_get_ptr();
   D3DDevice_SetFlickerFilter(global->console.screen.flicker_filter_index);
   D3DDevice_SetSoftDisplayFilter(global->console.softfilter_enable);
#endif

   if (chain->last_width != width || chain->last_height != height)
   {
      if (IDirect3DTexture8_LockRect(tex, 0, lr,
               NULL, D3DLOCK_NOSYSLOCK) == D3D_OK)
      {
         memset(lr->pBits, 0, chain->tex_h * lr->Pitch);
         IDirect3DTexture8_UnlockRect((LPDIRECT3DTEXTURE8)tex, 0);
      }
   }

   /* Set the texture to NULL so D3D doesn't complain about it being in use... */
   IDirect3DDevice8_SetTexture(d3dr, 0, NULL);

   if (IDirect3DTexture8_LockRect(tex, 0, &d3dlr, NULL, 0) == D3D_OK)
   {
      unsigned y;
      unsigned pixel_size = chain->pixel_size;

      for (y = 0; y < height; y++)
      {
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t      *out = (uint8_t*)lr->pBits + y * lr->Pitch;
         memcpy(out, in, width * pixel_size);
      }
      IDirect3DTexture8_UnlockRect(tex, 0);
   }
}

static void d3d8_render(
      d3d8_video_t *d3d,
      LPDIRECT3DDEVICE8 d3dr,
      d3d8_renderchain_t *chain,
      const void *frame,
      unsigned frame_width, unsigned frame_height,
      unsigned pitch, unsigned rotation)
{
   settings_t *settings      = config_get_ptr();
   bool video_smooth         = settings->bools.video_smooth;

   d3d8_blit_to_texture(chain, frame, frame_width, frame_height, pitch);
   d3d8_set_vertices(d3d, chain, 1, frame_width, frame_height, chain->frame_count);

   IDirect3DDevice8_SetTexture(d3dr, 0,
         (IDirect3DBaseTexture8*)chain->tex);
   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER,
         video_smooth ?
         D3DTEXF_LINEAR : D3DTEXF_POINT);
   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER,
         video_smooth ?
         D3DTEXF_LINEAR : D3DTEXF_POINT);

   IDirect3DDevice8_SetViewport(chain->dev, (D3DVIEWPORT8*)&d3d->out_vp);
   IDirect3DDevice8_SetVertexShader(d3dr,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
   IDirect3DDevice8_SetStreamSource(d3dr,
         0, chain->vertex_buf, sizeof(Vertex));
   d3d8_set_mvp(d3d->dev, &d3d->mvp_rotate);
   IDirect3DDevice8_BeginScene(d3dr);
   IDirect3DDevice8_DrawPrimitive(d3dr, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice8_EndScene(d3dr);

   chain->frame_count++;
}

static INLINE void *d3d8_vertex_buffer_new(
      LPDIRECT3DDEVICE8 dev,
      unsigned length, unsigned usage,
      unsigned fvf, D3DPOOL pool)
{
   void              *buf = NULL;
   if (FAILED(IDirect3DDevice8_CreateVertexBuffer(
               dev, length, usage, fvf,
               pool,
               (struct IDirect3DVertexBuffer8**)&buf)))
      return NULL;
   return buf;
}

static bool d3d8_setup_init(void *data,
      const video_info_t *video_info,
      void *dev_data,
      const struct LinkInfo *link_info,
      bool rgb32
      )
{
   unsigned width, height;
   d3d8_video_t *d3d                      = (d3d8_video_t*)data;
   settings_t *settings                   = config_get_ptr();
   LPDIRECT3DDEVICE8 d3dr                 = (LPDIRECT3DDEVICE8)d3d->dev;
   d3d8_renderchain_t *chain              = (d3d8_renderchain_t*)d3d->renderchain_data;
   unsigned fmt                           = (rgb32) ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   video_viewport_t *custom_vp            = &settings->video_vp_custom;

   video_driver_get_size(&width, &height);

   chain->dev                             = dev_data;
   chain->pixel_size                      = (fmt == RETRO_PIXEL_FORMAT_RGB565)
      ? 2
      : 4;
   chain->tex_w                           = link_info->tex_w;
   chain->tex_h                           = link_info->tex_h;

   chain->vertex_buf                      = (LPDIRECT3DVERTEXBUFFER8)d3d8_vertex_buffer_new(d3dr, 4 * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         D3DPOOL_MANAGED);

   if (!chain->vertex_buf)
      return false;

   chain->tex = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3dr,
         chain->tex_w, chain->tex_h, 1, 0,
         video_info->rgb32
         ?
         D3D8_XRGB8888_FORMAT : D3D8_RGB565_FORMAT,
         D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL,
         false);

   if (!chain->tex)
      return false;

   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetTextureStageState(d3dr, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetRenderState(d3dr, D3DRS_LIGHTING, 0);
   IDirect3DDevice8_SetRenderState(d3dr, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice8_SetRenderState(d3dr, D3DRS_ZENABLE,  FALSE);

   /* FIXME */
   if (custom_vp->width == 0)
      custom_vp->width = width;

   if (custom_vp->height == 0)
      custom_vp->height = height;

   return true;
}

static void *d3d8_renderchain_new(void)
{
   d3d8_renderchain_t *renderchain = (d3d8_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

/*
 * DISPLAY DRIVER
 */

static const float *gfx_display_d3d8_get_default_vertices(void)
{
   return &d3d8_vertexes[0];
}

static const float *gfx_display_d3d8_get_default_tex_coords(void)
{
   return &d3d8_tex_coords[0];
}

static void *gfx_display_d3d8_get_default_mvp(void *data)
{
   static float id[16] =       { 1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f
                               };
   return &id;
}

static INT32 gfx_display_prim_to_d3d8_enum(
      enum gfx_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLES:
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return D3DPT_COMM_TRIANGLESTRIP;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   /* TODO/FIXME - hack */
   return 0;
}

static void gfx_display_d3d8_blend_begin(void *data)
{
   d3d8_video_t *d3d             = (d3d8_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);
}

static void gfx_display_d3d8_blend_end(void *data)
{
   d3d8_video_t *d3d             = (d3d8_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, false);
}

static void gfx_display_d3d8_draw(gfx_display_ctx_draw_t *draw,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   static float default_mvp[] ={ 1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f
                               };
   unsigned i;
   math_matrix_4x4 mop, m1, m2;
   LPDIRECT3DVERTEXBUFFER8 vbo;
   LPDIRECT3DDEVICE8 dev;
   D3DPRIMITIVETYPE type;
   unsigned start                = 0;
   unsigned count                = 0;
   d3d8_video_t *d3d             = (d3d8_video_t*)data;
   Vertex * pv                   = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;

   if (!d3d || !draw || draw->pipeline_id)
      return;
   if ((d3d->menu_display.offset + draw->coords->vertices )
         > (unsigned)d3d->menu_display.size)
      return;
   vbo                           = (LPDIRECT3DVERTEXBUFFER8)d3d->menu_display.buffer;
   dev                           = d3d->dev;
   pv                            = (Vertex*)d3d8_vertex_buffer_lock(vbo);

   if (!pv)
      return;

   pv          += d3d->menu_display.offset;
   vertex       = draw->coords->vertex;
   tex_coord    = draw->coords->tex_coord;
   color        = draw->coords->color;

   if (!vertex)
      vertex    = &d3d8_vertexes[0];
   if (!tex_coord)
      tex_coord = &d3d8_tex_coords[0];

   for (i = 0; i < draw->coords->vertices; i++)
   {
      int colors[4];

      colors[0]   = *color++ * 0xFF;
      colors[1]   = *color++ * 0xFF;
      colors[2]   = *color++ * 0xFF;
      colors[3]   = *color++ * 0xFF;

      pv[i].x     = *vertex++;
      pv[i].y     = *vertex++;
      pv[i].z     = 0.5f;
      pv[i].u     = *tex_coord++;
      pv[i].v     = *tex_coord++;

      if ((void*)draw->texture)
      {
         D3DSURFACE_DESC desc;
         LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)draw->texture;
         if (SUCCEEDED(IDirect3DTexture8_GetLevelDesc(tex,
                     0, (D3DSURFACE_DESC*)&desc)))
         {
            pv[i].u *= desc.Width;
            pv[i].v *= desc.Height;
         }
      }

      pv[i].color =
         D3DCOLOR_ARGB(
               colors[3], /* A */
               colors[0], /* R */
               colors[1], /* G */
               colors[2]  /* B */
               );
   }
   IDirect3DVertexBuffer8_Unlock(vbo);

   if (!draw->matrix_data)
      draw->matrix_data = &default_mvp;

   /* ugh */
   matrix_4x4_scale(m1,       2.0,  2.0, 0);
   matrix_4x4_translate(mop, -1.0, -1.0, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_multiply(m1,
         *((math_matrix_4x4*)draw->matrix_data), m2);
   matrix_4x4_scale(mop,
         (draw->width  / 2.0) / video_width,
         (draw->height / 2.0) / video_height, 0);
   matrix_4x4_multiply(m2, mop, m1);
   matrix_4x4_translate(mop,
         (draw->x + (draw->width  / 2.0)) / video_width,
         (draw->y + (draw->height / 2.0)) / video_height,
         0);
   matrix_4x4_multiply(m1, mop, m2);
   matrix_4x4_multiply(m2, d3d->mvp_transposed, m1);
   d3d_matrix_transpose(&m1, &m2);

   d3d8_set_mvp(dev, &m1);

   if (draw->texture)
   {
      IDirect3DDevice8_SetTexture(dev, 0,
            (IDirect3DBaseTexture8*)draw->texture);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_COMM_CLAMP);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_COMM_CLAMP);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER, D3DTEXF_COMM_LINEAR);
      IDirect3DDevice8_SetTextureStageState(dev, 0,
            (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER, D3DTEXF_COMM_LINEAR);
   }

   type  = gfx_display_prim_to_d3d8_enum(draw->prim_type);
   start = d3d->menu_display.offset;
   count = draw->coords->vertices -
         ((draw->prim_type == GFX_DISPLAY_PRIM_TRIANGLESTRIP)
          ? 2 : 0);

   IDirect3DDevice8_BeginScene(dev);
   IDirect3DDevice8_DrawPrimitive(dev, type, start, count);
   IDirect3DDevice8_EndScene(dev);

   d3d->menu_display.offset += draw->coords->vertices;
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d8 = {
   gfx_display_d3d8_draw,
   NULL,                                        /* draw_pipeline */
   gfx_display_d3d8_blend_begin,
   gfx_display_d3d8_blend_end,
   gfx_display_d3d8_get_default_mvp,
   gfx_display_d3d8_get_default_vertices,
   gfx_display_d3d8_get_default_tex_coords,
   FONT_DRIVER_RENDER_D3D8_API,
   GFX_VIDEO_DRIVER_DIRECT3D8,
   "d3d8",
   false,
   NULL,
   NULL
};

/*
 * VIDEO DRIVER
 */

static void d3d8_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   d3d8_video_t *d3d   = (d3d8_video_t*)data;

   if (!d3d || !vp)
      return;

   video_driver_get_size(&width, &height);

   vp->x            = d3d->out_vp.X;
   vp->y            = d3d->out_vp.Y;
   vp->width        = d3d->out_vp.Width;
   vp->height       = d3d->out_vp.Height;

   vp->full_width   = width;
   vp->full_height  = height;
}

static void d3d8_overlay_render(d3d8_video_t *d3d,
      unsigned width, unsigned height,
      overlay_t *overlay, bool force_linear)
{
   D3DVIEWPORT8 vp_full;
   struct video_viewport vp;
   unsigned i;
   Vertex vert[4];
   enum D3DTEXTUREFILTERTYPE filter_type = D3DTEXF_LINEAR;

   if (!d3d || !overlay || !overlay->tex)
      return;

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = d3d8_vertex_buffer_new(
      d3d->dev, sizeof(vert), D3DUSAGE_WRITEONLY,
      D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
      D3DPOOL_MANAGED);

	  if (!overlay->vert_buf)
		  return;
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z    = 0.5f;
      vert[i].color   = (((uint32_t)(overlay->alpha_mod * 0xFF)) << 24) | 0xFFFFFF;
   }

   d3d8_viewport_info(d3d, &vp);

   vert[0].x      = overlay->vert_coords[0];
   vert[1].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[2].x      = overlay->vert_coords[0];
   vert[3].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[0].y      = overlay->vert_coords[1];
   vert[1].y      = overlay->vert_coords[1];
   vert[2].y      = overlay->vert_coords[1] + overlay->vert_coords[3];
   vert[3].y      = overlay->vert_coords[1] + overlay->vert_coords[3];

   vert[0].u      = overlay->tex_coords[0];
   vert[1].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[2].u      = overlay->tex_coords[0];
   vert[3].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[0].v      = overlay->tex_coords[1];
   vert[1].v      = overlay->tex_coords[1];
   vert[2].v      = overlay->tex_coords[1] + overlay->tex_coords[3];
   vert[3].v      = overlay->tex_coords[1] + overlay->tex_coords[3];

   if (overlay->vert_buf)
   {
      LPDIRECT3DVERTEXBUFFER8 vbo = (LPDIRECT3DVERTEXBUFFER8)overlay->vert_buf;
      void *verts = d3d8_vertex_buffer_lock(vbo);
      memcpy(verts, vert, sizeof(vert));
      IDirect3DVertexBuffer8_Unlock(vbo);
   }
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);
   IDirect3DDevice8_SetVertexShader(d3d->dev,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
   IDirect3DDevice8_SetStreamSource(d3d->dev,
         0, overlay->vert_buf, sizeof(*vert));

   if (overlay->fullscreen)
   {

      vp_full.X      = 0;
      vp_full.Y      = 0;
      vp_full.Width  = width;
      vp_full.Height = height;
      vp_full.MinZ   = 0.0f;
      vp_full.MaxZ   = 1.0f;
      IDirect3DDevice8_SetViewport(d3d->dev, &vp_full);
   }

   if (!force_linear)
   {
      settings_t *settings    = config_get_ptr();
      bool menu_linear_filter = settings->bools.menu_linear_filter;

      if (!menu_linear_filter)
         filter_type       = D3DTEXF_POINT;
   }

   /* Render overlay. */
   IDirect3DDevice8_SetTexture(d3d->dev, 0,
         (IDirect3DBaseTexture8*)overlay->tex);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MAGFILTER, filter_type);
   IDirect3DDevice8_SetTextureStageState(d3d->dev, 0,
         (D3DTEXTURESTAGESTATETYPE)D3DTSS_MINFILTER, filter_type);
   IDirect3DDevice8_BeginScene(d3d->dev);
   IDirect3DDevice8_DrawPrimitive(d3d->dev, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice8_EndScene(d3d->dev);

   /* Restore previous state. */
   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, false);
   IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&d3d->out_vp);
}

static void d3d8_free_overlay(d3d8_video_t *d3d, overlay_t *overlay)
{
   LPDIRECT3DTEXTURE8 tex      = overlay->tex;
   LPDIRECT3DVERTEXBUFFER8 vbo = (LPDIRECT3DVERTEXBUFFER8)overlay->vert_buf;
   if (tex)
      IDirect3DTexture8_Release(tex);
   if (vbo)
      IDirect3DVertexBuffer8_Release(vbo);
   overlay->vert_buf = NULL;
}

static void d3d8_deinitialize(d3d8_video_t *d3d)
{
   LPDIRECT3DVERTEXBUFFER8 _vbo;
   d3d8_renderchain_t *chain = NULL;
   if (!d3d)
      return;
   chain                     = (d3d8_renderchain_t*)d3d->renderchain_data;
   font_driver_free_osd();

   if (chain)
   {
      LPDIRECT3DTEXTURE8      tex = (LPDIRECT3DTEXTURE8)chain->tex;
      LPDIRECT3DVERTEXBUFFER8 vbo = (LPDIRECT3DVERTEXBUFFER8)chain->vertex_buf;
      if (tex)
         IDirect3DTexture8_Release(tex);
      if (vbo)
         IDirect3DVertexBuffer8_Release(vbo);
      chain->vertex_buf = NULL;
      chain->tex        = NULL;

      free(chain);
   }
   d3d->renderchain_data    = NULL;
   _vbo                     = (LPDIRECT3DVERTEXBUFFER8)d3d->menu_display.buffer;
   IDirect3DVertexBuffer8_Release(_vbo);
   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl   = NULL;
}

#define FS_PRESENTINTERVAL(pp) ((pp)->FullScreen_PresentationInterval)

static INLINE bool d3d8_get_adapter_display_mode(
      LPDIRECT3D8 d3d,
      unsigned idx,
      void *display_mode)
{
   if (d3d &&
         SUCCEEDED(IDirect3D8_GetAdapterDisplayMode(
               d3d, idx, (D3DDISPLAYMODE*)display_mode)))
      return true;
   return false;
}

static D3DFORMAT d3d8_get_color_format_backbuffer(bool rgb32, bool windowed)
{
   D3DFORMAT fmt = D3DFMT_X8R8G8B8;
#ifdef _XBOX
   if (!rgb32)
      fmt        = D3D8_RGB565_FORMAT;
#else
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (d3d8_get_adapter_display_mode(g_pD3D8, 0, &display_mode))
         fmt = display_mode.Format;
   }
#endif
   return fmt;
}

static bool d3d8_is_windowed_enable(bool info_fullscreen)
{
#ifndef _XBOX
   settings_t *settings = config_get_ptr();
   if (!info_fullscreen)
      return true;
   if (settings)
      return settings->bools.video_windowed_fullscreen;
#endif
   return false;
}

#ifdef _XBOX
static void d3d8_get_video_size(d3d8_video_t *d3d,
      unsigned *width, unsigned *height)
{
   DWORD video_mode      = XGetVideoFlags();

   *width                = 640;
   *height               = 480;

   d3d->widescreen_mode  = false;

   /* Only valid in PAL mode, not valid for HDTV modes! */

   if (XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      /* Check for 16:9 mode (PAL REGION) */
      if (video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         /* 60 Hz, 720x480i */
         if (video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
            *height = 480;
         else /* 50 Hz, 720x576i */
            *height = 576;
         d3d->widescreen_mode = true;
      }
   }
   else
   {
      /* Check for 16:9 mode (NTSC REGIONS) */
      if (video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width                    = 720;
         *height                   = 480;
         d3d->widescreen_mode      = true;
      }
   }

   if (XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width                    = 640;
         *height                   = 480;
         d3d->widescreen_mode      = false;
         d3d->resolution_hd_enable = true;
      }
      else if (video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         *width                    = 1280;
         *height                   = 720;
         d3d->widescreen_mode      = true;
         d3d->resolution_hd_enable = true;
      }
      else if (video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         *width                    = 1920;
         *height                   = 1080;
         d3d->widescreen_mode      = true;
         d3d->resolution_hd_enable = true;
      }
   }
}
#endif

static void d3d8_make_d3dpp(void *data,
      const video_info_t *info, void *_d3dpp)
{
   d3d8_video_t *d3d               = (d3d8_video_t*)data;
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
   bool windowed_enable           = d3d8_is_windowed_enable(info->fullscreen);

   memset(d3dpp, 0, sizeof(*d3dpp));

   d3dpp->Windowed                = windowed_enable;
   FS_PRESENTINTERVAL(d3dpp)      = D3DPRESENT_INTERVAL_IMMEDIATE;

   if (info->vsync)
   {
      settings_t *settings         = config_get_ptr();
      unsigned video_swap_interval = runloop_get_video_swap_interval(
            settings->uints.video_swap_interval);

      switch (video_swap_interval)
      {
         default:
         case 1:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_ONE;
            break;
         case 2:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_TWO;
            break;
         case 3:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_THREE;
            break;
         case 4:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_FOUR;
            break;
      }
   }

   /* PresentationInterval must be zero for windowed mode on DX8. */
   if (d3dpp->Windowed)
      FS_PRESENTINTERVAL(d3dpp)   = D3DPRESENT_INTERVAL_DEFAULT;

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount         = 2;
   d3dpp->BackBufferFormat        = d3d8_get_color_format_backbuffer(
         info->rgb32, windowed_enable);
#ifndef _XBOX
   d3dpp->hDeviceWindow           = win32_get_window();
#endif

   if (!windowed_enable)
   {
#ifdef _XBOX
      unsigned width              = 0;
      unsigned height             = 0;

      d3d8_get_video_size(d3d, &width, &height);
      video_driver_set_size(width, height);
#endif
      video_driver_get_size(&d3dpp->BackBufferWidth,
            &d3dpp->BackBufferHeight);
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   {
      /* Get the "video mode" */
      DWORD video_mode            = XGetVideoFlags();

      /* Check if we are able to use progressive mode. */
      if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
         d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      else
         d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;

      /* Only valid in PAL mode, not valid for HDTV modes. */
      if (XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
      {
         if (video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
            d3dpp->FullScreen_RefreshRateInHz = 60;
         else
            d3dpp->FullScreen_RefreshRateInHz = 50;
      }

      if (XGetAVPack() == XC_AV_PACK_HDTV)
      {
         if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
            d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
         else if (video_mode & XC_VIDEO_FLAGS_HDTV_720p)
            d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
         else if (video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
            d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;
      }

#if 0
      if (d3d->widescreen_mode)
         d3dpp->Flags |= D3DPRESENTFLAG_WIDESCREEN;
#endif
   }
#endif
}

static bool d3d8_init_base(void *data, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
#ifdef _XBOX
   HWND focus_window = NULL;
#else
   HWND focus_window = win32_get_window();
#endif
   d3d8_video_t *d3d = (d3d8_video_t*)data;

#ifdef _XBOX
   g_pD3D8           = (LPDIRECT3D8)D3DCreate(0);
#else
   g_pD3D8           = (LPDIRECT3D8)D3DCreate(220);
#endif

   /* this needs g_pD3D created first */
   d3d8_make_d3dpp(d3d, info, &d3dpp);

   if (!g_pD3D8)
      return false;
   if (!d3d8_create_device(&d3d->dev, &d3dpp,
            g_pD3D8,
            focus_window,
            d3d->cur_mon_id)
      )
      return false;
   return true;
}

static void d3d8_calculate_rect(void *data,
      unsigned *width, unsigned *height,
      int *x, int *y,
      bool force_full,
      bool allow_rotate)
{
   struct video_viewport vp;
   float device_aspect       = (float)*width / *height;
   d3d8_video_t *d3d         = (d3d8_video_t*)data;
   settings_t *settings      = config_get_ptr();
   bool video_scale_integer  = settings->bools.video_scale_integer;
   unsigned aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;

   video_driver_get_size(width, height);

   vp.x           = 0;
   vp.y           = 0;
   vp.width       = *width;
   vp.height      = *height;
   vp.full_width  = *width;
   vp.full_height = *height;

   if (video_scale_integer && !force_full)
      video_viewport_get_scaled_integer(&vp,
            *width,
            *height,
            video_driver_get_aspect_ratio(),
            d3d->keep_aspect,
            true);
   else if (d3d->keep_aspect && !force_full)
      video_viewport_get_scaled_aspect(&vp, *width, *height, true);
   *x                          = vp.x;
   *y                          = vp.y;
   *width                      = vp.width;
   *height                     = vp.height;
}

static void d3d8_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate)
{
   struct d3d_matrix proj, ortho, rot, matrix;
   int x               = 0;
   int y               = 0;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   d3d8_calculate_rect(data, &width, &height, &x, &y,
         force_full, allow_rotate);

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   d3d->out_vp.X      = x;
   d3d->out_vp.Y      = y;
   d3d->out_vp.Width  = width;
   d3d->out_vp.Height = height;
   d3d->out_vp.MinZ   = 0.0f;
   d3d->out_vp.MaxZ   = 0.0f;

   d3d_matrix_identity(&ortho);
   d3d_matrix_ortho_off_center_lh(&ortho, 0, 1, 0, 1, 0.0f, 1.0f);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, d3d->dev_rotation * (M_PI / 2.0));
   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&d3d->mvp, &ortho);
   d3d_matrix_transpose(&d3d->mvp_rotate, &matrix);
}

static bool d3d8_initialize(d3d8_video_t *d3d, const video_info_t *info)
{
   struct LinkInfo link_info;
   unsigned width, height;
   unsigned i           = 0;
   bool ret             = true;
   settings_t *settings = config_get_ptr();

   if (!d3d)
      return false;

   if (!g_pD3D8)
      ret = d3d8_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;

      d3d8_make_d3dpp(d3d, info, &d3dpp);

      /* the D3DX font driver uses POOL_DEFAULT resources
       * and will prevent a clean reset here
       * another approach would be to keep track of all created D3D
       * font objects and free/realloc them around the d3d_reset call  */
#ifdef HAVE_MENU
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
#endif

      if (!d3d8_reset(d3d->dev, &d3dpp))
      {
         d3d8_deinitialize(d3d);
         IDirect3D8_Release(g_pD3D8);
         g_pD3D8 = NULL;

         if ((ret = d3d8_init_base(d3d, info)))
            RARCH_LOG("[D3D8]: Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   /* Setup information. */
   link_info.pass               = NULL;
   link_info.tex_w              = info->input_scale * RARCH_SCALE_BASE;
   link_info.tex_h              = info->input_scale * RARCH_SCALE_BASE;
   link_info.pass               = &d3d->shader.pass[0];

   d3d->renderchain_data        = d3d8_renderchain_new();

   if (
         !d3d8_setup_init(
            d3d,
            &d3d->video_info,
            d3d->dev, &link_info,
            d3d->video_info.rgb32)
      )
      return false;

   video_driver_get_size(&width, &height);
   d3d8_set_viewport(d3d,
	   width, height, false, true);

   font_driver_init_osd(d3d, info,
         false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D8_API);

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   d3d->menu_display.buffer = d3d8_vertex_buffer_new(
         d3d->dev, d3d->menu_display.size * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         D3DPOOL_DEFAULT);

   if (!d3d->menu_display.buffer)
      return false;

   d3d_matrix_identity(&d3d->mvp_transposed);
   d3d_matrix_ortho_off_center_lh(&d3d->mvp_transposed, 0, 1, 0, 1, 0, 1);
   d3d_matrix_transpose(&d3d->mvp, &d3d->mvp_transposed);

   IDirect3DDevice8_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);

   return true;
}

static bool d3d8_restore(void *data)
{
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return false;

   d3d8_deinitialize(d3d);

   if (!d3d8_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D8]: Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}

static void d3d8_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   int      interval            = 0;
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   if (!state)
      interval                  = 1;
   d3d->video_info.vsync        = !state;

#ifdef _XBOX
   IDirect3DDevice8_SetRenderState(d3d->dev,
         D3D8_PRESENTATIONINTERVAL,
         interval ?
         D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE
         );
#else
   d3d->needs_restore           = true;
   d3d8_restore(d3d);
#endif
}

static void d3d8_set_resize(d3d8_video_t *d3d,
      unsigned new_width, unsigned new_height)
{
   /* No changes? */
   if (     (new_width  == d3d->video_info.width)
         && (new_height == d3d->video_info.height))
      return;

   d3d->video_info.width  = new_width;
   d3d->video_info.height = new_height;
   video_driver_set_size(new_width, new_height);
}

static bool d3d8_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool ret             = false;
   d3d8_video_t *d3d    = (d3d8_video_t*)data;
   bool        quit     = false;
   bool        resize   = false;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   win32_check_window(NULL, &quit, &resize, &temp_width, &temp_height);

   if (quit)
      d3d->quitting = quit;

   if (resize)
   {
      d3d->should_resize = true;
      d3d8_set_resize(d3d, temp_width, temp_height);
      d3d8_restore(d3d);
   }

   ret = !quit;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   return ret;
}

#ifdef _XBOX
static bool d3d8_suspend_screensaver(void *data, bool enable) { return true; }
#endif

static void d3d8_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

static void d3d8_apply_state_changes(void *data)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

static void d3d8_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   d3d8_video_t          *d3d = (d3d8_video_t*)data;

   IDirect3DDevice8_BeginScene(d3d->dev);
   font_driver_render_msg(d3d, msg, params, font);
   IDirect3DDevice8_EndScene(d3d->dev);
}

static bool d3d8_init_internal(d3d8_video_t *d3d,
      const video_info_t *info, input_driver_t **input,
      void **input_data)
{
#ifdef HAVE_MONITOR
   bool windowed_full;
   RECT mon_rect;
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use;
#endif
   struct video_shader_pass *pass = NULL;
#ifdef HAVE_WINDOW
   DWORD style;
   unsigned win_width        = 0;
   unsigned win_height       = 0;
   RECT rect                 = {0};
#endif
   unsigned full_x           = 0;
   unsigned full_y           = 0;
   settings_t    *settings   = config_get_ptr();
   overlay_t *menu           = (overlay_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return false;

   d3d->menu                 = menu;
   d3d->cur_mon_id           = 0;
   d3d->menu->tex_coords[0]  = 0;
   d3d->menu->tex_coords[1]  = 0;
   d3d->menu->tex_coords[2]  = 1;
   d3d->menu->tex_coords[3]  = 1;
   d3d->menu->vert_coords[0] = 0;
   d3d->menu->vert_coords[1] = 1;
   d3d->menu->vert_coords[2] = 1;
   d3d->menu->vert_coords[3] = -1;

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.lpfnWndProc = wnd_proc_d3d_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_winraw;
#endif
   win32_window_init(&d3d->windowClass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);

   mon_rect              = current_mon.rcMonitor;
   g_win32_resize_width  = info->width;
   g_win32_resize_height = info->height;

   windowed_full         = settings->bools.video_windowed_fullscreen;

   full_x                = (windowed_full || info->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y                = (windowed_full || info->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : info->height;
#else
   d3d8_get_video_size(d3d, &full_x, &full_y);
#endif
   {
      unsigned new_width  = info->fullscreen ? full_x : info->width;
      unsigned new_height = info->fullscreen ? full_y : info->height;
      video_driver_set_size(new_width, new_height);
   }

#ifdef HAVE_WINDOW
   video_driver_get_size(&win_width, &win_height);

   win32_set_style(&current_mon, &hm_to_use, &win_width, &win_height,
         info->fullscreen, windowed_full, &rect, &mon_rect, &style);

   win32_window_create(d3d, style, &mon_rect, win_width,
         win_height, info->fullscreen);

   win32_set_window(&win_width, &win_height, info->fullscreen,
	   windowed_full, &rect);
#endif

   memset(&d3d->shader, 0, sizeof(d3d->shader));
   d3d->shader.passes                    = 1;

   pass                                  = (struct video_shader_pass*)
      &d3d->shader.pass[0];

   pass->fbo.scale_y                     = 1.0;
   pass->fbo.type_y                      = RARCH_SCALE_VIEWPORT;
   pass->fbo.scale_x                     = pass->fbo.scale_y;
   pass->fbo.type_x                      = pass->fbo.type_y;
   pass->fbo.flags                      |= FBO_SCALE_FLAG_VALID;

   if (!string_is_empty(d3d->shader_path))
      strlcpy(pass->source.path, d3d->shader_path,
            sizeof(pass->source.path));

   d3d->video_info                       = *info;
   if (!d3d8_initialize(d3d, &d3d->video_info))
      return false;

   d3d_input_driver(settings->arrays.input_driver, settings->arrays.input_joypad_driver, input, input_data);

   return true;
}

static void d3d8_set_rotation(void *data, unsigned rot)
{
   d3d8_video_t *d3d  = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->dev_rotation  = rot;
   d3d->should_resize = true;
}

static void *d3d8_init(const video_info_t *info,
      input_driver_t **input, void **input_data)
{
   d3d8_video_t *d3d = (d3d8_video_t*)calloc(1, sizeof(*d3d));

   if (!d3d)
      return NULL;

   if (!d3d8_initialize_symbols(GFX_CTX_DIRECT3D8_API))
   {
      free(d3d);
      return NULL;
   }

#ifndef _XBOX
   win32_window_reset();
   win32_monitor_init();
#endif

   /* Default values */
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->needs_restore        = false;
#ifdef HAVE_OVERLAY
   d3d->overlays_enabled     = false;
#endif
   d3d->should_resize        = false;
   d3d->menu                 = NULL;

   if (!d3d8_init_internal(d3d, info, input, input_data))
   {
      RARCH_ERR("[D3D8]: Failed to init D3D.\n");
      free(d3d);
      return NULL;
   }

   d3d->keep_aspect       = info->force_aspect;

   return d3d;
}

#ifdef HAVE_OVERLAY
static void d3d8_free_overlays(d3d8_video_t *d3d)
{
   unsigned i;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d8_free_overlay(d3d, &d3d->overlays[i]);
   free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
}
#endif

static void d3d8_free(void *data)
{
   d3d8_video_t   *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d8_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d8_free_overlay(d3d, d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d8_deinitialize(d3d);

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);

   IDirect3DDevice8_Release(d3d->dev);
   IDirect3D8_Release(g_pD3D8);
   d3d->shader_path = NULL;
   d3d->dev         = NULL;
   g_pD3D8          = NULL;

#ifdef HAVE_DYNAMIC_D3D
   d3d8_deinitialize_symbols();
#endif

#ifndef _XBOX
   win32_monitor_from_window();
   win32_destroy_window();
#endif
   free(d3d);
}

#ifdef HAVE_OVERLAY
static void d3d8_overlay_tex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d8_video_t *d3d                   = (d3d8_video_t*)data;
   if (!d3d)
      return;

   d3d->overlays[index].tex_coords[0]  = x;
   d3d->overlays[index].tex_coords[1]  = y;
   d3d->overlays[index].tex_coords[2]  = w;
   d3d->overlays[index].tex_coords[3]  = h;
#ifdef _XBOX
   d3d->overlays[index].tex_coords[0] *= d3d->overlays[index].tex_w;
   d3d->overlays[index].tex_coords[1] *= d3d->overlays[index].tex_h;
   d3d->overlays[index].tex_coords[2] *= d3d->overlays[index].tex_w;
   d3d->overlays[index].tex_coords[3] *= d3d->overlays[index].tex_h;
#endif
}

static void d3d8_overlay_vertex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (!d3d)
      return;

   y                                   = 1.0f - y;
   h                                   = -h;
   d3d->overlays[index].vert_coords[0] = x;
   d3d->overlays[index].vert_coords[1] = y;
   d3d->overlays[index].vert_coords[2] = w;
   d3d->overlays[index].vert_coords[3] = h;
}

static bool d3d8_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, y;
   overlay_t *new_overlays            = NULL;
   d3d8_video_t *d3d                  = (d3d8_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)image_data;

   if (!d3d)
      return false;

   d3d8_free_overlays(d3d);
   d3d->overlays      = (overlay_t*)calloc(num_images, sizeof(*d3d->overlays));
   d3d->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      D3DLOCKED_RECT d3dlr;
      unsigned width     = images[i].width;
      unsigned height    = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex       = d3d8_texture_new(d3d->dev,
                  width, height, 1, 0,
                  D3D8_ARGB8888_FORMAT,
                  D3DPOOL_MANAGED, 0, 0, 0,
                  NULL, NULL, false);

      if (!overlay->tex)
         return false;

      if (IDirect3DTexture8_LockRect(
               (LPDIRECT3DTEXTURE8)overlay->tex, 0,
               &d3dlr, NULL, D3DLOCK_NOSYSLOCK) == D3D_OK)
      {
         uint32_t       *dst    = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src    = images[i].pixels;
         unsigned      pitch    = d3dlr.Pitch >> 2;
         LPDIRECT3DTEXTURE8 tex = overlay->tex;
         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         IDirect3DTexture8_UnlockRect(tex, 0);
      }

      overlay->tex_w         = width;
      overlay->tex_h         = height;

      /* Default. Stretch to whole screen. */
      d3d8_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d8_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d8_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

#ifndef _XBOX
   win32_show_cursor(d3d, state);
#endif
}

static void d3d8_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d8_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d8_overlay_interface = {
   d3d8_overlay_enable,
   d3d8_overlay_load,
   d3d8_overlay_tex_geom,
   d3d8_overlay_vertex_geom,
   d3d8_overlay_full_screen,
   d3d8_overlay_set_alpha,
};

static void d3d8_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d8_overlay_interface;
}
#endif

static bool d3d8_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   D3DVIEWPORT8 screen_vp;
   unsigned i                          = 0;
   d3d8_video_t *d3d                    = (d3d8_video_t*)data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;
   struct font_params *osd_params      = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text               = video_info->stat_text;
   bool statistics_show                = video_info->statistics_show;
   unsigned black_frame_insertion      = video_info->black_frame_insertion;
#ifdef HAVE_MENU
   bool menu_is_alive                  = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (!frame)
      return true;

   /* We cannot recover in fullscreen. */
   if (d3d->needs_restore)
   {
#ifndef _XBOX
      HWND window = win32_get_window();
      if (IsIconic(window))
         return true;
#endif

      if (!d3d8_restore(d3d))
      {
         RARCH_ERR("[D3D8]: Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      d3d8_set_viewport(d3d, width, height, false, true);
      d3d->should_resize = false;
   }

   /* render_chain() only clears out viewport,
    * clear out everything. */
   screen_vp.X      = 0;
   screen_vp.Y      = 0;
   screen_vp.MinZ   = 0;
   screen_vp.MaxZ   = 1;
   screen_vp.Width  = width;
   screen_vp.Height = height;
   IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&screen_vp);
   IDirect3DDevice8_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   d3d8_render(d3d, d3d->dev, (d3d8_renderchain_t*)d3d->renderchain_data, frame,
            frame_width, frame_height,
            pitch, d3d->dev_rotation);

   if (black_frame_insertion && !d3d->menu->enabled)
   {
      unsigned n;
      for (n = 0; n < video_info->black_frame_insertion; ++n)
      {
         if (IDirect3DDevice8_Present(d3d->dev, NULL, NULL, NULL, NULL)
               == D3DERR_DEVICELOST)
            return true;
         if (d3d->needs_restore)
            return true;
         IDirect3DDevice8_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
      }
   }

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      d3d8_set_mvp(d3d->dev, &d3d->mvp);
      d3d8_overlay_render(d3d, width, height, d3d->menu, false);

      d3d->menu_display.offset = 0;
      IDirect3DDevice8_SetStreamSource(d3d->dev,
            0, d3d->menu_display.buffer, sizeof(Vertex));

      IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&screen_vp);
      menu_driver_frame(menu_is_alive, video_info);
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(d3d, stat_text,
               (const struct font_params*)osd_params, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled)
   {
      d3d8_set_mvp(d3d->dev, &d3d->mvp);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d8_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

   if (!string_is_empty(msg))
   {
      IDirect3DDevice8_SetViewport(d3d->dev, (D3DVIEWPORT8*)&screen_vp);
      IDirect3DDevice8_BeginScene(d3d->dev);
      font_driver_render_msg(d3d, msg, NULL, NULL);
      IDirect3DDevice8_EndScene(d3d->dev);
   }

   video_driver_update_title(NULL);
   IDirect3DDevice8_Present(d3d->dev, NULL, NULL, NULL, NULL);

   return true;
}

static bool d3d8_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   return false;
}

static void d3d8_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DLOCKED_RECT d3dlr;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   if (    !d3d->menu->tex            ||
            d3d->menu->tex_w != width ||
            d3d->menu->tex_h != height)
   {
      LPDIRECT3DTEXTURE8 tex = d3d->menu->tex;
      if (tex)
         IDirect3DTexture8_Release(tex);

      d3d->menu->tex = d3d8_texture_new(d3d->dev,
            width, height, 1,
            0, D3D8_ARGB8888_FORMAT,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!d3d->menu->tex)
         return;

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
#ifdef _XBOX
      d3d->menu->tex_coords [2] = width;
      d3d->menu->tex_coords[3]  = height;
#endif
   }

   d3d->menu->alpha_mod = alpha;

   {
      LPDIRECT3DTEXTURE8 tex = d3d->menu->tex;
      if (IDirect3DTexture8_LockRect(tex,
               0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK) == D3D_OK)
      {
         unsigned h, w;

         if (rgb32)
         {
            uint8_t        *dst = (uint8_t*)d3dlr.pBits;
            const uint32_t *src = (const uint32_t*)frame;

            for (h = 0; h < height; h++, dst += d3dlr.Pitch, src += width)
            {
               memcpy(dst, src, width * sizeof(uint32_t));
               memset(dst + width * sizeof(uint32_t), 0,
                     d3dlr.Pitch - width * sizeof(uint32_t));
            }
         }
         else
         {
            uint32_t       *dst = (uint32_t*)d3dlr.pBits;
            const uint16_t *src = (const uint16_t*)frame;

            for (h = 0; h < height; h++, dst += d3dlr.Pitch >> 2, src += width)
            {
               for (w = 0; w < width; w++)
               {
                  uint16_t c = src[w];
                  uint32_t r = (c >> 12) & 0xf;
                  uint32_t g = (c >>  8) & 0xf;
                  uint32_t b = (c >>  4) & 0xf;
                  uint32_t a = (c >>  0) & 0xf;
                  r          = ((r << 4) | r) << 16;
                  g          = ((g << 4) | g) <<  8;
                  b          = ((b << 4) | b) <<  0;
                  a          = ((a << 4) | a) << 24;
                  dst[w]     = r | g | b | a;
               }
            }
         }

         IDirect3DTexture8_UnlockRect(tex, 0);
      }
   }
}

static void d3d8_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

static void d3d8_video_texture_load_d3d(
      struct d3d8_texture_info *info,
      uintptr_t *id)
{
   D3DLOCKED_RECT d3dlr;
   unsigned usage            = 0;
   d3d8_video_t *d3d         = (d3d8_video_t*)info->userdata;
   struct texture_image *ti  = (struct texture_image*)info->data;
   LPDIRECT3DTEXTURE8 tex    = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3d->dev,
               ti->width, ti->height, 0,
               usage, D3D8_ARGB8888_FORMAT,
               D3DPOOL_MANAGED, 0, 0, 0,
               NULL, NULL, false);

   if (!tex)
      return;

   if (IDirect3DTexture8_LockRect(tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK) ==
         D3D_OK)
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;

      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
      IDirect3DTexture8_UnlockRect(tex, 0);
   }

   *id = (uintptr_t)tex;
}

static int d3d8_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   struct d3d8_texture_info *info = (struct d3d8_texture_info*)data;
   if (!info)
      return 0;
   d3d8_video_texture_load_d3d(info, &id);
   return id;
}

static uintptr_t d3d8_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   struct d3d8_texture_info info;
   uintptr_t id  = 0;

   info.userdata = video_data;
   info.data     = data;
   info.type     = filter_type;

   if (threaded)
      return video_thread_texture_handle(&info,
            d3d8_video_texture_load_wrap_d3d);

   d3d8_video_texture_load_d3d(&info, &id);
   return id;
}

static void d3d8_unload_texture(void *data, bool threaded,
      uintptr_t id)
{
   LPDIRECT3DTEXTURE8 texid;
   if (!id)
	   return;

   texid = (LPDIRECT3DTEXTURE8)id;
   IDirect3DTexture8_Release(texid);
}

static void d3d8_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef _XBOX
   win32_show_cursor(data, !fullscreen);
#endif
}

static uint32_t d3d8_get_flags(void *data)
{
   uint32_t             flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);

   return flags;
}

static const video_poke_interface_t d3d_poke_interface = {
   d3d8_get_flags,
   d3d8_load_texture,
   d3d8_unload_texture,
   d3d8_set_video_mode,
#if defined(_XBOX) || defined(__WINRT__)
   NULL, /* get_refresh_rate */
#else
   /* UWP does not expose this information easily */
   win32_get_refresh_rate,
#endif
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d8_set_aspect_ratio,
   d3d8_apply_state_changes,
   d3d8_set_menu_texture_frame,
   d3d8_set_menu_texture_enable,
   d3d8_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void d3d8_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &d3d_poke_interface;
}

#ifdef _XBOX
static bool d3d8_has_windowed(void *data) { return false; }
#else
static bool d3d8_has_windowed(void *data) { return true; }
#endif

video_driver_t video_d3d8 = {
   d3d8_init,
   d3d8_frame,
   d3d8_set_nonblock_state,
   d3d8_alive,
   NULL, /* focus */
#ifdef _XBOX
   d3d8_suspend_screensaver,
#else
   win32_suspend_screensaver,
#endif
   d3d8_has_windowed,
   d3d8_set_shader,
   d3d8_free,
   "d3d8",
   d3d8_set_viewport,
   d3d8_set_rotation,
   d3d8_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d8_get_overlay_interface,
#endif
   d3d8_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
