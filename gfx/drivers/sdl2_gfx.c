/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Higor Euripedes
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

#include "SDL.h"
#include "SDL_syswm.h"
#include "../../driver.h"
#include <stdlib.h>
#include <string.h>
#include "../../general.h"
#include "../../retroarch.h"
#include "../../performance.h"
#include <retro_inline.h>
#include <gfx/scaler/scaler.h>
#include "../video_monitor.h"
#include "../video_context_driver.h"
#include "../font_renderer_driver.h"

#ifdef HAVE_X11
#include "../common/x11_common.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

typedef struct sdl2_tex
{
   SDL_Texture *tex;

   unsigned w;
   unsigned h;
   size_t pitch;
   bool active;
   bool rgb32;
} sdl2_tex_t;

typedef struct _sdl2_video
{
   SDL_Window *window;
   SDL_Renderer *renderer;

   sdl2_tex_t frame;
   sdl2_tex_t menu;
   sdl2_tex_t font;

   bool gl;
   bool quitting;

   void *font_data;
   const font_renderer_driver_t *font_driver;
   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;

   struct video_viewport vp;

   video_info_t video;

   bool should_resize;
   double rotation;

} sdl2_video_t;

static void sdl2_gfx_free(void *data);

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
   SDL_Surface *tmp = NULL;
   SDL_Palette *pal = NULL;
   const struct font_atlas *atlas = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings->video.font_enable)
      return;

   if (!font_renderer_create_default(&vid->font_driver, &vid->font_data,
                                    *font_path ? font_path : NULL, font_size))
   {
      RARCH_WARN("[SDL]: Could not initialize fonts.\n");
      return;
   }

   r = settings->video.msg_color_r * 255;
   g = settings->video.msg_color_g * 255;
   b = settings->video.msg_color_b * 255;

   r = (r < 0) ? 0 : (r > 255 ? 255 : r);
   g = (g < 0) ? 0 : (g > 255 ? 255 : g);
   b = (b < 0) ? 0 : (b > 255 ? 255 : b);

   vid->font_r = r;
   vid->font_g = g;
   vid->font_b = b;

   atlas = vid->font_driver->get_atlas(vid->font_data);

   tmp = SDL_CreateRGBSurfaceFrom(atlas->buffer, atlas->width,
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
      RARCH_WARN("[SDL]: Failed to initialize font texture: %s\n", SDL_GetError());

   SDL_FreePalette(pal);
   SDL_FreeSurface(tmp);
}

static void sdl2_render_msg(sdl2_video_t *vid, const char *msg)
{
   int x, y, delta_x, delta_y;
   unsigned width  = vid->vp.width;
   unsigned height = vid->vp.height;
   settings_t *settings = config_get_ptr();

   if (!vid->font_data)
      return;

   x       = settings->video.msg_pos_x * width;
   y       = (1.0f - settings->video.msg_pos_y) * height;
   delta_x = 0;
   delta_y = 0;

   SDL_SetTextureColorMod(vid->font.tex, vid->font_r, vid->font_g, vid->font_b);

   for (; *msg; msg++)
   {
      SDL_Rect src_rect, dst_rect;
      int off_x, off_y, tex_x, tex_y;
      const struct font_glyph *gly = 
         vid->font_driver->get_glyph(vid->font_data, (uint8_t)*msg);

      if (!gly)
         gly = vid->font_driver->get_glyph(vid->font_data, '?');

      if (!gly)
         continue;

      off_x      = gly->draw_offset_x;
      off_y      = gly->draw_offset_y;
      tex_x      = gly->atlas_offset_x;
      tex_y      = gly->atlas_offset_y;

      src_rect.x = tex_x;
      src_rect.y = tex_y;
      src_rect.w = (int)gly->width;
      src_rect.h = (int)gly->height;

      dst_rect.x = x + delta_x + off_x;
      dst_rect.y = y + delta_y + off_y;
      dst_rect.w = (int)gly->width;
      dst_rect.h = (int)gly->height;

      SDL_RenderCopyEx(vid->renderer, vid->font.tex,
            &src_rect, &dst_rect, 0, NULL, SDL_FLIP_NONE);

      delta_x += gly->advance_x;
      delta_y -= gly->advance_y;
   }
}

static void sdl2_gfx_set_handles(sdl2_video_t *vid)
{
   /* SysWMinfo headers are broken on OSX. */
#if defined(_WIN32) || defined(HAVE_X11)
   driver_t *driver = driver_get_ptr();

   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (SDL_GetWindowWMInfo(vid->window, &info) != 1)
      return;

#if defined(_WIN32)
   driver->display_type  = RARCH_DISPLAY_WIN32;
   driver->video_display = 0;
   driver->video_window  = (uintptr_t)info.info.win.window;
#elif defined(HAVE_X11)
   driver->display_type  = RARCH_DISPLAY_X11;
   driver->video_display = (uintptr_t)info.info.x11.display;
   driver->video_window  = (uintptr_t)info.info.x11.window;
#endif
#endif
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
      RARCH_ERR("[SDL]: Failed to initialize renderer: %s", SDL_GetError());
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
   settings_t *settings = config_get_ptr();

   SDL_GetWindowSize(vid->window, &win_w, &win_h);

   vid->vp.x = 0;
   vid->vp.y = 0;
   vid->vp.width  = win_w;
   vid->vp.height = win_h;
   vid->vp.full_width  = win_w;
   vid->vp.full_height = win_h;

   if (settings->video.scale_integer)
      video_viewport_get_scaled_integer(&vid->vp,
            win_w, win_h, video_driver_get_aspect_ratio(),
            vid->video.force_aspect);
   else if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      const struct video_viewport *custom = 
         (const struct video_viewport*)video_viewport_get_custom();

      if (custom)
      {
         vid->vp.x = custom->x;
         vid->vp.y = custom->y;
         vid->vp.width  = custom->width;
         vid->vp.height = custom->height;
      }
   }
   else if (vid->video.force_aspect)
   {
      float delta;
      float device_aspect  = (float)win_w / win_h;
      float desired_aspect = video_driver_get_aspect_ratio();

      if (fabsf(device_aspect - desired_aspect) < 0.0001f)
      {
         /* If the aspect ratios of screen and desired aspect ratio are
             * sufficiently equal (floating point stuff), assume they are
             * actually equal. */
      }
      else if (device_aspect > desired_aspect)
      {
         delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
         vid->vp.x     = (int)roundf(win_w * (0.5f - delta));
         vid->vp.width = (unsigned)roundf(2.0f * win_w * delta);
      }
      else
      {
         delta  = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
         vid->vp.y      = (int)roundf(win_h * (0.5f - delta));
         vid->vp.height = (unsigned)roundf(2.0f * win_h * delta);
      }
   }

   vid->should_resize = false;

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
      static struct retro_perf_counter sdl_create_texture = {0};

      sdl_tex_zero(target);

      rarch_perf_init(&sdl_create_texture, "sdl_create_texture");
      retro_perf_start(&sdl_create_texture);

      if (menu)
         format = rgb32 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGBA4444;
      else /* this assumes the frontend will convert 0RGB1555 to RGB565 */
         format = rgb32 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGB565;

      SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY,
                              (vid->video.smooth || menu) ? "linear" : "nearest",
                              SDL_HINT_OVERRIDE);

      target->tex = SDL_CreateTexture(vid->renderer, format,
                                      SDL_TEXTUREACCESS_STREAMING, width, height);

      retro_perf_stop(&sdl_create_texture);

      if (!target->tex)
      {
         RARCH_ERR("Failed to create %s texture: %s\n", menu ? "menu" : "main",
                   SDL_GetError());
         return;
      }

      if (menu)
         SDL_SetTextureBlendMode(target->tex, SDL_BLENDMODE_BLEND);

      target->w = width;
      target->h = height;
      target->pitch = pitch;
      target->rgb32 = rgb32;
      target->active = true;
   }
}

static void *sdl2_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   int i;
   unsigned flags;
   settings_t *settings = config_get_ptr();
   sdl2_video_t *vid;

#ifdef HAVE_X11
   XInitThreads();
#endif

   if (SDL_WasInit(0) == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         return NULL;
   }
   else if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
      return NULL;

   vid = (sdl2_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

   RARCH_LOG("[SDL]: Available renderers (change with $SDL_RENDER_DRIVER):\n");
   for (i = 0; i < SDL_GetNumRenderDrivers(); ++i)
   {
      SDL_RendererInfo renderer;
      if (SDL_GetRenderDriverInfo(i, &renderer) == 0)
         RARCH_LOG("\t%s\n", renderer.name);
   }

   RARCH_LOG("[SDL]: Available displays:\n");
   for(i = 0; i < SDL_GetNumVideoDisplays(); ++i)
   {
      SDL_DisplayMode mode;

      if (SDL_GetCurrentDisplayMode(i, &mode) < 0)
         RARCH_LOG("\tDisplay #%i mode: unknown.\n", i);
      else
         RARCH_LOG("\tDisplay #%i mode: %ix%i@%ihz.\n", i, mode.w, mode.h,
                   mode.refresh_rate);
   }

   if (!video->fullscreen)
      RARCH_LOG("[SDL]: Creating window @ %ux%u\n", video->width, video->height);


   if (video->fullscreen)
      flags = settings->video.windowed_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
   else
      flags = SDL_WINDOW_RESIZABLE;

   vid->window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  video->width, video->height, flags);

   if (!vid->window)
   {
      RARCH_ERR("[SDL]: Failed to init SDL window: %s\n", SDL_GetError());
      goto error;
   }

   vid->video         = *video;
   vid->video.smooth  = settings->video.smooth;
   vid->should_resize = true;

   sdl_tex_zero(&vid->frame);
   sdl_tex_zero(&vid->menu);

   if (video->fullscreen)
      SDL_ShowCursor(SDL_DISABLE);

   sdl2_init_renderer(vid);
   sdl2_init_font(vid, settings->video.font_path, settings->video.font_size);

   sdl2_gfx_set_handles(vid);

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
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_WINDOWEVENT) > 0)
   {
      switch (event.type)
      {
         case SDL_QUIT:
            vid->quitting = true;
            break;

         case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
               vid->should_resize = true;
            break;

         default:
            break;
      }
   }
}

static bool sdl2_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   char buf[128]     = {0};
   sdl2_video_t *vid = (sdl2_video_t*)data;

   if (vid->should_resize)
      sdl_refresh_viewport(vid);

   if (frame)
   {
      static struct retro_perf_counter sdl_copy_frame = {0};

      sdl_refresh_input_size(vid, false, vid->video.rgb32, width, height, pitch);

      rarch_perf_init(&sdl_copy_frame, "sdl_copy_frame");
      retro_perf_start(&sdl_copy_frame);

      SDL_UpdateTexture(vid->frame.tex, NULL, frame, pitch);

      retro_perf_stop(&sdl_copy_frame);
   }

   SDL_RenderCopyEx(vid->renderer, vid->frame.tex, NULL, NULL, vid->rotation, NULL, SDL_FLIP_NONE);

#ifdef HAVE_MENU
   if (menu_driver_alive())
      menu_driver_frame();
#endif

   if (vid->menu.active)
      SDL_RenderCopy(vid->renderer, vid->menu.tex, NULL, NULL);

   if (msg)
      sdl2_render_msg(vid, msg);

   SDL_RenderPresent(vid->renderer);

   if (video_monitor_get_fps(buf, sizeof(buf), NULL, 0))
      SDL_SetWindowTitle(vid->window, buf);

   return true;
}

static void sdl2_gfx_set_nonblock_state(void *data, bool toggle)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;

   vid->video.vsync = !toggle;
   sdl_refresh_renderer(vid);
}

static bool sdl2_gfx_alive(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   check_window(vid);
   return !vid->quitting;
}

static bool sdl2_gfx_focus(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   unsigned flags = (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
   return (SDL_GetWindowFlags(vid->window) & flags) == flags;
}

static bool sdl2_gfx_suppress_screensaver(void *data, bool enable)
{
   driver_t *driver = driver_get_ptr();

   (void)data;
   (void)enable;

   if (driver->display_type == RARCH_DISPLAY_X11)
   {
#ifdef HAVE_X11
      x11_suspend_screensaver(driver->video_window);
#endif
      return true;
   }

   return false;
}

static bool sdl2_gfx_has_windowed(void *data)
{
   (void)data;

   /* TODO - implement */

   return true;
}

static void sdl2_gfx_free(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   if (!vid)
      return;

   if (vid->renderer)
      SDL_DestroyRenderer(vid->renderer);

   if (vid->window)
      SDL_DestroyWindow(vid->window);

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

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

static bool sdl2_gfx_read_viewport(void *data, uint8_t *buffer)
{
   SDL_Surface *surf = NULL, *bgr24 = NULL;
   sdl2_video_t *vid = (sdl2_video_t*)data;
   static struct retro_perf_counter sdl2_gfx_read_viewport = {0};

   rarch_perf_init(&sdl2_gfx_read_viewport, "sdl2_gfx_read_viewport");
   retro_perf_start(&sdl2_gfx_read_viewport);

   video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);

   surf  = SDL_GetWindowSurface(vid->window);
   bgr24 = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_BGR24, 0);

   if (!bgr24)
   {
      RARCH_WARN("Failed to convert viewport data to BGR24: %s", SDL_GetError());
      return false;
   }

   memcpy(buffer, bgr24->pixels, bgr24->h * bgr24->pitch);

   retro_perf_stop(&sdl2_gfx_read_viewport);

   return true;
}

static void sdl2_poke_set_filtering(void *data, unsigned index, bool smooth)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   vid->video.smooth = smooth;

   sdl_tex_zero(&vid->frame);
}

static void sdl2_poke_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   sdl2_video_t *vid    = (sdl2_video_t*)data;

   switch (aspectratio_index)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIEWPORT_SQUARE_PIXEL, NULL);
         break;

      case ASPECT_RATIO_CORE:
         video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIEWPORT_CORE, NULL);
         break;

      case ASPECT_RATIO_CONFIG:
         video_viewport_set_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(
         aspectratio_lut[aspectratio_index].value);

   vid->video.force_aspect = true;
   vid->should_resize = true;
}

static void sdl2_poke_apply_state_changes(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   vid->should_resize = true;
}

#ifdef HAVE_MENU
static void sdl2_poke_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;

   if (frame)
   {
      static struct retro_perf_counter copy_texture_frame = {0};

      sdl_refresh_input_size(vid, true, rgb32, width, height,
                             width * (rgb32 ? 4 : 2));

      rarch_perf_init(&copy_texture_frame, "copy_texture_frame");
      retro_perf_start(&copy_texture_frame);

      SDL_UpdateTexture(vid->menu.tex, NULL, frame, vid->menu.pitch);

      retro_perf_stop(&copy_texture_frame);
   }
}

static void sdl2_poke_texture_enable(void *data, bool enable, bool full_screen)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;

   vid->menu.active = enable;
}

static void sdl2_poke_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   sdl2_render_msg(vid, msg);
   RARCH_LOG("[SDL]: OSD MSG: %s\n", msg);
}

static void sdl2_show_mouse(void *data, bool state)
{
   (void)data;
   SDL_ShowCursor(state);
}

static void sdl2_grab_mouse_toggle(void *data)
{
   sdl2_video_t *vid = (sdl2_video_t*)data;
   SDL_SetWindowGrab(vid->window, SDL_GetWindowGrab(vid->window));
}
#endif

static video_poke_interface_t sdl2_video_poke_interface = {
   NULL,
   sdl2_poke_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   sdl2_poke_set_aspect_ratio,
   sdl2_poke_apply_state_changes,
#ifdef HAVE_MENU
   sdl2_poke_set_texture_frame,
   sdl2_poke_texture_enable,
   sdl2_poke_set_osd_msg,
   sdl2_show_mouse,
   sdl2_grab_mouse_toggle,
#else
   NULL,
   NULL,
   NULL,
   NULL,
   NULL<
#endif
   NULL,
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

video_driver_t video_sdl2 = {
   sdl2_gfx_init,
   sdl2_gfx_frame,
   sdl2_gfx_set_nonblock_state,
   sdl2_gfx_alive,
   sdl2_gfx_focus,
   sdl2_gfx_suppress_screensaver,
   sdl2_gfx_has_windowed,
   sdl2_gfx_set_shader,
   sdl2_gfx_free,
   "sdl2",

   NULL,
   sdl2_gfx_set_rotation,
   sdl2_gfx_viewport_info,
   sdl2_gfx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
    NULL,
#endif
    sdl2_gfx_poke_interface
};

