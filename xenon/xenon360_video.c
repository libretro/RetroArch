/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <xenos/xe.h>
#include <xenos/xenos.h>
#include <xenos/edram.h>
#include <xenos/xenos.h>

#include "driver.h"
#include "general.h"

#define TEX_W 512
#define TEX_H 512

static const uint32_t g_PS[] =
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

static const uint32_t g_VS[] =
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

struct Vertex
{
   float x, y, z, w;
   uint32_t color;
   float u, v;
};

static bool g_quitting;

typedef struct xe
{
   struct XenosDevice dev;
   struct XenosVertexBuffer *vb;
   struct XenosShader *vertex_shader;
   struct XenosShader *pixel_shader;
   struct XenosSurface *tex;
   unsigned frame_count;
} xe_t;

static void xenon360_gfx_free(void *data)
{
   xe_t *vid = data;
   if (!vid)
      return;

   // FIXME: Proper deinitialization of device resources.

   free(vid);
}

static void init_vertex(xe_t *xe)
{
   xe->vb = Xe_CreateVertexBuffer(&xe->dev, 4 * sizeof(struct Vertex));
   struct Vertex *rect = Xe_VB_Lock(&xe->dev, xe->vb, 0, 4 * sizeof(struct Vertex), XE_LOCK_WRITE);

   rect[0].x = -1.0;
   rect[0].y = -1.0;
   rect[0].u = 0.0;
   rect[0].v = 0.0;
   rect[0].color = 0;

   rect[1].x = 1.0;
   rect[1].y = -1.0;
   rect[1].u = 1.0;
   rect[1].v = 1.0;
   rect[1].color = 0;

   rect[2].x = -1.0;
   rect[2].y = 1.0;
   rect[2].u = 0.0;
   rect[2].v = 1.0;
   rect[2].color = 0;

   rect[3].x = 1.0;
   rect[3].y = 1.0;
   rect[3].u = 1.0;
   rect[3].v = 1.0;
   rect[3].color = 0;

   for (unsigned i = 0; i < 4; i++)
   {
      rect[i].z = 0.0;
      rect[i].w = 1.0;
   }

   Xe_VB_Unlock(&xe->dev, xe->vb);
}

static void *xenon360_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   xe_t *xe = calloc(1, sizeof(*xe));
   if (!xe)
      return NULL;

   Xe_Init(&xe->dev);
   Xe_SetRenderTarget(&xe->dev, Xe_GetFramebufferSurface(&xe->dev));

   init_vertex(xe);

   static const struct XenosVBFFormat vbf = {
      3,
      {
         { XE_USAGE_POSITION, 0, XE_TYPE_FLOAT4 },
         { XE_USAGE_COLOR,    0, XE_TYPE_UBYTE4 },
         { XE_USAGE_TEXCOORD, 0, XE_TYPE_FLOAT2 },
      },
   };

   xe->pixel_shader = Xe_LoadShaderFromMemory(&xe->dev, (void*)g_PS);
   Xe_InstantiateShader(&xe->dev, xe->pixel_shader, 0);

   xe->vertex_shader = Xe_LoadShaderFromMemory(&xe->dev, (void*)g_VS);
   Xe_InstantiateShader(&xe->dev, xe->vertex_shader, 0);
   Xe_ShaderApplyVFetchPatches(&xe->dev, xe->vertex_shader, 0, &vbf);

   Xe_SetShader(&xe->dev, SHADER_TYPE_PIXEL, xe->pixel_shader, 0);
   Xe_SetShader(&xe->dev, SHADER_TYPE_VERTEX, xe->vertex_shader, 0);
   edram_init(&xe->dev);

   Xe_SetClearColor(&xe->dev, 0);

   xe->tex = Xe_CreateTexture(&xe->dev, TEX_W, TEX_H, 1, XE_FMT_5551, 0);
   xe->tex->use_filtering = true;
   Xe_SetTexture(&xe->dev, 0, xe->tex);

   Xe_SetClearColor(&xe->dev, 0);
   Xe_SetCullMode(&xe->dev, XE_CULL_NONE);
   Xe_SetStreamSource(&xe->dev, 0, xe->vb, 0, sizeof(struct Vertex));

   return xe;
}

static bool xenon360_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   xe_t *xe = data;
   xe->frame_count++;

   //struct Vertex *rect = Xe_VB_Lock(vid->xe_device, vid->vb, 0, 4 * sizeof(struct Vertex), XE_LOCK_WRITE);
   // FIXME: Proper UV handling goes here later.
   //Xe_VB_Unlock(&xe->dev, xe->vb);

   Xe_InvalidateState(&xe->dev);

   uint16_t *dst = Xe_Surface_LockRect(&xe->dev, xe->tex, 0, 0, 0, 0, XE_LOCK_WRITE);
   const uint16_t *src = frame;
   unsigned stride_in = pitch >> 1;
   unsigned stride_out = xe->tex->wpitch >> 1;
   unsigned copy_size = width << 1;

   for (unsigned y = 0; y < height; y++, dst += stride_out, src += stride_in)
      memcpy(dst, src, copy_size);

   Xe_Surface_Unlock(&xe->dev, xe->tex);
   
   Xe_DrawPrimitive(&xe->dev, XE_PRIMTYPE_TRIANGLESTRIP, 0, 2);

   Xe_SetClearColor(&xe->dev, 0);
   Xe_Resolve(&xe->dev);
   Xe_Sync(&xe->dev);

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

const video_driver_t video_xenon360 = {
   .init = xenon360_gfx_init,
   .frame = xenon360_gfx_frame,
   .alive = xenon360_gfx_alive,
   .set_nonblock_state = xenon360_gfx_set_nonblock_state,
   .focus = xenon360_gfx_focus,
   .free = xenon360_gfx_free,
   .ident = "xenon360"
};

