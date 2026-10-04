/*  RetroArch - A frontend for libretro.
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

/* Video on os/gekko.  Each frame is a textured quad into the embedded
 * framebuffer, in pixel coordinates; the menu's surface and overlays
 * go on top; on-screen text is poked into the EFB; then the EFB is
 * copied to whichever of two XFBs the video interface is not showing,
 * and that one is shown from the next field.  With vsync, a frame
 * waits for the retrace that put the previous one up. */

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include <gekko/gx.h>
#include <gekko/video.h>
#ifdef HW_RVL
#include <gekko/conf.h>
#endif

#include <libretro.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../bitmapfont.h"
#include "../display_servers/dispserv_gx.h"
#include "../../configuration.h"
#include "../../driver.h"
#include "../../verbosity.h"

#define FIFO_BYTES  (256 * 1024)
#define XFB_BYTES   (720 * 576 * 2)
#define EFB_LINES   480

/* RGUI's surface, at most 560 x 240 RGB5A3. */
#define MENU_MAX_WIDTH  560
#define MENU_MAX_HEIGHT 240

struct gekko_overlay
{
   gk_gx_tex_t tex;
   float       tex_coord[4];    /* x, y, w, h */
   float       vertex_coord[4]; /* x, y, w, h; 0..1, y down */
   float       alpha;
};

typedef struct gekko_video
{
   video_viewport_t      vp;
   gk_vi_mode_t          mode;
   gk_gx_tex_t           frame_tex;
   gk_gx_tex_t           menu_tex;
   const uint16_t       *menu_data;
#ifdef HAVE_OVERLAY
   struct gekko_overlay *overlay;
   unsigned              overlays;
#endif
   uint32_t             *tex_data;
   uint32_t              swap_retrace;
   unsigned              tex_size;      /* texels a side the buffer holds */
   unsigned              scale;
   unsigned              efb_lines;
   unsigned              tex_w, tex_h;  /* what frame_tex describes */
   unsigned              overscan_top, overscan_bottom;
   unsigned              orientation;
   unsigned              cur_xfb;
   int                   system_xorigin;
   bool                  should_resize;
   bool                  double_strike;
   bool                  rgb32;
   bool                  menu_enable;
   bool                  vsync;
   bool                  smooth;
#ifdef HAVE_OVERLAY
   bool                  overlay_enable;
   bool                  overlay_full_screen;
#endif
} gekko_video_t;

/* The XFBs and the GPU's ring outlive driver instances: the VI keeps
 * showing an XFB between a free and the next init. */
static void    *g_xfb[2];
static uint16_t g_menu_tiles[MENU_MAX_WIDTH * MENU_MAX_HEIGHT]
   __attribute__((aligned(32)));
static bool     g_gx_up;

/* ---- texels ---- */

/* 16-bit texels into 4x4 tiles, rows of tiles left to right. */
static void tile16(const uint16_t *src, uint16_t *dst, unsigned w,
      unsigned h, unsigned pitch)
{
   unsigned x, y, r;
   size_t stride = pitch / 2;
   for (y = 0; y < h; y += 4)
      for (x = 0; x < w; x += 4)
         for (r = 0; r < 4; r++)
         {
            const uint32_t *s = (const uint32_t*)(src + (y + r) * stride + x);
            uint32_t       *d = (uint32_t*)dst;
            d[0] = s[0];
            d[1] = s[1];
            dst += 4;
         }
}

/* XRGB8888 into RGBA8 tiles: per 4x4 tile, the 16 AR pairs then the
 * 16 GB pairs. */
static void tile32(const uint32_t *src, uint16_t *dst, unsigned w,
      unsigned h, unsigned pitch)
{
   unsigned x, y, r, c;
   size_t stride = pitch / 4;
   for (y = 0; y < h; y += 4)
      for (x = 0; x < w; x += 4)
      {
         for (r = 0; r < 4; r++)
         {
            const uint32_t *s = src + (y + r) * stride + x;
            for (c = 0; c < 4; c++)
            {
               uint32_t p = s[c];
               dst[r * 4 + c]      = (uint16_t)(0xff00 | ((p >> 16) & 0xff));
               dst[16 + r * 4 + c] = (uint16_t)p;
            }
         }
         dst += 32;
      }
}

/* ---- the screen ---- */

static void wait_swap(gekko_video_t *gx)
{
   /* The XFB shown before the last swap is free once a retrace has
    * put the new one up. */
   while (gk_vi_retraces() == gx->swap_retrace)
      gk_vi_wait_retrace();
}

static void gekko_set_video_mode(void *data, unsigned dims, bool fullscreen)
{
   static const uint8_t vfilter_soft[7] = { 8, 8, 10, 12, 10, 8, 8 };
   gx_vi_standard_t std;
   gx_vi_mode_t vi;
   unsigned tvmode, lines, vi_width, menu_w, menu_h;
   int x;
   float hz;
   settings_t *settings = config_get_ptr();
   gekko_video_t *gx    = (gekko_video_t*)data;

   if (!gx || !settings)
      return;

   gx_display_server_query(&std, &tvmode);
   gx_modes_resolve(&std, dims, &vi);

   lines = vi.lines;
   if (vi.double_strike && lines > std.max_height / 2)
      lines = std.max_height / 2;
   else if (!vi.double_strike && lines > std.max_height)
      lines = std.max_height;

   vi_width = settings->uints.video_viwidth;
   if (vi_width < 640 || vi_width > 720)
      vi_width = 640;

   memset(&gx->mode, 0, sizeof(gx->mode));
   gx->mode.std      = (uint8_t)tvmode;
   gx->mode.scan     = vi.double_strike ? GK_VI_DOUBLE_STRIKE
      : vi.interlaced ? GK_VI_INTERLACED : GK_VI_PROGRESSIVE;
   gx->mode.fb_width = (uint16_t)((vi.width + 15) & ~15u);
   gx->mode.fb_lines = (uint16_t)lines;
   gx->mode.width    = (uint16_t)vi_width;
   gx->mode.lines    = (uint16_t)lines;
   /* Centred in the 720-pixel line, moved by the console's offset as
    * far as the line allows. */
   x = (int)(720 - vi_width) / 2 + gx->system_xorigin;
   if (x < 0)
      x = 0;
   if (x + (int)vi_width > 720)
      x = (int)(720 - vi_width);
   gx->mode.x        = (int16_t)x;
   gx->mode.y        = -1;

   gx->efb_lines     = MIN(lines, EFB_LINES);
   gx->double_strike = vi.double_strike;

   gk_vi_set_black(1);
   if (gk_vi_configure(&gx->mode))
   {
      RARCH_ERR("[Gekko] The VI cannot show %ux%u.\n",
            gx->mode.fb_width, gx->mode.lines);
      gk_vi_preferred(&gx->mode);
      gk_vi_configure(&gx->mode);
      gx->efb_lines     = MIN(gx->mode.lines, EFB_LINES);
      gx->double_strike = false;
   }
   gk_vi_clear_fb(g_xfb[0], &gx->mode, GK_YUYV_BLACK);
   gk_vi_clear_fb(g_xfb[1], &gx->mode, GK_YUYV_BLACK);
   gk_vi_set_fb(g_xfb[0]);
   gx->cur_xfb      = 0;
   gx->swap_retrace = gk_vi_retraces() - 1;
   gk_vi_set_black(0);

   /* Interlaced output can take the soft vertical filter; a field on
    * its own has nothing to blend with. */
   gk_gx_copy_filter(!vi.double_strike && settings->bools.video_vfilter
         ? vfilter_soft : NULL);

   gx->vp.full_dims  = VIDEO_SCALE_PACK(gx->mode.fb_width, gx->efb_lines);
   gx->should_resize = true;

   /* The menu's surface: as tall as the EFB allows up to RGUI's 240,
    * as wide as its aspect ratio asks, in whole tiles. */
   menu_h = (gx->efb_lines / (gx->double_strike ? 1 : 2)) & ~3u;
   if (menu_h > MENU_MAX_HEIGHT)
      menu_h = MENU_MAX_HEIGHT;
   switch (settings->uints.menu_rgui_aspect_ratio)
   {
      case RGUI_ASPECT_RATIO_16_9:
      case RGUI_ASPECT_RATIO_16_9_CENTRE:
         menu_w = menu_h == 240 ? 424
            : (unsigned)((16.0f / 9.0f) * (float)menu_h) & ~3u;
         break;
      case RGUI_ASPECT_RATIO_16_10:
      case RGUI_ASPECT_RATIO_16_10_CENTRE:
         menu_w = menu_h == 240 ? 384
            : (unsigned)((16.0f / 10.0f) * (float)menu_h) & ~3u;
         break;
      case RGUI_ASPECT_RATIO_21_9:
      case RGUI_ASPECT_RATIO_21_9_CENTRE:
         menu_w = menu_h == 240 ? 560
            : (unsigned)((21.0f / 9.0f) * (float)menu_h) & ~3u;
         break;
      default:
         menu_w = menu_h == 240 ? 320
            : (unsigned)((4.0f / 3.0f) * (float)menu_h) & ~3u;
         break;
   }
   if (menu_w > MENU_MAX_WIDTH)
      menu_w = MENU_MAX_WIDTH;
   {
      gfx_display_t *p_disp  = disp_get_ptr();
      p_disp->framebuf_dims  = VIDEO_SCALE_PACK(menu_w, menu_h);
      p_disp->framebuf_pitch = menu_w * 2;
   }

   RARCH_LOG("[Gekko] Resolution: %ux%u (%s).\n", gx->mode.fb_width,
         gx->mode.lines, gx->mode.scan == GK_VI_INTERLACED ? "interlaced"
         : gx->mode.scan == GK_VI_DOUBLE_STRIKE ? "double strike"
         : "progressive");

   hz = vi.hz;
   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &hz);
}

static void setup_gx(gekko_video_t *gx)
{
   gk_mtx44 p;
   gk_mtx34 m;
   unsigned w = gx->mode.fb_width, h = gx->efb_lines;

   gk_mtx_ortho(p, 0, (float)h, 0, (float)w, 0, 1);
   gk_gx_projection(p, 1);
   gk_mtx_identity(m);
   gk_gx_load_pos_mtx(m, 0);
   gk_gx_current_mtx(0);
   gk_gx_viewport(0, 0, (float)w, (float)h, 0, 1, 0);
   gk_gx_scissor(0, 0, w, h);
   gk_gx_cull(GK_GX_CULL_NONE);
   gk_gx_clip(0);
   gk_gx_z_mode(0, GK_GX_ALWAYS, 0);
   gk_gx_color_update(1);
   gk_gx_alpha_update(0);
   gk_gx_blend(1, GK_GX_BL_SRCALPHA, GK_GX_BL_INVSRCALPHA);

   gk_gx_vtx_clear();
   gk_gx_vtx_desc(GK_GX_POS, GK_GX_DIRECT);
   gk_gx_vtx_desc(GK_GX_CLR0, GK_GX_DIRECT);
   gk_gx_vtx_desc(GK_GX_TEX0, GK_GX_DIRECT);
   gk_gx_vtx_fmt(0, GK_GX_POS, 3, GK_GX_F32, 0);
   gk_gx_vtx_fmt(0, GK_GX_CLR0, 4, GK_GX_RGBA8, 0);
   gk_gx_vtx_fmt(0, GK_GX_TEX0, 2, GK_GX_F32, 0);
   gk_gx_num_texgens(1);
   gk_gx_num_stages(1);
   gk_gx_tev_order(0, 0, 0);
   gk_gx_tev(0, GK_GX_MODULATE);
   gk_gx_copy_clear(0x000000ffu, 0xffffff);
}

/* Called from frame(): the settings it applies come with the frame,
 * not from config_get_ptr(), which the main thread may be writing. */
static void viewport_resize(gekko_video_t *gx, unsigned overscan_top,
      unsigned overscan_bottom, unsigned gamma, bool soft_filter)
{
   unsigned width  = VIDEO_SCALE_W(gx->vp.full_dims);
   unsigned height = VIDEO_SCALE_H(gx->vp.full_dims);
   int x = 0, y = 0;

#ifdef HW_RVL
   /* The A/V encoder: gamma in tenths, 0 standing for 1.0. */
   gk_vi_set_trap_filter(soft_filter);
   gk_vi_set_gamma(gamma ? gamma : 10);
#else
   gk_gx_copy_gamma(MIN(2, gamma));
#endif

   /* Below 192 lines is a custom mode: as it is. */
   if (gx->efb_lines >= 192)
   {
      float device_aspect  = 4.0f / 3.0f;
      float desired_aspect = video_driver_get_aspect_ratio();
#ifdef HW_RVL
      if (gk_conf_wide() > 0)
         device_aspect = 16.0f / 9.0f;
#endif
      if (desired_aspect == 0.0f)
         desired_aspect = 1.0f;
      if (     gx->orientation == ORIENTATION_VERTICAL
            || gx->orientation == ORIENTATION_FLIPPED_ROTATED)
         desired_aspect = 1.0f / desired_aspect;
      video_viewport_get_scaled_aspect2(&gx->vp, gx->vp.full_dims, true,
            device_aspect, desired_aspect);
      x      = VIDEO_POS_X(gx->vp.pos);
      y      = VIDEO_POS_Y(gx->vp.pos);
      width  = VIDEO_SCALE_W(gx->vp.dims);
      height = VIDEO_SCALE_H(gx->vp.dims);
   }

   if (overscan_top || overscan_bottom)
   {
      float aspect   = (float)width / (float)height;
      int new_height = (int)height - (int)(overscan_top + overscan_bottom);
      int new_width  = (int)((float)new_height * aspect + 0.5f);
      if (new_height > 0 && new_width > 0)
      {
         x     += (int)((float)((int)width - new_width) * 0.5f);
         y     += (int)overscan_top;
         width  = (unsigned)new_width;
         height = (unsigned)new_height;
      }
   }

   gx->vp.pos        = VIDEO_POS_PACK(x, y);
   gx->vp.dims       = VIDEO_SCALE_PACK(width, height);
   gx->should_resize = false;
}

/* A textured quad from (x, y), corners' texture coordinates turned
 * for the orientation. */
static void quad(float x, float y, float w, float h, const float *u,
      const float *v, uint32_t rgba)
{
   gk_gx_begin(GK_GX_QUADS, 0, 4);
   gk_gx_f32(x);     gk_gx_f32(y);     gk_gx_f32(0);
   gk_gx_u32(rgba);  gk_gx_f32(u[0]);  gk_gx_f32(v[0]);
   gk_gx_f32(x + w); gk_gx_f32(y);     gk_gx_f32(0);
   gk_gx_u32(rgba);  gk_gx_f32(u[1]);  gk_gx_f32(v[1]);
   gk_gx_f32(x + w); gk_gx_f32(y + h); gk_gx_f32(0);
   gk_gx_u32(rgba);  gk_gx_f32(u[2]);  gk_gx_f32(v[2]);
   gk_gx_f32(x);     gk_gx_f32(y + h); gk_gx_f32(0);
   gk_gx_u32(rgba);  gk_gx_f32(u[3]);  gk_gx_f32(v[3]);
}

static void draw_frame(gekko_video_t *gx)
{
   /* Corners top-left, top-right, bottom-right, bottom-left; each
    * quarter turn moves the texture one corner on. */
   static const float cu[4] = { 0, 1, 1, 0 }, cv[4] = { 0, 0, 1, 1 };
   float u[4], v[4];
   unsigned i, turn;
   switch (gx->orientation)
   {
      case ORIENTATION_VERTICAL:        turn = 1; break;
      case ORIENTATION_FLIPPED:         turn = 2; break;
      case ORIENTATION_FLIPPED_ROTATED: turn = 3; break;
      default:                          turn = 0; break;
   }
   for (i = 0; i < 4; i++)
   {
      u[i] = cu[(i + turn) & 3];
      v[i] = cv[(i + turn) & 3];
   }
   gk_gx_tex_load(&gx->frame_tex, 0, 0);
   quad((float)VIDEO_POS_X(gx->vp.pos), (float)VIDEO_POS_Y(gx->vp.pos),
         (float)VIDEO_SCALE_W(gx->vp.dims), (float)VIDEO_SCALE_H(gx->vp.dims),
         u, v, 0xffffffffu);
}

static void draw_menu(gekko_video_t *gx)
{
   static const float u[4] = { 0, 1, 1, 0 }, v[4] = { 0, 0, 1, 1 };
   gfx_display_t *p_disp = disp_get_ptr();
   unsigned w = VIDEO_SCALE_W(p_disp->framebuf_dims) & ~3u;
   unsigned h = VIDEO_SCALE_H(p_disp->framebuf_dims) & ~3u;
   if (!w || !h)
      return;
   tile16(gx->menu_data, g_menu_tiles, w, h, (unsigned)p_disp->framebuf_pitch);
   gk_dcache_flush(g_menu_tiles, (size_t)w * h * 2);
   gk_gx_invalidate_tex();
   gk_gx_tex_init(&gx->menu_tex, g_menu_tiles, w, h, GK_GX_TF_RGB5A3,
         GK_GX_CLAMP, GK_GX_CLAMP);
   gk_gx_tex_filter(&gx->menu_tex, gx->smooth ? GK_GX_LINEAR : GK_GX_NEAR,
         gx->smooth ? GK_GX_LINEAR : GK_GX_NEAR);
   gk_gx_tex_load(&gx->menu_tex, 0, 0);
   quad(0, 0, (float)gx->mode.fb_width, (float)gx->efb_lines, u, v,
         0xffffffffu);
}

#ifdef HAVE_OVERLAY
static void draw_overlays(gekko_video_t *gx)
{
   unsigned i;
   float bx = 0, by = 0;
   float bw = (float)gx->mode.fb_width, bh = (float)gx->efb_lines;
   if (!gx->overlay_full_screen)
   {
      bx = (float)VIDEO_POS_X(gx->vp.pos);
      by = (float)VIDEO_POS_Y(gx->vp.pos);
      bw = (float)VIDEO_SCALE_W(gx->vp.dims);
      bh = (float)VIDEO_SCALE_H(gx->vp.dims);
   }
   for (i = 0; i < gx->overlays; i++)
   {
      const struct gekko_overlay *o = &gx->overlay[i];
      float u[4], v[4];
      uint32_t a = (uint32_t)VIDEO_ALPHA_BYTE(o->alpha);
      u[0] = u[3] = o->tex_coord[0];
      u[1] = u[2] = o->tex_coord[0] + o->tex_coord[2];
      v[0] = v[1] = o->tex_coord[1];
      v[2] = v[3] = o->tex_coord[1] + o->tex_coord[3];
      gk_gx_tex_load(&o->tex, 0, 0);
      quad(bx + o->vertex_coord[0] * bw, by + o->vertex_coord[1] * bh,
            o->vertex_coord[2] * bw, o->vertex_coord[3] * bh, u, v,
            0xffffff00u | a);
   }
}
#endif

/* Text straight into the EFB, after the GPU is done with it: the
 * bitmap font on black, twice as tall on interlaced output and twice
 * as wide on wide framebuffers. */
static void blit_line(gekko_video_t *gx, unsigned x, unsigned y,
      const char *msg)
{
   const unsigned sx = gx->mode.fb_width > 400 ? 2 : 1;
   const unsigned sy = gx->double_strike ? 1 : 2;
   unsigned i, j, a, b;

   for (j = 0; j < FONT_HEIGHT * sy; j++)
      for (a = 0; a < sx; a++)
         gk_gx_poke(x + a, y + j, 0x000000ffu);
   x += sx;

   while (*msg)
   {
      uint32_t code = utf8_walk(&msg);
      /* The font's 256 codepoints are extended ASCII, with the 'oe'
       * ligatures where Windows-1252 puts them. */
      if (code == 339)
         code = 156;
      else if (code == 338)
         code = 140;
      else if (code > 255)
         code = '?';
      if (x + FONT_WIDTH_STRIDE * sx >= gx->mode.fb_width)
         break;
      for (j = 0; j < FONT_HEIGHT; j++)
         for (i = 0; i < FONT_WIDTH_STRIDE; i++)
         {
            unsigned bit = i + j * FONT_WIDTH;
            int on = i < FONT_WIDTH && (bitmap_bin[FONT_OFFSET(code)
                  + (bit >> 3)] & (1u << (bit & 7)));
            uint32_t c = on ? 0xffffffffu : 0x000000ffu;
            for (a = 0; a < sx; a++)
               for (b = 0; b < sy; b++)
                  gk_gx_poke(x + i * sx + a, y + j * sy + b, c);
         }
      x += FONT_WIDTH_STRIDE * sx;
   }
}

/* ---- the driver ---- */

static void *gekko_init(const video_info_t *video, input_driver_t **input,
      void **input_data)
{
   settings_t *settings = config_get_ptr();
   gekko_video_t *gx    = (gekko_video_t*)calloc(1, sizeof(*gx));

   if (!gx)
      return NULL;

   /* no input driver of this driver's own: the frontend starts the
    * platform's */
   input_driver_left_to_frontend(INPUT_WINDOW_PLATFORM, input, input_data);

   if (!g_xfb[0])
   {
      g_xfb[0] = gk_arena_take_top(&gk_mem1, XFB_BYTES, 32);
      g_xfb[1] = gk_arena_take_top(&gk_mem1, XFB_BYTES, 32);
   }
   if (!g_gx_up)
   {
      if (!g_xfb[0] || !g_xfb[1] || gk_gx_init(FIFO_BYTES))
      {
         RARCH_ERR("[Gekko] No memory for the framebuffers.\n");
         free(gx);
         return NULL;
      }
      g_gx_up = true;
   }

#ifdef HW_RVL
   gx->system_xorigin = gk_conf_display_offset_h();
#endif
   gx->vsync       = video->vsync;
   gx->smooth      = settings->bools.video_smooth;
   gx->rgb32       = video->rgb32;
   gx->scale       = video->input_scale;
   gx->orientation = ORIENTATION_NORMAL;
   gx->tex_size    = RARCH_SCALE_BASE * video->input_scale;
   gx->tex_data    = (uint32_t*)memalign(32, (size_t)gx->tex_size
         * gx->tex_size * (gx->rgb32 ? 4 : 2));
   if (!gx->tex_data)
   {
      RARCH_ERR("[Gekko] No memory for the frame texture.\n");
      free(gx);
      return NULL;
   }

   gekko_set_video_mode(gx,
         gx_modes_dims(gx_display_server_current_id()), true);
   setup_gx(gx);
   return gx;
}

static bool gekko_frame(void *data, const void *frame, unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   unsigned width    = VIDEO_SCALE_W(dims) & ~3u;
   unsigned height   = VIDEO_SCALE_H(dims) & ~3u;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE)
      ? true : false;
#endif

   if (!gx || (!frame && !gx->menu_enable))
      return true;

   if (     gx->overscan_top    != video_info->overscan_correction_top
         || gx->overscan_bottom != video_info->overscan_correction_bottom)
   {
      gx->overscan_top    = video_info->overscan_correction_top;
      gx->overscan_bottom = video_info->overscan_correction_bottom;
      gx->should_resize   = true;
   }
   if (gx->smooth != video_info->video_smooth)
   {
      gx->smooth = video_info->video_smooth;
      gx->tex_w  = 0;
   }
   if (gx->should_resize)
   {
      setup_gx(gx);
      viewport_resize(gx, gx->overscan_top, gx->overscan_bottom,
            video_info->video_gamma, video_info->video_soft_filter);
   }

   if (gx->vsync || gx->menu_enable)
      wait_swap(gx);

   if (frame && width && height)
   {
      width  = MIN(width,  gx->tex_size);
      height = MIN(height, gx->tex_size);
      if (gx->rgb32)
         tile32((const uint32_t*)frame, (uint16_t*)gx->tex_data, width,
               height, pitch);
      else
         tile16((const uint16_t*)frame, (uint16_t*)gx->tex_data, width,
               height, pitch);
      gk_dcache_flush(gx->tex_data, (size_t)width * height
            * (gx->rgb32 ? 4 : 2));
      gk_gx_invalidate_tex();
      if (width != gx->tex_w || height != gx->tex_h)
      {
         gk_gx_tex_init(&gx->frame_tex, gx->tex_data, width, height,
               gx->rgb32 ? GK_GX_TF_RGBA8 : GK_GX_TF_RGB565,
               GK_GX_CLAMP, GK_GX_CLAMP);
         gk_gx_tex_filter(&gx->frame_tex,
               gx->smooth ? GK_GX_LINEAR : GK_GX_NEAR,
               gx->smooth ? GK_GX_LINEAR : GK_GX_NEAR);
         gx->tex_w = width;
         gx->tex_h = height;
      }
   }

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (frame && gx->tex_w)
      draw_frame(gx);
   if (gx->menu_enable && gx->menu_data)
      draw_menu(gx);
#ifdef HAVE_OVERLAY
   if (gx->overlay_enable && gx->overlay)
      draw_overlays(gx);
#endif

   gk_gx_draw_done();
   if (msg && *msg && !gx->menu_enable)
      blit_line(gx, 7 * (gx->double_strike ? 1 : 2),
            gx->efb_lines - 35 * (gx->double_strike ? 1 : 2), msg);

   gx->cur_xfb ^= 1;
   gk_gx_copy_xfb(g_xfb[gx->cur_xfb], gx->mode.fb_width, 0, 0,
         gx->mode.fb_width, gx->efb_lines, gx->mode.fb_lines, 1);
   gk_gx_draw_done();
   gk_vi_set_fb(g_xfb[gx->cur_xfb]);
   gx->swap_retrace = gk_vi_retraces();
   return true;
}

static void gekko_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      gx->vsync = !state;
}

static bool gekko_alive(void *data) { return true; }
static bool gekko_focus(void *data) { return true; }
static bool gekko_suppress_screensaver(void *data, bool enable) { return false; }

static void gekko_set_rotation(void *data, unsigned orientation)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
   {
      gx->orientation   = orientation;
      gx->should_resize = true;
   }
}

static void gekko_viewport_info(void *data, struct video_viewport *vp)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      *vp = gx->vp;
}

static void gekko_free(void *data)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (!gx)
      return;
   gk_gx_draw_done();
#ifdef HAVE_OVERLAY
   free(gx->overlay);
#endif
   /* The VI goes on showing a black XFB, so whatever comes next (a
    * crash screen, say) is not hidden behind a blanked display. */
   gk_vi_clear_fb(g_xfb[gx->cur_xfb], &gx->mode, GK_YUYV_BLACK);
   free(gx->tex_data);
   free(gx);
}

static bool gekko_set_shader(void *data, enum rarch_shader_type type,
      const char *path)
{
   return false;
}

/* ---- poke ---- */

static uint32_t gekko_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   return flags;
}

static void gekko_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      gx->should_resize = true;
}

static void gekko_apply_state_changes(void *data)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      gx->should_resize = true;
}

static void gekko_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned dims, float alpha)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      gx->menu_data = (const uint16_t*)frame;
}

static void gekko_set_texture_enable(void *data, bool enable,
      bool full_screen)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      gx->menu_enable = enable;
}

static void gekko_set_video_mode_poke(void *data, unsigned dims,
      bool fullscreen)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (!gx)
      return;
   gk_gx_draw_done();
   gekko_set_video_mode(gx, dims, fullscreen);
   setup_gx(gx);
}

static const video_poke_interface_t gekko_poke_interface = {
   gekko_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   gekko_set_video_mode_poke,
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size: the display server */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   gekko_set_aspect_ratio,
   gekko_apply_state_changes,
   gekko_set_texture_frame,
   gekko_set_texture_enable,
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void gekko_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &gekko_poke_interface;
}

/* ---- overlays ---- */

#ifdef HAVE_OVERLAY
static struct gekko_overlay *overlay_at(void *data, unsigned image)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx && gx->overlay && image < gx->overlays)
      return &gx->overlay[image];
   return NULL;
}

static void gekko_overlay_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   struct gekko_overlay *o = overlay_at(data, image);
   if (o)
   {
      o->tex_coord[0] = x;
      o->tex_coord[1] = y;
      o->tex_coord[2] = w;
      o->tex_coord[3] = h;
   }
}

static void gekko_overlay_vertex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   struct gekko_overlay *o = overlay_at(data, image);
   if (o)
   {
      o->vertex_coord[0] = x;
      o->vertex_coord[1] = y;
      o->vertex_coord[2] = w;
      o->vertex_coord[3] = h;
   }
}

static bool gekko_overlay_load(void *data, const void *image_data,
      unsigned num_images)
{
   unsigned i;
   gekko_video_t *gx = (gekko_video_t*)data;
   /* The image loader has already tiled these as RGBA8. */
   const struct texture_image *images =
      (const struct texture_image*)image_data;
   if (!gx)
      return false;

   free(gx->overlay);
   gx->overlays = 0;
   if (!(gx->overlay = (struct gekko_overlay*)calloc(num_images,
               sizeof(*gx->overlay))))
      return false;
   gx->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct gekko_overlay *o = &gx->overlay[i];
      gk_dcache_flush(images[i].pixels,
            (size_t)images[i].width * images[i].height * 4);
      gk_gx_tex_init(&o->tex, images[i].pixels, images[i].width,
            images[i].height, GK_GX_TF_RGBA8, GK_GX_CLAMP, GK_GX_CLAMP);
      gk_gx_tex_filter(&o->tex, GK_GX_LINEAR, GK_GX_LINEAR);
      gekko_overlay_tex_geom(gx, i, 0, 0, 1, 1);
      gekko_overlay_vertex_geom(gx, i, 0, 0, 1, 1);
      o->alpha = 1.0f;
   }
   gk_gx_invalidate_tex();
   return true;
}

static void gekko_overlay_enable(void *data, bool state)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      gx->overlay_enable = state;
}

static void gekko_overlay_full_screen(void *data, bool enable)
{
   gekko_video_t *gx = (gekko_video_t*)data;
   if (gx)
      gx->overlay_full_screen = enable;
}

static void gekko_overlay_set_alpha(void *data, unsigned image, float mod)
{
   struct gekko_overlay *o = overlay_at(data, image);
   if (o)
      o->alpha = mod;
}

static const video_overlay_interface_t gekko_overlay_interface = {
   gekko_overlay_enable,
   gekko_overlay_load,
   NULL, /* load_textures */
   gekko_overlay_tex_geom,
   gekko_overlay_vertex_geom,
   gekko_overlay_full_screen,
   gekko_overlay_set_alpha,
};

static void gekko_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &gekko_overlay_interface;
}
#endif

video_driver_t video_gx = {
   gekko_init,
   gekko_frame,
   gekko_set_nonblock_state,
   gekko_alive,
   gekko_focus,
   gekko_suppress_screensaver,
   NULL, /* has_windowed */
   gekko_set_shader,
   gekko_free,
   "gx",
   NULL, /* set_viewport */
   gekko_set_rotation,
   gekko_viewport_info,
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   gekko_get_overlay_interface,
#endif
   gekko_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
