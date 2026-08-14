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

/* SDL3 Vulkan context driver. While the SDL3 video driver itself is
 * software rendered via SDL_Renderer, this context driver allows
 * the vulkan video driver to render within the SDL3 window. */

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_timers.h>

#include "../../verbosity.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "../common/sdl3_common.h"
#include "../common/vulkan_common.h"

typedef struct gfx_ctx_sdl3_vk_data
{
   SDL_Window *win; /* Must be first because it's shared across
                     * sdl3_ctx_* callbacks. */
   int  interval;
   gfx_ctx_vulkan_data_t vk;
} gfx_ctx_sdl3_vk_data_t;

static void sdl3_vk_ctx_destroy(void *data)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;

   if (!sdl)
      return;

   vulkan_context_destroy(&sdl->vk, sdl->win != NULL);

#if defined(HAVE_THREADS)
   if (sdl->vk.context.queue_lock)
      slock_free(sdl->vk.context.queue_lock);
#endif

   if (sdl->win)
   {
      SDL_StopTextInput(sdl->win);
      SDL_DestroyWindow(sdl->win);
   }

   SDL_Vulkan_UnloadLibrary();
   /* Balances the SDL_InitSubSystem in init (SDL3 refcounts). */
   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   free(sdl);
}

static void *sdl3_vk_ctx_init(void *video_driver)
{
   gfx_ctx_sdl3_vk_data_t *sdl;

   if (!sdl3_ctx_enabled("vk_sdl3"))
      return NULL;

   sdl = (gfx_ctx_sdl3_vk_data_t*)
      calloc(1, sizeof(gfx_ctx_sdl3_vk_data_t));

   if (!sdl)
      return NULL;

   sdl3_set_app_metadata();

   /* SDL's X11 backend calls XInitThreads itself, so no Xlib setup
    * is needed here. */
   if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
   {
      RARCH_WARN("[SDL3 Vulkan] Failed to initialize SDL video subsystem: %s.\n",
            SDL_GetError());
      free(sdl);
      return NULL;
   }

   /* Load SDL's Vulkan support up front: instance creation queries
    * SDL_Vulkan_GetInstanceExtensions before any window exists. */
   if (!SDL_Vulkan_LoadLibrary(NULL))
   {
      RARCH_WARN("[SDL3 Vulkan] Failed to load Vulkan library: %s.\n",
            SDL_GetError());
      goto error;
   }

   if (!vulkan_context_init(&sdl->vk, VULKAN_WSI_SDL3))
   {
      SDL_Vulkan_UnloadLibrary();
      goto error;
   }

   RARCH_LOG("[SDL3 Vulkan] SDL %d.%d.%d gfx context driver initialized.\n",
         SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);

   return sdl;

error:
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
   free(sdl);
   return NULL;
}

static void sdl3_vk_ctx_swap_interval(void *data, int interval)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;

   if (sdl->interval != interval)
   {
      sdl->interval = interval;
      if (sdl->vk.swapchain)
         sdl->vk.flags |= VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   }
}

static void sdl3_vk_ctx_swap_buffers(void *data)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;

   if (sdl->vk.context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
   {
      sdl->vk.context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      if (sdl->vk.swapchain == VK_NULL_HANDLE)
      {
         retro_sleep(10);
      }
      else
         vulkan_present(&sdl->vk, sdl->vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&sdl->vk);
}

static void sdl3_vk_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;

   /* A pending swapchain rebuild counts as a resize; the shared
    * handler pumps events and fetches the new size. */
   if (sdl->vk.flags & VK_DATA_FLAG_NEED_NEW_SWAPCHAIN)
      *resize = true;

   sdl3_ctx_check_window(data, quit, resize, width, height);
}

static bool sdl3_vk_ctx_set_resize(void *data,
      unsigned width, unsigned height)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;

   if (!sdl)
      return false;

   if (!vulkan_create_swapchain(&sdl->vk, width, height, sdl->interval))
   {
      RARCH_ERR("[SDL3 Vulkan] Failed to update swapchain.\n");
      sdl->vk.swapchain           = VK_NULL_HANDLE;
      return false;
   }

   if (sdl->vk.flags & VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN)
      vulkan_acquire_next_image(&sdl->vk);
   sdl->vk.context.flags         |=  VK_CTX_FLAG_INVALID_SWAPCHAIN;
   sdl->vk.flags                 &= ~VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   return true;
}

static bool sdl3_vk_ctx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;
   unsigned win_width = 0;
   unsigned win_height = 0;

   if (!sdl)
      return false;

   if (!sdl3_window_set_video_mode(&sdl->win, width, height, fullscreen,
            SDL_WINDOW_VULKAN))
      goto error;

   sdl3_window_get_video_size(sdl->win, &win_width, &win_height);

   if (!vulkan_surface_create(&sdl->vk, VULKAN_WSI_SDL3,
            NULL, sdl->win,
            win_width, win_height, sdl->interval))
      goto error;

   return true;

error:
   /* Avoids destroying SDL here since vulkan_init() treats a failure
    * and will call vulkan_free() itself. This is the same for x_vk,
    * cocoa, and android. */
   RARCH_WARN("[SDL3 Vulkan] Failed to set video mode: %s.\n",
         SDL_GetError());
   return false;
}

static enum gfx_ctx_api sdl3_vk_ctx_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool sdl3_vk_ctx_bind_api(void *data, enum gfx_ctx_api api,
      unsigned major, unsigned minor) { return (api == GFX_CTX_VULKAN_API); }

/* Vulkan HW-render cores share the frontend's VkDevice through
 * context negotiation - there is no context to switch. */
static void sdl3_vk_ctx_bind_hw_render(void *data, bool enable) { }

static void *sdl3_vk_ctx_get_context_data(void *data)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;
   return &sdl->vk.context;
}

static uint32_t sdl3_vk_ctx_get_flags(void *data)
{
   gfx_ctx_sdl3_vk_data_t *sdl = (gfx_ctx_sdl3_vk_data_t*)data;
   uint32_t flags              = 0;
   uint8_t present_mode_count  = 16;
   uint8_t i                   = 0;

   /* Check for FIFO_RELAXED_KHR capability. */
   if (sdl)
   {
      for (i = 0; i < present_mode_count; i++)
      {
         if (sdl->vk.context.present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
         {
            BIT32_SET(flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC);
            break;
         }
      }
   }

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static void sdl3_vk_ctx_set_flags(void *data, uint32_t flags) { }

const gfx_ctx_driver_t gfx_ctx_sdl3_vk = {
   sdl3_vk_ctx_init,
   sdl3_vk_ctx_destroy,
   sdl3_vk_ctx_get_api,
   sdl3_vk_ctx_bind_api,
   sdl3_vk_ctx_swap_interval,
   sdl3_vk_ctx_set_video_mode,
   sdl3_ctx_get_video_size,
   sdl3_ctx_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   sdl3_ctx_get_metrics,
   NULL, /* translate_aspect */
   sdl3_ctx_update_title,
   sdl3_vk_ctx_check_window,
   sdl3_vk_ctx_set_resize,
   sdl3_ctx_has_focus,
   sdl3_suppress_screensaver,
   true, /* has_windowed */
   sdl3_vk_ctx_swap_buffers,
   sdl3_ctx_input_driver,
   NULL, /* get_proc_address */
   NULL, /* image_buffer_init */
   NULL, /* image_buffer_write */
   sdl3_show_mouse,
   "vk_sdl3",
   sdl3_vk_ctx_get_flags,
   sdl3_vk_ctx_set_flags,
   sdl3_vk_ctx_bind_hw_render,
   sdl3_vk_ctx_get_context_data,
   /* make_current: GL-only. Vulkan has no per-thread context
    * to bind. x, wayland, android, and other Vulkan drivers
    * do the same. */
   NULL,
   NULL, /* create_surface */
   NULL  /* destroy_surface */
};
