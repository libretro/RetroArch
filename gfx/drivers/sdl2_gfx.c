/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2011-2017 - Higor Euripedes
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
#include <math.h>

#include <retro_inline.h>
#include <gfx/scaler/scaler.h>
#include <formats/image.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_X11
#include "../common/x11_common.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "SDL.h"
#include "SDL_syswm.h"
#include "../common/sdl2_common.h"

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

/*
 * FORWARD DECLARATIONS
 */

static void sdl2_gfx_free(void *data);
#ifdef HAVE_OVERLAY
static void sdl2_overlay_free(sdl2_video_t *vid);
static void sdl2_overlays_render(sdl2_video_t *vid);
#endif

static INLINE void sdl_tex_zero(sdl2_tex_t *t)
{
   if (t->tex)
      SDL_DestroyTexture(t->tex);

   t->tex = NULL;
   t->w = t->h = t->pitch = 0;
}

static void sdl2_init_font(sdl2_video_t *vid, const char *font_path,
      unsigned font_size)
{
   int i, r, g, b;
   SDL_Color colors[256];
   SDL_Surface               *tmp = NULL;
   SDL_Palette               *pal = NULL;
   const struct font_atlas *atlas = NULL;
   settings_t           *settings = config_get_ptr();
   bool video_font_enable         = settings->bools.video_font_enable;
   float msg_color_r              = settings->floats.video_msg_color_r;
   float msg_color_g              = settings->floats.video_msg_color_g;
   float msg_color_b              = settings->floats.video_msg_color_b;

   if (!video_font_enable)
      return;

   if (!font_renderer_create_default(
            &vid->font_driver, &vid->font_data,
            *font_path ? font_path : NULL, font_size))
   {
      RARCH_WARN("[SDL2] Could not initialize fonts.\n");
      return;
   }

   r           = msg_color_r * 255;
   g           = msg_color_g * 255;
   b           = msg_color_b * 255;

   r           = (r < 0) ? 0 : (r > 255 ? 255 : r);
   g           = (g < 0) ? 0 : (g > 255 ? 255 : g);
   b           = (b < 0) ? 0 : (b > 255 ? 255 : b);

   vid->font_r = r;
   vid->font_g = g;
   vid->font_b = b;

   atlas       = vid->font_driver->get_atlas(vid->font_data);

   tmp         = SDL_CreateRGBSurfaceFrom(
         atlas->buffer, atlas->width,
         atlas->height, 8, atlas->width,
         0, 0, 0, 0);

   for (i = 0; i < 256; ++i)
   {
      colors[i].r = colors[i].g = colors[i].b = i;
      colors[i].a = 255;
   }

   pal = SDL_AllocPalette(256);
   SDL_SetPaletteColors(pal, colors, 0, 256);
   SDL_SetSurfacePalette(tmp, pal);
   SDL_SetColorKey(tmp, SDL_TRUE, 0);

   vid->font.tex  = SDL_CreateTextureFromSurface(vid->renderer, tmp);

   if (vid->font.tex)
   {
      vid->font.w      = atlas->width;
      vid->font.h      = atlas->height;
      vid->font.active = true;

      SDL_SetTextureBlendMode(vid->font.tex, SDL_BLENDMODE_ADD);
   }
   else
      RARCH_WARN("[SDL2] Failed to initialize font texture: %s\n", SDL_GetError());

   SDL_FreePalette(pal);
   SDL_FreeSurface(tmp);
}

static void sdl2_render_msg(sdl2_video_t *vid, const char *msg)
{
   int delta_x, delta_y, x, y;
   unsigned width, height;
   settings_t *settings;
   float msg_pos_x, msg_pos_y;

   /* Legacy bitmap OSD font path.  Used as a fallback for the
    * yellow-text OSD output when widgets are disabled, and as the
    * set_osd_msg fallback when the supplied font_data isn't a
    * sdl2_raster_font.  Anything else (menu / widget text, OSD
    * with widgets enabled) goes through the proper render_msg
    * dispatch in sdl2_poke_set_osd_msg. */
   if (!msg || !*msg || !vid->font_data || !vid->font.tex)
      return;

   delta_x   = 0;
   delta_y   = 0;
   width     = vid->vp.width;
   height    = vid->vp.height;
   settings  = config_get_ptr();
   msg_pos_x = settings->floats.video_msg_pos_x;
   msg_pos_y = settings->floats.video_msg_pos_y;
   x         = (int)(msg_pos_x * width);
   y         = (int)((1.0f - msg_pos_y) * height);

   SDL_SetTextureColorMod(vid->font.tex,
         vid->font_r, vid->font_g, vid->font_b);

   for (; *msg; msg++)
   {
      SDL_Rect src_rect, dst_rect;
      const struct font_glyph *gly =
         vid->font_driver->get_glyph(vid->font_data, (uint8_t)*msg);

      if (!gly)
         gly = vid->font_driver->get_glyph(vid->font_data, '?');

      if (!gly)
         continue;

      src_rect.x = gly->atlas_offset_x;
      src_rect.y = gly->atlas_offset_y;
      src_rect.w = (int)gly->width;
      src_rect.h = (int)gly->height;

      dst_rect.x = x + delta_x + gly->draw_offset_x;
      dst_rect.y = y + delta_y + gly->draw_offset_y;
      dst_rect.w = (int)gly->width;
      dst_rect.h = (int)gly->height;

      SDL_RenderCopy(vid->renderer, vid->font.tex, &src_rect, &dst_rect);

      delta_x += gly->advance_x;
      delta_y -= gly->advance_y;
   }
}

static void sdl2_init_renderer(sdl2_video_t *vid)
{
   unsigned flags = SDL_RENDERER_ACCELERATED;

   if (vid->video.vsync)
      flags |= SDL_RENDERER_PRESENTVSYNC;

   SDL_ClearHints();
   SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC,
                           vid->video.vsync ? "1" : "0", SDL_HINT_OVERRIDE);
   vid->renderer = SDL_CreateRenderer(vid->window, -1, flags);

   if (!vid->renderer)
   {
      RARCH_ERR("[SDL2] Failed to initialize renderer: %s.", SDL_GetError());
      return;
   }

   SDL_SetRenderDrawColor(vid->renderer, 0, 0, 0, 255);
}

static void sdl_refresh_renderer(sdl2_video_t *vid)
{
   SDL_Rect r;

   SDL_RenderClear(vid->renderer);

   r.x      = vid->vp.x;
   r.y      = vid->vp.y;
   r.w      = (int)vid->vp.width;
   r.h      = (int)vid->vp.height;

   SDL_RenderSetViewport(vid->renderer, &r);

   /* breaks int scaling */
#if 0
   SDL_RenderSetLogicalSize(vid->renderer, vid->vp.width, vid->vp.height);
#endif
}

static void sdl_refresh_viewport(sdl2_video_t *vid)
{
   int win_w, win_h;

   SDL_GetWindowSize(vid->window, &win_w, &win_h);

   vid->vp.full_width  = win_w;
   vid->vp.full_height = win_h;
   video_driver_update_viewport(&vid->vp, false, vid->video.force_aspect, true);

   /* Tell the rest of the engine about our actual window dimensions.
    * Without this, video_st->width/height retain whatever value was
    * computed from core geometry at init time (eg 320x240 for an
    * NES-like core), so menu_driver_frame and gfx_widgets_frame
    * receive a tiny video_height in video_info, position widgets
    * relative to that tiny coordinate space, and end up drawing into
    * the top-left corner of the actual SDL framebuffer.  Most other
    * drivers (vga, gx2, d3d8, d3d9 common) make this call too. */
   video_driver_set_output_size(win_w, win_h);

   vid->flags &= ~SDL2_FLAG_SHOULD_RESIZE;

   sdl_refresh_renderer(vid);
}

static void sdl_refresh_input_size(sdl2_video_t *vid, bool menu, bool rgb32,
      unsigned width, unsigned height, unsigned pitch)
{
   sdl2_tex_t *target = menu ? &vid->menu : &vid->frame;

   if (!target->tex || target->w != width || target->h != height
       || target->rgb32 != rgb32 || target->pitch != pitch)
   {
      unsigned format;

      sdl_tex_zero(target);

      if (menu)
         format = rgb32 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGBA4444;
      else /* this assumes the frontend will convert 0RGB1555 to RGB565 */
         format = rgb32 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGB565;

      SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY,
                              (menu ? "nearest" : (vid->video.smooth ? "linear" : "nearest")),
                              SDL_HINT_OVERRIDE);

      target->tex = SDL_CreateTexture(vid->renderer, format,
                                      SDL_TEXTUREACCESS_STREAMING, width, height);

      if (!target->tex)
      {
         RARCH_ERR("[SDL2] Failed to create %s texture: %s.\n", menu ? "menu" : "main",
                   SDL_GetError());
         return;
      }

      if (menu)
         SDL_SetTextureBlendMode(target->tex, SDL_BLENDMODE_BLEND);

      target->w = width;
      target->h = height;
      target->pitch = pitch;
      target->rgb32 = rgb32;

      /* If target is menu, do not override 'active'
       * state (this should only be set by
       * sdl2_poke_texture_enable()) */
      if (!menu)
         target->active = true;
   }
}

static void *sdl2_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   int i;
   unsigned flags;
   sdl2_video_t *vid            = NULL;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);
   settings_t *settings         = config_get_ptr();
#if defined(HAVE_X11) || defined(HAVE_WAYLAND)
   const char *video_driver     = NULL;
#endif

#ifdef HAVE_X11
   XInitThreads();
#endif

   /* Initialise graphics subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_VIDEO) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
         return NULL;
   }

   vid = (sdl2_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   /* SDL2 internally uses RegisterRawInputDevices and drains the raw
    * input buffer via GetRawInputBuffer() to feed its own keyboard /
    * mouse APIs.  This consumes the WM_INPUT stream that the winraw
    * input driver's hidden HWND_MESSAGE window depends on, so its
    * keyboard / mouse polling stops receiving events.
    *
    * Unlike the d3d8 / d3d9 / d3d11 / d3d12 drivers - which create
    * their main window with a winraw-aware WndProc
    * (wnd_proc_d3d_winraw, see gfx/common/win32_common.c) - the SDL2
    * window's WndProc is owned by SDL and we cannot replace it.
    *
    * Warn loudly so users hitting silent broken-input understand
    * what to change instead of assuming the driver is broken. */
   if (string_is_equal(settings->arrays.input_driver, "raw"))
   {
      RARCH_WARN("[SDL2] The 'raw' (winraw) input driver is "
            "incompatible with the SDL2 video driver - SDL2 consumes "
            "Windows raw input internally.\n");
      RARCH_WARN("[SDL2] Mouse and keyboard input will not work.  "
            "Switch the input driver to 'dinput' under "
            "Settings -> Drivers -> Input.\n");
   }
#endif

   RARCH_LOG("[SDL2] Available renderers (change with $SDL_RENDER_DRIVER):\n");
   for (i = 0; i < SDL_GetNumRenderDrivers(); ++i)
   {
      SDL_RendererInfo renderer;
      if (SDL_GetRenderDriverInfo(i, &renderer) == 0)
         RARCH_LOG("[SDL2] \t%s\n", renderer.name);
   }

   RARCH_LOG("[SDL2] Available displays:\n");
   for (i = 0; i < SDL_GetNumVideoDisplays(); ++i)
   {
      SDL_DisplayMode mode;

      if (SDL_GetCurrentDisplayMode(i, &mode) < 0)
         RARCH_LOG("[SDL2] \tDisplay #%i mode: unknown.\n", i);
      else
         RARCH_LOG("[SDL2] \tDisplay #%i mode: %ix%i@%ihz.\n", i, mode.w, mode.h,
                   mode.refresh_rate);
   }

   if (!video->fullscreen)
      RARCH_LOG("[SDL2] Creating window @ %ux%u.\n", video->width, video->height);

   if (video->fullscreen)
      flags = settings->bools.video_windowed_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
   else
      flags = SDL_WINDOW_RESIZABLE;

   vid->window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  video->width, video->height, flags);

   if (!vid->window)
   {
      RARCH_ERR("[SDL2] Failed to init SDL window: %s.\n", SDL_GetError());
      goto error;
   }

   vid->video         = *video;
   vid->video.smooth  = video->smooth;
   vid->flags        |=  SDL2_FLAG_SHOULD_RESIZE;

   sdl_tex_zero(&vid->frame);
   sdl_tex_zero(&vid->menu);

   if (video->fullscreen)
      SDL_ShowCursor(SDL_DISABLE);

   sdl2_init_renderer(vid);
   sdl2_init_font(vid,
         settings->paths.path_font,
         settings->floats.video_font_size);

#if defined(_WIN32)
   sdl2_set_handles(vid->window, RARCH_DISPLAY_WIN32);
#elif defined(HAVE_COCOA)
   sdl2_set_handles(vid->window, RARCH_DISPLAY_OSX);
#else
#if defined(HAVE_X11) || defined(HAVE_WAYLAND)
   video_driver = SDL_GetCurrentVideoDriver();
#endif
#ifdef HAVE_X11
   if (strcmp(video_driver, "x11") == 0)
      sdl2_set_handles(vid->window, RARCH_DISPLAY_X11);
   else
#endif
#ifdef HAVE_WAYLAND
   if (strcmp(video_driver, "wayland") == 0)
      sdl2_set_handles(vid->window, RARCH_DISPLAY_WAYLAND);
   else
#endif
      sdl2_set_handles(vid->window, RARCH_DISPLAY_NONE);
#endif

   sdl_refresh_viewport(vid);

#if SDL_VERSION_ATLEAST(2, 0, 18)
   /* Set up the global OSD font (video_font_driver) using our
    * sdl2_raster_font.  Required for the "Display Statistics"
    * overlay - the central video_driver_frame path renders that
    * via font_driver_render_msg(driver_data, stat_text, params, NULL),
    * and font_driver_render_msg falls through to video_font_driver
    * when font_data is NULL.  Without this, video_font_driver stays
    * NULL on SDL2 and statistics never render.
    *
    * We also gain a working render_msg path for any other RA
    * subsystem that calls font_driver_render_msg with NULL font -
    * this is the same wiring every other modern driver does. */
   if (video->font_enable)
      font_driver_init_osd(vid, video, false, video->is_threaded,
            FONT_DRIVER_RENDER_SDL2);
#endif

   *input      = NULL;
   *input_data = NULL;

   return vid;

error:
   sdl2_gfx_free(vid);
   return NULL;
}

static void check_window(sdl2_video_t *vid)
{
   SDL_Event event;

   SDL_PumpEvents();
   while (SDL_PeepEvents(&event, 1,
            SDL_GETEVENT, SDL_QUIT, SDL_WINDOWEVENT) > 0)
   {
      switch (event.type)
      {
         case SDL_QUIT:
            vid->flags |= SDL2_FLAG_QUITTING;
            break;

         case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
               vid->flags |= SDL2_FLAG_SHOULD_RESIZE;
            break;
         default:
            break;
      }
   }
}

static bool sdl2_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   char title[128];
   sdl2_video_t *vid  = (sdl2_video_t*)data;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (vid->flags & SDL2_FLAG_SHOULD_RESIZE)
      sdl_refresh_viewport(vid);

   if (frame)
   {
      SDL_RenderClear(vid->renderer);
      sdl_refresh_input_size(vid, false, vid->video.rgb32, width, height, pitch);
      SDL_UpdateTexture(vid->frame.tex, NULL, frame, pitch);
   }

   SDL_RenderCopyEx(vid->renderer, vid->frame.tex, NULL, NULL, vid->rotation, NULL, SDL_FLIP_NONE);

#ifdef HAVE_MENU
   {
#if SDL_VERSION_ATLEAST(2, 0, 18)
      /* Non-RGUI menus (XMB / Ozone / MaterialUI) issue gfx_display
       * draw calls during menu_driver_frame and need the full-window
       * viewport for the same reason widgets do (see widget block
       * below).  RGUI doesn't care - it renders into vid->menu.tex
       * and is composited via SDL_RenderCopy after - but setting the
       * viewport unconditionally is harmless for RGUI and saves a
       * branch on whether the active menu is RGUI or not. */
      SDL_Rect screen_vp_menu;
      SDL_Rect saved_vp_menu;
      SDL_RenderGetViewport(vid->renderer, &saved_vp_menu);
      screen_vp_menu.x = 0;
      screen_vp_menu.y = 0;
      screen_vp_menu.w = (int)vid->vp.full_width;
      screen_vp_menu.h = (int)vid->vp.full_height;
      SDL_RenderSetViewport(vid->renderer, &screen_vp_menu);

      menu_driver_frame(menu_is_alive, video_info);

      SDL_RenderSetViewport(vid->renderer, &saved_vp_menu);
#else
      menu_driver_frame(menu_is_alive, video_info);
#endif
   }
#endif

   if (vid->menu.active)
      SDL_RenderCopy(vid->renderer, vid->menu.tex, NULL, NULL);

#if SDL_VERSION_ATLEAST(2, 0, 18)
   /* "Display Statistics" overlay (Settings -> Onscreen Notifications
    * -> Display Statistics).  Rendered between the menu composite
    * and widgets, matching gdi/d3d8/d3d9 order so widget toasts can
    * still paint over the stats block in the rare case they overlap.
    *
    * Suppressed while the menu is alive - the menu drivers
    * (XMB/Ozone/MaterialUI) own the screen and the overlay would
    * just bleed under their UI.  RGUI is software-rendered into
    * vid->menu.tex which is composited above, so it would actually
    * cover the stats; suppressing here keeps the behaviour
    * symmetric across menu drivers.
    *
    * Needs the full-window viewport for the same reason widgets do
    * - OSD font math is done against video_info->width/height. */
   {
      const char *stat_text          = video_info->stat_text;
      struct font_params *osd_params = (struct font_params*)
            &video_info->osd_stat_params;
      bool show_stats                = video_info->statistics_show
            && stat_text && stat_text[0] != '\0'
#ifdef HAVE_MENU
            && !menu_is_alive
#endif
            ;
      if (show_stats)
      {
         SDL_Rect screen_vp_stats;
         SDL_Rect saved_vp_stats;
         SDL_RenderGetViewport(vid->renderer, &saved_vp_stats);
         screen_vp_stats.x = 0;
         screen_vp_stats.y = 0;
         screen_vp_stats.w = (int)vid->vp.full_width;
         screen_vp_stats.h = (int)vid->vp.full_height;
         SDL_RenderSetViewport(vid->renderer, &screen_vp_stats);

         font_driver_render_msg(vid, stat_text, osd_params, NULL);

         SDL_RenderSetViewport(vid->renderer, &saved_vp_stats);
      }
   }
#endif

#ifdef HAVE_OVERLAY
   /* Input overlay (touch / virtual gamepad images).  Rendered
    * between stats and widgets so a widget toast can paint on top
    * of a button if they overlap (rare in practice — widgets land
    * in their own corner — but keeping the order consistent across
    * backends means an overlay-on-d3d9 config carries over here
    * unchanged).
    *
    * Needs the full-window viewport (the same one widgets use):
    * fullscreen overlays span the whole window including
    * letterbox bars, and even non-fullscreen overlays compute
    * against vid->vp.x/y/width/height which are also window-space
    * pixel coordinates.  Going through the game viewport (the
    * default sdl_refresh_renderer state) would clip overlay
    * buttons drawn on the pillar/letter bars. */
   if (vid->overlays_enabled && vid->overlays && vid->overlays_size)
   {
      SDL_Rect screen_vp_ov;
      SDL_Rect saved_vp_ov;
      SDL_RenderGetViewport(vid->renderer, &saved_vp_ov);
      screen_vp_ov.x = 0;
      screen_vp_ov.y = 0;
      screen_vp_ov.w = (int)vid->vp.full_width;
      screen_vp_ov.h = (int)vid->vp.full_height;
      SDL_RenderSetViewport(vid->renderer, &screen_vp_ov);

      sdl2_overlays_render(vid);

      SDL_RenderSetViewport(vid->renderer, &saved_vp_ov);
   }
#endif

#if defined(HAVE_GFX_WIDGETS) && SDL_VERSION_ATLEAST(2, 0, 18)
   /* Widgets composite over the framebuffer (and over the menu, if
    * the menu is active). gfx_widgets_frame ultimately calls back
    * into gfx_display_sdl2_draw and sdl2_raster_font_render_msg,
    * so the SDL_Renderer must already be in a state where its
    * draw color/blend won't poison the widget submissions. The
    * blend_begin/blend_end callbacks in our gfx_display backend
    * handle that on a per-call basis.
    *
    * Critically, sdl_refresh_renderer set a viewport equal to
    * vid->vp (the aspect-corrected GAME area, not the window).
    * Widgets compute coords against video_info->width/height which
    * are the full window dimensions, so without a full-window
    * viewport reset here the widget would draw in the wrong place
    * (or be clipped entirely - at 4K with a 4:3 game, the widget's
    * bottom-of-screen position lands outside vid->vp's bottom edge
    * and SDL clips it to nothing).
    *
    * Same hazard exists for menu_driver_frame above when XMB / Ozone
    * etc. issue gfx_display draws, but the existing sdl2 driver only
    * ever supported RGUI (drawn into vid->menu.tex via SDL_RenderCopy
    * which doesn't care about viewport), so that path was implicitly
    * fine.  Now that we accept arbitrary menus, set the viewport for
    * the menu pass too. */
   if (video_info->widgets_active)
   {
      SDL_Rect screen_vp;
      SDL_Rect saved_vp;
      SDL_RenderGetViewport(vid->renderer, &saved_vp);

      screen_vp.x = 0;
      screen_vp.y = 0;
      screen_vp.w = (int)vid->vp.full_width;
      screen_vp.h = (int)vid->vp.full_height;
      SDL_RenderSetViewport(vid->renderer, &screen_vp);

      gfx_widgets_frame(video_info);

      /* Restore the game viewport so the next frame's core blit
       * lands in the right place. */
      SDL_RenderSetViewport(vid->renderer, &saved_vp);
   }
#endif

   if (msg)
      sdl2_render_msg(vid, msg);

   SDL_RenderPresent(vid->renderer);

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
      SDL_SetWindowTitle((SDL_Window*)video_driver_display_userdata_get(), title);

   return true;
}

static void sdl2_gfx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   vid->video.vsync  = !toggle;
   sdl_refresh_renderer(vid);
}

static bool sdl2_gfx_alive(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   check_window(vid);
   if (vid->flags & SDL2_FLAG_QUITTING)
      return false;
   return true;
}

static bool sdl2_gfx_focus(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   unsigned flags = (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
   return (SDL_GetWindowFlags(vid->window) & flags) == flags;
}

#if !defined(HAVE_X11)
static bool sdl2_gfx_suspend_screensaver(void *data, bool enable) { return false; }
#endif

/* TODO/FIXME - implement */
static bool sdl2_gfx_has_windowed(void *data) { return true; }

static void sdl2_gfx_free(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid)
      return;

#if SDL_VERSION_ATLEAST(2, 0, 18)
   /* Mirror font_driver_init_osd in sdl2_gfx_init.  Must run before
    * SDL_DestroyRenderer because the OSD font owns SDL_Textures
    * created against vid->renderer; tearing the renderer down first
    * would leave the font driver holding dangling texture pointers
    * for the next free() call. */
   font_driver_free_osd();
#endif

#ifdef HAVE_OVERLAY
   /* Same constraint - overlay textures are owned by vid->renderer.
    * Drop them before SDL_DestroyRenderer below. */
   sdl2_overlay_free(vid);
#endif

   if (vid->renderer)
      SDL_DestroyRenderer(vid->renderer);

   if (vid->window)
      SDL_DestroyWindow(vid->window);

   if (vid->font_data)
      vid->font_driver->free(vid->font_data);

   free(vid);
}

static void sdl2_gfx_set_rotation(void *data, unsigned rotation)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;

   if (vid)
      vid->rotation = 270 * rotation;
}

static void sdl2_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   *vp = vid->vp;
}

static bool sdl2_gfx_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   SDL_Surface *surf = NULL, *bgr24 = NULL;
   sdl2_video_t *vid = (sdl2_video_t*)data;

   if (!is_idle)
      video_driver_cached_frame();

   surf  = SDL_GetWindowSurface(vid->window);
   bgr24 = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_BGR24, 0);

   if (!bgr24)
   {
      RARCH_WARN("[SDL2] Failed to convert viewport data to BGR24: %s.", SDL_GetError());
      return false;
   }

   memcpy(buffer, bgr24->pixels, bgr24->h * bgr24->pitch);

   return true;
}

static void sdl2_poke_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   vid->video.smooth = smooth;

   sdl_tex_zero(&vid->frame);
}

static void sdl2_poke_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   sdl2_video_t *vid    = (sdl2_video_t*)data;

   /* FIXME: Why is vid NULL here when starting content? */
   if (!vid)
      return;

   vid->video.force_aspect = true;
   vid->flags             |= SDL2_FLAG_SHOULD_RESIZE;
}

static void sdl2_poke_apply_state_changes(void *data)
{
   sdl2_video_t *vid       = (sdl2_video_t*)data;
   vid->flags             |= SDL2_FLAG_SHOULD_RESIZE;
}

static void sdl2_poke_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   if (frame)
   {
      sdl2_video_t *vid = (sdl2_video_t*)data;

      sdl_refresh_input_size(vid, true, rgb32, width, height,
            width * (rgb32 ? 4 : 2));

      SDL_UpdateTexture(vid->menu.tex, NULL, frame, (int)vid->menu.pitch);
   }
}

static void sdl2_poke_texture_enable(void *data,
      bool enable, bool full_screen)
{
   sdl2_video_t *vid   = (sdl2_video_t*)data;

   if (!vid)
      return;

   vid->menu.active = enable;
}

static void sdl2_poke_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;

   /* The poke->set_osd_msg API is dual-purpose:
    *
    * 1. Real OSD path (called from video_driver_frame with msg arg).
    *    `font` is NULL and we render via sdl2_render_msg's bitmap
    *    font - this is the legacy yellow-text OSD output.
    *
    * 2. gfx_display_draw_text path (used by all menu drivers and
    *    by widgets via gfx_widgets_draw_text).  `font` is a valid
    *    font_data_t* whose renderer was selected via
    *    FONT_DRIVER_RENDER_SDL2 - i.e. our sdl2_raster_font.  We
    *    must dispatch to that font driver's render_msg, otherwise
    *    every menu/widget text call falls through to the OSD font
    *    and lands in the wrong place with the wrong glyphs.
    *
    * The original sdl2 driver only supported RGUI (which renders
    * text into vid->menu.tex itself, never via gfx_display_draw_text)
    * so this branch never had a non-NULL font_data and the bug was
    * masked. */
   if (font && params)
   {
      const font_data_t *fd = (const font_data_t*)font;
      if (fd->renderer && fd->renderer->render_msg)
      {
         fd->renderer->render_msg(vid, fd->renderer_data, msg, params);
         return;
      }
   }

   sdl2_render_msg(vid, msg);
}

static void sdl2_show_mouse(void *data, bool state) { SDL_ShowCursor(state); }
static void sdl2_grab_mouse_toggle(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   SDL_SetWindowGrab(vid->window, SDL_GetWindowGrab(vid->window));
}
static uint32_t sdl2_get_flags(void *data) { return 0; }

#if SDL_VERSION_ATLEAST(2, 0, 18)
/* Texture upload hook for menu icons, the gfx_display white texture,
 * and any other gfx_display-driven texture loads. Without this hook,
 * gfx_display_init_white_texture is a no-op (gfx_white_texture stays
 * 0), which means every menu/widget quad that doesn't bind its own
 * texture passes NULL to SDL_RenderGeometry - and widgets in
 * particular rely heavily on the white texture for their tinted
 * backgrounds and panels. So the menu/widgets are visibly broken
 * until this is wired up.
 *
 * Pixel format: sdl2_get_flags returns 0 (no VIDEO_FLAG_USE_RGBA),
 * so the image task gives us pixels in BGRA byte order packed into
 * uint32. On little-endian that maps to native-uint32 0xAARRGGBB,
 * which is SDL_PIXELFORMAT_ARGB8888. On big-endian SDL_BYTEORDER
 * matches and the same format constant resolves correctly. */
static uintptr_t sdl2_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   sdl2_video_t          *vid = (sdl2_video_t*)video_data;
   struct texture_image  *ti  = (struct texture_image*)data;
   SDL_Texture           *tex = NULL;

   if (!vid || !vid->renderer || !ti || !ti->pixels || !ti->width || !ti->height)
      return 0;

   tex = SDL_CreateTexture(vid->renderer,
         SDL_PIXELFORMAT_ARGB8888,
         SDL_TEXTUREACCESS_STATIC,
         ti->width, ti->height);
   if (!tex)
      return 0;

   SDL_UpdateTexture(tex, NULL, ti->pixels, ti->width * sizeof(uint32_t));
   SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

   /* filter_type maps loosely; SDL_Renderer doesn't expose mipmaps,
    * and the only meaningful distinction is nearest vs linear. */
   if (filter_type == TEXTURE_FILTER_NEAREST
    || filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
      SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
   else
      SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);

   return (uintptr_t)tex;
}

static void sdl2_unload_texture(void *data,
      bool threaded, uintptr_t id)
{
   SDL_Texture *tex = (SDL_Texture*)id;
   (void)data;
   (void)threaded;
   if (tex)
      SDL_DestroyTexture(tex);
}
#endif

static video_poke_interface_t sdl2_video_poke_interface = {
   sdl2_get_flags,
#if SDL_VERSION_ATLEAST(2, 0, 18)
   sdl2_load_texture,
   sdl2_unload_texture,
#else
   NULL, /* load_texture - menus/widgets need SDL_RenderGeometry */
   NULL, /* unload_texture */
#endif
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   sdl2_poke_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   sdl2_poke_set_aspect_ratio,
   sdl2_poke_apply_state_changes,
   sdl2_poke_set_texture_frame,
   sdl2_poke_texture_enable,
   sdl2_poke_set_osd_msg,
   sdl2_show_mouse,
   sdl2_grab_mouse_toggle,
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void sdl2_gfx_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &sdl2_video_poke_interface;
}

static bool sdl2_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

#if defined(HAVE_GFX_WIDGETS) && SDL_VERSION_ATLEAST(2, 0, 18)
static bool sdl2_gfx_widgets_enabled(void *data) { (void)data; return true; }
#endif

/*
 * gfx_display BACKEND
 *
 * Hardware-accelerated menu/widget rendering for SDL2. Built on
 * SDL_RenderGeometry (added in SDL 2.0.18) so the menu and widget
 * quads run on the GPU through SDL_Renderer's underlying backend
 * (D3D9/11, Metal, GL/GLES depending on platform).
 *
 * The whole gfx_display backend, font driver, and the
 * gfx_widgets_enabled hook are gated on SDL >= 2.0.18. Older SDL
 * builds fall back to the existing rgui-only path (rgui renders
 * its own software framebuffer that we composite via
 * SDL_RenderCopy), which check_menu_driver_compatibility allows
 * unconditionally. The runtime guard is needed because the qb
 * configure script only requires SDL 2.0.0.
 */

#if SDL_VERSION_ATLEAST(2, 0, 18)

static void *gfx_display_sdl2_get_default_mvp(void *data)
{
   /* SDL_Renderer has no MVP concept; transforms go through
    * SDL_RenderSetScale / SDL_RenderSetViewport. The menu code
    * tolerates a NULL here for fixed-function drivers. */
   (void)data;
   return NULL;
}

static void gfx_display_sdl2_blend_begin(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid)
      return;
   SDL_SetRenderDrawBlendMode(vid->renderer, SDL_BLENDMODE_BLEND);
}

static void gfx_display_sdl2_blend_end(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid)
      return;
   SDL_SetRenderDrawBlendMode(vid->renderer, SDL_BLENDMODE_NONE);
}

static void gfx_display_sdl2_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   SDL_Rect rect;
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid)
      return;

   /* gfx_display passes scissor rects in GL convention (origin
    * bottom-left). SDL_RenderSetClipRect is top-left, so flip. */
   rect.x = x;
   rect.y = (int)video_height - y - (int)height;
   rect.w = (int)width;
   rect.h = (int)height;

   if (rect.x < 0) { rect.w += rect.x; rect.x = 0; }
   if (rect.y < 0) { rect.h += rect.y; rect.y = 0; }
   if (rect.w <= 0 || rect.h <= 0)
      return;

   SDL_RenderSetClipRect(vid->renderer, &rect);
}

static void gfx_display_sdl2_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   (void)video_width;
   (void)video_height;
   if (!vid)
      return;
   SDL_RenderSetClipRect(vid->renderer, NULL);
}

/* All gfx_display draws are triangle strips (see gl1's draw, which
 * does GL_TRIANGLE_STRIP unconditionally). SDL_RenderGeometry only
 * does triangle lists, so we expand the strip into indices on the
 * fly. Most draws are the 4-vertex quad case; we special-case it.
 *
 * Two distinct call paths converge here:
 *
 * 1. gfx_display_draw_quad - used by widgets and most menu chrome.
 *    Sets coords->vertex = NULL and coords->tex_coord = NULL, and
 *    encodes the quad rectangle in draw->x / draw->y / draw->width /
 *    draw->height (pixel coords, Y already flipped to top-left
 *    origin by the caller). gl1 handles this by substituting a
 *    static 0..1 vertex array and calling glViewport with the rect,
 *    but per-quad viewport changes don't make sense for SDL_Renderer
 *    so we synthesize the four corners directly in pixel space.
 *
 * 2. The general path - menu drivers that build their own vertex
 *    arrays in 0..1 normalized coords (origin bottom-left). We
 *    scale these to video_width / video_height and flip Y to match
 *    SDL's top-left origin.
 *
 * Without case 1, every widget call to gfx_display_draw_quad gets
 * silently dropped (vertex pointer is NULL) and the entire widget
 * system renders as nothing. */
static void gfx_display_sdl2_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   sdl2_video_t  *vid    = (sdl2_video_t*)data;
   SDL_Vertex    *verts  = NULL;
   int           *indices = NULL;
   int            quad_idx[6] = { 0, 1, 2, 2, 1, 3 };
   SDL_Texture   *tex    = NULL;
   const float   *vtx;
   const float   *tc;
   const float   *col;
   unsigned       n;
   unsigned       i;
   unsigned       num_idx;

   if (!vid || !draw || !draw->coords)
      return;

   n = draw->coords->vertices;
   if (n < 3)
      return;

   vtx = draw->coords->vertex;
   tc  = draw->coords->tex_coord;
   col = draw->coords->color;

   /* The texture handle is a uintptr_t cast of an SDL_Texture*
    * registered via sdl2_load_texture (poke->load_texture). For
    * gfx_display_draw_quad calls without an explicit texture the
    * caller substitutes gfx_white_texture, so passing this through
    * directly is safe; if SDL_RenderGeometry receives NULL we get
    * flat-shaded geometry, which is a reasonable degraded path. */
   tex = (SDL_Texture*)(uintptr_t)draw->texture;

   verts = (SDL_Vertex*)alloca(sizeof(SDL_Vertex) * n);

   /* Path 1: gfx_display_draw_quad - vtx is NULL, geometry comes
    * from draw->x/y/width/height with y bottom-up.  n is always 4.
    *
    * COORDINATE CONVENTIONS (cribbed from gdi_gfx.c, the canonical
    * reference for a top-down-pixel target):
    *
    * - draw->x / y / width / height: pixel coords, y bottom-up
    *   (gfx_display_draw_quad pre-flips: draw.y = height - y - h).
    *   To put the rect at the right spot in SDL's top-down pixel
    *   space, re-flip:
    *      dst_y = video_height - draw->height - draw->y
    *
    * - coords->tex_coord (when non-NULL): 0..1 normalised, TOP-DOWN
    *   (yes, opposite to the bottom-up vertex convention; this is
    *   documented in gfx_display.c).  Used directly without flip.
    *   When NULL we synthesise (0,0)..(1,1).
    *
    * Without honouring those conventions, widgets render at the top
    * of the screen instead of the bottom (Y not flipped), and icons
    * with sub-rect texcoords - 9-patch slices, achievement badge
    * sprites - sample from the wrong part of the atlas. */
   if (!vtx && n == 4)
   {
      float x0, x1, y0, y1;

      /* Defensive clamp: gfx_widgets_draw_icon's coordinate math
       * depends on widget layout values that can underflow during
       * the first few frames after icon load, producing
       * draw->y == INT_MIN.  Float-converting that and feeding it
       * to SDL_RenderGeometry produces NaN vertex positions and a
       * spurious SDL error that pollutes the renderer state for
       * subsequent draws, making the entire widget invisible.
       * Reject any rect whose origin / extent can't fit in a
       * reasonable floating point coord. */
      if (   draw->x < -65536 || draw->x > 65536
          || draw->y < -65536 || draw->y > 65536
          || draw->width  > 65536
          || draw->height > 65536)
         return;

      x0 = (float)draw->x;
      x1 = (float)draw->x + (float)draw->width;
      /* Re-flip Y from bottom-up to SDL top-down. */
      y0 = (float)video_height - (float)draw->height - (float)draw->y;
      y1 = y0 + (float)draw->height;

      /* Apply draw->scale_factor (centred scaling around the quad's
       * midpoint).  XMB sets this on icon draws (node->zoom) to grow
       * the active tab icon ~2x; without honouring it every icon
       * renders at base size and the active tab is visually
       * indistinguishable.  Only the plain-quad path uses
       * scale_factor; the slice path encodes scaling in
       * coords->vertex. */
      if (draw->scale_factor > 0.0f && draw->scale_factor != 1.0f)
      {
         float cx = (x0 + x1) * 0.5f;
         float cy = (y0 + y1) * 0.5f;
         float hw = (x1 - x0) * 0.5f * draw->scale_factor;
         float hh = (y1 - y0) * 0.5f * draw->scale_factor;
         x0 = cx - hw; x1 = cx + hw;
         y0 = cy - hh; y1 = cy + hh;
      }

      /* Default texcoords cover the whole texture; tex_coord is
       * top-down so (0,0)..(1,1) means the upper-left source pixel
       * maps to the upper-left of the quad. */
      {
         float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
         if (tc)
         {
            /* tex_coord is 4 (u,v) pairs.  For axis-aligned quads
             * (everything that hits this path) the bbox IS the
             * slice, so we don't need to track per-vertex order. */
            float min_u = tc[0], max_u = tc[0];
            float min_v = tc[1], max_v = tc[1];
            unsigned k;
            for (k = 1; k < 4; k++)
            {
               float u = tc[k * 2 + 0];
               float v = tc[k * 2 + 1];
               if (u < min_u) min_u = u;
               if (u > max_u) max_u = u;
               if (v < min_v) min_v = v;
               if (v > max_v) max_v = v;
            }
            u0 = min_u; v0 = min_v; u1 = max_u; v1 = max_v;
         }

         /* Vertex order matches gl1_menu_vertexes (the canonical
          * triangle-strip layout used by every gfx_display caller):
          *
          *   vertex 0 -> (x=0, y=0) bottom-left in GL bottom-up coords
          *   vertex 1 -> (x=1, y=0) bottom-right
          *   vertex 2 -> (x=0, y=1) top-left
          *   vertex 3 -> (x=1, y=1) top-right
          *
          * After Y is flipped to SDL's top-down screen space, vertex
          * indices stay the same but the geometric meaning becomes:
          *
          *   vertex 0 -> bottom-left in screen space  (high pixel Y)
          *   vertex 1 -> bottom-right                 (high pixel Y)
          *   vertex 2 -> top-left                     (low  pixel Y)
          *   vertex 3 -> top-right                    (low  pixel Y)
          *
          * This matters for per-vertex colour interpolation -
          * gfx_widget_load_content_animation puts an opaque alpha on
          * vertices 0/1 of its top-shadow gradient, expecting that
          * pair to be the edge touching the main bar.  If we map
          * indices 0/1 to the top edge in screen space the gradient
          * inverts and a visible hard line appears at the bar/shadow
          * boundary. */
         verts[0].position.x = x0; verts[0].position.y = y1;
         verts[0].tex_coord.x = u0; verts[0].tex_coord.y = v1;
         verts[1].position.x = x1; verts[1].position.y = y1;
         verts[1].tex_coord.x = u1; verts[1].tex_coord.y = v1;
         verts[2].position.x = x0; verts[2].position.y = y0;
         verts[2].tex_coord.x = u0; verts[2].tex_coord.y = v0;
         verts[3].position.x = x1; verts[3].position.y = y0;
         verts[3].tex_coord.x = u1; verts[3].tex_coord.y = v0;

         /* Apply 2D rotation around the rect's centre.  Used by
          * gfx_widgets_draw_icon for the spinning hourglass on
          * pending tasks - draw->rotation is the angle in radians,
          * draw->matrix_data also encodes it but rotating the four
          * corners directly is simpler than multiplying every vertex
          * by a 4x4.  SDL_RenderGeometry happily takes non-axis-
          * aligned quads.
          *
          * Y axis flips because SDL is top-down: a positive radians
          * value should rotate clockwise on screen (matching what
          * gl/d3d produce after their viewport transform), which
          * means negating sine. */
         if (draw->rotation != 0.0f)
         {
            float cx  = (x0 + x1) * 0.5f;
            float cy  = (y0 + y1) * 0.5f;
            float c   = cosf(draw->rotation);
            float s   = sinf(draw->rotation);
            unsigned k;
            for (k = 0; k < 4; k++)
            {
               float dx = verts[k].position.x - cx;
               float dy = verts[k].position.y - cy;
               verts[k].position.x = cx + dx * c + dy * s;
               verts[k].position.y = cy - dx * s + dy * c;
            }
         }
      }

      for (i = 0; i < 4; i++)
      {
         if (col)
         {
            verts[i].color.r = (Uint8)(col[i * 4 + 0] * 255.0f);
            verts[i].color.g = (Uint8)(col[i * 4 + 1] * 255.0f);
            verts[i].color.b = (Uint8)(col[i * 4 + 2] * 255.0f);
            verts[i].color.a = (Uint8)(col[i * 4 + 3] * 255.0f);
         }
         else
         {
            verts[i].color.r = verts[i].color.g
                             = verts[i].color.b
                             = verts[i].color.a = 255;
         }
      }

      SDL_RenderGeometry(vid->renderer, tex, verts, 4, quad_idx, 6);
      return;
   }

   /* Path 2: caller-supplied vertex array in 0..1 normalised coords
    * (origin bottom-up).  Convert to SDL pixel coords (top-down) by
    * scaling and flipping Y.  Texcoords are top-down already - no
    * flip. */
   if (!vtx)
      return;

   for (i = 0; i < n; i++)
   {
      float vx = vtx[i * 2 + 0];
      float vy = vtx[i * 2 + 1];

      verts[i].position.x = vx * (float)video_width;
      verts[i].position.y = (1.0f - vy) * (float)video_height;

      if (tc)
      {
         /* tex_coord is top-down per gfx_display.c convention; no
          * flip.  Vertex Y was flipped above (bottom-up to top-down)
          * but tex_coord is already in the same orientation as our
          * SDL pixel target. */
         verts[i].tex_coord.x = tc[i * 2 + 0];
         verts[i].tex_coord.y = tc[i * 2 + 1];
      }
      else
      {
         verts[i].tex_coord.x = 0.0f;
         verts[i].tex_coord.y = 0.0f;
      }

      if (col)
      {
         verts[i].color.r = (Uint8)(col[i * 4 + 0] * 255.0f);
         verts[i].color.g = (Uint8)(col[i * 4 + 1] * 255.0f);
         verts[i].color.b = (Uint8)(col[i * 4 + 2] * 255.0f);
         verts[i].color.a = (Uint8)(col[i * 4 + 3] * 255.0f);
      }
      else
      {
         verts[i].color.r = verts[i].color.g
                          = verts[i].color.b
                          = verts[i].color.a = 255;
      }
   }

   /* Quad fast path - the common case for menu items. */
   if (n == 4)
   {
      SDL_RenderGeometry(vid->renderer, tex, verts, 4, quad_idx, 6);
      return;
   }

   /* General triangle-strip expansion: n verts -> (n-2) triangles. */
   num_idx = (n - 2) * 3;
   indices = (int*)alloca(sizeof(int) * num_idx);
   for (i = 0; i < n - 2; i++)
   {
      if ((i & 1) == 0)
      {
         indices[i * 3 + 0] = (int)i;
         indices[i * 3 + 1] = (int)i + 1;
         indices[i * 3 + 2] = (int)i + 2;
      }
      else
      {
         /* Flip winding on odd triangles to keep the strip
          * consistent (matches GL_TRIANGLE_STRIP semantics). */
         indices[i * 3 + 0] = (int)i + 1;
         indices[i * 3 + 1] = (int)i;
         indices[i * 3 + 2] = (int)i + 2;
      }
   }

   SDL_RenderGeometry(vid->renderer, tex, verts, n, indices, num_idx);
}

/* Pipeline draws (XMB ribbon, snow, snowflake, bokeh) require a
 * programmable pipeline. SDL_Renderer does not expose one, so we
 * leave this as a no-op - the menu still renders, just without the
 * animated background. Documented as a feature gap. */
static void gfx_display_sdl2_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width, unsigned video_height)
{
   (void)draw;
   (void)p_disp;
   (void)data;
   (void)video_width;
   (void)video_height;
}

gfx_display_ctx_driver_t gfx_display_ctx_sdl2 = {
   gfx_display_sdl2_draw,
   gfx_display_sdl2_draw_pipeline,
   gfx_display_sdl2_blend_begin,
   gfx_display_sdl2_blend_end,
   gfx_display_sdl2_get_default_mvp,
   NULL, /* get_default_vertices */
   NULL, /* get_default_tex_coords */
   FONT_DRIVER_RENDER_SDL2,
   GFX_VIDEO_DRIVER_SDL2,
   "sdl2",
   false,
   gfx_display_sdl2_scissor_begin,
   gfx_display_sdl2_scissor_end
};

/*
 * FONT DRIVER
 *
 * Submits glyph quads through SDL_RenderGeometry so menu/widget text
 * shares the exact same vertex pipeline (and therefore the same
 * batched blend state, scissor state, and z-ordering) as the menu
 * quads emitted by gfx_display_sdl2_draw. The atlas is uploaded as a
 * single RGBA SDL_Texture; we expand the 8-bit alpha buffer that
 * font_renderer produces into RGBA so vertex colors can tint it.
 *
 * This is independent of the OSD font path (sdl2_init_font /
 * sdl2_render_msg) which is used for on-screen messages drawn over
 * the emulated framebuffer. Both can coexist.
 */

typedef struct
{
   sdl2_video_t                  *vid;
   SDL_Texture                   *tex;
   const font_renderer_driver_t  *font_driver;
   void                          *font_data;
   struct font_atlas             *atlas;
   int                            tex_width;
   int                            tex_height;
   bool                           atlas_dirty;
} sdl2_raster_t;

static void sdl2_raster_font_upload_atlas(sdl2_raster_t *font)
{
   uint32_t *rgba;
   int       i, total;
   const uint8_t *src;

   if (!font || !font->atlas)
      return;

   if (font->tex)
   {
      SDL_DestroyTexture(font->tex);
      font->tex = NULL;
   }

   font->tex_width  = (int)font->atlas->width;
   font->tex_height = (int)font->atlas->height;

   font->tex = SDL_CreateTexture(font->vid->renderer,
         SDL_PIXELFORMAT_ABGR8888,
         SDL_TEXTUREACCESS_STATIC,
         font->tex_width, font->tex_height);
   if (!font->tex)
      return;

   total = font->tex_width * font->tex_height;
   rgba  = (uint32_t*)malloc(total * sizeof(uint32_t));
   if (!rgba)
   {
      SDL_DestroyTexture(font->tex);
      font->tex = NULL;
      return;
   }

   /* Atlas buffer is 8-bit alpha. Expand to white-RGB plus the alpha
    * value so vertex color modulation produces correctly-tinted
    * glyphs. SDL_PIXELFORMAT_ABGR8888 is byte order R,G,B,A on
    * little-endian, packed as 0xAABBGGRR in a uint32_t. */
   src = font->atlas->buffer;
   for (i = 0; i < total; i++)
   {
      uint32_t a = src[i];
      rgba[i] = (a << 24) | 0x00FFFFFFu;
   }

   SDL_UpdateTexture(font->tex, NULL, rgba, font->tex_width * sizeof(uint32_t));
   SDL_SetTextureBlendMode(font->tex, SDL_BLENDMODE_BLEND);

   free(rgba);
   font->atlas->dirty = false;
   font->atlas_dirty  = false;
}

static void *sdl2_raster_font_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   sdl2_raster_t *font;
   sdl2_video_t  *vid = (sdl2_video_t*)data;

   if (!vid || !vid->renderer)
   {
      RARCH_WARN("[SDL2] sdl2_raster_font_init: no video data or renderer "
            "(vid=%p, renderer=%p) - widget/menu fonts will be unavailable\n",
            (void*)vid, vid ? (void*)vid->renderer : NULL);
      return NULL;
   }

   font = (sdl2_raster_t*)calloc(1, sizeof(*font));
   if (!font)
      return NULL;

   font->vid = vid;

   if (!font_renderer_create_default(
            &font->font_driver, &font->font_data,
            font_path, font_size))
   {
      RARCH_WARN("[SDL2] sdl2_raster_font_init: font_renderer_create_default "
            "failed for path '%s' size %.1f\n",
            font_path ? font_path : "(default)", font_size);
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);
   sdl2_raster_font_upload_atlas(font);

   if (!font->tex)
   {
      RARCH_WARN("[SDL2] sdl2_raster_font_init: atlas upload failed: %s\n",
            SDL_GetError());
      font->font_driver->free(font->font_data);
      free(font);
      return NULL;
   }

   return font;
}

static void sdl2_raster_font_free(void *data, bool is_threaded)
{
   sdl2_raster_t *font = (sdl2_raster_t*)data;
   if (!font)
      return;
   if (font->tex)
      SDL_DestroyTexture(font->tex);
   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);
   free(font);
}

static int sdl2_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   sdl2_raster_t *font = (sdl2_raster_t*)data;
   const char    *cur  = msg;
   size_t         i    = 0;
   int            width = 0;

   if (!font || !msg)
      return 0;

   while (i < msg_len && *cur)
   {
      const struct font_glyph *glyph =
         font->font_driver->get_glyph(font->font_data, (uint8_t)*cur);
      if (!glyph)
         glyph = font->font_driver->get_glyph(font->font_data, '?');
      if (glyph)
         width += glyph->advance_x;
      i++;
      cur++;
   }

   return (int)((float)width * scale);
}

/* Render a single line into one SDL_RenderGeometry batch. Up to
 * MAX_GLYPHS per submitted batch; we flush mid-line for longer runs.
 *
 * Coordinates: render_msg gives us params->x/y in 0..1 normalized
 * space. We convert to pixel coords against the full window, with
 * a top-left origin (SDL convention). */
static void sdl2_raster_font_render_line(
      sdl2_raster_t *font,
      const char *msg, size_t msg_len,
      float scale,
      const SDL_Color col,
      float pos_x, float pos_y,
      enum text_alignment align,
      unsigned width, unsigned height)
{
#define SDL2_FONT_MAX_GLYPHS 256
   SDL_Vertex  verts[SDL2_FONT_MAX_GLYPHS * 4];
   int         idx[SDL2_FONT_MAX_GLYPHS * 6];
   int         n_glyphs = 0;
   const char *cur      = msg;
   size_t      ci       = 0;
   float       x        = pos_x;
   float       y        = pos_y;
   float       inv_w;
   float       inv_h;

   if (!font || !font->tex)
      return;

   if (font->atlas_dirty || font->atlas->dirty)
      sdl2_raster_font_upload_atlas(font);

   /* gfx_display_draw_text gives us params->x/y in normalized 0..1
    * coords (origin bottom-left to match GL). Convert to pixels
    * with a top-left origin. */
   x = pos_x * (float)width;
   y = (1.0f - pos_y) * (float)height;

   if (align == TEXT_ALIGN_RIGHT)
      x -= sdl2_raster_font_get_message_width(font, msg, msg_len, scale);
   else if (align == TEXT_ALIGN_CENTER)
      x -= sdl2_raster_font_get_message_width(font, msg, msg_len, scale)
         * 0.5f;

   inv_w = 1.0f / (float)font->tex_width;
   inv_h = 1.0f / (float)font->tex_height;

   /* gfx_display draw_text passes 8-bit-clean strings; we walk byte
    * by byte. UTF-8 multi-byte sequences are handled by the upstream
    * font renderer's glyph lookup (which keys on uint32_t codepoints
    * but tolerates byte-by-byte queries for the ASCII-only RA UI). */
   while (ci < msg_len && *cur)
   {
      const struct font_glyph *glyph =
         font->font_driver->get_glyph(font->font_data, (uint8_t)*cur);
      float gx, gy, gw, gh;
      float u0, v0, u1, v1;
      int   base;

      if (!glyph)
         glyph = font->font_driver->get_glyph(font->font_data, '?');
      if (!glyph)
      {
         ci++;
         cur++;
         continue;
      }

      gx = x + glyph->draw_offset_x * scale;
      gy = y + glyph->draw_offset_y * scale;
      gw = glyph->width  * scale;
      gh = glyph->height * scale;

      u0 = (float)glyph->atlas_offset_x * inv_w;
      v0 = (float)glyph->atlas_offset_y * inv_h;
      u1 = u0 + (float)glyph->width     * inv_w;
      v1 = v0 + (float)glyph->height    * inv_h;

      base = n_glyphs * 4;

      verts[base + 0].position.x  = gx;
      verts[base + 0].position.y  = gy;
      verts[base + 0].tex_coord.x = u0;
      verts[base + 0].tex_coord.y = v0;
      verts[base + 0].color       = col;

      verts[base + 1].position.x  = gx + gw;
      verts[base + 1].position.y  = gy;
      verts[base + 1].tex_coord.x = u1;
      verts[base + 1].tex_coord.y = v0;
      verts[base + 1].color       = col;

      verts[base + 2].position.x  = gx;
      verts[base + 2].position.y  = gy + gh;
      verts[base + 2].tex_coord.x = u0;
      verts[base + 2].tex_coord.y = v1;
      verts[base + 2].color       = col;

      verts[base + 3].position.x  = gx + gw;
      verts[base + 3].position.y  = gy + gh;
      verts[base + 3].tex_coord.x = u1;
      verts[base + 3].tex_coord.y = v1;
      verts[base + 3].color       = col;

      idx[n_glyphs * 6 + 0] = base + 0;
      idx[n_glyphs * 6 + 1] = base + 1;
      idx[n_glyphs * 6 + 2] = base + 2;
      idx[n_glyphs * 6 + 3] = base + 2;
      idx[n_glyphs * 6 + 4] = base + 1;
      idx[n_glyphs * 6 + 5] = base + 3;

      x += glyph->advance_x * scale;
      n_glyphs++;
      ci++;
      cur++;

      if (n_glyphs >= SDL2_FONT_MAX_GLYPHS)
      {
         SDL_RenderGeometry(font->vid->renderer, font->tex,
               verts, n_glyphs * 4, idx, n_glyphs * 6);
         n_glyphs = 0;
      }
   }

   if (n_glyphs > 0)
      SDL_RenderGeometry(font->vid->renderer, font->tex,
            verts, n_glyphs * 4, idx, n_glyphs * 6);
#undef SDL2_FONT_MAX_GLYPHS
}

/* Walk a (possibly multi-line) string and call render_line once per
 * line segment, dropping each subsequent line by one line-height in
 * GL-convention (params->y increases upward, so we subtract).
 *
 * Required because callers like XMB sublabels embed real '\n' bytes
 * into their wrapped text — gfx_display_draw_text doesn't pre-split
 * for us, and feeding the newline straight to render_line just looks
 * up '\n' in the glyph atlas, gets a tofu placeholder, and renders it
 * as garbage (visible as a small box between words).  Mirrors gl1's
 * gl1_raster_font_render_message wrapper. */
static void sdl2_raster_font_render_message(
      sdl2_raster_t *font, const char *msg, float scale,
      const SDL_Color col, float pos_x, float pos_y,
      enum text_alignment align, unsigned width, unsigned height)
{
   struct font_line_metrics *line_metrics = NULL;
   float line_height_norm                 = 0.0f;
   int   lines                            = 0;

   if (font->font_driver && font->font_driver->get_line_metrics)
   {
      font->font_driver->get_line_metrics(font->font_data, &line_metrics);
      if (line_metrics && height > 0)
         line_height_norm = (float)line_metrics->height * scale
                          / (float)height;
   }

   for (;;)
   {
      const char *p   = msg;
      size_t      len;

      while (*p && *p != '\n')
         p++;
      len = (size_t)(p - msg);

      if (len > 0)
         sdl2_raster_font_render_line(font, msg, len, scale, col,
               pos_x,
               pos_y - (float)lines * line_height_norm,
               align, width, height);

      if (!*p)
         break;
      msg = p + 1;
      lines++;
   }
}

static void sdl2_raster_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   sdl2_raster_t *font = (sdl2_raster_t*)data;
   sdl2_video_t  *vid  = (sdl2_video_t*)userdata;
   SDL_Color      col;
   SDL_Color      col_drop;
   float          x, y, scale;
   int            drop_x, drop_y;
   float          drop_mod, drop_alpha;
   enum text_alignment align = TEXT_ALIGN_LEFT;
   unsigned       width;
   unsigned       height;

   if (!font || !msg || !*msg || !vid)
      return;

   width  = vid->vp.full_width  ? vid->vp.full_width  : vid->video.width;
   height = vid->vp.full_height ? vid->vp.full_height : vid->video.height;
   if (!width || !height)
   {
      /* viewport not set up yet (very early frames) - skip rather
       * than divide by zero downstream. */
      return;
   }

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      align      = params->text_align;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;

      col.r = FONT_COLOR_GET_RED(params->color);
      col.g = FONT_COLOR_GET_GREEN(params->color);
      col.b = FONT_COLOR_GET_BLUE(params->color);
      col.a = FONT_COLOR_GET_ALPHA(params->color);
      if (col.a == 0)
         col.a = 255;
   }
   else
   {
      x          = 0.0f;
      y          = 0.0f;
      scale      = 1.0f;
      drop_x     = 0;
      drop_y     = 0;
      drop_mod   = 0.0f;
      drop_alpha = 0.0f;
      col.r = col.g = col.b = col.a = 255;
   }

   if (drop_x || drop_y)
   {
      col_drop.r = (Uint8)(col.r * drop_mod);
      col_drop.g = (Uint8)(col.g * drop_mod);
      col_drop.b = (Uint8)(col.b * drop_mod);
      col_drop.a = (Uint8)(col.a * drop_alpha);

      sdl2_raster_font_render_message(font, msg, scale, col_drop,
            x + scale * drop_x / (float)width,
            y + scale * drop_y / (float)height,
            align, width, height);
   }

   sdl2_raster_font_render_message(font, msg, scale, col,
         x, y, align, width, height);
}

static const struct font_glyph *sdl2_raster_font_get_glyph(
      void *data, uint32_t code)
{
   sdl2_raster_t *font = (sdl2_raster_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph(font->font_data, code);
   return NULL;
}

static bool sdl2_raster_font_get_line_metrics(void *data,
      struct font_line_metrics **metrics)
{
   sdl2_raster_t *font = (sdl2_raster_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t sdl2_raster_font = {
   sdl2_raster_font_init,
   sdl2_raster_font_free,
   sdl2_raster_font_render_msg,
   "sdl2",
   sdl2_raster_font_get_glyph,
   NULL, /* bind_block  - no batched render-block support yet  */
   NULL, /* flush_block - widgets/menu work without it         */
   sdl2_raster_font_get_message_width,
   sdl2_raster_font_get_line_metrics
};

#endif /* SDL_VERSION_ATLEAST(2, 0, 18) */

#ifdef HAVE_OVERLAY
/*
 * INPUT OVERLAY DRIVER
 *
 * Implements video_overlay_interface_t for SDL2.  The overlay
 * subsystem (input/input_overlay.c) hands us BGRA32 RGBA images
 * via load(), tells us where they go in 0..1 normalised space via
 * vertex_geom() / tex_geom(), and we draw them on top of the game
 * frame each frame using SDL_RenderCopy with per-texture alpha
 * modulation.
 *
 * Storage: each loaded overlay is uploaded once into a streaming
 * SDL_Texture at load time, then redrawn every frame from that
 * texture.  Mirrors the per-load static-texture pattern d3d8 / d3d9
 * use.
 *
 * Coordinate conventions (matching d3d8/d3d9/gl):
 *   - vert_coords[0..3] = (x, y, w, h) in 0..1 normalised space.
 *     The setter flips y to (1.0f - y) and negates h, the same
 *     adjustment d3d8 makes for D3D's y-up convention.  We undo
 *     that flip at render time so the overlay lands the right way
 *     up in SDL's y-down space.
 *   - tex_coords[0..3] = (x, y, w, h) in 0..1 texture space.
 *     Used to sub-rect the source texture - typical touch overlays
 *     pack many buttons into one atlas, then split via tex_geom().
 *   - fullscreen=true: vert_coords span the entire window, including
 *     letterbox/pillarbox bars.  Touch overlays use this so the
 *     buttons keep working when the game is letterboxed.
 *   - fullscreen=false: vert_coords span only the game viewport.
 */
static void sdl2_overlay_free(sdl2_video_t *vid)
{
   unsigned i;
   if (!vid || !vid->overlays)
      return;
   for (i = 0; i < vid->overlays_size; i++)
   {
      if (vid->overlays[i].tex)
         SDL_DestroyTexture(vid->overlays[i].tex);
   }
   free(vid->overlays);
   vid->overlays      = NULL;
   vid->overlays_size = 0;
}

static bool sdl2_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i;
   sdl2_video_t                *vid  = (sdl2_video_t*)data;
   const struct texture_image  *imgs = (const struct texture_image*)image_data;

   if (!vid)
      return false;

   /* Drop any prior overlay set first.  load() is the install
    * point - input_overlay.c calls it once per overlay activation
    * with the full image array, never incrementally. */
   sdl2_overlay_free(vid);

   if (num_images == 0 || !imgs)
      return true;

   vid->overlays = (struct sdl2_overlay*)calloc(num_images,
         sizeof(*vid->overlays));
   if (!vid->overlays)
      return false;
   vid->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      SDL_Texture *tex;
      struct sdl2_overlay *o = &vid->overlays[i];
      unsigned             w = imgs[i].width;
      unsigned             h = imgs[i].height;

      if (w == 0 || h == 0 || !imgs[i].pixels)
         continue;

      /* Static so we can SDL_UpdateTexture once at load time;
       * source pixels are 0xAARRGGBB in memory order, which on
       * little-endian maps to SDL_PIXELFORMAT_ARGB8888. */
      tex = SDL_CreateTexture(vid->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STATIC,
            (int)w, (int)h);
      if (!tex)
         continue;

      SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
      SDL_UpdateTexture(tex, NULL, imgs[i].pixels, (int)(w * 4));

      o->tex            = tex;
      o->tex_w          = w;
      o->tex_h          = h;
      o->alpha_mod      = 1.0f;
      o->fullscreen     = false;
      /* Default to whole-texture / whole-target until vertex_geom
       * and tex_geom set the real values. */
      o->tex_coords[0]  = 0.0f;
      o->tex_coords[1]  = 0.0f;
      o->tex_coords[2]  = 1.0f;
      o->tex_coords[3]  = 1.0f;
      o->vert_coords[0] = 0.0f;
      o->vert_coords[1] = 0.0f;
      o->vert_coords[2] = 1.0f;
      o->vert_coords[3] = 1.0f;
   }

   return true;
}

static void sdl2_overlay_tex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid || index >= vid->overlays_size)
      return;
   vid->overlays[index].tex_coords[0] = x;
   vid->overlays[index].tex_coords[1] = y;
   vid->overlays[index].tex_coords[2] = w;
   vid->overlays[index].tex_coords[3] = h;
}

static void sdl2_overlay_vertex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid || index >= vid->overlays_size)
      return;
   /* Overlay descriptor coordinates from input_overlay are already
    * y-down (y=0 top of screen, y=1 bottom — same convention as
    * RETRO_DEVICE_POINTER which the hit-test path uses) and (x, y)
    * names the top-left corner of the rect.  SDL2's render space
    * (SDL_RenderCopy with y-down SDL_Rect) is also y-down with the
    * same origin, so we store the values verbatim and the render
    * path does a direct multiply.
    *
    * d3d8 / d3d9 / gl all flip y here (y = 1.0f - y; h = -h;)
    * because their pipelines emit vertices in y-up clip space and
    * rely on the viewport transform to flip back to screen.  SDL's
    * high-level render API has no such pipeline - SDL_RenderCopy
    * blits straight to pixel space - so the flip is a bug for us,
    * not a compatibility shim.  Mirrors gdi_overlay_vertex_geom for
    * the same reason. */
   vid->overlays[index].vert_coords[0] = x;
   vid->overlays[index].vert_coords[1] = y;
   vid->overlays[index].vert_coords[2] = w;
   vid->overlays[index].vert_coords[3] = h;
}

static void sdl2_overlay_enable(void *data, bool state)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid)
      return;
   vid->overlays_enabled = state;
}

static void sdl2_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid || !vid->overlays)
      return;
   for (i = 0; i < vid->overlays_size; i++)
      vid->overlays[i].fullscreen = enable;
}

static void sdl2_overlay_set_alpha(void *data, unsigned index, float mod)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid || index >= vid->overlays_size)
      return;
   vid->overlays[index].alpha_mod = mod;
}

/* Render every enabled overlay with the current SDL_Renderer.
 * Caller is responsible for setting an appropriate viewport before
 * calling - typically the full-window viewport so fullscreen
 * overlays cover the entire output, with non-fullscreen overlays
 * placed in window-relative coords by the caller's choice of
 * base_x/base_y/base_w/base_h. */
static void sdl2_overlays_render(sdl2_video_t *vid)
{
   unsigned i;

   if (!vid || !vid->overlays || !vid->overlays_enabled)
      return;

   for (i = 0; i < vid->overlays_size; i++)
   {
      SDL_Rect dst;
      struct sdl2_overlay *o = &vid->overlays[i];
      float vx, vy, vw, vh;
      int   base_x, base_y;
      unsigned base_w, base_h;
      Uint8 alpha_byte;

      if (!o->tex || o->tex_w == 0 || o->tex_h == 0)
         continue;
      if (o->alpha_mod <= 0.0f)
         continue;

      /* vert_coords are stored verbatim in y-down 0..1 space with
       * (x, y) = top-left corner of the rect, so a straight multiply
       * by base_w/base_h gives the destination rect in pixel coords
       * (no flip).  See sdl2_overlay_vertex_geom for the rationale. */
      vx = o->vert_coords[0];
      vy = o->vert_coords[1];
      vw = o->vert_coords[2];
      vh = o->vert_coords[3];

      if (o->fullscreen)
      {
         base_x = 0;
         base_y = 0;
         base_w = vid->vp.full_width;
         base_h = vid->vp.full_height;
      }
      else
      {
         base_x = (int)vid->vp.x;
         base_y = (int)vid->vp.y;
         base_w = vid->vp.width;
         base_h = vid->vp.height;
      }

      dst.x = base_x + (int)(vx * (float)base_w);
      dst.y = base_y + (int)(vy * (float)base_h);
      dst.w =          (int)(vw * (float)base_w);
      dst.h =          (int)(vh * (float)base_h);

      if (dst.w <= 0 || dst.h <= 0)
         continue;

      alpha_byte = (Uint8)(o->alpha_mod * 255.0f);
      SDL_SetTextureAlphaMod(o->tex, alpha_byte);

      /* tex_coords sub-rect into the source texture.  Most touch
       * overlays use the full image (0,0,1,1) but atlases need this
       * to pick the right button. */
      {
         SDL_Rect src;
         src.x = (int)(o->tex_coords[0] * (float)o->tex_w);
         src.y = (int)(o->tex_coords[1] * (float)o->tex_h);
         src.w = (int)(o->tex_coords[2] * (float)o->tex_w);
         src.h = (int)(o->tex_coords[3] * (float)o->tex_h);
         if (src.w <= 0 || src.h <= 0)
         {
            src.x = 0; src.y = 0;
            src.w = (int)o->tex_w; src.h = (int)o->tex_h;
         }
         SDL_RenderCopy(vid->renderer, o->tex, &src, &dst);
      }
   }
}

static const video_overlay_interface_t sdl2_overlay_iface = {
   sdl2_overlay_enable,
   sdl2_overlay_load,
   sdl2_overlay_tex_geom,
   sdl2_overlay_vertex_geom,
   sdl2_overlay_full_screen,
   sdl2_overlay_set_alpha
};

static void sdl2_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &sdl2_overlay_iface;
}
#endif /* HAVE_OVERLAY */

video_driver_t video_sdl2 = {
   sdl2_gfx_init,
   sdl2_gfx_frame,
   sdl2_gfx_set_nonblock_state,
   sdl2_gfx_alive,
   sdl2_gfx_focus,
#ifdef HAVE_X11
   x11_suspend_screensaver,
#else
   sdl2_gfx_suspend_screensaver,
#endif
   sdl2_gfx_has_windowed,
   sdl2_gfx_set_shader,
   sdl2_gfx_free,
   "sdl2",
   NULL, /* set_viewport */
   sdl2_gfx_set_rotation,
   sdl2_gfx_viewport_info,
   sdl2_gfx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   sdl2_get_overlay_interface,
#endif
   sdl2_gfx_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
#if SDL_VERSION_ATLEAST(2, 0, 18)
   sdl2_gfx_widgets_enabled
#else
   NULL  /* widgets need SDL_RenderGeometry */
#endif
#endif
};
