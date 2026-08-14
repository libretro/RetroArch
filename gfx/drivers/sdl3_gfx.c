/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C)      2026 - Rob Loach
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
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef _WIN32
#include "../common/win32_common.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include <formats/image.h>

#include <SDL3/SDL.h>
#include "../common/sdl3_common.h"

#include "../font_driver.h"
#include "../gfx_display.h"
#include "../video_thread_wrapper.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

static void sdl3_gfx_free(void *data);

static INLINE void sdl3_tex_zero(sdl3_tex_t *t)
{
   if (t->tex)
      SDL_DestroyTexture(t->tex);

   t->tex = NULL;
   t->w = 0;
   t->h = 0;
}

static bool sdl3_init_renderer(sdl3_video_t *vid)
{
   int vsync;

   /* Let SDL pick the graphics backend.
    *
    * It's possible to force a backend by running:
    *     SDL_RENDER_DRIVER=gpu ./retroarch
    */
   if (!(vid->renderer = SDL_CreateRenderer(vid->window, NULL)))
   {
      RARCH_ERR("[SDL3] Failed to initialize renderer: %s.\n", SDL_GetError());
      return false;
   }

   RARCH_LOG("[SDL3] Renderer: %s.\n", SDL_GetRendererName(vid->renderer));

   /* Try setting vsync in order to determine whether or not it's supported. */
   if (SDL_SetRenderVSync(vid->renderer, SDL_RENDERER_VSYNC_ADAPTIVE))
      vid->flags |= SDL3_FLAG_ADAPTIVE_VSYNC;

   /* Get the desired vsync setting from the config. */
   vsync = vid->video.vsync ?
         ((vid->video.swap_interval > 0) ? vid->video.swap_interval : 1) :
         SDL_RENDERER_VSYNC_DISABLED;

   if (!SDL_SetRenderVSync(vid->renderer, vsync) && vsync != SDL_RENDERER_VSYNC_DISABLED)
      SDL_SetRenderVSync(vid->renderer, 1);

   SDL_SetRenderDrawColor(vid->renderer, 0, 0, 0, 255);

   return true;
}

static void sdl3_refresh_renderer(sdl3_video_t *vid)
{
   SDL_Rect r;

   /* We avoid clearing the screen here, since that's owned by
    * sdl3_gfx_frame(). Instead, we just update the viewport. */
   r.x = vid->vp.x;
   r.y = vid->vp.y;
   r.w = (int)vid->vp.width;
   r.h = (int)vid->vp.height;

   SDL_SetRenderViewport(vid->renderer, &r);
}

static void sdl3_refresh_viewport(sdl3_video_t *vid)
{
   int win_w, win_h;

   /* Render coordinates are in output pixels (no logical presentation
    * is set), which on HiDPI displays differ from the point-based
    * SDL_GetWindowSize - size the viewport in pixels to match. */
   SDL_GetWindowSizeInPixels(vid->window, &win_w, &win_h);

   vid->vp.full_width  = win_w;
   vid->vp.full_height = win_h;
   video_driver_update_viewport(&vid->vp, false, vid->video.force_aspect, true);

   /* Tell the rest of the engine about our actual window dimensions
    * (mirrors sdl2_gfx / vga / gx2). */
   video_driver_set_output_size(win_w, win_h);

   vid->flags &= ~SDL3_FLAG_SHOULD_RESIZE;

   sdl3_refresh_renderer(vid);
}

static void sdl3_refresh_input_size(sdl3_video_t *vid, bool menu, bool rgb32,
      unsigned width, unsigned height)
{
   sdl3_tex_t *target = menu ? &vid->menu : &vid->frame;

   /* Pitch is deliberately not part of the recreate check: the texture
    * doesn't depend on source pitch (uploads pass it per call), so a
    * core varying pitch at the same dimensions won't churn textures. */
   if (!target->tex ||
         target->w != width ||
         target->h != height ||
         target->rgb32 != rgb32)
   {
      SDL_PixelFormat format;

      sdl3_tex_zero(target);

      if (menu)
         format = rgb32 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGBA4444;
      else /* assumes the frontend converts 0RGB1555 to RGB565 */
         format = rgb32 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGB565;

      target->tex = SDL_CreateTexture(vid->renderer, format,
            SDL_TEXTUREACCESS_STREAMING, width, height);

      if (!target->tex)
      {
         RARCH_ERR("[SDL3] Failed to create %s texture: %s.\n",
               menu ? "menu" : "main", SDL_GetError());
         return;
      }

      SDL_SetTextureScaleMode(target->tex,
            (menu || !vid->video.smooth)
            ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

      /* Blend when displaying the menu, but disable it otherwise
       * because the alpha byte may be 0 like when rendering
       * XRGB8888 content. */
      SDL_SetTextureBlendMode(target->tex, menu ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);

      target->w     = width;
      target->h     = height;
      target->rgb32 = rgb32;
   }
}

/* Upload pixels into a streaming texture through SDL_LockTexture -
 * the documented fast path. SDL_UpdateTexture on a STREAMING texture
 * routes through an extra internal staging copy per call. */
static void sdl3_stream_upload(sdl3_tex_t *target, const void *src,
      size_t src_pitch)
{
   void *dst = NULL;
   int dst_pitch = 0;
   size_t len;
   unsigned y;
   const uint8_t *in;
   uint8_t *out;

   if (!target->tex || !src)
      return;

   if (!SDL_LockTexture(target->tex, NULL, &dst, &dst_pitch))
      return;

   /* Copy only the bytes a row actually uses; either pitch may be
    * padded past width * bpp. */
   len = (size_t)target->w * (target->rgb32 ? 4 : 2);
   if (len > src_pitch)
      len = src_pitch;
   if (len > (size_t)dst_pitch)
      len = (size_t)dst_pitch;

   in  = (const uint8_t*)src;
   out = (uint8_t*)dst;
   for (y = 0; y < target->h; y++)
   {
      memcpy(out, in, len);
      in  += src_pitch;
      out += dst_pitch;
   }

   SDL_UnlockTexture(target->tex);
}

static void *sdl3_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   int i;
   sdl3_video_t *vid = NULL;

   sdl3_set_app_metadata();

   /* Initialize the video system. */
   if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
      return NULL;

   vid = (sdl3_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
   {
      /* SDL_InitSubSystem above succeeded; undo it so the video
       * subsystem refcount doesn't leak when the frontend falls back
       * to another driver. */
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
      return NULL;
   }

   RARCH_LOG("[SDL3] Available renderers (change with $SDL_RENDER_DRIVER):\n");
   for (i = 0; i < SDL_GetNumRenderDrivers(); ++i)
      RARCH_LOG("[SDL3] \t%s\n", SDL_GetRenderDriver(i));

   if (!video->fullscreen)
      RARCH_LOG("[SDL3] Creating window @ %ux%u.\n", video->width, video->height);

   /* No backend flag: SDL_CreateRenderer picks the render driver. */
   if (!sdl3_window_set_video_mode(&vid->window,
            video->width, video->height, video->fullscreen, 0))
   {
      RARCH_ERR("[SDL3] Failed to init SDL window: %s.\n", SDL_GetError());
      goto error;
   }

   vid->video  = *video;
   vid->flags |= SDL3_FLAG_SHOULD_RESIZE;

   if (!sdl3_init_renderer(vid))
      goto error;

   sdl3_refresh_viewport(vid);

   sdl3_input_driver(config_get_ptr()->arrays.input_joypad_driver, input, input_data);

   return vid;

error:
   sdl3_gfx_free(vid);
   return NULL;
}

static void sdl3_check_window(sdl3_video_t *vid)
{
   bool quit   = false;
   bool resize = false;

   sdl3_pump_window_events(&quit, &resize);

   if (quit)
      vid->flags |= SDL3_FLAG_QUITTING;
   if (resize)
      vid->flags |= SDL3_FLAG_SHOULD_RESIZE;
}

/* Menu, statistics and widget passes compute their coordinates
 * against the full window, not the aspect-corrected game viewport
 * set by sdl3_refresh_renderer - swap viewports around those calls
 * (restore with SDL_SetRenderViewport(renderer, &saved)). */
static void sdl3_viewport_push_full(sdl3_video_t *vid, SDL_Rect *saved)
{
   SDL_Rect full;

   SDL_GetRenderViewport(vid->renderer, saved);

   full.x = 0;
   full.y = 0;
   full.w = (int)vid->vp.full_width;
   full.h = (int)vid->vp.full_height;
   SDL_SetRenderViewport(vid->renderer, &full);
}

/* Draw the retained core frame (vid->frame.tex) into the backbuffer.
 * Skipped while no core frame has arrived yet in menu-only sessions -
 * don't feed SDL a NULL texture every frame. */
static void sdl3_blit_frame(sdl3_video_t *vid)
{
   int deg;

   if (!vid->frame.tex)
      return;

   /* For 90/270 degree rotation the frame must not be clipped by the
    * aspect-corrected game viewport (see sdl2_gfx.c / issue #17893):
    * render into the full window with an explicit dst rect whose
    * width/height are the viewport's swapped, centred on the game
    * area. Even rotations keep the (viewport, NULL dst) path.
    * vid->rotation is normalized to 0/90/180/270 at set time. */
   deg = (int)vid->rotation;
   if (deg == 90 || deg == 270)
   {
      SDL_FRect dst;
      SDL_Rect  game_vp;
      dst.w = (float)vid->vp.height;
      dst.h = (float)vid->vp.width;
      dst.x = (float)(vid->vp.x + ((int)vid->vp.width  - (int)vid->vp.height) / 2);
      dst.y = (float)(vid->vp.y + ((int)vid->vp.height - (int)vid->vp.width)  / 2);
      SDL_SetRenderViewport(vid->renderer, NULL);
      SDL_RenderTextureRotated(vid->renderer, vid->frame.tex, NULL, &dst,
            vid->rotation, NULL, SDL_FLIP_NONE);
      game_vp.x = vid->vp.x;
      game_vp.y = vid->vp.y;
      game_vp.w = (int)vid->vp.width;
      game_vp.h = (int)vid->vp.height;
      SDL_SetRenderViewport(vid->renderer, &game_vp);
   }
   else
      SDL_RenderTextureRotated(vid->renderer, vid->frame.tex, NULL, NULL,
            vid->rotation, NULL, SDL_FLIP_NONE);
}

/* Read the game viewport back as bottom-up BGR24 into buffer, which
 * the screenshot task sized as vp.width * vp.height * 3. Rows are
 * flipped because read_viewport callers expect bottom-up data.
 * Must run before SDL_RenderPresent - afterwards the backbuffer
 * contents are undefined on several backends. */
static bool sdl3_capture_viewport(sdl3_video_t *vid, uint8_t *buffer)
{
   SDL_Surface *surf;
   int w, h, y;
   bool ok = true;

   /* NULL rect = the current render viewport, which is the
    * aspect-corrected game viewport at this point in the frame. */
   surf = SDL_RenderReadPixels(vid->renderer, NULL);
   if (!surf)
   {
      RARCH_WARN("[SDL3] Failed to read viewport: %s.\n", SDL_GetError());
      return false;
   }

   /* Clamp against the viewport dimensions the caller sized its
    * buffer from, so a mismatch can never overrun it. */
   w = (surf->w < (int)vid->vp.width)  ? surf->w : (int)vid->vp.width;
   h = (surf->h < (int)vid->vp.height) ? surf->h : (int)vid->vp.height;

   if (w < (int)vid->vp.width || h < (int)vid->vp.height)
      memset(buffer, 0, (size_t)vid->vp.width * vid->vp.height * 3);

   for (y = 0; y < h; y++)
   {
      if (!SDL_ConvertPixels(w, 1, surf->format,
            (const uint8_t*)surf->pixels + (size_t)y * surf->pitch,
            surf->pitch,
            SDL_PIXELFORMAT_BGR24,
            buffer + (size_t)(h - 1 - y) * vid->vp.width * 3,
            (int)vid->vp.width * 3))
      {
         RARCH_WARN("[SDL3] Failed to convert viewport data to BGR24: %s.\n",
               SDL_GetError());
         ok = false;
         break;
      }
   }

   SDL_DestroySurface(surf);
   return ok;
}

/* Menu, statistics, widgets and OSD text all compute their
 * coordinates against video_info->width/height - the full window
 * dimensions - and every viewport change flushes SDL's render batch.
 * Run all of these passes under a single full-window viewport switch,
 * restoring the game viewport at the end (readback and the next
 * frame's blit rely on it). */
static void sdl3_render_ui(sdl3_video_t *vid, const char *msg,
      video_frame_info_t *video_info)
{
   SDL_Rect saved_vp;
   const char *stat_text          = video_info->stat_text;
   struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;
   bool menu_is_alive             = false;
   bool widgets_active            = false;
   bool menu_visible;
   bool show_stats;

#ifdef HAVE_MENU
   menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) != 0;
   /* With no menu alive menu_driver_frame draws nothing - keep the
    * call outside the viewport switch. */
   if (!menu_is_alive)
      menu_driver_frame(false, video_info);
#endif
#ifdef HAVE_GFX_WIDGETS
   widgets_active = video_info->widgets_active;
#endif

   /* Statistics are suppressed while the menu is alive - the menu
    * drivers own the screen. */
   show_stats = video_info->statistics_show
         && stat_text && stat_text[0] != '\0' && !menu_is_alive;

   menu_visible = vid->menu.active && vid->menu.tex;

   if (!menu_is_alive && !widgets_active && !show_stats && !menu_visible && !(msg && *msg))
      return;

   sdl3_viewport_push_full(vid, &saved_vp);

#ifdef HAVE_MENU
   if (menu_is_alive)
      menu_driver_frame(true, video_info);
#endif

   /* RGUI renders into vid->menu.tex; composite it over the game
    * area explicitly, since the full-window viewport is active. */
   if (menu_visible)
   {
      SDL_FRect menu_dst;
      menu_dst.x = (float)vid->vp.x;
      menu_dst.y = (float)vid->vp.y;
      menu_dst.w = (float)vid->vp.width;
      menu_dst.h = (float)vid->vp.height;
      SDL_RenderTexture(vid->renderer, vid->menu.tex, NULL, &menu_dst);
   }

   /* "Display Statistics" overlay, between the menu composite and
    * widgets to match gdi/d3d8/d3d9 order. */
   if (show_stats)
      font_driver_render_msg(vid, stat_text,
            video_info->stat_text_len, osd_params, NULL);

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   /* Legacy yellow-text OSD messages (when widgets don't handle
    * them). Routed through the raster-font OSD instance - there is
    * no paletted bitmap font path on SDL3. */
   if (msg)
      font_driver_render_msg(vid, msg, strlen(msg), NULL, NULL);

   SDL_SetRenderViewport(vid->renderer, &saved_vp);
}

static bool sdl3_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;

   if (vid->flags & SDL3_FLAG_SHOULD_RESIZE)
      sdl3_refresh_viewport(vid);

   /* Clear every frame, even menu-only ones: after SDL_RenderPresent
    * the back buffer is undefined on several backends. */
   SDL_RenderClear(vid->renderer);

   if (frame)
   {
      sdl3_refresh_input_size(vid, false, vid->video.rgb32, width, height);
      sdl3_stream_upload(&vid->frame, frame, pitch);
   }

   sdl3_blit_frame(vid);

   sdl3_render_ui(vid, msg, video_info);

   SDL_RenderPresent(vid->renderer);

   sdl3_window_update_title(vid->window);

   return true;
}

static void sdl3_gfx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   int vsync;

   vid->video.vsync  = !toggle;

   /* When toggle is true, it means non-blocking (vsync off). Otherwise
    * use the configured swap interval where:
    * 1: every refresh
    * 2: every other */
   if (toggle)
      vsync = SDL_RENDERER_VSYNC_DISABLED;
   else if (adaptive_vsync_enabled)
      vsync = SDL_RENDERER_VSYNC_ADAPTIVE;
   else
      vsync = (swap_interval > 0) ? (int)swap_interval : 1;

   if (vid->renderer)
   {
      /* Adaptive vsync and higher intervals aren't supported by every
       * backend; fall back to a plain 1-frame interval if SDL rejects
       * the requested value. */
      if (!SDL_SetRenderVSync(vid->renderer, vsync) && vsync != SDL_RENDERER_VSYNC_DISABLED)
         SDL_SetRenderVSync(vid->renderer, 1);
   }
}

static bool sdl3_gfx_alive(void *data)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   sdl3_check_window(vid);
   return !(vid->flags & SDL3_FLAG_QUITTING);
}

static bool sdl3_gfx_has_windowed(void *data)
{
   /* kmsdrm backend has no windowing. Everything is an exclusive
    * fullscreen surface. */
   return !string_is_equal(SDL_GetCurrentVideoDriver(), "kmsdrm");
}

static void sdl3_gfx_free(void *data)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;

   /* Make sure the on-screen display font is cleared out. */

   sdl3_tex_zero(&vid->frame);
   sdl3_tex_zero(&vid->menu);

   if (vid->window)
      SDL_StopTextInput(vid->window);

   if (vid->renderer)
      SDL_DestroyRenderer(vid->renderer);

   if (vid->window)
      SDL_DestroyWindow(vid->window);

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   free(vid);
}

static void sdl3_gfx_set_rotation(void *data, unsigned rotation)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (vid)
      vid->rotation = (double)((270 * rotation) % 360);
}

static void sdl3_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   *vp = vid->vp;
}

static bool sdl3_gfx_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;

   /* The backbuffer contents after SDL_RenderPresent are undefined on
    * several backends, so the presented frame can't be read directly.
    * The last core frame is still retained in vid->frame.tex, though:
    * redraw it into the backbuffer and capture that. Nothing here is
    * presented - the next real frame starts with a full clear - so
    * this scratch rendering is never visible. Menu/widget/OSD passes
    * are not replayed; the capture is the raw game viewport. */
   SDL_RenderClear(vid->renderer);
   sdl3_blit_frame(vid);

   return sdl3_capture_viewport(vid, buffer);
}

/* Applies a new window size / fullscreen in place, without tearing
 * down the entire driver. */
static void sdl3_poke_set_video_mode(void *data, unsigned width,
      unsigned height, bool fullscreen)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;

   if (!vid || !vid->window)
      return;

   if (!sdl3_window_set_video_mode(&vid->window, width, height, fullscreen, 0))
   {
      RARCH_WARN("[SDL3] Failed to set video mode: %s.\n", SDL_GetError());
      return;
   }

   /* On the next frame, recompute the viewport pixel size. */
   vid->flags |= SDL3_FLAG_SHOULD_RESIZE;
   vid->video.width = width;
   vid->video.height = height;
   vid->video.fullscreen = fullscreen;
}

static void sdl3_poke_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;
   vid->video.smooth = smooth;
   /* SDL3 can retag the sampler in place - no need to drop and
    * recreate the streaming texture. */
   if (vid->frame.tex)
      SDL_SetTextureScaleMode(vid->frame.tex,
            smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
}

static void sdl3_poke_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;
   vid->video.force_aspect = true;
   vid->flags |= SDL3_FLAG_SHOULD_RESIZE;
}

static void sdl3_poke_apply_state_changes(void *data)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   vid->flags |= SDL3_FLAG_SHOULD_RESIZE;
}

static void sdl3_poke_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid || !frame)
      return;
   sdl3_refresh_input_size(vid, true, rgb32, width, height);
   sdl3_stream_upload(&vid->menu, frame, width * (rgb32 ? 4 : 2));
}

static void sdl3_poke_texture_enable(void *data, bool enable, bool full_screen)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;
   vid->menu.active = enable;
}

static void sdl3_grab_mouse_toggle(void *data)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;
   SDL_SetWindowMouseGrab(vid->window, !SDL_GetWindowMouseGrab(vid->window));
}

static uint32_t sdl3_get_flags(void *data)
{
   uint32_t flags    = 0;
   sdl3_video_t *vid = (sdl3_video_t*)data;

   /* Advertising this makes the Adaptive VSync setting visible in the
    * menu, which is what routes adaptive_vsync_enabled into
    * sdl3_gfx_set_nonblock_state. */
   if (vid && (vid->flags & SDL3_FLAG_ADAPTIVE_VSYNC))
      BIT32_SET(flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC);

   return flags;
}

#ifdef HAVE_GFX_WIDGETS
static bool sdl3_gfx_widgets_enabled(void *data) { return true; }
#endif

/* Texture upload hook for menu icons, the gfx_display white texture
 * (which widgets tint for backgrounds/panels), and any other
 * gfx_display-driven texture loads.
 *
 * Pixel format: this driver never calls video_driver_set_rgba(), so
 * the image task gives us pixels in BGRA byte order packed into
 * uint32, which maps to SDL_PIXELFORMAT_ARGB8888. */
static uintptr_t sdl3_load_texture_internal(sdl3_video_t *vid,
      struct texture_image *ti, enum texture_filter_type filter_type)
{
   SDL_Texture *tex = NULL;

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
      SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
   else
      SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);

   return (uintptr_t)tex;
}

#ifdef HAVE_THREADS
/* Command payload for marshalling texture (un)load onto the video
 * thread - see sdl3_load_texture for why. Only needed in threaded
 * builds; the non-threaded path calls the internal helpers directly. */
typedef struct
{
   sdl3_video_t             *vid;
   void                     *data;
   enum texture_filter_type  filter_type;
} sdl3_texture_cmd_t;

static uintptr_t sdl3_load_texture_wrap(void *data)
{
   sdl3_texture_cmd_t *cmd = (sdl3_texture_cmd_t*)data;
   return sdl3_load_texture_internal(cmd->vid,
         (struct texture_image*)cmd->data, cmd->filter_type);
}

static uintptr_t sdl3_unload_texture_wrap(void *data)
{
   sdl3_texture_cmd_t *cmd = (sdl3_texture_cmd_t*)data;
   SDL_Texture        *tex = (SDL_Texture*)cmd->data;
   if (tex)
      SDL_DestroyTexture(tex);
   return 0;
}
#endif

/* With threaded video the SDL renderer - and, on the OpenGL backend,
 * its GL context - lives on the video thread. SDL_CreateTexture /
 * SDL_UpdateTexture / SDL_DestroyTexture must run on that same thread
 * or (on OpenGL) the texture is created outside the active context and
 * samples as solid black. The menu/widgets call load_texture from the
 * task thread, so marshal the work onto the video thread when threaded
 * (mirrors gl3_load_texture). */
static uintptr_t sdl3_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   sdl3_video_t *vid = (sdl3_video_t*)video_data;

#ifdef HAVE_THREADS
   if (threaded)
   {
      sdl3_texture_cmd_t cmd;
      cmd.vid         = vid;
      cmd.data        = data;
      cmd.filter_type = filter_type;
      return video_thread_texture_handle(&cmd, sdl3_load_texture_wrap);
   }
#endif

   return sdl3_load_texture_internal(vid,
         (struct texture_image*)data, filter_type);
}

static void sdl3_unload_texture(void *data, bool threaded, uintptr_t id)
{
   SDL_Texture *tex = (SDL_Texture*)id;
   if (!tex)
      return;

#ifdef HAVE_THREADS
   if (threaded)
   {
      sdl3_texture_cmd_t cmd;
      cmd.vid         = (sdl3_video_t*)data;
      cmd.data        = (void*)tex;
      cmd.filter_type = TEXTURE_FILTER_LINEAR;
      video_thread_texture_handle(&cmd, sdl3_unload_texture_wrap);
      return;
   }
#endif

   SDL_DestroyTexture(tex);
}

static void sdl3_poke_set_osd_msg(void *data, const char *msg, size_t msg_len,
      const struct font_params *params, void *font)
{
   /* font_driver_render_msg already dispatches to the given font (or
    * to the global OSD font when NULL) and applies text reshaping -
    * no per-driver dispatch needed. Unlike the SDL2 driver there is
    * no legacy paletted bitmap font path here; all text goes through
    * the raster font. */
   font_driver_render_msg(data, msg, msg_len, params, font);
}

/*
 * gfx_display BACKEND
 *
 * Hardware-accelerated menu/widget rendering, built on
 * SDL_RenderGeometry so the menu and widget quads run on the GPU
 * through SDL_Renderer's underlying backend (the SDL_gpu "gpu"
 * renderer, or whichever fallback was selected).
 */

static void gfx_display_sdl3_blend_begin(void *data)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;
   SDL_SetRenderDrawBlendMode(vid->renderer, SDL_BLENDMODE_BLEND);
}

static void gfx_display_sdl3_blend_end(void *data)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;
   SDL_SetRenderDrawBlendMode(vid->renderer, SDL_BLENDMODE_NONE);
}

static void gfx_display_sdl3_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   SDL_Rect rect;
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;

   /* gfx_display passes scissor rects with a top-left origin. The
    * vulkan driver uses y directly and only GL flips (its scissor
    * origin is bottom-left). SDL_SetRenderClipRect is top-left too,
    * so we can pass the values directly to the rectangle. */
   rect.x = x;
   rect.y = y;
   rect.w = (int)width;
   rect.h = (int)height;

   if (rect.x < 0) { rect.w += rect.x; rect.x = 0; }
   if (rect.y < 0) { rect.h += rect.y; rect.y = 0; }
   if (rect.w <= 0 || rect.h <= 0)
      return;

   SDL_SetRenderClipRect(vid->renderer, &rect);
}

static void gfx_display_sdl3_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   sdl3_video_t *vid = (sdl3_video_t*)data;
   if (!vid)
      return;
   SDL_SetRenderClipRect(vid->renderer, NULL);
}

/* All gfx_display draws are triangle strips (see gl1's draw, which
 * does GL_TRIANGLE_STRIP unconditionally). SDL_RenderGeometry only
 * does triangle lists, so we expand the strip into indices on the
 * fly. Most draws are the 4-vertex quad case; we special-case it.
 *
 * Two distinct call paths converge here:
 *
 * 1. gfx_display_draw_quad - used by widgets and most menu chrome.
 *    Sets coords->vertex = NULL and encodes the quad rectangle in
 *    draw->x / draw->y / draw->width / draw->height (pixel coords,
 *    y bottom-up). We synthesize the four corners in pixel space.
 *
 * 2. The general path - menu drivers that build their own vertex
 *    arrays in 0..1 normalized coords (origin bottom-left). We
 *    scale these to video_width / video_height and flip Y to match
 *    SDL's top-left origin. */
/* SDL3's SDL_Vertex carries SDL_FColor (normalized floats), matching
 * gfx_display's own color format - assign directly. */
static INLINE void sdl3_vertex_color(SDL_Vertex *v, const float *col, unsigned i)
{
   if (col)
   {
      v->color.r = col[i * 4 + 0];
      v->color.g = col[i * 4 + 1];
      v->color.b = col[i * 4 + 2];
      v->color.a = col[i * 4 + 3];
   }
   else
      v->color.r = v->color.g = v->color.b = v->color.a = 1.0f;
}

static void gfx_display_sdl3_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
#define SDL3_DRAW_COORD_LIMIT 65536
#define SDL3_DISPLAY_STACK_VERTS 64
   static const int quad_idx[6] = { 0, 1, 2, 2, 1, 3 };
   SDL_Vertex verts_stack[SDL3_DISPLAY_STACK_VERTS];
   int idx_stack[(SDL3_DISPLAY_STACK_VERTS - 2) * 3];
   sdl3_video_t *vid = (sdl3_video_t*)data;
   SDL_Vertex *verts = verts_stack;
   int *indices = idx_stack;
   SDL_Texture *tex = NULL;
   const float *vtx, *tc, *col;
   unsigned n, i, num_idx;

   if (!vid || !draw || !draw->coords)
      return;

   n = draw->coords->vertices;
   if (n < 3)
      return;

   vtx = draw->coords->vertex;
   tc  = draw->coords->tex_coord;
   col = draw->coords->color;

   /* The texture handle is a uintptr_t cast of an SDL_Texture*
    * registered via sdl3_load_texture (poke->load_texture). For
    * gfx_display_draw_quad calls without an explicit texture the
    * caller substitutes gfx_white_texture, so passing this through
    * directly is safe; if SDL_RenderGeometry receives NULL we get
    * flat-shaded geometry, which is a reasonable degraded path. */
   tex = (SDL_Texture*)(uintptr_t)draw->texture;

   /* Path 1: gfx_display_draw_quad - vtx is NULL, geometry comes
    * from draw->x/y/width/height with y bottom-up.  n is always 4.
    *
    * - draw->x / y / width / height: pixel coords, y bottom-up
    *   (gfx_display_draw_quad pre-flips: draw.y = height - y - h).
    *   To put the rect at the right spot in SDL's top-down pixel
    *   space, re-flip:
    *      dst_y = video_height - draw->height - draw->y
    *
    * - coords->tex_coord (when non-NULL): 0..1 normalised, TOP-DOWN
    *   (opposite to the bottom-up vertex convention; documented in
    *   gfx_display.c). Used directly without flip. When NULL we
    *   synthesise (0,0)..(1,1). */
   if (!vtx && n == 4)
   {
      float x0, x1, y0, y1;

      /* Defensive clamp: gfx_widgets_draw_icon's coordinate math
       * can underflow during the first few frames after icon load,
       * producing draw->y == INT_MIN. Float-converting that produces
       * NaN vertex positions and a spurious SDL error that pollutes
       * the renderer state for subsequent draws. */
      if (   draw->x < -SDL3_DRAW_COORD_LIMIT || draw->x > SDL3_DRAW_COORD_LIMIT
          || draw->y < -SDL3_DRAW_COORD_LIMIT || draw->y > SDL3_DRAW_COORD_LIMIT
          || draw->width  > SDL3_DRAW_COORD_LIMIT
          || draw->height > SDL3_DRAW_COORD_LIMIT)
         return;

      x0 = (float)draw->x;
      x1 = (float)draw->x + (float)draw->width;
      /* Re-flip Y from bottom-up to SDL top-down. */
      y0 = (float)video_height - (float)draw->height - (float)draw->y;
      y1 = y0 + (float)draw->height;

      /* Apply draw->scale_factor (centred scaling around the quad's
       * midpoint). XMB sets this on icon draws (node->zoom) to grow
       * the active tab icon; without honouring it every icon renders
       * at base size. */
      if (draw->scale_factor > 0.0f && draw->scale_factor != 1.0f)
      {
         float cx = (x0 + x1) * 0.5f;
         float cy = (y0 + y1) * 0.5f;
         float hw = (x1 - x0) * 0.5f * draw->scale_factor;
         float hh = (y1 - y0) * 0.5f * draw->scale_factor;
         x0 = cx - hw; x1 = cx + hw;
         y0 = cy - hh; y1 = cy + hh;
      }

      {
         float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
         if (tc)
         {
            /* tex_coord is 4 (u,v) pairs. For axis-aligned quads
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
          * triangle-strip layout used by every gfx_display caller).
          * After the Y flip to SDL's top-down screen space, vertices
          * 0/1 are the bottom edge and 2/3 the top edge - this
          * matters for per-vertex colour gradients (e.g. the
          * load-content animation's top shadow). */
         verts[0].position.x = x0; verts[0].position.y = y1;
         verts[0].tex_coord.x = u0; verts[0].tex_coord.y = v1;
         verts[1].position.x = x1; verts[1].position.y = y1;
         verts[1].tex_coord.x = u1; verts[1].tex_coord.y = v1;
         verts[2].position.x = x0; verts[2].position.y = y0;
         verts[2].tex_coord.x = u0; verts[2].tex_coord.y = v0;
         verts[3].position.x = x1; verts[3].position.y = y0;
         verts[3].tex_coord.x = u1; verts[3].tex_coord.y = v0;

         /* Colors are set before the rotation pass below so every
          * vertex field is initialized before any is re-read. */
         for (i = 0; i < 4; i++)
            sdl3_vertex_color(&verts[i], col, i);

         /* Apply 2D rotation around the rect's centre (spinning
          * hourglass on pending-task widgets). Sine is negated
          * because SDL is top-down: a positive radians value should
          * rotate clockwise on screen. */
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

      SDL_RenderGeometry(vid->renderer, tex, verts, 4, quad_idx, 6);
      return;
   }

   /* Path 2: caller-supplied vertex array in 0..1 normalised coords
    * (origin bottom-up). Convert to SDL pixel coords (top-down) by
    * scaling and flipping Y. Texcoords are top-down already - no
    * flip. */
   if (!vtx)
      return;

   /* Large vertex arrays (nothing in the current menu drivers, but
    * nothing enforces that either) go through the heap instead of
    * blowing the stack. */
   if (n > SDL3_DISPLAY_STACK_VERTS)
   {
      verts   = (SDL_Vertex*)malloc(sizeof(SDL_Vertex) * n);
      indices = (int*)malloc(sizeof(int) * (n - 2) * 3);
      if (!verts || !indices)
      {
         free(verts);
         free(indices);
         return;
      }
   }

   for (i = 0; i < n; i++)
   {
      float vx = vtx[i * 2 + 0];
      float vy = vtx[i * 2 + 1];

      verts[i].position.x = vx * (float)video_width;
      verts[i].position.y = (1.0f - vy) * (float)video_height;

      if (tc)
      {
         verts[i].tex_coord.x = tc[i * 2 + 0];
         verts[i].tex_coord.y = tc[i * 2 + 1];
      }
      else
      {
         verts[i].tex_coord.x = 0.0f;
         verts[i].tex_coord.y = 0.0f;
      }

      sdl3_vertex_color(&verts[i], col, i);
   }

   /* Quad fast path - the common case for menu items. */
   if (n == 4)
   {
      SDL_RenderGeometry(vid->renderer, tex, verts, 4, quad_idx, 6);
      return;
   }

   /* General triangle-strip expansion: n verts -> (n-2) triangles. */
   num_idx = (n - 2) * 3;
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

   if (verts != verts_stack)
   {
      free(verts);
      free(indices);
   }
#undef SDL3_DISPLAY_STACK_VERTS
#undef SDL3_DRAW_COORD_LIMIT
}


/*
 * FONT DRIVER
 *
 * Submits glyph quads through SDL_RenderGeometry so menu/widget/OSD
 * text shares the same vertex pipeline as the gfx_display backend.
 */

typedef struct
{
   sdl3_video_t                  *vid;
   SDL_Texture                   *tex;
   const font_renderer_driver_t  *font_driver;
   void                          *font_data;
   struct font_atlas             *atlas;
   uint32_t                      *staging; /* A8 -> RGBA expansion buffer */
   int                            tex_width;
   int                            tex_height;
} sdl3_raster_t;

static void sdl3_raster_font_upload_atlas(sdl3_raster_t *font)
{
   int i, total;
   const uint8_t *src;

   if (!font || !font->atlas)
      return;

   /* The atlas dimensions are fixed after init, so the texture and
    * staging buffer are created once and the atlas is re-uploaded in
    * place whenever the glyph cache grows (atlas->dirty). */
   if (  !font->tex
       || font->tex_width  != (int)font->atlas->width
       || font->tex_height != (int)font->atlas->height)
   {
      if (font->tex)
         SDL_DestroyTexture(font->tex);

      font->tex_width  = (int)font->atlas->width;
      font->tex_height = (int)font->atlas->height;

      font->tex = SDL_CreateTexture(font->vid->renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC,
            font->tex_width, font->tex_height);
      if (!font->tex)
         return;

      SDL_SetTextureBlendMode(font->tex, SDL_BLENDMODE_BLEND);

      free(font->staging);
      font->staging = (uint32_t*)malloc(
            font->tex_width * font->tex_height * sizeof(uint32_t));
      if (!font->staging)
      {
         SDL_DestroyTexture(font->tex);
         font->tex = NULL;
         return;
      }
   }

   /* Atlas buffer is 8-bit alpha. Expand to white-RGB plus the alpha
    * value so vertex color modulation produces correctly-tinted
    * glyphs. SDL_PIXELFORMAT_RGBA32 is the endian-neutral alias for
    * byte order R,G,B,A, so fill the staging buffer byte-wise. */
   total = font->tex_width * font->tex_height;
   src   = font->atlas->buffer;
   {
      uint8_t *dst = (uint8_t*)font->staging;
      for (i = 0; i < total; i++)
      {
         *dst++ = 0xFF;
         *dst++ = 0xFF;
         *dst++ = 0xFF;
         *dst++ = src[i];
      }
   }

   SDL_UpdateTexture(font->tex, NULL, font->staging,
         font->tex_width * sizeof(uint32_t));

   font->atlas->dirty = false;
}

static void *sdl3_raster_font_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   sdl3_raster_t *font;
   sdl3_video_t  *vid = (sdl3_video_t*)data;

   if (!vid || !vid->renderer)
   {
      RARCH_WARN("[SDL3] sdl3_raster_font_init: no video data or renderer "
            "(vid=%p, renderer=%p) - widget/menu fonts will be unavailable.\n",
            (void*)vid, vid ? (void*)vid->renderer : NULL);
      return NULL;
   }

   font = (sdl3_raster_t*)calloc(1, sizeof(*font));
   if (!font)
      return NULL;

   font->vid = vid;

   if (!font_renderer_create_default(
            &font->font_driver, &font->font_data,
            font_path, font_size, FONT_ATLAS_FORMAT_A8))
   {
      RARCH_WARN("[SDL3] sdl3_raster_font_init: font_renderer_create_default "
            "failed for path \"%s\" size %.1f.\n",
            font_path ? font_path : "(default)", font_size);
      free(font);
      return NULL;
   }

   font->atlas = font->font_driver->get_atlas(font->font_data);
   sdl3_raster_font_upload_atlas(font);

   if (!font->tex)
   {
      RARCH_WARN("[SDL3] sdl3_raster_font_init: atlas upload failed: %s.\n",
            SDL_GetError());
      font->font_driver->free(font->font_data);
      free(font);
      return NULL;
   }

   return font;
}

static void sdl3_raster_font_free(void *data, bool is_threaded)
{
   sdl3_raster_t *font = (sdl3_raster_t*)data;
   if (!font)
      return;
   if (font->tex)
      SDL_DestroyTexture(font->tex);
   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);
   free(font->staging);
   free(font);
}

static int sdl3_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   sdl3_raster_t *font = (sdl3_raster_t*)data;
   const char *msg_end = msg + msg_len;
   int width = 0;

   if (!font || !msg)
      return 0;

   while (msg < msg_end)
   {
      uint32_t code = utf8_walk(&msg);
      const struct font_glyph *glyph = font->font_driver->get_glyph(font->font_data, code);
      if (!glyph)
         glyph = font->font_driver->get_glyph(font->font_data, '?');
      if (glyph)
         width += glyph->advance_x;
   }

   return (int)((float)width * scale);
}

/* Render a single line into one SDL_RenderGeometry batch. Up to
 * MAX_GLYPHS per submitted batch; we flush mid-line for longer runs.
 *
 * Coordinates: render_msg gives us params->x/y in 0..1 normalized
 * space. We convert to pixel coords against the full window, with
 * a top-left origin (SDL convention). */
static void sdl3_raster_font_render_line(
      sdl3_raster_t *font,
      const char *msg, size_t msg_len,
      float scale,
      const SDL_FColor col,
      float pos_x, float pos_y,
      enum text_alignment align,
      unsigned width, unsigned height)
{
/* Kept small deliberately: SDL_Vertex is 32 bytes, so the vertex array
 * alone is MAX_GLYPHS * 128 bytes of stack. The loop below flushes and
 * reuses the buffer whenever it fills, so a bigger batch buys very
 * little - 64 glyphs holds most real strings in one draw call while
 * keeping this frame under 10 KB. */
#define SDL3_FONT_MAX_GLYPHS 64
   SDL_Vertex  verts[SDL3_FONT_MAX_GLYPHS * 4];
   int         idx[SDL3_FONT_MAX_GLYPHS * 6];
   int         n_glyphs = 0;
   const char *msg_end  = msg + msg_len;
   float       x;
   float       y;
   float       inv_w;
   float       inv_h;

   if (!font || !font->tex)
      return;

   if (font->atlas->dirty)
      sdl3_raster_font_upload_atlas(font);

   /* gfx_display_draw_text gives us params->x/y in normalized 0..1
    * coords (origin bottom-left to match GL). Convert to pixels
    * with a top-left origin. */
   x = pos_x * (float)width;
   y = (1.0f - pos_y) * (float)height;

   if (align == TEXT_ALIGN_RIGHT)
      x -= sdl3_raster_font_get_message_width(font, msg, msg_len, scale);
   else if (align == TEXT_ALIGN_CENTER)
      x -= sdl3_raster_font_get_message_width(font, msg, msg_len, scale)
         * 0.5f;

   inv_w = 1.0f / (float)font->tex_width;
   inv_h = 1.0f / (float)font->tex_height;

   while (msg < msg_end)
   {
      uint32_t code = utf8_walk(&msg);
      const struct font_glyph *glyph =
         font->font_driver->get_glyph(font->font_data, code);
      float gx, gy, gw, gh;
      float u0, v0, u1, v1;
      int   base;

      if (!glyph)
         glyph = font->font_driver->get_glyph(font->font_data, '?');
      if (!glyph)
         continue;

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

      if (n_glyphs >= SDL3_FONT_MAX_GLYPHS)
      {
         SDL_RenderGeometry(font->vid->renderer, font->tex,
               verts, n_glyphs * 4, idx, n_glyphs * 6);
         n_glyphs = 0;
      }
   }

   if (n_glyphs > 0)
      SDL_RenderGeometry(font->vid->renderer, font->tex,
            verts, n_glyphs * 4, idx, n_glyphs * 6);
#undef SDL3_FONT_MAX_GLYPHS
}

/* Walk a (possibly multi-line) string and call render_line once per
 * line segment, dropping each subsequent line by one line-height in
 * GL-convention (params->y increases upward, so we subtract).
 *
 * Required because callers like XMB sublabels embed real '\n' bytes
 * into their wrapped text - gfx_display_draw_text doesn't pre-split
 * for us. Mirrors gl1's gl1_raster_font_render_message wrapper. */
static void sdl3_raster_font_render_message(
      sdl3_raster_t *font, const char *msg, float scale,
      const SDL_FColor col, float pos_x, float pos_y,
      enum text_alignment align, unsigned width, unsigned height)
{
   struct font_line_metrics *line_metrics = NULL;
   float line_height_norm = 0.0f;
   int lines = 0;

   if (font->font_driver && font->font_driver->get_line_metrics)
   {
      font->font_driver->get_line_metrics(font->font_data, &line_metrics);
      if (line_metrics && height > 0)
         line_height_norm = (float)line_metrics->height * scale / (float)height;
   }

   for (;;)
   {
      const char *p = msg;
      size_t len;

      while (*p && *p != '\n')
         p++;
      len = (size_t)(p - msg);

      if (len > 0)
         sdl3_raster_font_render_line(font, msg, len, scale, col,
               pos_x,
               pos_y - (float)lines * line_height_norm,
               align, width, height);

      if (!*p)
         break;
      msg = p + 1;
      lines++;
   }
}

static void sdl3_raster_font_render_msg(
      void *userdata,
      void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params)
{
   sdl3_raster_t *font = (sdl3_raster_t*)data;
   sdl3_video_t *vid = (sdl3_video_t*)userdata;
   SDL_FColor col, col_drop;
   float x, y, scale;
   int drop_x, drop_y;
   float drop_mod, drop_alpha;
   enum text_alignment align = TEXT_ALIGN_LEFT;
   unsigned width, height;

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
      x = params->x;
      y = params->y;
      scale = params->scale;
      align = params->text_align;
      drop_x = params->drop_x;
      drop_y = params->drop_y;
      drop_mod = params->drop_mod;
      drop_alpha = params->drop_alpha;

      /* SDL_Vertex takes normalized float colors. */
      col.r = (float)FONT_COLOR_GET_RED(params->color)   / 255.0f;
      col.g = (float)FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      col.b = (float)FONT_COLOR_GET_BLUE(params->color)  / 255.0f;
      col.a = (float)FONT_COLOR_GET_ALPHA(params->color) / 255.0f;
      if (col.a <= 0.0f)
         col.a = 1.0f;
   }
   else
   {
      /* NULL params = legacy OSD message path; honor the user's
       * message position/color settings (mirrors gl1). */
      settings_t *settings = config_get_ptr();
      x = settings->floats.video_msg_pos_x;
      y = settings->floats.video_msg_pos_y;
      scale = 1.0f;
      drop_x = -2;
      drop_y = -2;
      drop_mod = 0.3f;
      drop_alpha = 1.0f;
      col.r = settings->floats.video_msg_color_r;
      col.g = settings->floats.video_msg_color_g;
      col.b = settings->floats.video_msg_color_b;
      col.a = 1.0f;
   }

   if (drop_x || drop_y)
   {
      col_drop.r = col.r * drop_mod;
      col_drop.g = col.g * drop_mod;
      col_drop.b = col.b * drop_mod;
      col_drop.a = col.a * drop_alpha;

      sdl3_raster_font_render_message(font, msg, scale, col_drop,
            x + scale * drop_x / (float)width,
            y + scale * drop_y / (float)height,
            align, width, height);
   }

   sdl3_raster_font_render_message(font, msg, scale, col,
         x, y, align, width, height);
}

static const struct font_glyph *sdl3_raster_font_get_glyph(
      void *data, uint32_t code)
{
   sdl3_raster_t *font = (sdl3_raster_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph(font->font_data, code);
   return NULL;
}

static bool sdl3_raster_font_get_line_metrics(void *data,
      struct font_line_metrics **metrics)
{
   sdl3_raster_t *font = (sdl3_raster_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t sdl3_raster_font = {
   sdl3_raster_font_init,
   sdl3_raster_font_free,
   sdl3_raster_font_render_msg,
   "sdl3",
   sdl3_raster_font_get_glyph,
   NULL, /* bind_block  - no batched render-block support yet  */
   NULL, /* flush_block - widgets/menu work without it         */
   sdl3_raster_font_get_message_width,
   sdl3_raster_font_get_line_metrics
};

static video_poke_interface_t sdl3_video_poke_interface = {
   sdl3_get_flags,
   sdl3_load_texture,
   sdl3_unload_texture,
   sdl3_poke_set_video_mode,
   sdl3_ctx_get_refresh_rate,
   sdl3_poke_set_filtering,
   NULL,                            /* get_video_output_size */
   NULL,                            /* get_video_output_prev */
   NULL,                            /* get_video_output_next */
   NULL,                            /* get_current_framebuffer */
   NULL,                            /* get_proc_address */
   sdl3_poke_set_aspect_ratio,
   sdl3_poke_apply_state_changes,
   sdl3_poke_set_texture_frame,
   sdl3_poke_texture_enable,
   sdl3_poke_set_osd_msg,
   sdl3_show_mouse,
   sdl3_grab_mouse_toggle,
   NULL,                            /* get_current_shader */
   NULL,                            /* get_current_software_framebuffer */
   NULL,                            /* get_hw_render_interface */
   NULL,                            /* set_hdr_menu_nits */
   NULL,                            /* set_hdr_paper_white_nits */
   NULL,                            /* set_hdr_expand_gamut */
   NULL,                            /* set_hdr_scanlines */
   NULL                             /* set_hdr_subpixel_layout */
};

static void sdl3_gfx_poke_interface(void *data, const video_poke_interface_t **iface)
{
   *iface = &sdl3_video_poke_interface;
}

video_driver_t video_sdl3 = {
   sdl3_gfx_init,
   sdl3_gfx_frame,
   sdl3_gfx_set_nonblock_state,
   sdl3_gfx_alive,
   sdl3_ctx_has_focus,
   sdl3_suppress_screensaver,
   sdl3_gfx_has_windowed,
   NULL, /* set_shader - all call sites treat this as optional */
   sdl3_gfx_free,
   "sdl3",
   NULL,                        /* set_viewport */
   sdl3_gfx_set_rotation,
   sdl3_gfx_viewport_info,
   sdl3_gfx_read_viewport,
   NULL,                        /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,                        /* overlay_interface */
#endif
   sdl3_gfx_poke_interface,
   NULL,                        /* wrap_type_to_enum */
   NULL,                        /* shader_load_begin */
   NULL,                        /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   sdl3_gfx_widgets_enabled,
#endif
   NULL, /* invalidate_hw_render_cache */
   NULL, /* read_viewport_hdr */
   &sdl3_raster_font
};

gfx_display_ctx_driver_t gfx_display_ctx_sdl3 = {
   gfx_display_sdl3_draw,
   /* Pipeline draws (XMB ribbon, snow, bokeh) need a programmable
    * pipeline, which SDL_Renderer doesn't expose - the menu renders
    * without the animated background. */
   NULL, /* draw_pipeline */
   gfx_display_sdl3_blend_begin,
   gfx_display_sdl3_blend_end,
   NULL, /* get_default_mvp - SDL_Renderer has no MVP concept */
   NULL, /* get_default_vertices */
   NULL, /* get_default_tex_coords */
   &sdl3_raster_font,
   GFX_VIDEO_DRIVER_SDL3,
   "sdl3",
   false,
   gfx_display_sdl3_scissor_begin,
   gfx_display_sdl3_scissor_end
};
