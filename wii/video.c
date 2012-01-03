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

#include "../driver.h"
#include "../general.h"
#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

// All very hardcoded for now.

static void *g_framebuf[2];
static unsigned g_framebuf_index;
static unsigned g_filter;

struct
{
   uint32_t data[512 * 256];
   GXTexObj obj;
} static g_tex ATTRIBUTE_ALIGN(32);

static uint8_t gx_fifo[256 * 1024] ATTRIBUTE_ALIGN(32);
static uint8_t display_list[1024] ATTRIBUTE_ALIGN(32);
static size_t display_list_size;

static void setup_video_mode(GXRModeObj *mode)
{
   VIDEO_Configure(mode);
   for (unsigned i = 0; i < 2; i++)
   {
      g_framebuf[i] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
      VIDEO_ClearFrameBuffer(mode, g_framebuf[i], COLOR_BLACK);
   }

   VIDEO_SetNextFramebuffer(g_framebuf[0]);
   VIDEO_SetBlack(false);
   VIDEO_Flush();
   VIDEO_WaitVSync();
   if (mode->viTVMode & VI_NON_INTERLACE)
      VIDEO_WaitVSync();
}

static float verts[16] ATTRIBUTE_ALIGN(32)  = {
   -1,  1, -0.5,
   -1, -1, -0.5,
    1, -1, -0.5,
    1,  1, -0.5,
};

static float tex_coords[8] ATTRIBUTE_ALIGN(32) = {
   0, 0,
   0, 1,
   1, 1,
   1, 0,
};

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
   GX_SetArray(GX_VA_TEX0, tex_coords, 2 * sizeof(float));

   GX_SetNumTexGens(1);
   GX_SetNumChans(0);
   GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
   GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
   GX_InvVtxCache();

   GX_Flush();
}

static void init_texture(unsigned width, unsigned height)
{
   GX_InitTexObj(&g_tex.obj, g_tex.data, width, height, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
   GX_InitTexObjLOD(&g_tex.obj, g_filter, g_filter, 0, 0, 0, GX_TRUE, GX_FALSE, GX_ANISO_1);
   GX_LoadTexObj(&g_tex.obj, GX_TEXMAP0);
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

static void *wii_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   VIDEO_Init();
   GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
   setup_video_mode(mode);

   GX_Init(gx_fifo, sizeof(gx_fifo));
   GX_SetDispCopyGamma(GX_GM_1_0);
   GX_SetCullMode(GX_CULL_NONE);
   GX_SetClipMode(GX_CLIP_DISABLE);

   init_vtx(mode);
   build_disp_list();
   g_filter = video->smooth ? GX_LINEAR : GX_NEAR;

   *input = NULL;
   *input_data = NULL;
   return (void*)-1;
}

static void update_texture(const uint32_t *src,
      unsigned width, unsigned height, unsigned pitch)
{
   pitch >>= 2;
   width &= ~15;
   height &= ~3;

// Set MSB to get full RGB555.
#define RGB15toRGB5A3(col) ((col) | 0x80008000u)

#define BLIT_CHUNK(off) { \
         tmp_dst[ 0 + off] = RGB15toRGB5A3(tmp_src[0]); \
         tmp_dst[ 1 + off] = RGB15toRGB5A3(tmp_src[1]); \
         tmp_dst[ 8 + off] = RGB15toRGB5A3(tmp_src[2]); \
         tmp_dst[ 9 + off] = RGB15toRGB5A3(tmp_src[3]); \
         tmp_dst[16 + off] = RGB15toRGB5A3(tmp_src[4]); \
         tmp_dst[17 + off] = RGB15toRGB5A3(tmp_src[5]); \
         tmp_dst[24 + off] = RGB15toRGB5A3(tmp_src[6]); \
         tmp_dst[25 + off] = RGB15toRGB5A3(tmp_src[7]); }

#define BLIT_LINE(off) { \
   const uint32_t *tmp_src = src; \
   uint32_t *tmp_dst = dst; \
   for (unsigned x = 0; x < width; x += 8, tmp_src += 8, tmp_dst += 32) \
      BLIT_CHUNK(off) \
   src += pitch; }

   width >>= 1;

   // Texture data is 4x4 tiled @ 15bpp.
   // Use 32-bit to transfer more data per cycle.
   uint32_t *dst = g_tex.data;
   for (unsigned i = 0; i < height; i += 4, dst += 4 * width)
   {
      BLIT_LINE(0)
      BLIT_LINE(2)
      BLIT_LINE(4)
      BLIT_LINE(6)
   }
   
   init_texture(width << 1, height);
   DCFlushRange(g_tex.data, sizeof(g_tex.data));
   GX_InvalidateTexAll();
}

static bool wii_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   (void)data;
   (void)msg;

   update_texture(frame, width, height, pitch);
   GX_CallDispList(display_list, display_list_size);
   GX_DrawDone();

   g_framebuf_index ^= 1;
   GX_CopyDisp(g_framebuf[g_framebuf_index], GX_TRUE);
   GX_Flush();

   VIDEO_SetNextFramebuffer(g_framebuf[g_framebuf_index]);
   VIDEO_Flush();
   //VIDEO_WaitVSync();

   return true;
}

static void wii_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
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
   GX_AbortFrame();
   GX_Flush();
   VIDEO_SetBlack(true);
   VIDEO_Flush();

   for (unsigned i = 0; i < 2; i++)
      free(MEM_K1_TO_K0(g_framebuf[i]));
}

const video_driver_t video_wii = {
   .init = wii_init,
   .frame = wii_frame,
   .alive = wii_alive,
   .set_nonblock_state = wii_set_nonblock_state,
   .focus = wii_focus,
   .free = wii_free,
   .ident = "wii"
};

