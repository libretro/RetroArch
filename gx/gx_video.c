/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *  Copyright (C) 2012 - Michael Lelli
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

#include "../driver.h"
#include "../general.h"
#include "gx_video.h"
#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

void *g_framebuf[2];
unsigned g_current_framebuf;

bool g_vsync;
lwpq_t g_video_cond;
volatile bool g_draw_done;

struct
{
   uint32_t data[512 * 256];
   GXTexObj obj;
} g_tex ATTRIBUTE_ALIGN(32);

struct
{
   uint32_t data[240 * 320];
   GXTexObj obj;
} menu_tex ATTRIBUTE_ALIGN(32);

uint8_t gx_fifo[256 * 1024] ATTRIBUTE_ALIGN(32);
uint8_t display_list[1024] ATTRIBUTE_ALIGN(32);
size_t display_list_size;

float verts[16] ATTRIBUTE_ALIGN(32) = {
   -1,  1, -0.5,
   -1, -1, -0.5,
    1, -1, -0.5,
    1,  1, -0.5,
};

float tex_coords[8] ATTRIBUTE_ALIGN(32) = {
   0, 0,
   0, 1,
   1, 1,
   1, 0,
};

float tex_coords_90[8] ATTRIBUTE_ALIGN(32) = {
   0, 1,
   1, 1,
   1, 0,
   0, 0
};

float tex_coords_180[8] ATTRIBUTE_ALIGN(32) = {
   1, 1,
   1, 0,
   0, 0,
   0, 1
};

float tex_coords_270[8] ATTRIBUTE_ALIGN(32) = {
   1, 0,
   0, 0,
   0, 1,
   1, 1
};

float *vertex_ptr = tex_coords;

static void retrace_callback(u32 retrace_count)
{
   (void)retrace_count;
   g_draw_done = true;
   LWP_ThreadSignal(g_video_cond);
}

static void setup_video_mode(GXRModeObj *mode)
{
   VIDEO_Configure(mode);
   for (unsigned i = 0; i < 2; i++)
   {
      g_framebuf[i] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
      VIDEO_ClearFrameBuffer(mode, g_framebuf[i], COLOR_BLACK);
   }

   g_current_framebuf = 0;
   g_draw_done = true;
   LWP_InitQueue(&g_video_cond);
   VIDEO_SetNextFramebuffer(g_framebuf[0]);
   VIDEO_SetPostRetraceCallback(retrace_callback);
   VIDEO_SetBlack(false);
   VIDEO_Flush();
   VIDEO_WaitVSync();
   if (mode->viTVMode & VI_NON_INTERLACE)
      VIDEO_WaitVSync();
}

static void init_vtx(GXRModeObj *mode)
{
   GX_SetViewport(0, 0, mode->fbWidth, mode->efbHeight, 0, 1);
   GX_SetDispCopyYScale(GX_GetYScaleFactor(mode->efbHeight, mode->xfbHeight));
   GX_SetScissor(0, 0, mode->fbWidth, mode->efbHeight);
   GX_SetDispCopySrc(0, 0, mode->fbWidth, mode->efbHeight);
   GX_SetDispCopyDst(mode->fbWidth, mode->xfbHeight);
   GX_SetCopyFilter(mode->aa, mode->sample_pattern, (mode->xfbMode == VI_XFBMODE_SF) ? GX_FALSE : GX_TRUE,
         mode->vfilter);
   GX_SetCopyClear((GXColor) { 0, 0, 0, 0xff }, GX_MAX_Z24);
   GX_SetFieldMode(mode->field_rendering, (mode->viHeight == 2 * mode->xfbHeight) ? GX_ENABLE : GX_DISABLE);

   GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
   GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_ENABLE);
   GX_SetColorUpdate(GX_TRUE);
   GX_SetAlphaUpdate(GX_FALSE);

   Mtx44 m;
   guOrtho(m, 1, -1, -1, 1, 0.4, 0.6);
   GX_LoadProjectionMtx(m, GX_ORTHOGRAPHIC);

   GX_ClearVtxDesc();
   GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);

   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
   GX_SetArray(GX_VA_POS, verts, 3 * sizeof(float));
   GX_SetArray(GX_VA_TEX0, vertex_ptr, 2 * sizeof(float));

   GX_SetNumTexGens(1);
   GX_SetNumChans(0);
   GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
   GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
   GX_InvVtxCache();

   GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_INVSRCALPHA, 0);

   GX_Flush();
}

static void init_texture(unsigned width, unsigned height)
{
   unsigned g_filter = g_settings.video.smooth ? GX_LINEAR : GX_NEAR;

   GX_InitTexObj(&g_tex.obj, g_tex.data, width, height, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
   GX_InitTexObjLOD(&g_tex.obj, g_filter, g_filter, 0, 0, 0, GX_TRUE, GX_FALSE, GX_ANISO_1);
   GX_InitTexObj(&menu_tex.obj, menu_tex.data, 320, 240, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
   GX_InitTexObjLOD(&menu_tex.obj, g_filter, g_filter, 0, 0, 0, GX_TRUE, GX_FALSE, GX_ANISO_1);
   GX_InvalidateTexAll();
}

static void build_disp_list(void)
{
   DCInvalidateRange(display_list, sizeof(display_list));
   GX_BeginDispList(display_list, sizeof(display_list));
   GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
   for (unsigned i = 0; i < 4; i++)
   {
      GX_Position1x8(i);
      GX_TexCoord1x8(i);
   }
   GX_End();
   display_list_size = GX_EndDispList();
}

static void gx_stop(void)
{
   GX_AbortFrame();
   GX_Flush();
   VIDEO_SetBlack(true);
   VIDEO_Flush();

   for (unsigned i = 0; i < 2; i++)
      free(MEM_K1_TO_K0(g_framebuf[i]));
}

static void gx_restart(void)
{
}

static void *wii_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   if (driver.video_data)
      return driver.video_data;

   gx_video_t *gx = (gx_video_t*)calloc(1, sizeof(gx_video_t));
   if (!gx)
      return NULL;

   g_vsync = video->vsync;

   return gx;
}

static void gx_start(void)
{
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.fullscreen = true;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;

   driver.video_data = wii_init(&video_info, NULL, NULL);

   //gx_video_t *gx = (gx_video_t*)driver.video_data;
   VIDEO_Init();
   GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
   setup_video_mode(mode);

   GX_Init(gx_fifo, sizeof(gx_fifo));
   GX_SetDispCopyGamma(GX_GM_1_0);
   GX_SetCullMode(GX_CULL_NONE);
   GX_SetClipMode(GX_CLIP_DISABLE);

   init_vtx(mode);
   build_disp_list();

   g_vsync = true;
}

#if 0
// TODO: Fix

static void update_texture_asm(const uint32_t *src,
      unsigned width, unsigned height, unsigned pitch)
{
   register uint32_t tmp0, tmp1, tmp2, tmp3, line2, line2b, line3, line3b, line4, line4b, line5;
   register uint32_t ormask = 0x80008000u;
   register uint32_t *dst = g_tex.data;

   __asm__ __volatile__ (
      "     srwi     %[width],   %[width],   2           \n"
      "     srwi     %[height],  %[height],  2           \n"
      "     subi     %[tmp3],    %[dst],     4           \n"
      "     mr       %[dst],     %[tmp3]                 \n"
      "     subi     %[dst],     %[dst],     4           \n"
      "     mr       %[line2],   %[pitch]                \n"
      "     addi     %[line2b],  %[line2],   4           \n"
      "     mulli    %[line3],   %[pitch],   2           \n"
      "     addi     %[line3b],  %[line3],   4           \n"
      "     mulli    %[line4],   %[pitch],   3           \n"
      "     addi     %[line4b],  %[line4],   4           \n"
      "     mulli    %[line5],   %[pitch],   4           \n"

      "2:   mtctr    %[width]                            \n"
      "     mr       %[tmp0],    %[src]                  \n"

      "1:   lwz      %[tmp1],    0(%[src])               \n"
      "     or       %[tmp1],    %[tmp1],    %[ormask]   \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwz      %[tmp2],    4(%[src])               \n"
      "     or       %[tmp2],    %[tmp2],    %[ormask]   \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line2],   %[src]      \n"
      "     or       %[tmp1],    %[tmp1],    %[ormask]   \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line2b],  %[src]      \n"
      "     or       %[tmp2],    %[tmp2],    %[ormask]   \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line3],   %[src]      \n"
      "     or       %[tmp1],    %[tmp1],    %[ormask]   \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line3b],  %[src]      \n"
      "     or       %[tmp2],    %[tmp2],    %[ormask]   \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line4],   %[src]      \n"
      "     or       %[tmp1],    %[tmp1],    %[ormask]   \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line4b],  %[src]      \n"
      "     or       %[tmp2],    %[tmp2],    %[ormask]   \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     addi     %[src],     %[src],     8           \n"
      "     bdnz     1b                                  \n"

      "     add      %[src],     %[tmp0],    %[line5]    \n"
      "     subic.   %[height],  %[height],  1           \n"
      "     bne      2b                                  \n"
      :  [tmp0]   "=&b" (tmp0),
         [tmp1]   "=&b" (tmp1),
         [tmp2]   "=&b" (tmp2),
         [tmp3]   "=&b" (tmp3),
         [line2]  "=&b" (line2),
         [line2b] "=&b" (line2b),
         [line3]  "=&b" (line3),
         [line3b] "=&b" (line3b),
         [line4]  "=&b" (line4),
         [line4b] "=&b" (line4b),
         [line5]  "=&b" (line5),
         [dst]    "+b"  (dst)
      :  [src]    "b"   (src),
         [width]  "b"   (width),
         [height] "b"   (height),
         [pitch]  "b"   (pitch),
         [ormask] "b"   (ormask)
   );
}

#endif

// Set MSB to get full RGB555.
#define RGB15toRGB5A3(col) ((col) | 0x80008000u)

#define BLIT_LINE(off) \
{ \
   const uint32_t *tmp_src = src; \
   uint32_t *tmp_dst = dst; \
   for (unsigned x = 0; x < width2; x += 8, tmp_src += 8, tmp_dst += 32) \
   { \
      tmp_dst[ 0 + off] = RGB15toRGB5A3(tmp_src[0]); \
      tmp_dst[ 1 + off] = RGB15toRGB5A3(tmp_src[1]); \
      tmp_dst[ 8 + off] = RGB15toRGB5A3(tmp_src[2]); \
      tmp_dst[ 9 + off] = RGB15toRGB5A3(tmp_src[3]); \
      tmp_dst[16 + off] = RGB15toRGB5A3(tmp_src[4]); \
      tmp_dst[17 + off] = RGB15toRGB5A3(tmp_src[5]); \
      tmp_dst[24 + off] = RGB15toRGB5A3(tmp_src[6]); \
      tmp_dst[25 + off] = RGB15toRGB5A3(tmp_src[7]); \
   } \
   src += pitch; \
}

#define BLIT_16(x) \
{ \
   block[0 + 0 ] = line[x][0]; \
   block[0 + 16] = line[x][1]; \
   block[1 + 0 ] = line[x][2]; \
   block[1 + 16] = line[x][3]; \
   block[2 + 0 ] = line[x][4]; \
   block[2 + 16] = line[x][5]; \
   block[3 + 0 ] = line[x][6]; \
   block[3 + 16] = line[x][7]; \
   block += 4; \
   line[x] += 8; \
}

static void update_texture(const uint32_t *src,
      unsigned width, unsigned height, unsigned pitch)
{
   gx_video_t *gx = (gx_video_t*)driver.video_data;
#if 0
   if (!(width & 3) && !(height & 3))
   {
      update_texture_asm(src, width, height, pitch);
   }
   else
#endif
   {
      pitch >>= 2;
      width &= ~15;
      height &= ~3;
      unsigned width2 = width >> 1;

      // Texture data is 4x4 tiled @ 15bpp.
      // Use 32-bit to transfer more data per cycle.
      uint32_t *dst = g_tex.data;
      for (unsigned i = 0; i < height; i += 4, dst += 4 * width2)
      {
         BLIT_LINE(0)
         BLIT_LINE(2)
         BLIT_LINE(4)
         BLIT_LINE(6)
      }
   }

   // TODO: only convert when menu is visible
   if(gx->menu_render)
   {
      uint16_t *block = (uint16_t *) menu_tex.data;
      uint16_t *line[4];
      for (uint32_t y = 0; y < 240; y += 4)
      {
         uint32_t *menu_data = gx->menu_data;
         // fetch the next 4 scanlines
         line[0] = (uint16_t *) &menu_data[(y + 0) * 320];
         line[1] = (uint16_t *) &menu_data[(y + 1) * 320];
         line[2] = (uint16_t *) &menu_data[(y + 2) * 320];
         line[3] = (uint16_t *) &menu_data[(y + 3) * 320];

         for (unsigned x = 0; x < 320; x += 4)
         {
            BLIT_16(0)
            BLIT_16(1)
            BLIT_16(2)
            BLIT_16(3)

            block += 16;
         }
      }
   }

   init_texture(width, height);
   DCFlushRange(g_tex.data, sizeof(g_tex.data));
   DCFlushRange(menu_tex.data, sizeof(menu_tex.data));
   GX_InvalidateTexAll();
}

static bool wii_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   gx_video_t *gx = (gx_video_t*)driver.video_data;
   bool menu_render = gx->menu_render;

   (void)data;
   (void)msg;

   if(!frame && !menu_render)
      return true;

   gx->frame_count++;

   while (g_vsync && !g_draw_done)
      LWP_ThreadSleep(g_video_cond);

   g_draw_done = false;
   g_current_framebuf ^= 1;
   update_texture(frame, width, height, pitch);

   if (frame)
   {
      GX_LoadTexObj(&g_tex.obj, GX_TEXMAP0);
      GX_CallDispList(display_list, display_list_size);
      GX_DrawDone();
   }

   if(menu_render)
   {
      GX_LoadTexObj(&menu_tex.obj, GX_TEXMAP0);
      GX_CallDispList(display_list, display_list_size);
      GX_DrawDone();
   }

   GX_CopyDisp(g_framebuf[g_current_framebuf], GX_TRUE);
   GX_Flush();
   VIDEO_SetNextFramebuffer(g_framebuf[g_current_framebuf]);
   VIDEO_Flush();

   return true;
}

static void wii_set_nonblock_state(void *data, bool state)
{
   (void)data;
   g_vsync = !state;
}

static bool wii_alive(void *data)
{
   (void)data;
   return true;
}

static bool wii_focus(void *data)
{
   (void)data;
   return true;
}

static void wii_free(void *data)
{
   (void)data;
}

static void wii_set_rotation(void * data, uint32_t orientation)
{
   (void)data;
   (void)orientation;

   switch(orientation)
   {
      case ORIENTATION_NORMAL:
         vertex_ptr = tex_coords;
         break;
      case ORIENTATION_VERTICAL:
         vertex_ptr = tex_coords_90;
         break;
      case ORIENTATION_FLIPPED:
         vertex_ptr = tex_coords_180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         vertex_ptr = tex_coords_270;
         break;
   }

   GX_SetArray(GX_VA_TEX0, vertex_ptr, 2 * sizeof(float));
}

const video_driver_t video_wii = {
   .init = wii_init,
   .frame = wii_frame,
   .alive = wii_alive,
   .set_nonblock_state = wii_set_nonblock_state,
   .focus = wii_focus,
   .free = wii_free,
   .ident = "wii",
   .set_rotation = wii_set_rotation,
   .start = gx_start,
   .stop = gx_stop,
   .restart = gx_restart,
};
