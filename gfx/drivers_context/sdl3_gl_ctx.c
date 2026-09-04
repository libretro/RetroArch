/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* SDL3 OpenGL context driver. While the SDL3 video driver itself is
 * software rendered via SDL_Renderer, this context driver allows
 * the gl/gl1/glcore video drivers to render within the SDL3 window. */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <string/stdstring.h>

#include "../../gfx/video_defines.h"
#include "../../gfx/video_driver.h"
#include "../../verbosity.h"

#include <SDL3/SDL.h>

#include "../common/sdl3_common.h"

typedef struct gfx_ctx_sdl3_data
{
   SDL_Window *win; /* Must be first because it's shared across
                     * sdl3_ctx_* callbacks. */
   SDL_GLContext ctx;
   SDL_GLContext shared_ctx;
   bool core_hw_context_enable;
   bool adaptive_vsync;
} gfx_ctx_sdl3_data_t;

/* bind_api runs before init (see video_context_driver_init), so there
 * is no driver instance to store these in yet. */
static enum gfx_ctx_api sdl3_gl_api = GFX_CTX_OPENGL_API;
static unsigned sdl3_gl_major = 0;
static unsigned sdl3_gl_minor = 0;

/* Cached GL contexts. Allow using the GL context across video
 * rebuilds. */
static SDL_GLContext sdl3_gl_cached_ctx = NULL;
static SDL_GLContext sdl3_gl_cached_shared_ctx = NULL;

/* The make_current hook receives no pointer, so track it at
 * the file scope (same as x_ctx). */
static gfx_ctx_sdl3_data_t *sdl3_gl_current = NULL;

/* Core-profile context: explicitly requested, or GL 3.1+. */
static bool sdl3_gl_core_profile(gfx_ctx_sdl3_data_t *sdl)
{
   if (sdl && sdl->core_hw_context_enable)
      return true;
   return sdl3_gl_major > 3 || (sdl3_gl_major == 3 && sdl3_gl_minor >= 1);
}

static void sdl3_ctx_destroy(void *data)
{
   gfx_ctx_sdl3_data_t *sdl = (gfx_ctx_sdl3_data_t*)data;
   video_driver_state_t *video_st = video_state_get_ptr();

   if (!sdl)
      return;

   if (sdl->ctx && (video_st->flags & VIDEO_FLAG_CACHE_CONTEXT))
   {
      /* hw_render.cache_context reinit: keep the context alive for
       * the next set_video_mode instead of destroying it. */
      SDL_GL_MakeCurrent(sdl->win, NULL);
      sdl3_gl_cached_ctx = sdl->ctx;
      sdl3_gl_cached_shared_ctx = sdl->shared_ctx;
   }
   else
   {
      if (sdl->ctx)
         SDL_GL_DestroyContext(sdl->ctx);

      if (sdl->shared_ctx)
         SDL_GL_DestroyContext(sdl->shared_ctx);
   }

   if (sdl->win)
   {
      SDL_StopTextInput(sdl->win);
      SDL_DestroyWindow(sdl->win);
   }

   /* SDL3 refcounts subsystems; this balances the SDL_InitSubSystem
    * in sdl3_ctx_init. While a cached context is stashed, keep our
    * reference: letting the refcount hit zero would tear the context
    * down with the subsystem. The next init adopts it. */
   if (!sdl3_gl_cached_ctx)
      SDL_QuitSubSystem(SDL_INIT_VIDEO);

   if (sdl3_gl_current == sdl)
      sdl3_gl_current = NULL;

   free(sdl);
}

static void *sdl3_ctx_init(void *video_driver)
{
   gfx_ctx_sdl3_data_t *sdl;

   if (!sdl3_ctx_enabled("gl_sdl3"))
      return NULL;

   sdl = (gfx_ctx_sdl3_data_t*)
      calloc(1, sizeof(gfx_ctx_sdl3_data_t));

   if (!sdl)
      return NULL;

   sdl3_set_app_metadata();

   /* When a cached context is stashed, the previous instance kept its
    * video-subsystem reference to protect it - adopt that reference
    * instead of initializing again. */
   if (!sdl3_gl_cached_ctx && !SDL_InitSubSystem(SDL_INIT_VIDEO))
   {
      RARCH_WARN("[SDL3 GL] Failed to initialize SDL video subsystem: %s.\n", SDL_GetError());
      free(sdl);
      return NULL;
   }

   RARCH_LOG("[SDL3 GL] SDL %d.%d.%d gfx context driver initialized.\n",
         SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);

   sdl3_gl_current = sdl;

   return sdl;
}

static enum gfx_ctx_api sdl3_ctx_get_api(void *data) { return sdl3_gl_api; }

static bool sdl3_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor)
{
   if (api != GFX_CTX_OPENGL_API && api != GFX_CTX_OPENGL_ES_API)
      return false;

   sdl3_gl_api = api;
   sdl3_gl_major = major;
   sdl3_gl_minor = minor;

   return true;
}

static void sdl3_ctx_swap_interval(void *data, int interval)
{
   /* Adaptive vsync and multi-frame intervals aren't supported
    * everywhere. Fallback to 1-frame interval rather than reverting
    * to the previous setting. */
   if (!SDL_GL_SetSwapInterval(interval) && interval != 0)
      SDL_GL_SetSwapInterval(1);
}

static bool sdl3_ctx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_sdl3_data_t *sdl = (gfx_ctx_sdl3_data_t*)data;

   if (!sdl)
      return false;

   /* GL attributes must be set before the window is created. */
   if (!sdl->win)
   {
#ifdef GL_DEBUG
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#else
      struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
      if (hwr && hwr->debug_context)
         SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

      if (sdl3_gl_api == GFX_CTX_OPENGL_ES_API)
         SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
      else if (sdl3_gl_core_profile(sdl))
         SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
      else
         SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

      if (sdl3_gl_major > 0)
      {
         SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, sdl3_gl_major);
         SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, sdl3_gl_minor);
      }
   }

   if (!sdl3_window_set_video_mode(&sdl->win, width, height, fullscreen, SDL_WINDOW_OPENGL))
      goto error;

   /* Hold onto the context across video reinit (hw_render.cache_context). */
   if (!sdl->ctx && sdl3_gl_cached_ctx)
   {
      sdl->ctx = sdl3_gl_cached_ctx;
      sdl->shared_ctx = sdl3_gl_cached_shared_ctx;
      sdl3_gl_cached_ctx = NULL;
      sdl3_gl_cached_shared_ctx = NULL;
   }

   if (sdl->ctx)
   {
      /* Bind the context to the new window. */
      if (SDL_GL_MakeCurrent(sdl->win, sdl->ctx))
      {
         video_driver_cache_context_ack_set();
         RARCH_LOG("[SDL3 GL] Using cached GL context.\n");
      }
      else
      {
         /* Stale cached context: drop it and create a fresh one
          * below. The ack stays unset, so the core receives a
          * context_reset and rebuilds its GL state. */
         RARCH_WARN("[SDL3 GL] Failed to bind cached GL context, creating anew: %s.\n",
               SDL_GetError());
         SDL_GL_DestroyContext(sdl->ctx);
         if (sdl->shared_ctx)
            SDL_GL_DestroyContext(sdl->shared_ctx);
         sdl->ctx = NULL;
         sdl->shared_ctx = NULL;
      }
   }

   if (!sdl->ctx && !(sdl->ctx = SDL_GL_CreateContext(sdl->win)))
      goto error;

   /* Check whether or not adaptive vsync is supported. Probed on the
    * cached path too: this instance is freshly alloc'd on reinit, so
    * the flag would otherwise be lost with the old instance. */
   sdl->adaptive_vsync = SDL_GL_SetSwapInterval(-1);
   SDL_GL_SetSwapInterval(0);

   return true;

error:
   RARCH_WARN("[SDL3 GL] Failed to set video mode: %s.\n", SDL_GetError());
   return false;
}

static void sdl3_ctx_swap_buffers(void *data)
{
   gfx_ctx_sdl3_data_t *sdl = (gfx_ctx_sdl3_data_t*)data;
   if (sdl && sdl->win)
      SDL_GL_SwapWindow(sdl->win);
}

static gfx_ctx_proc_t sdl3_ctx_get_proc_address(const char *name)
{
   return (gfx_ctx_proc_t)SDL_GL_GetProcAddress(name);
}

static uint32_t sdl3_ctx_get_flags(void *data)
{
   uint32_t flags = 0;
   gfx_ctx_sdl3_data_t *sdl = (gfx_ctx_sdl3_data_t*)data;

   if (sdl && sdl->adaptive_vsync)
      BIT32_SET(flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC);

   if (sdl3_gl_core_profile(sdl))
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   /* Shader usage depends on the video driver:
    * - glcore uses slang
    * - gl uses GLSLS
    * - gl1 is fixed-function, without shaders */
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   if (string_is_equal(video_driver_get_ident(), "glcore"))
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
#ifdef HAVE_GLSL
   if (string_is_equal(video_driver_get_ident(), "gl"))
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif

   return flags;
}

static void sdl3_ctx_set_flags(void *data, uint32_t flags)
{
   gfx_ctx_sdl3_data_t *sdl = (gfx_ctx_sdl3_data_t*)data;
   if (!sdl)
      return;
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      sdl->core_hw_context_enable = true;
}

static void sdl3_ctx_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_sdl3_data_t *sdl = (gfx_ctx_sdl3_data_t*)data;

   if (!sdl || !sdl->win || !sdl->ctx)
      return;

   if (enable)
   {
      if (!sdl->shared_ctx)
      {
         SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
         SDL_GL_MakeCurrent(sdl->win, sdl->ctx);
         sdl->shared_ctx = SDL_GL_CreateContext(sdl->win);
         /* Reset the attribute so later SDL_GL_CreateContext calls
          * (video reinit) don't silently inherit sharing. */
         SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
         if (!sdl->shared_ctx)
         {
            RARCH_ERR("[SDL3 GL] Failed to create shared GL context: %s.\n",
                  SDL_GetError());
            return;
         }
      }
      SDL_GL_MakeCurrent(sdl->win, sdl->shared_ctx);
   }
   else
      SDL_GL_MakeCurrent(sdl->win, sdl->ctx);
}

/* With threaded video, gl3/gl2 bind the GL context on the video
 * thread. Without this hook those binds don't do anything and
 * the textures created without a current context, resulting in
 * black menu icons. */
static void sdl3_ctx_make_current(bool release)
{
   if (!sdl3_gl_current || !sdl3_gl_current->win)
      return;
   SDL_GL_MakeCurrent(sdl3_gl_current->win,
         release ? NULL : sdl3_gl_current->ctx);
}

/* As in the SDL2 context: minimised or hidden means SDL_GL_SwapWindow()
 * returns at once rather than blocking to vblank. */
static bool sdl3_ctx_presentable(void *data)
{
   gfx_ctx_sdl3_data_t *sdl = (gfx_ctx_sdl3_data_t*)data;
   if (sdl && sdl->win)
      return !(SDL_GetWindowFlags(sdl->win)
            & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
   return true;
}

const gfx_ctx_driver_t gfx_ctx_sdl3_gl =
{
   sdl3_ctx_init,
   sdl3_ctx_destroy,
   sdl3_ctx_get_api,
   sdl3_ctx_bind_api,
   sdl3_ctx_swap_interval,
   sdl3_ctx_set_video_mode,
   sdl3_ctx_get_video_size,
   sdl3_ctx_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   sdl3_ctx_get_metrics,
   NULL, /* translate_aspect */
   sdl3_ctx_update_title,
   sdl3_ctx_check_window,
   NULL, /* set_resize */
   sdl3_ctx_has_focus,
   sdl3_suppress_screensaver,
   true, /* has_windowed */
   sdl3_ctx_swap_buffers,
   sdl3_ctx_input_driver,
   sdl3_ctx_get_proc_address,
   NULL, /* image_buffer_init */
   NULL, /* image_buffer_write */
   sdl3_show_mouse,
   "gl_sdl3",
   sdl3_ctx_get_flags,
   sdl3_ctx_set_flags,
   sdl3_ctx_bind_hw_render,
   NULL, /* get_context_data */
   sdl3_ctx_make_current,
   NULL, /* create_surface */
   NULL, /* destroy_surface */
   sdl3_ctx_presentable
};
