/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <unistd.h>

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/wayland_common.h"
#include "../../frontend/frontend_driver.h"
#include "../../input/common/wayland_common.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../verbosity.h"

#include "../common/vulkan_common.h"

#include <retro_timers.h>

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

/* Shell surface callbacks. */
static void xdg_toplevel_handle_configure(void *data,
      struct xdg_toplevel *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (wl->ignore_configuration)
      return;
   xdg_toplevel_handle_configure_common(wl, toplevel, width, height, states);
   wl->configured = false;
}

static void gfx_ctx_wl_destroy_resources(gfx_ctx_wayland_data_t *wl)
{
   if (!wl)
      return;
   vulkan_context_destroy(&wl->vk, wl->surface);
   gfx_ctx_wl_destroy_resources_common(wl);
}

static void gfx_ctx_wl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   /* Swapchains are recreated in set_resize as a
    * central place, so use that to trigger swapchain reinit. */
   *resize = wl->vk.flags & VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;

   gfx_ctx_wl_check_window_common(wl, gfx_ctx_wl_get_video_size_common, quit, resize, 
      width, height);

}

static bool gfx_ctx_wl_set_resize(void *data, unsigned width, unsigned height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl->last_buffer_scale = wl->buffer_scale;
   wl->last_fractional_scale_num = wl->fractional_scale_num;
   if (!wl->fractional_scale)
      wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);

   if (vulkan_create_swapchain(&wl->vk, width, height, wl->swap_interval))
   {
      wl->ignore_configuration = false;
      wl->vk.context.flags |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
      if (wl->vk.flags & VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN)
         vulkan_acquire_next_image(&wl->vk);

      wl->vk.flags         &= ~VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;

      return true;
   }

   RARCH_ERR("[Wayland/Vulkan]: Failed to update swapchain.\n");
   return false;
}

#ifdef HAVE_LIBDECOR_H
static void
libdecor_frame_handle_configure(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration, void *data)
{
   gfx_ctx_wayland_data_t *wl   = (gfx_ctx_wayland_data_t*)data;
   if (wl->ignore_configuration)
      return;
   libdecor_frame_handle_configure_common(frame, configuration, wl);

   wl->configured = false;
}
#endif

static const toplevel_listener_t toplevel_listener = {
#ifdef HAVE_LIBDECOR_H
   .libdecor_frame_interface = {
     libdecor_frame_handle_configure,
     libdecor_frame_handle_close,
     libdecor_frame_handle_commit,
   },
#endif
   .xdg_toplevel_listener = {
      xdg_toplevel_handle_configure,
      xdg_toplevel_handle_close,
   },
};

static void *gfx_ctx_wl_init(void *data)
{
   int i;
   gfx_ctx_wayland_data_t *wl = NULL;

   if (!gfx_ctx_wl_init_common(&toplevel_listener, &wl))
      goto error;

   if (!vulkan_context_init(&wl->vk, VULKAN_WSI_WAYLAND))
      goto error;

   return wl;

error:
   gfx_ctx_wl_destroy_resources(wl);

   if (wl)
      free(wl);

   return NULL;
}

static void gfx_ctx_wl_destroy(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl)
      return;

   gfx_ctx_wl_destroy_resources(wl);

#if defined(HAVE_THREADS)
   if (wl->vk.context.queue_lock)
      slock_free(wl->vk.context.queue_lock);
#endif

   free(wl);
}

static void gfx_ctx_wl_set_swap_interval(void *data, int swap_interval)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->swap_interval != swap_interval)
   {
      wl->swap_interval = swap_interval;
      if (wl->vk.swapchain)
         wl->vk.flags  |= VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   }
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_wayland_data_t *wl   = (gfx_ctx_wayland_data_t*)data;

   if (!gfx_ctx_wl_set_video_mode_common_size(wl, width, height, fullscreen))
      goto error;

   if (!vulkan_surface_create(&wl->vk, VULKAN_WSI_WAYLAND,
         wl->input.dpy, wl->surface,
         wl->buffer_width,
         wl->buffer_height,
         wl->swap_interval))
      goto error;

   if (!gfx_ctx_wl_set_video_mode_common_fullscreen(wl, fullscreen))
      goto error;

   return true;

error:
   gfx_ctx_wl_destroy(data);
   return false;
}

bool input_wl_init(void *data, const char *joypad_name);

static void gfx_ctx_wl_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   /* Input is heavily tied to the window stuff
    * on Wayland, so just implement the input driver here. */
   if (!input_wl_init(&wl->input, joypad_name))
   {
      wl->input.gfx = NULL;
      *input        = NULL;
      *input_data   = NULL;
   }
   else
   {
      wl->input.gfx = wl;
      *input        = &input_wayland;
      *input_data   = &wl->input;
      input_driver_init_joypads();
   }
}

static enum gfx_ctx_api gfx_ctx_wl_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool gfx_ctx_wl_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return (api == GFX_CTX_VULKAN_API);
}

static void *gfx_ctx_wl_get_context_data(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return &wl->vk.context;
}

static void gfx_ctx_wl_swap_buffers(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->vk.context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
   {
      wl->vk.context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      if (wl->vk.swapchain == VK_NULL_HANDLE)
      {
         retro_sleep(10);
      }
      else
         vulkan_present(&wl->vk, wl->vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&wl->vk);
   flush_wayland_fd(&wl->input);
}

static gfx_ctx_proc_t gfx_ctx_wl_get_proc_address(const char *symbol) { return NULL; }
static void gfx_ctx_wl_bind_hw_render(void *data, bool enable) { }

static uint32_t gfx_ctx_wl_get_flags(void *data)
{
   uint32_t             flags = 0;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static void gfx_ctx_wl_set_flags(void *data, uint32_t flags) { }

const gfx_ctx_driver_t gfx_ctx_vk_wayland = {
   gfx_ctx_wl_init,
   gfx_ctx_wl_destroy,
   gfx_ctx_wl_get_api,
   gfx_ctx_wl_bind_api,
   gfx_ctx_wl_set_swap_interval,
   gfx_ctx_wl_set_video_mode,
   gfx_ctx_wl_get_video_size_common,
   gfx_ctx_wl_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_wl_get_metrics_common,
   NULL,
   gfx_ctx_wl_update_title_common,
   gfx_ctx_wl_check_window,
   gfx_ctx_wl_set_resize,
   gfx_ctx_wl_has_focus,
   gfx_ctx_wl_suppress_screensaver,
   true, /* has_windowed */
   gfx_ctx_wl_swap_buffers,
   gfx_ctx_wl_input_driver,
   gfx_ctx_wl_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_wl_show_mouse,
   "vk_wayland",
   gfx_ctx_wl_get_flags,
   gfx_ctx_wl_set_flags,
   gfx_ctx_wl_bind_hw_render,
   gfx_ctx_wl_get_context_data,
   NULL,
};
