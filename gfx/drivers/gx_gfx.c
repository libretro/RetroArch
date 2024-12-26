/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include <gccore.h>
#include <ogcsys.h>

#include <libretro.h>
#include <verbosity.h>
#include <streams/interface_stream.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HW_RVL
#include "../../memory/wii/mem2_manager.h"
#endif

#include <defines/gx_defines.h>

#include "../drivers_font_renderer/bitmap.h"
#include "../../configuration.h"
#include "../../driver.h"

#ifndef _CPU_ISR_Disable
#define _CPU_ISR_Disable( _isr_cookie ) \
  { register u32 _disable_mask = 0; \
	_isr_cookie = 0; \
    __asm__ __volatile__ ( \
	  "mfmsr %0\n" \
	  "rlwinm %1,%0,0,17,15\n" \
	  "mtmsr %1\n" \
	  "extrwi %0,%0,1,16" \
	  : "=&r" ((_isr_cookie)), "=&r" ((_disable_mask)) \
	  : "0" ((_isr_cookie)), "1" ((_disable_mask)) \
	); \
  }
#endif

#ifndef _CPU_ISR_Restore
#define _CPU_ISR_Restore( _isr_cookie )  \
  { register u32 _enable_mask = 0; \
	__asm__ __volatile__ ( \
    "    cmpwi %0,0\n" \
	"    beq 1f\n" \
	"    mfmsr %1\n" \
	"    ori %1,%1,0x8000\n" \
	"    mtmsr %1\n" \
	"1:" \
	: "=r"((_isr_cookie)),"=&r" ((_enable_mask)) \
	: "0"((_isr_cookie)),"1" ((_enable_mask)) \
	); \
  }
#endif

enum
{
   GX_RESOLUTIONS_DEFAULT = 0,
   GX_RESOLUTIONS_512_192,
   GX_RESOLUTIONS_598_200,
   GX_RESOLUTIONS_640_200,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_448_224,
   GX_RESOLUTIONS_480_224,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_340_232,
   GX_RESOLUTIONS_512_232,
   GX_RESOLUTIONS_512_236,
   GX_RESOLUTIONS_336_240,
   GX_RESOLUTIONS_352_240,
   GX_RESOLUTIONS_384_240,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_530_240,
   GX_RESOLUTIONS_608_240,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_512_384,
   GX_RESOLUTIONS_598_400,
   GX_RESOLUTIONS_640_400,
   GX_RESOLUTIONS_384_448,
   GX_RESOLUTIONS_448_448,
   GX_RESOLUTIONS_480_448,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_576_448,
   GX_RESOLUTIONS_608_448,
   GX_RESOLUTIONS_640_448,
   GX_RESOLUTIONS_340_464,
   GX_RESOLUTIONS_512_464,
   GX_RESOLUTIONS_512_472,
   GX_RESOLUTIONS_352_480,
   GX_RESOLUTIONS_384_480,
   GX_RESOLUTIONS_512_480,
   GX_RESOLUTIONS_530_480,
   GX_RESOLUTIONS_608_480,
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST = GX_RESOLUTIONS_640_480,
};

struct gx_overlay_data
{
   GXTexObj tex;
   float tex_coord[8];
   float vertex_coord[8];
   float alpha_mod;
};

typedef struct gx_video
{
   video_viewport_t vp;
   void *framebuf[2];
   uint32_t *menu_data; /* FIXME: Should be const uint16_t*. */
#ifdef HAVE_OVERLAY
   struct gx_overlay_data *overlay;
#endif

   unsigned scale;
   unsigned overscan_correction_top;
   unsigned overscan_correction_bottom;
   unsigned old_width;
   unsigned old_height;
   unsigned current_framebuf;
#ifdef HAVE_OVERLAY
   unsigned overlays;
#endif
   uint32_t orientation;
   uint16_t xOrigin;
   uint16_t yOrigin;
   int8_t system_xOrigin;
   int8_t used_system_xOrigin;
   int8_t xOriginNeg;
   int8_t xOriginPos;
   int8_t yOriginNeg;
   int8_t yOriginPos;

   bool should_resize;
   bool keep_aspect;
   bool double_strike;
   bool rgb32;
   bool menu_texture_enable;
   bool vsync;
#ifdef HAVE_OVERLAY
   bool overlay_enable;
   bool overlay_full_screen;
#endif
} gx_video_t;

static struct
{
   uint32_t *data; /* needs to be resizable. */
   unsigned width;
   unsigned height;
   GXTexObj obj;
} g_tex;

static struct
{
   uint32_t data[240 * 212];
   GXTexObj obj;
} menu_tex ATTRIBUTE_ALIGN(32);

static OSCond g_video_cond;

static volatile bool g_draw_done       = false;

static uint8_t gx_fifo[256 * 1024] ATTRIBUTE_ALIGN(32);
static uint8_t display_list[1024]  ATTRIBUTE_ALIGN(32);

static uint32_t retraceCount           = 0;
static uint32_t referenceRetraceCount  = 0;

static unsigned max_height             = 0;

static size_t display_list_size        = 0;

static GXRModeObj gx_mode;

float verts[16] ATTRIBUTE_ALIGN(32)     = {
   -1,  1, -0.5,
    1,  1, -0.5,
   -1, -1, -0.5,
    1, -1, -0.5,
};

float vertex_ptr[8] ATTRIBUTE_ALIGN(32) = {
   0, 0,
   1, 0,
   0, 1,
   1, 1,
};

u8 color_ptr[16] ATTRIBUTE_ALIGN(32)    = {
   0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF,
};

unsigned menu_gx_resolutions[][2] = {
   { 0, 0 }, /* Let the system choose its preferred resolution, for NTSC is 640x480 */
   { 512, 192 },
   { 598, 200 },
   { 640, 200 },
   { 384, 224 },
   { 448, 224 },
   { 480, 224 },
   { 512, 224 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 340, 232 },
   { 512, 232 },
   { 512, 236 },
   { 336, 240 },
   { 352, 240 },
   { 384, 240 },
   { 512, 240 },
   { 530, 240 },
   { 608, 240 },
   { 640, 240 },
   { 512, 384 },
   { 598, 400 },
   { 640, 400 },
   { 384, 448 },
   { 448, 448 },
   { 480, 448 },
   { 512, 448 },
   { 576, 448 },
   { 608, 448 },
   { 640, 448 },
   { 340, 464 },
   { 512, 464 },
   { 512, 472 },
   { 352, 480 },
   { 384, 480 },
   { 512, 480 },
   { 530, 480 },
   { 608, 480 },
   { 640, 480 },
};

/*
 * FORWARD DECLARATIONS
 */
extern syssram* __SYS_LockSram(void);
extern u32 __SYS_UnlockSram(u32 write);

static void retrace_callback(u32 retrace_count)
{
   uint32_t level = 0;

   g_draw_done    = true;
   OSSignalCond(g_video_cond);
   _CPU_ISR_Disable(level);
   retraceCount = retrace_count;
   _CPU_ISR_Restore(level);
}

static bool gx_is_valid_xorigin(gx_video_t *gx, int origin)
{
   if (!gx)
      return false;
   if (     (origin < 0)
         || (origin + gx->used_system_xOrigin < 0)
         || (gx_mode.viWidth + origin + gx->used_system_xOrigin > 720))
      return false;
   return true;
}

static bool gx_is_valid_yorigin(int origin)
{
	if (origin < 0 || gx_mode.viHeight + origin > max_height)
	   return false;
	return true;
}

static void gx_set_video_mode(void *data, unsigned fbWidth, unsigned lines,
      bool fullscreen)
{
   int tmpOrigin;
   float refresh_rate;
   bool progressive, vfilter;
   unsigned modetype, tvmode, max_width, i, viWidth, rgui_aspect_ratio;
   GXColor color               = { 0, 0, 0, 0xff };
   unsigned viHeightMultiplier = 1;
   size_t new_fb_pitch         = 0;
   unsigned new_fb_width       = 0;
   unsigned new_fb_height      = 0;
   float y_scale               = 0.0f;
   uint16_t xfbWidth           = 0;
   uint16_t xfbHeight          = 0;
   settings_t *settings        = config_get_ptr();
   gx_video_t *gx              = (gx_video_t*)data;
   if (!gx || !settings)
      return;
   vfilter                     = settings->bools.video_vfilter;
   viWidth                     = settings->uints.video_viwidth;
   rgui_aspect_ratio           = settings->uints.menu_rgui_aspect_ratio;

   /* stop vsync callback */
   VIDEO_SetPostRetraceCallback(NULL);
   g_draw_done                 = false;
   /* wait for next even field */
   /* this prevents screen artifacts when switching
    * between interlaced & non-interlaced modes.
    *
    * But move on if it takes over 3 frames as sometimes under dolphin
    * this stays at constant value.
    */
   for (i = 0; i < 3; i++)
   {
     VIDEO_WaitVSync();
     if (VIDEO_GetNextField())
       break;
   }

   VIDEO_SetBlack(true);
   VIDEO_Flush();

#if defined(HW_RVL)
   progressive = CONF_GetProgressiveScan() > 0 && VIDEO_HaveComponentCable();

   switch (CONF_GetVideo())
   {
      case CONF_VIDEO_PAL:
         if (CONF_GetEuRGB60() > 0)
            tvmode = VI_EURGB60;
         else
            tvmode = VI_PAL;
         break;
      case CONF_VIDEO_MPAL:
         tvmode = VI_MPAL;
         break;
      default:
         tvmode = VI_NTSC;
         break;
   }
#else
   progressive = VIDEO_HaveComponentCable();
   tvmode      = VIDEO_GetCurrentTvMode();
#endif

   switch (tvmode)
   {
      case VI_PAL:
         max_width = VI_MAX_WIDTH_PAL;
         max_height = VI_MAX_HEIGHT_PAL;
         break;
      case VI_MPAL:
         max_width = VI_MAX_WIDTH_MPAL;
         max_height = VI_MAX_HEIGHT_MPAL;
         break;
      case VI_EURGB60:
         max_width = VI_MAX_WIDTH_EURGB60;
         max_height = VI_MAX_HEIGHT_EURGB60;
         break;
      default:
         tvmode = VI_NTSC;
         max_width = VI_MAX_WIDTH_NTSC;
         max_height = VI_MAX_HEIGHT_NTSC;
         break;
   }

   if (lines == 0 || fbWidth == 0)
   {
      GXRModeObj tmp_mode;
      VIDEO_GetPreferredMode(&tmp_mode);
      fbWidth = tmp_mode.fbWidth;
      lines = tmp_mode.xfbHeight;
   }

   if (lines <= max_height / 2)
   {
      modetype = VI_NON_INTERLACE;
      viHeightMultiplier = 2;
   }
   else
      modetype = progressive ? VI_PROGRESSIVE : VI_INTERLACE;

   if (lines > max_height)
      lines = max_height;

   if (fbWidth > max_width)
      fbWidth = max_width;

   gx_mode.viTVMode     = VI_TVMODE(tvmode, modetype);
   gx_mode.fbWidth      = fbWidth;
   gx_mode.efbHeight    = MIN(lines, 480);

   if (modetype == VI_NON_INTERLACE && lines > max_height / 2)
      gx_mode.xfbHeight    = max_height / 2;
   else if (modetype != VI_NON_INTERLACE && lines > max_height)
      gx_mode.xfbHeight    = max_height;
   else
      gx_mode.xfbHeight    = lines;

   gx_mode.viWidth         = viWidth;
   gx_mode.viHeight        = gx_mode.xfbHeight * viHeightMultiplier;

   gx->used_system_xOrigin = gx->system_xOrigin;

   if (gx->used_system_xOrigin > 0)
   {
      while (viWidth + gx->used_system_xOrigin > 720)
         gx->used_system_xOrigin--;
   }
   else if (gx->used_system_xOrigin < 0)
   {
      while (viWidth + gx->used_system_xOrigin > 720)
         gx->used_system_xOrigin++;
   }

   tmpOrigin = (max_width - gx_mode.viWidth) / 2;

   if (gx->system_xOrigin > 0)
   {
      while (!gx_is_valid_xorigin(gx, tmpOrigin))
         tmpOrigin--;
   }
   else if (gx->system_xOrigin < 0)
   {
      while (!gx_is_valid_xorigin(gx, tmpOrigin))
         tmpOrigin++;
   }

   gx_mode.viXOrigin = gx->xOrigin = tmpOrigin;
   gx_mode.viYOrigin = gx->yOrigin =
      (max_height - gx_mode.viHeight) / (2 * viHeightMultiplier);

   gx->xOriginNeg = 0;
   gx->xOriginPos = 0;

   while (gx_is_valid_xorigin(gx, gx_mode.viXOrigin+(gx->xOriginNeg-1)))
      gx->xOriginNeg--;
   while (gx_is_valid_xorigin(gx, gx_mode.viXOrigin+(gx->xOriginPos+1)))
      gx->xOriginPos++;
   gx->yOriginNeg = 0;
   gx->yOriginPos = 0;
   while (gx_is_valid_yorigin(gx_mode.viYOrigin+(gx->yOriginNeg-1)))
      gx->yOriginNeg--;
   while (gx_is_valid_yorigin(gx_mode.viYOrigin+(gx->yOriginPos+1)))
      gx->yOriginPos++;

   gx_mode.xfbMode         = (modetype == VI_INTERLACE)
      ? VI_XFBMODE_DF
      : VI_XFBMODE_SF;
   gx_mode.field_rendering = GX_FALSE;
   gx_mode.aa              = GX_FALSE;

   for (i = 0; i < 12; i++)
      gx_mode.sample_pattern[i][0] = gx_mode.sample_pattern[i][1] = 6;

   if (modetype != VI_NON_INTERLACE && vfilter)
   {
      gx_mode.vfilter[0] = 8;
      gx_mode.vfilter[1] = 8;
      gx_mode.vfilter[2] = 10;
      gx_mode.vfilter[3] = 12;
      gx_mode.vfilter[4] = 10;
      gx_mode.vfilter[5] = 8;
      gx_mode.vfilter[6] = 8;
   }
   else
   {
      gx_mode.vfilter[0] = 0;
      gx_mode.vfilter[1] = 0;
      gx_mode.vfilter[2] = 21;
      gx_mode.vfilter[3] = 22;
      gx_mode.vfilter[4] = 21;
      gx_mode.vfilter[5] = 0;
      gx_mode.vfilter[6] = 0;
   }

   gx->vp.full_width  = gx_mode.fbWidth;
   gx->vp.full_height = gx_mode.xfbHeight;
   gx->double_strike  = (modetype == VI_NON_INTERLACE);
   gx->should_resize  = true;

   /* Calculate menu dimensions
    * > Height is set as large as possible, limited to
    *   maximum of 240 (standard RGUI framebuffer height) */
   new_fb_height    = (gx_mode.efbHeight / (gx->double_strike ? 1 : 2)) & ~3;
   if (new_fb_height > 240)
      new_fb_height = 240;
   /* > Width is dertermined by current RGUI aspect ratio
    *   (note that width is in principal limited by hardware
    *    constraints to 640, but we impose a lower limit of
    *    424 since this is the nearest to the RGUI 'standard'
    *    for 16:9 aspect ratios which is supported by the Wii
    *    - i.e. last two bits of value must be zero, so 426->424) */
   switch (rgui_aspect_ratio)
   {
      case RGUI_ASPECT_RATIO_16_9:
      case RGUI_ASPECT_RATIO_16_9_CENTRE:
         if (new_fb_height == 240)
            new_fb_width = 424;
         else
            new_fb_width = (unsigned)((16.0f / 9.0f) * (float)new_fb_height) & ~3;
         break;
      case RGUI_ASPECT_RATIO_16_10:
      case RGUI_ASPECT_RATIO_16_10_CENTRE:
         if (new_fb_height == 240)
            new_fb_width = 384;
         else
            new_fb_width = (unsigned)((16.0f / 10.0f) * (float)new_fb_height) & ~3;
         break;
      case RGUI_ASPECT_RATIO_21_9:
      case RGUI_ASPECT_RATIO_21_9_CENTRE:
         if (new_fb_height == 240)
            new_fb_width = 560;
         else
            new_fb_width = (unsigned)((21.0f / 9.0f) * (float)new_fb_height) & ~3;
         break;
      default:
         /* 4:3 */
         if (new_fb_height == 240)
            new_fb_width = 320;
         else
            new_fb_width = (unsigned)((4.0f / 3.0f) * (float)new_fb_height) & ~3;
         break;
   }
   if (new_fb_width > 560)
      new_fb_width = 560;

   new_fb_pitch = new_fb_width * 2;

   {
      gfx_display_t *p_disp   = disp_get_ptr();
      p_disp->framebuf_width  = new_fb_width;
      p_disp->framebuf_height = new_fb_height;
      p_disp->framebuf_pitch  = new_fb_pitch;
   }

   GX_SetViewportJitter(0, 0, gx_mode.fbWidth, gx_mode.efbHeight, 0, 1, 1);
   GX_SetDispCopySrc(0, 0, gx_mode.fbWidth, gx_mode.efbHeight);

   y_scale   = GX_GetYScaleFactor(gx_mode.efbHeight, gx_mode.xfbHeight);
   xfbWidth  = VIDEO_PadFramebufferWidth(gx_mode.fbWidth);
   xfbHeight = GX_SetDispCopyYScale((f32)y_scale);
   GX_SetDispCopyDst((u16)xfbWidth, (u16)xfbHeight);

   GX_SetCopyFilter(gx_mode.aa, gx_mode.sample_pattern,
         GX_TRUE, gx_mode.vfilter);
   GX_SetCopyClear(color, GX_MAX_Z24);
   GX_SetFieldMode(gx_mode.field_rendering,
         (gx_mode.viHeight == 2 * gx_mode.xfbHeight) ? GX_ENABLE : GX_DISABLE);
   GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
   GX_InvalidateTexAll();
   GX_Flush();

   /* Now apply all the configuration to the screen */
   VIDEO_Configure(&gx_mode);
   VIDEO_ClearFrameBuffer(&gx_mode, gx->framebuf[0], COLOR_BLACK);
   VIDEO_ClearFrameBuffer(&gx_mode, gx->framebuf[1], COLOR_BLACK);
   VIDEO_SetNextFramebuffer(gx->framebuf[0]);
   gx->current_framebuf = 0;
   /* re-activate the Vsync callback */
   VIDEO_SetPostRetraceCallback(retrace_callback);
   VIDEO_SetBlack(false);
   VIDEO_Flush();
   VIDEO_WaitVSync();

   RARCH_LOG("[GX]: Resolution: %dx%d (%s)\n", gx_mode.fbWidth,
         gx_mode.efbHeight, (gx_mode.viTVMode & 3) == VI_INTERLACE
         ? "interlaced" : "progressive");

   if (tvmode == VI_PAL)
   {
      refresh_rate = 50.0f;
      if (modetype == VI_NON_INTERLACE)
         refresh_rate = 50.0801f;
   }
   else
   {
      refresh_rate = 59.94f;
      if (modetype == VI_NON_INTERLACE)
         refresh_rate = 59.8261f;
   }

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);
}

static void gx_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gx_video_t *gx = (gx_video_t*)data;

   if (!gx)
      return;

   gx->keep_aspect   = true;
   gx->should_resize = true;
}

static void gx_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   global_t *global = global_get_ptr();
   if (!global)
      return;

   /* If the current index is out of bound default it to zero */
   if (global->console.screen.resolutions.current.id > GX_RESOLUTIONS_LAST)
      global->console.screen.resolutions.current.id = 0;

   *width  = menu_gx_resolutions[
      global->console.screen.resolutions.current.id][0];
   *height = menu_gx_resolutions[
      global->console.screen.resolutions.current.id][1];
}

static void setup_video_mode(gx_video_t *gx)
{
   unsigned width  = 0;
   unsigned height = 0;
   char desc[64]   = {0};

   if (!gx->framebuf[0])
   {
      unsigned i;
      for (i = 0; i < 2; i++)
         gx->framebuf[i] = MEM_K0_TO_K1(
               memalign(32, 640 * 576 * VI_DISPLAY_PIX_SZ));
   }

   gx->orientation = ORIENTATION_NORMAL;
   OSInitThreadQueue(&g_video_cond);

   gx_get_video_output_size(gx, &width, &height, desc, sizeof(desc));
   gx_set_video_mode(gx, width, height, true);
}

static void init_texture(gx_video_t *gx, unsigned width, unsigned height,
      unsigned g_filter)
{
   unsigned fb_width, fb_height;
   GXTexObj *fb_ptr   	   = (GXTexObj*)&g_tex.obj;
   GXTexObj *menu_ptr 	   = (GXTexObj*)&menu_tex.obj;
   gfx_display_t *p_disp   = disp_get_ptr();

   if (!gx)
      return;

   width                  &= ~3;
   height                 &= ~3;

   fb_width                = p_disp->framebuf_width;
   fb_height               = p_disp->framebuf_height;

   GX_InitTexObj(fb_ptr, g_tex.data, width, height,
         (gx->rgb32)
         ? GX_TF_RGBA8
         : gx->menu_texture_enable
         ? GX_TF_RGB5A3
         : GX_TF_RGB565,
         GX_CLAMP, GX_CLAMP, GX_FALSE);
   GX_InitTexObjFilterMode(fb_ptr, g_filter, g_filter);
   GX_InitTexObj(menu_ptr, menu_tex.data, fb_width, fb_height,
         GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
   GX_InitTexObjFilterMode(menu_ptr, g_filter, g_filter);
   GX_InvalidateTexAll();
}

static void init_vtx(gx_video_t *gx, const video_info_t *video,
      bool video_smooth)
{
   Mtx44 m;
   uint32_t level      = 0;
   if (!gx || !video)
      return;

   _CPU_ISR_Disable(level);
   referenceRetraceCount = retraceCount;
   _CPU_ISR_Restore(level);

   GX_SetCullMode(GX_CULL_NONE);
   GX_SetClipMode(GX_CLIP_DISABLE);
   GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_ENABLE);
   GX_SetColorUpdate(GX_TRUE);
   GX_SetAlphaUpdate(GX_FALSE);

   guOrtho(m, 1, -1, -1, 1, 0.4, 0.6);
   GX_LoadProjectionMtx(m, GX_ORTHOGRAPHIC);

   GX_ClearVtxDesc();
   GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
   GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
   GX_SetArray(GX_VA_POS, verts, 3 * sizeof(float));
   GX_SetArray(GX_VA_TEX0, vertex_ptr, 2 * sizeof(float));
   GX_SetArray(GX_VA_CLR0, color_ptr, 4 * sizeof(u8));

   GX_SetNumTexGens(1);
   GX_SetNumChans(1);
   GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG,
         GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
   GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
   GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
   GX_InvVtxCache();

   GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA,
         GX_BL_INVSRCALPHA, GX_LO_CLEAR);

   if (gx->scale != video->input_scale || gx->rgb32 != video->rgb32)
   {
      RARCH_LOG("[GX]: Reallocate texture.\n");
      free(g_tex.data);
      g_tex.data = memalign(32,
            RARCH_SCALE_BASE * RARCH_SCALE_BASE * video->input_scale *
            video->input_scale * (video->rgb32 ? 4 : 2));
      g_tex.width = g_tex.height = RARCH_SCALE_BASE * video->input_scale;

      if (!g_tex.data)
      {
         RARCH_ERR("[GX]: Error allocating video texture\n");
         exit(1);
      }
   }

   DCFlushRange(g_tex.data, ((g_tex.width *
         g_tex.height) * video->rgb32) ? 4 : 2);

   gx->rgb32 = video->rgb32;
   gx->scale = video->input_scale;
   gx->should_resize = true;

   init_texture(gx, g_tex.width, g_tex.height,
         video_smooth ? GX_LINEAR : GX_NEAR);
   GX_Flush();
}

static void build_disp_list(void)
{
   unsigned i;

   DCInvalidateRange(display_list, sizeof(display_list));
   GX_BeginDispList(display_list, sizeof(display_list));
   GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);

   for (i = 0; i < 4; i++)
   {
      GX_Position1x8(i);
      GX_Color1x8(i);
      GX_TexCoord1x8(i);
   }
   GX_End();
   display_list_size = GX_EndDispList();
}

#if 0
#define TAKE_EFB_SCREENSHOT_ON_EXIT
#endif

#ifdef TAKE_EFB_SCREENSHOT_ON_EXIT

/* Adapted from code by Crayon for GRRLIB (http://code.google.com/p/grrlib) */
static void gx_efb_screenshot(void)
{
   int x, y;
   uint8_t tga_header[] = {0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02, 0xE0, 0x01, 0x18, 0x00};
   intfstream_t    *out = intfstream_open("/screenshot.tga",
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!out)
      return;

   intfstream_write(out, tga_header, sizeof(tga_header));

   for (y = 479; y >= 0; --y)
   {
      uint8_t line[640 * 3];
      unsigned i = 0;

      for (x = 0; x < 640; x++)
      {
         GXColor color;
         GX_PeekARGB(x, y, &color);
         line[i++] = color.b;
         line[i++] = color.g;
         line[i++] = color.r;
      }
      intfstream_write(out, line, sizeof(line));
   }

   intfstream_close(out);
   free(out);
}

#endif

static void *gx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   void *gxinput                   = NULL;
   settings_t *settings            = config_get_ptr();
   gx_video_t *gx                  = (gx_video_t*)calloc(1, sizeof(gx_video_t));
   bool video_smooth               = settings->bools.video_smooth;
   const char *input_joypad_driver = settings->arrays.input_joypad_driver;
   if (!gx)
      return NULL;

   gxinput                         = input_driver_init_wrap(&input_gx, input_joypad_driver);
   *input                          = gxinput ? &input_gx : NULL;
   *input_data                     = gxinput;

   VIDEO_Init();
   GX_Init(gx_fifo, sizeof(gx_fifo));
   gx->vsync = video->vsync;

   setup_video_mode(gx);
   init_vtx(gx, video, video_smooth);
   build_disp_list();

   gx->vp.full_width  = gx_mode.fbWidth;
   gx->vp.full_height = gx_mode.xfbHeight;
   gx->should_resize  = true;
   gx->old_width      = 0;
   gx->old_height     = 0;

   gx->system_xOrigin = 0;

#ifdef HW_RVL
   int8_t offset;
   if (CONF_GetDisplayOffsetH(&offset) == 0)
      gx->system_xOrigin = offset;
#else
   syssram      *sram = __SYS_LockSram();
   gx->system_xOrigin = sram->display_offsetH;
   __SYS_UnlockSram(0);
#endif
   return gx;
}

static void update_texture_asm(const uint32_t *src, const uint32_t *dst,
      unsigned width, unsigned height, unsigned pitch)
{
   register uint32_t tmp0, tmp1, tmp2, tmp3, line2, line2b,
            line3, line3b, line4, line4b, line5;

   __asm__ volatile (
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
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwz      %[tmp2],    4(%[src])               \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line2],   %[src]      \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line2b],  %[src]      \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line3],   %[src]      \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line3b],  %[src]      \n"
      "     stwu     %[tmp2],    8(%[tmp3])              \n"

      "     lwzx     %[tmp1],    %[line4],   %[src]      \n"
      "     stwu     %[tmp1],    8(%[dst])               \n"
      "     lwzx     %[tmp2],    %[line4b],  %[src]      \n"
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
         [dst]    "+&b"  (dst)
      :  [src]    "b"   (src),
         [width]  "b"   (width),
         [height] "b"   (height),
         [pitch]  "b"   (pitch)
      :  "cc"
   );
}

static void convert_texture16(const uint32_t *_src, uint32_t *_dst,
      unsigned width, unsigned height, unsigned pitch)
{
   width &= ~3;
   height &= ~3;
   update_texture_asm(_src, _dst, width, height, pitch);
}

static void convert_texture16_conv(const uint32_t *_src, uint32_t *_dst,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned i, tmp_pitch, width2;
   const uint32_t *src = (const uint32_t*)_src;
   uint32_t       *dst = (uint32_t*)_dst;

   width              &= ~3;
   height             &= ~3;
   tmp_pitch           = pitch >> 2;
   width2              = width >> 1;

   for (i = 0; i < height; i += 4, dst += 4 * width2)
   {
#define BLIT_LINE_16_CONV(x) (0x80008000 | (((x) & 0xFFC0FFC0) >> 1) | ((x) & 0x001F001F))
         BLIT_LINE_16(0)
         BLIT_LINE_16(2)
         BLIT_LINE_16(4)
         BLIT_LINE_16(6)
#undef BLIT_LINE_16_CONV
   }
}

static void convert_texture32(const uint32_t *_src, uint32_t *_dst,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned i, tmp_pitch, width2;
   const uint16_t *src = (uint16_t *) _src;
   uint16_t *dst       = (uint16_t *) _dst;

   width              &= ~3;
   height             &= ~3;
   tmp_pitch           = pitch >> 1;
   width2              = width << 1;

   for (i = 0; i < height; i += 4, dst += 4 * width2)
   {
      BLIT_LINE_32(0)
      BLIT_LINE_32(4)
      BLIT_LINE_32(8)
      BLIT_LINE_32(12)
   }
}

static void gx_resize(gx_video_t *gx,
      bool video_smooth,
      unsigned aspect_ratio_idx,
      unsigned overscan_corr_top,
      unsigned overscan_corr_bottom)
{
   int gamma;
   unsigned degrees;
   Mtx44 m1, m2;
   float top = 1, bottom = -1, left = -1, right = 1;
   int x = 0, y = 0;
   const global_t           *global = global_get_ptr();
   settings_t             *settings = config_get_ptr();
   unsigned width                   = gx->vp.full_width;
   unsigned height                  = gx->vp.full_height;

   if (!gx)
      return;

#ifdef HW_RVL
   VIDEO_SetTrapFilter(global->console.softfilter_enable);
   gamma = global->console.screen.gamma_correction;
   if (gamma == 0)
      gamma = 10; /* default 1.0 gamma value */
   VIDEO_SetGamma(gamma);
#else
	gamma = global->console.screen.gamma_correction;
	GX_SetDispCopyGamma(MAX(0,MIN(2,gamma)));
#endif

   /* Ignore this for custom resolutions */
   if (gx->keep_aspect && gx_mode.efbHeight >= 192)
   {
      float desired_aspect = video_driver_get_aspect_ratio();
      if (desired_aspect == 0.0)
         desired_aspect    = 1.0;
      if (     (gx->orientation == ORIENTATION_VERTICAL)
            || (gx->orientation == ORIENTATION_FLIPPED_ROTATED))
         desired_aspect    = 1.0 / desired_aspect;

      if (aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         video_viewport_t *custom_vp = &settings->video_viewport_custom;

         if (!custom_vp->width || !custom_vp->height)
         {
            custom_vp->x      = 0;
            custom_vp->y      = 0;
            custom_vp->width  = gx->vp.full_width;
            custom_vp->height = gx->vp.full_height;
         }

         x      = custom_vp->x;
         y      = custom_vp->y;
         width  = custom_vp->width;
         height = custom_vp->height;
      }
      else
      {
         float delta;
#ifdef HW_RVL
         float device_aspect = CONF_GetAspectRatio() == CONF_ASPECT_4_3 ?
            4.0 / 3.0 : 16.0 / 9.0;
#else
         float device_aspect = 4.0 / 3.0;
#endif
         if (fabs(device_aspect - desired_aspect) < 0.0001)
         {
            /* If the aspect ratios of screen and desired aspect ratio
             * are sufficiently equal (floating point stuff),
             * assume they are actually equal. */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
            x     = (unsigned)(width * (0.5 - delta));
            width = (unsigned)(2.0 * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
            y      = (unsigned)(height * (0.5 - delta));
            height = (unsigned)(2.0 * height * delta);
         }
      }
   }

   /* Overscan correction */
   if (   (overscan_corr_top    > 0)
       || (overscan_corr_bottom > 0))
   {
      float current_aspect = (float)width / (float)height;
      int new_height       = height -
         (overscan_corr_top +
          overscan_corr_bottom);
      int new_width        = (int)((new_height * current_aspect) + 0.5f);

      if ((new_height > 0) && (new_width > 0))
      {
         x      += (int)((float)(width - new_width) * 0.5f);
         y      += (int)overscan_corr_top;
         width   = (unsigned)new_width;
         height  = (unsigned)new_height;
      }
   }

   if (gx_is_valid_xorigin(gx, gx->xOrigin + x))
   {
      gx_mode.viXOrigin = gx->xOrigin + x;
      x = 0;
   }
   else if (x < 0)
   {
      gx_mode.viXOrigin = gx->xOrigin + gx->xOriginNeg;
      x -= gx->xOriginNeg;
   }
   else if (x > 0)
   {
      gx_mode.viXOrigin = gx->xOrigin + gx->xOriginPos;
      x -= gx->xOriginPos;
   }

   if (gx_is_valid_yorigin(gx->yOrigin + y))
   {
      gx_mode.viYOrigin = gx->yOrigin + y;
      y = 0;
   }
   else if (y < 0)
   {
      gx_mode.viYOrigin = gx->yOrigin + gx->yOriginNeg;
      y -= gx->yOriginNeg;
   }
   else if (y > 0)
   {
      gx_mode.viYOrigin = gx->yOrigin + gx->yOriginPos;
      y -= gx->yOriginPos;
   }

   VIDEO_Configure(&gx_mode);

   gx->vp.x      = x;
   gx->vp.y      = y;
   gx->vp.width  = width;
   gx->vp.height = height;

   GX_SetViewportJitter(x, y, width, height, 0, 1, 1);

   guOrtho(m1, top, bottom, left, right, 0, 1);
   GX_LoadPosMtxImm(m1, GX_PNMTX1);

   switch(gx->orientation)
   {
      case ORIENTATION_VERTICAL:
         degrees = 90;
         break;
      case ORIENTATION_FLIPPED:
         degrees = 180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         degrees = 270;
         break;
      default:
         degrees = 0;
         break;
   }
   guMtxIdentity(m2);
   guMtxRotDeg(m2, 'Z', degrees);
   c_guMtxConcat(m1, m2, m1);
   GX_LoadPosMtxImm(m1, GX_PNMTX0);

   init_texture(gx, 4, 4,
         video_smooth ? GX_LINEAR : GX_NEAR);
   gx->old_width     = 0;
   gx->old_height    = 0;
   gx->should_resize = false;
}

static void gx_blit_line(gx_video_t *gx,
      unsigned x, unsigned y, const char *message)
{
   unsigned width, height, h;
   bool double_width = false;
   const GXColor b   = { 0x00, 0x00, 0x00, 0xFF };
   const GXColor w   = { 0xFF, 0xFF, 0xFF, 0xFF };

   if (!gx || !*message)
      return;

   double_width = gx_mode.fbWidth > 400;
   width        = (double_width ? 2 : 1);
   height       = FONT_HEIGHT * (gx->double_strike ? 1 : 2);

   for (h = 0; h < height; h++)
   {
      GX_PokeARGB(x, y + h, b);
      if (double_width)
         GX_PokeARGB(x + 1, y + h, b);
   }

   x += (double_width ? 2 : 1);

   while (*message)
   {
      unsigned i, j;
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t     rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset = (i + j * FONT_WIDTH) >> 3;
            bool        col =
               (bitmap_bin[FONT_OFFSET((unsigned char)*message) + offset]
                & rem);
            GXColor       c = b;

            if (col)
               c = w;

            if (!gx->double_strike)
            {
               GX_PokeARGB(x + (i * width),     y + (j * 2),     c);
               if (double_width)
               {
                  GX_PokeARGB(x + (i * width) + 1, y + (j * 2),     c);
                  GX_PokeARGB(x + (i * width) + 1, y + (j * 2) + 1, c);
               }
               GX_PokeARGB(x + (i * width),     y + (j * 2) + 1, c);
            }
            else
            {
               GX_PokeARGB(x + (i * width),     y + j, c);
               if (double_width)
                  GX_PokeARGB(x + (i * width) + 1, y + j, c);
            }
         }
      }

      for (h = 0; h < height; h++)
      {
         GX_PokeARGB(x + (FONT_WIDTH * width), y + h, b);
         if (double_width)
            GX_PokeARGB(x + (FONT_WIDTH * width) + 1, y + h, b);
      }

      x += FONT_WIDTH_STRIDE * (double_width ? 2 : 1);
      message++;
   }
}

static void gx_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   gx_video_t *gx  = (gx_video_t*)data;
   if (gx)
      gx->vsync       = !state;
}

static bool gx_alive(void *data) { return true; }
static bool gx_focus(void *data) { return true; }
static bool gx_suppress_screensaver(void *data, bool enable) { return false; }

static void gx_set_rotation(void *data, unsigned orientation)
{
   gx_video_t *gx    = (gx_video_t*)data;

   if (!gx)
      return;

   gx->orientation   = orientation;
   gx->should_resize = true;
}

static void gx_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned width, unsigned height, float alpha)
{
   gx_video_t *gx = (gx_video_t*)data;
   if (gx)
      gx->menu_data = (uint32_t*)frame;
}

static void gx_set_texture_enable(void *data, bool enable, bool full_screen)
{
   gx_video_t *gx = (gx_video_t*)data;

   if (!gx)
      return;

   gx->menu_texture_enable = enable;
   /* Need to make sure the game texture is the right pixel
    * format for menu overlay. */
   gx->should_resize       = true;
}

static void gx_apply_state_changes(void *data)
{
   gx_video_t *gx = (gx_video_t*)data;

   if (gx)
      gx->should_resize = true;
}

static void gx_viewport_info(void *data, struct video_viewport *vp)
{
   gx_video_t *gx = (gx_video_t*)data;
   if (gx)
      *vp = gx->vp;
}

static void gx_get_video_output_prev(void *data)
{
   global_t *global = global_get_ptr();

   if (global->console.screen.resolutions.current.id == 0)
   {
      global->console.screen.resolutions.current.id = GX_RESOLUTIONS_LAST;
      return;
   }

   global->console.screen.resolutions.current.id--;
}

static void gx_get_video_output_next(void *data)
{
   global_t *global = global_get_ptr();
   if (!global)
      return;

   if (global->console.screen.resolutions.current.id >= GX_RESOLUTIONS_LAST)
   {
      global->console.screen.resolutions.current.id = 0;
      return;
   }

   global->console.screen.resolutions.current.id++;
}

static uint32_t gx_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

   return flags;
}

static const video_poke_interface_t gx_poke_interface = {
   gx_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   gx_set_video_mode,
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   gx_get_video_output_size,
   gx_get_video_output_prev,
   gx_get_video_output_next,
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   gx_set_aspect_ratio,
   gx_apply_state_changes,
   gx_set_texture_frame,
   gx_set_texture_enable,
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void gx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &gx_poke_interface;
}

#ifdef HAVE_OVERLAY
static void gx_overlay_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   gx_video_t            *gx = (gx_video_t*)data;
   struct gx_overlay_data *o = NULL;

   if (gx)
      o = (struct gx_overlay_data*)&gx->overlay[image];

   if (!o)
      return;

   o->tex_coord[0] = x;
   o->tex_coord[1] = y;
   o->tex_coord[2] = x + w;
   o->tex_coord[3] = y;
   o->tex_coord[4] = x;
   o->tex_coord[5] = y + h;
   o->tex_coord[6] = x + w;
   o->tex_coord[7] = y + h;
}

static void gx_overlay_vertex_geom(void *data, unsigned image,
         float x, float y, float w, float h)
{
   gx_video_t            *gx = (gx_video_t*)data;
   struct gx_overlay_data *o = NULL;

   /* Flipped, so we preserve top-down semantics. */
   y                         = 1.0f - y;
   h                         = -h;

   /* Expand from 0 - 1 to -1 - 1 */
   x                         = (x * 2.0f) - 1.0f;
   y                         = (y * 2.0f) - 1.0f;
   w                         = (w * 2.0f);
   h                         = (h * 2.0f);

   if (gx)
      o = (struct gx_overlay_data*)&gx->overlay[image];

   if (!o)
      return;

   o->vertex_coord[0] = x;
   o->vertex_coord[1] = y;
   o->vertex_coord[2] = x + w;
   o->vertex_coord[3] = y;
   o->vertex_coord[4] = x;
   o->vertex_coord[5] = y + h;
   o->vertex_coord[6] = x + w;
   o->vertex_coord[7] = y + h;
}

static void gx_free_overlay(gx_video_t *gx)
{
   if (gx)
   {
      free(gx->overlay);
      gx->overlay = NULL;
      gx->overlays = 0;
   }
   GX_InvalidateTexAll();
}

static bool gx_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i;
   gx_video_t *gx = (gx_video_t*)data;
   if (!gx)
      return false;
   const struct texture_image *images = (const struct texture_image*)image_data;

   gx_free_overlay(gx);
   gx->overlay = (struct gx_overlay_data*)calloc(num_images, sizeof(*gx->overlay));
   if (!gx->overlay)
      return false;

   gx->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct gx_overlay_data *o = (struct gx_overlay_data*)&gx->overlay[i];

      GX_InitTexObj(&o->tex, images[i].pixels, images[i].width,
            images[i].height,
            GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
      GX_InitTexObjFilterMode(&g_tex.obj, GX_LINEAR, GX_LINEAR);
      DCFlushRange(images[i].pixels, images[i].width *
            images[i].height * sizeof(uint32_t));

      /* Default. Stretch to whole screen. */
      gx_overlay_tex_geom(gx, i, 0, 0, 1, 1);
      gx_overlay_vertex_geom(gx, i, 0, 0, 1, 1);
      gx->overlay[i].alpha_mod = 1.0f;
   }

   GX_InvalidateTexAll();
   return true;
}

static void gx_overlay_enable(void *data, bool state)
{
   gx_video_t *gx = (gx_video_t*)data;

   if (gx)
      gx->overlay_enable = state;
}

static void gx_overlay_full_screen(void *data, bool enable)
{
   gx_video_t *gx = (gx_video_t*)data;

   if (gx)
      gx->overlay_full_screen = enable;
}

static void gx_overlay_set_alpha(void *data, unsigned image, float mod)
{
   gx_video_t *gx = (gx_video_t*)data;

   if (gx)
      gx->overlay[image].alpha_mod = mod;
}

static void gx_render_overlay(void *data)
{
   unsigned i;
   gx_video_t *gx = (gx_video_t*)data;
   if (!gx)
      return;

   GX_SetCurrentMtx(GX_PNMTX1);
   GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
   GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
   GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

   for (i = 0; i < gx->overlays; i++)
   {
      GX_LoadTexObj(&gx->overlay[i].tex, GX_TEXMAP0);

      GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
      GX_Position3f32(gx->overlay[i].vertex_coord[0],
            gx->overlay[i].vertex_coord[1],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[0],
            gx->overlay[i].tex_coord[1]);

      GX_Position3f32(gx->overlay[i].vertex_coord[2],
            gx->overlay[i].vertex_coord[3],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[2],
            gx->overlay[i].tex_coord[3]);

      GX_Position3f32(gx->overlay[i].vertex_coord[4],
            gx->overlay[i].vertex_coord[5],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[4],
            gx->overlay[i].tex_coord[5]);

      GX_Position3f32(gx->overlay[i].vertex_coord[6],
            gx->overlay[i].vertex_coord[7],  -0.5);
      GX_Color4u8(255, 255, 255, (u8)(gx->overlay[i].alpha_mod * 255.0f));
      GX_TexCoord2f32(gx->overlay[i].tex_coord[6],
            gx->overlay[i].tex_coord[7]);
      GX_End();
   }

   GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
   GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
}

static const video_overlay_interface_t gx_overlay_interface = {
   gx_overlay_enable,
   gx_overlay_load,
   gx_overlay_tex_geom,
   gx_overlay_vertex_geom,
   gx_overlay_full_screen,
   gx_overlay_set_alpha,
};

static void gx_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gx_overlay_interface;
}
#endif

static void gx_free(void *data)
{
#ifdef HAVE_OVERLAY
   gx_video_t *gx = (gx_video_t*)data;

   gx_free_overlay(gx);
#endif

   GX_DrawDone();
   GX_AbortFrame();
   GX_Flush();
   VIDEO_SetBlack(true);
   VIDEO_Flush();
   VIDEO_WaitVSync();

   if (g_video_cond)
      OSCloseThreadQueue(g_video_cond);
   g_video_cond = 0;

   free(data);
}

static bool gx_frame(void *data, const void *frame,
      unsigned width, unsigned height,
      uint64_t frame_count, unsigned pitch,
      const char *msg,
      video_frame_info_t *video_info)
{
   char fps_text_buf[128];
   settings_t               *settings = config_get_ptr();
   gx_video_t *gx                     = (gx_video_t*)data;
   u8                       clear_efb = GX_FALSE;
   uint32_t level                     = 0;
   unsigned overscan_corr_top         = settings->uints.video_overscan_correction_top;
   unsigned overscan_corr_bottom      = settings->uints.video_overscan_correction_bottom;
   bool video_smooth                  = settings->bools.video_smooth;
   unsigned video_aspect_ratio_idx    = settings->uints.video_aspect_ratio_idx;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
   bool fps_show                      = video_info->fps_show;

   fps_text_buf[0]                    = '\0';

   if (!gx || (!frame && !gx->menu_texture_enable))
      return true;

   if (!frame)
      width = height = 4; /* draw a black square in the background */

   if (   (gx->overscan_correction_top    != overscan_corr_top)
       || (gx->overscan_correction_bottom != overscan_corr_bottom))
   {
      gx->overscan_correction_top    = overscan_corr_top;
      gx->overscan_correction_bottom = overscan_corr_bottom;
      gx->should_resize              = true;
   }

   if (gx->should_resize)
   {
      gx_resize(gx,
            video_smooth,
            video_aspect_ratio_idx,
            overscan_corr_top,
            overscan_corr_bottom);
      clear_efb = GX_TRUE;
   }

   while (((gx->vsync || gx->menu_texture_enable)) && !g_draw_done)
      OSSleepThread(g_video_cond);

   width  = MIN(g_tex.width, width);
   height = MIN(g_tex.height, height);

   if (width != gx->old_width || height != gx->old_height)
   {
      init_texture(gx, width, height,
            video_smooth ? GX_LINEAR : GX_NEAR);
      gx->old_width  = width;
      gx->old_height = height;
   }

   g_draw_done           = false;
   gx->current_framebuf ^= 1;

   if (frame)
   {
      if (gx->rgb32)
         convert_texture32(frame, g_tex.data, width, height, pitch);
      else if (gx->menu_texture_enable)
         convert_texture16_conv(frame, g_tex.data, width, height, pitch);
      else
         convert_texture16(frame, g_tex.data, width, height, pitch);
      DCFlushRange(g_tex.data, height * (width << (gx->rgb32 ? 2 : 1)));
   }

   if (gx->menu_texture_enable && gx->menu_data)
   {
      gfx_display_t *p_disp   = disp_get_ptr();
      unsigned fb_width       = p_disp->framebuf_width;
      unsigned fb_height      = p_disp->framebuf_height;
      unsigned fb_pitch       = p_disp->framebuf_pitch;

      convert_texture16(
            gx->menu_data,
            menu_tex.data,
            fb_width,
            fb_height,
            fb_pitch);
      DCFlushRange(
            menu_tex.data,
            fb_width * fb_pitch);
   }

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   GX_InvalidateTexAll();

   GX_SetCurrentMtx(GX_PNMTX0);
   GX_LoadTexObj(&g_tex.obj, GX_TEXMAP0);
   GX_CallDispList(display_list, display_list_size);

   if (gx->menu_texture_enable)
   {
      GX_SetCurrentMtx(GX_PNMTX1);
      GX_LoadTexObj(&menu_tex.obj, GX_TEXMAP0);
      GX_CallDispList(display_list, display_list_size);
   }

#ifdef HAVE_OVERLAY
   if (gx->overlay_enable)
      gx_render_overlay(gx);
#endif

   _CPU_ISR_Disable(level);
   if (referenceRetraceCount > retraceCount)
   {
      if (gx->vsync)
         VIDEO_WaitVSync();
   }

   referenceRetraceCount = retraceCount;
   _CPU_ISR_Restore(level);

   GX_DrawDone();

   if (fps_show)
   {
      char mem1_txt[128];
      char mem2_txt[128];
      unsigned x         = 15;
      unsigned y         = 35;

      mem1_txt[0] = mem2_txt[0] = '\0';

      gx_blit_line(gx, x, y, fps_text_buf);
      y += FONT_HEIGHT * (gx->double_strike ? 1 : 2);
      snprintf(mem1_txt, sizeof(mem1_txt), "MEM1: %8d / %8d",
            SYSMEM1_SIZE - SYS_GetArena1Size(), SYSMEM1_SIZE);
      gx_blit_line(gx, x, y, mem1_txt);
#ifdef HW_RVL
      y += FONT_HEIGHT * (gx->double_strike ? 1 : 2);
      snprintf(mem2_txt, sizeof(mem2_txt), "MEM2: %8d / %8d",
            gx_mem2_used(), gx_mem2_total());
      gx_blit_line(gx, x, y, mem2_txt);
#endif
   }

   if (msg && !gx->menu_texture_enable)
   {
      unsigned x = 7 * (gx->double_strike ? 1 : 2);
      unsigned y = gx->vp.full_height - (35 * (gx->double_strike ? 1 : 2));

      gx_blit_line(gx, x, y, msg);
      clear_efb = GX_TRUE;
   }

   GX_CopyDisp(gx->framebuf[gx->current_framebuf], clear_efb);
   GX_Flush();
   VIDEO_SetNextFramebuffer(gx->framebuf[gx->current_framebuf]);
   VIDEO_Flush();

   _CPU_ISR_Disable(level);
   ++referenceRetraceCount;
   _CPU_ISR_Restore(level);

   return true;
}

static bool gx_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

video_driver_t video_gx = {
   gx_init,
   gx_frame,
   gx_set_nonblock_state,
   gx_alive,
   gx_focus,
   gx_suppress_screensaver,
   NULL, /* has_windowed */
   gx_set_shader,
   gx_free,
   "gx",
   NULL, /* set_viewport */
   gx_set_rotation,
   gx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   gx_get_overlay_interface,
#endif
   gx_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
