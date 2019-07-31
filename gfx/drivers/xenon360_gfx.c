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

#include <stdlib.h>
#include <string.h>

#include <xenos/xe.h>
#include <xenos/xenos.h>
#include <xenos/edram.h>
#include <xenos/xenos.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../font_driver.h"

#include "../../driver.h"

#define XE_W 512
#define XE_H 512

#define UV_BOTTOM   0
#define UV_TOP 1
#define UV_LEFT 2
#define UV_RIGHT 3

/* pixel shader */
const unsigned int g_xps_PS[] =
{
   0x102a1100, 0x000000b4, 0x0000003c, 0x00000000, 0x00000024, 0x00000000,
   0x0000008c, 0x00000000, 0x00000000, 0x00000064, 0x0000001c, 0x00000057,
   0xffff0300, 0x00000001, 0x0000001c, 0x00000000, 0x00000050, 0x00000030,
   0x00030000, 0x00010000, 0x00000040, 0x00000000, 0x54657874, 0x75726553,
   0x616d706c, 0x657200ab, 0x0004000c, 0x00010001, 0x00010000, 0x00000000,
   0x70735f33, 0x5f300032, 0x2e302e32, 0x30333533, 0x2e3000ab, 0x00000000,
   0x0000003c, 0x10000100, 0x00000008, 0x00000000, 0x00001842, 0x00010003,
   0x00000001, 0x00003050, 0x0000f1a0, 0x00011002, 0x00001200, 0xc4000000,
   0x00001003, 0x00002200, 0x00000000, 0x10081001, 0x1f1ff688, 0x00004000,
   0xc80f8000, 0x00000000, 0xe2010100, 0x00000000, 0x00000000, 0x00000000
};

/* vertex shader */
const unsigned int g_xvs_VS[] =
{
   0x102a1101, 0x0000009c, 0x00000078, 0x00000000, 0x00000024, 0x00000000,
   0x00000058, 0x00000000, 0x00000000, 0x00000030, 0x0000001c, 0x00000023,
   0xfffe0300, 0x00000000, 0x00000000, 0x00000000, 0x0000001c, 0x76735f33,
   0x5f300032, 0x2e302e32, 0x30333533, 0x2e3000ab, 0x00000000, 0x00000078,
   0x00110002, 0x00000000, 0x00000000, 0x00001842, 0x00000001, 0x00000003,
   0x00000002, 0x00000290, 0x00100003, 0x0000a004, 0x00305005, 0x00003050,
   0x0001f1a0, 0x00001007, 0x00001008, 0x70153003, 0x00001200, 0xc2000000,
   0x00001006, 0x00001200, 0xc4000000, 0x00002007, 0x00002200, 0x00000000,
   0x05f82000, 0x00000688, 0x00000000, 0x05f81000, 0x00000688, 0x00000000,
   0x05f80000, 0x00000fc8, 0x00000000, 0xc80f803e, 0x00000000, 0xe2020200,
   0xc8038000, 0x00b0b000, 0xe2000000, 0xc80f8001, 0x00000000, 0xe2010100,
   0x00000000, 0x00000000, 0x00000000
};

typedef struct DrawVerticeFormats
{
   float x, y, z, w;
   unsigned int color;
   float u, v;
} DrawVerticeFormats;

typedef struct xenon360_video xenon360_video_t;

static bool g_quitting;

typedef struct gl
{
   unsigned char *screen;
   struct XenosVertexBuffer *vb;
   struct XenosDevice * gl_device;
   struct XenosDevice real_device;
   struct XenosShader * g_pVertexShader;
   struct XenosShader * g_pPixelTexturedShader;
   struct XenosSurface * g_pTexture;
} gl_t;

static float ScreenUv[4] = {0.f, 1.0f, 1.0f, 0.f};

static void xenon360_gfx_free(void *data)
{
   gl_t *vid = data;
   if (!vid)
      return;

   free(vid);
}

static void *xenon360_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   gl_t * gl = calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   gl->gl_device = &gl->real_device;

   Xe_Init(gl->gl_device);

   Xe_SetRenderTarget(gl->gl_device, Xe_GetFramebufferSurface(gl->gl_device));

   static const struct XenosVBFFormat vbf =
   {
      3,
      {
         {XE_USAGE_POSITION, 0, XE_TYPE_FLOAT4},
	 {XE_USAGE_COLOR, 0, XE_TYPE_UBYTE4},
	 {XE_USAGE_TEXCOORD, 0, XE_TYPE_FLOAT2},
      }
   };

   gl->g_pPixelTexturedShader = Xe_LoadShaderFromMemory(gl->gl_device, (void*)g_xps_PS);
   Xe_InstantiateShader(gl->gl_device, gl->g_pPixelTexturedShader, 0);

   gl->g_pVertexShader = Xe_LoadShaderFromMemory(gl->gl_device, (void*)g_xvs_VS);
   Xe_InstantiateShader(gl->gl_device, gl->g_pVertexShader, 0);
   Xe_ShaderApplyVFetchPatches(gl->gl_device, gl->g_pVertexShader, 0, &vbf);

   gl->g_pTexture = Xe_CreateTexture(gl->gl_device, XE_W, XE_H, 1, XE_FMT_5551 | XE_FMT_16BE, 0);
   gl->g_pTexture->use_filtering = 1;

   edram_init(gl->gl_device);

   /* enable filtering for now */

   float x = -1.0f;
   float y = 1.0f;
   float w = 4.0f;
   float h = 4.0f;

   gl->vb = Xe_CreateVertexBuffer(gl->gl_device, 3 * sizeof(DrawVerticeFormats));
   DrawVerticeFormats *Rect = Xe_VB_Lock(gl->gl_device, gl->vb, 0, 3 * sizeof (DrawVerticeFormats), XE_LOCK_WRITE);

   ScreenUv[UV_TOP] = ScreenUv[UV_TOP] * 2;
   ScreenUv[UV_LEFT] = ScreenUv[UV_LEFT] * 2;

   /* top left */
   Rect[0].x = x;
   Rect[0].y = y;
   Rect[0].u = ScreenUv[UV_BOTTOM];
   Rect[0].v = ScreenUv[UV_RIGHT];
   Rect[0].color = 0;

   /* bottom left */
   Rect[1].x = x;
   Rect[1].y = y - h;
   Rect[1].u = ScreenUv[UV_BOTTOM];
   Rect[1].v = ScreenUv[UV_LEFT];
   Rect[1].color = 0;

   /* top right */
   Rect[2].x = x + w;
   Rect[2].y = y;
   Rect[2].u = ScreenUv[UV_TOP];
   Rect[2].v = ScreenUv[UV_RIGHT];
   Rect[2].color = 0;

   Rect[3].x = x + w;
   Rect[3].y = y;
   Rect[3].u = ScreenUv[UV_TOP];
   Rect[3].v = ScreenUv[UV_RIGHT];
   Rect[3].color = 0;

   int i = 0;
   for (i = 0; i < 3; i++)
   {
      Rect[i].z = 0.0;
      Rect[i].w = 1.0;
   }

   Xe_VB_Unlock(gl->gl_device, gl->vb);

   Xe_SetClearColor(gl->gl_device, 0);

   return gl;
}

static bool xenon360_gfx_frame(void *data, const void *frame, unsigned width, unsigned height,
      uint64_t frame_count, unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   gl_t *vid = data;

   ScreenUv[UV_TOP]	= ((float) (width) / (float) XE_W)*2;
   ScreenUv[UV_LEFT]	= ((float) (height) / (float) XE_H)*2;

   DrawVerticeFormats * Rect = Xe_VB_Lock(vid->gl_device, vid->vb, 0, 3 * sizeof(DrawVerticeFormats), XE_LOCK_WRITE);

   /* bottom left */
   Rect[1].v = ScreenUv[UV_LEFT];
   Rect[2].u = ScreenUv[UV_TOP];

   Xe_VB_Unlock(vid->gl_device, vid->vb);

   /* Refresh texture cache */
   uint16_t *dst       = Xe_Surface_LockRect(vid->gl_device, vid->g_pTexture, 0, 0, 0, 0, XE_LOCK_WRITE);
   const uint16_t *src = frame;
   unsigned stride_in  = pitch >>1;
   unsigned stride_out = vid->g_pTexture->wpitch >> 1;
   unsigned copy_size  = width << 1;

   for (unsigned y = 0; y < height; y++, dst += stride_out, src += stride_in)
      memcpy(dst, src, copy_size);
   Xe_Surface_Unlock(vid->gl_device, vid->g_pTexture);

   /* Reset states */
   Xe_InvalidateState(vid->gl_device);
   Xe_SetClearColor(vid->gl_device, 0);

   /* Select stream */
   Xe_SetTexture(vid->gl_device, 0, vid->g_pTexture);
   Xe_SetCullMode(vid->gl_device, XE_CULL_NONE);
   Xe_SetStreamSource(vid->gl_device, 0, vid->vb, 0, sizeof(DrawVerticeFormats));

   /* Select shaders */
   Xe_SetShader(vid->gl_device, SHADER_TYPE_PIXEL, vid->g_pPixelTexturedShader, 0);
   Xe_SetShader(vid->gl_device, SHADER_TYPE_VERTEX, vid->g_pVertexShader, 0);

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   /* Draw */
   Xe_DrawPrimitive(vid->gl_device, XE_PRIMTYPE_TRIANGLELIST, 0, 1);

   /* Resolve */
   Xe_Resolve(vid->gl_device);
   Xe_Sync(vid->gl_device);

   return true;
}

static void xenon360_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool xenon360_gfx_alive(void *data)
{
   (void)data;
   return !g_quitting;
}

static bool xenon360_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool xenon360_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool xenon360_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void xenon360_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   (void)iface;
}

video_driver_t video_xenon360 = {
   xenon360_gfx_init,
   xenon360_gfx_frame,
   xenon360_gfx_set_nonblock_state,
   xenon360_gfx_alive,
   xenon360_gfx_focus,
   xenon360_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   xenon360_gfx_set_shader,
   xenon360_gfx_free,
   "xenon360",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   xenon360_gfx_get_poke_interface
};
