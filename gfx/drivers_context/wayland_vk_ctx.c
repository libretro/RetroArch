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

#include "../common/vulkan_common.h"

#include "../../frontend/frontend_driver.h"
#include "../../input/common/wayland_common.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../verbosity.h"

/* Generated from idle-inhibit-unstable-v1.xml */
#include "../common/wayland/idle-inhibit-unstable-v1.h"

/* Generated from xdg-shell.xml */
#include "../common/wayland/xdg-shell.h"

/* Generated from xdg-decoration-unstable-v1.h */
#include "../common/wayland/xdg-decoration-unstable-v1.h"

#include <retro_timers.h>

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

static void handle_toplevel_config_common(void *data,
      void *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   const uint32_t *state;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl->fullscreen             = false;
   wl->maximized              = false;

   WL_ARRAY_FOR_EACH(state, states, const uint32_t*)
   {
      switch (*state)
      {
         case XDG_TOPLEVEL_STATE_FULLSCREEN:
            wl->fullscreen = true;
            break;
         case XDG_TOPLEVEL_STATE_MAXIMIZED:
            wl->maximized = true;
            break;
         case XDG_TOPLEVEL_STATE_RESIZING:
            wl->resize = true;
            break;
         case XDG_TOPLEVEL_STATE_ACTIVATED:
            wl->activated = true;
            break;
      }
   }
   if (width > 0 && height > 0)
   {
      wl->prev_width  = width;
      wl->prev_height = height;
      wl->width       = width;
      wl->height      = height;
   }

   wl->configured = false;
}

/* Shell surface callbacks. */
static void handle_toplevel_config(void *data,
      struct xdg_toplevel *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   handle_toplevel_config_common(data, toplevel, width, height, states);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_toplevel_config,
    handle_toplevel_close,
};

static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   *width  = wl->width  * wl->buffer_scale;
   *height = wl->height * wl->buffer_scale;
}

static void gfx_ctx_wl_destroy_resources(gfx_ctx_wayland_data_t *wl)
{
   if (!wl)
      return;

   vulkan_context_destroy(&wl->vk, wl->surface);

   if (wl->input.dpy != NULL && wl->input.fd >= 0)
      close(wl->input.fd);

#ifdef HAVE_XKBCOMMON
   free_xkb();
#endif

   if (wl->wl_keyboard)
      wl_keyboard_destroy(wl->wl_keyboard);
   if (wl->wl_pointer)
      wl_pointer_destroy(wl->wl_pointer);
   if (wl->wl_touch)
      wl_touch_destroy(wl->wl_touch);

   if (wl->cursor.theme)
      wl_cursor_theme_destroy(wl->cursor.theme);
   if (wl->cursor.surface)
      wl_surface_destroy(wl->cursor.surface);

   if (wl->seat)
      wl_seat_destroy(wl->seat);
   if (wl->xdg_shell)
      xdg_wm_base_destroy(wl->xdg_shell);
   if (wl->compositor)
      wl_compositor_destroy(wl->compositor);
   if (wl->registry)
      wl_registry_destroy(wl->registry);
   if (wl->xdg_surface)
      xdg_surface_destroy(wl->xdg_surface);
   if (wl->surface)
      wl_surface_destroy(wl->surface);
   if (wl->xdg_toplevel)
      xdg_toplevel_destroy(wl->xdg_toplevel);
   if (wl->idle_inhibit_manager)
      zwp_idle_inhibit_manager_v1_destroy(wl->idle_inhibit_manager);
   if (wl->deco)
      zxdg_toplevel_decoration_v1_destroy(wl->deco);
   if (wl->deco_manager)
      zxdg_decoration_manager_v1_destroy(wl->deco_manager);
   if (wl->idle_inhibitor)
      zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);

   if (wl->input.dpy)
   {
      wl_display_flush(wl->input.dpy);
      wl_display_disconnect(wl->input.dpy);
   }

   wl->xdg_shell        = NULL;
   wl->compositor       = NULL;
   wl->registry         = NULL;
   wl->input.dpy        = NULL;
   wl->xdg_surface      = NULL;
   wl->surface          = NULL;
   wl->xdg_toplevel     = NULL;

   wl->width            = 0;
   wl->height           = 0;

}

static void gfx_ctx_wl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   /* this function works with SCALED sizes, it's used from the renderer */
   unsigned new_width, new_height;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   flush_wayland_fd(&wl->input);

   new_width  = *width  * wl->last_buffer_scale;
   new_height = *height * wl->last_buffer_scale;

   gfx_ctx_wl_get_video_size(data, &new_width, &new_height);

   /* Swapchains are recreated in set_resize as a
    * central place, so use that to trigger swapchain reinit. */
   *resize = wl->vk.need_new_swapchain;

   if (new_width != *width * wl->last_buffer_scale || new_height != *height * wl->last_buffer_scale)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;

      wl->last_buffer_scale = wl->buffer_scale;
   }

   *quit = (bool)frontend_driver_get_signal_handler_state();
}

static bool gfx_ctx_wl_set_resize(void *data, unsigned width, unsigned height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (vulkan_create_swapchain(&wl->vk, width, height, wl->swap_interval))
   {
      wl->vk.context.invalid_swapchain = true;
      if (wl->vk.created_new_swapchain)
         vulkan_acquire_next_image(&wl->vk);
   }
   else
   {
      RARCH_ERR("[Wayland/Vulkan]: Failed to update swapchain.\n");
      return false;
   }

   wl->vk.need_new_swapchain = false;

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   return true;
}

static void gfx_ctx_wl_update_title(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (wl && title[0])
   {
      if (wl->deco)
         {
            zxdg_toplevel_decoration_v1_set_mode(wl->deco,
                  ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
         }

      xdg_toplevel_set_title(wl->xdg_toplevel, title);
   }
}

static bool gfx_ctx_wl_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl || !wl->current_output || wl->current_output->physical_width == 0 || wl->current_output->physical_height == 0)
      return false;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = (float)wl->current_output->physical_width;
         break;

      case DISPLAY_METRIC_MM_HEIGHT:
         *value = (float)wl->current_output->physical_height;
         break;

      case DISPLAY_METRIC_DPI:
         *value = (float)wl->current_output->width * 25.4f / (float)wl->current_output->physical_width;
         break;

      default:
         *value = 0.0f;
         return false;
   }

   return true;
}

#define DEFAULT_WINDOWED_WIDTH 640
#define DEFAULT_WINDOWED_HEIGHT 480

static void *gfx_ctx_wl_init(void *video_driver)
{
   int i;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      calloc(1, sizeof(gfx_ctx_wayland_data_t));

   if (!wl)
      return NULL;

   (void)video_driver;

   wl_list_init(&wl->all_outputs);

   frontend_driver_destroy_signal_handler_state();

   wl->input.dpy = wl_display_connect(NULL);
   wl->last_buffer_scale = 1;
   wl->buffer_scale = 1;

   if (!wl->input.dpy)
   {
      RARCH_ERR("[Wayland]: Failed to connect to Wayland server.\n");
      goto error;
   }

   frontend_driver_install_signal_handler();

   wl->registry = wl_display_get_registry(wl->input.dpy);
   wl_registry_add_listener(wl->registry, &registry_listener, wl);
   wl_display_roundtrip(wl->input.dpy);

   if (!wl->compositor)
   {
      RARCH_ERR("[Wayland]: Failed to create compositor.\n");
      goto error;
   }

   if (!wl->shm)
   {
      RARCH_ERR("[Wayland]: Failed to create shm.\n");
      goto error;
   }

   if (!wl->xdg_shell)
   {
	   RARCH_ERR("[Wayland]: Failed to create shell.\n");
	   goto error;
   }

   if (!wl->idle_inhibit_manager)
   {
	   RARCH_WARN("[Wayland]: Compositor doesn't support zwp_idle_inhibit_manager_v1 protocol!\n");
   }

   if (!wl->deco_manager)
   {
	   RARCH_WARN("[Wayland]: Compositor doesn't support zxdg_decoration_manager_v1 protocol!\n");
   }

   wl->input.fd = wl_display_get_fd(wl->input.dpy);

   if (!vulkan_context_init(&wl->vk, VULKAN_WSI_WAYLAND))
      goto error;

   wl->input.keyboard_focus = true;
   wl->input.mouse.focus = true;

   wl->cursor.surface = wl_compositor_create_surface(wl->compositor);
   wl->cursor.theme = wl_cursor_theme_load(NULL, 16, wl->shm);
   wl->cursor.default_cursor = wl_cursor_theme_get_cursor(wl->cursor.theme, "left_ptr");

   wl->num_active_touches = 0;

   for (i = 0;i < MAX_TOUCHES;i++)
   {
       wl->active_touch_positions[i].active = false;
       wl->active_touch_positions[i].id     = -1;
       wl->active_touch_positions[i].x      = (unsigned) 0;
       wl->active_touch_positions[i].y      = (unsigned) 0;
   }

   flush_wayland_fd(&wl->input);

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
         wl->vk.need_new_swapchain = true;
   }
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl->width                  = width  ? width  : DEFAULT_WINDOWED_WIDTH;
   wl->height                 = height ? height : DEFAULT_WINDOWED_HEIGHT;

   wl->surface                = wl_compositor_create_surface(wl->compositor);

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   wl_surface_add_listener(wl->surface, &wl_surface_listener, wl);

   wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->xdg_shell, wl->surface);
   xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

   wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
   xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

   xdg_toplevel_set_app_id(wl->xdg_toplevel, "retroarch");
   xdg_toplevel_set_title(wl->xdg_toplevel, "RetroArch");

   if (wl->deco_manager)
      {
         wl->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
               wl->deco_manager, wl->xdg_toplevel);
      }

      /* Waiting for xdg_toplevel to be configured before starting to draw */
      wl_surface_commit(wl->surface);
      wl->configured = true;

      while (wl->configured)
         wl_display_dispatch(wl->input.dpy);

      wl_display_roundtrip(wl->input.dpy);
      xdg_wm_base_add_listener(wl->xdg_shell, &xdg_shell_listener, NULL);

   if (fullscreen)
   {
	   xdg_toplevel_set_fullscreen(wl->xdg_toplevel, NULL);
	}

   flush_wayland_fd(&wl->input);

   wl_display_roundtrip(wl->input.dpy);

   if (!vulkan_surface_create(&wl->vk, VULKAN_WSI_WAYLAND,
            wl->input.dpy, wl->surface,
            wl->width * wl->buffer_scale, wl->height * wl->buffer_scale, wl->swap_interval))
      goto error;

   if (fullscreen)
   {
      wl->cursor.visible = false;
      gfx_ctx_wl_show_mouse(wl, false);
   }
   else
      wl->cursor.visible = true;

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

static bool gfx_ctx_wl_has_focus(void *data)
{
   (void)data;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return wl->input.keyboard_focus;
}

static bool gfx_ctx_wl_suppress_screensaver(void *data, bool state)
{
	(void)data;

	gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

    if (!wl->idle_inhibit_manager)
        return false;
    if (state == (!!wl->idle_inhibitor))
        return true;
    if (state)
    {
        RARCH_LOG("[Wayland]: Enabling idle inhibitor\n");
        struct zwp_idle_inhibit_manager_v1 *mgr = wl->idle_inhibit_manager;
        wl->idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(mgr, wl->surface);
    }
    else
    {
        RARCH_LOG("[Wayland]: Disabling the idle inhibitor\n");
        zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);
        wl->idle_inhibitor = NULL;
    }
    return true;
}

static enum gfx_ctx_api gfx_ctx_wl_get_api(void *data) { return GFX_CTX_VULKAN_API; }

static bool gfx_ctx_wl_bind_api(void *video_driver,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   if (api == GFX_CTX_VULKAN_API)
         return true;
   return false;
}

static void *gfx_ctx_wl_get_context_data(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return &wl->vk.context;
}

static void gfx_ctx_wl_swap_buffers(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->vk.context.has_acquired_swapchain)
   {
      wl->vk.context.has_acquired_swapchain = false;
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

static gfx_ctx_proc_t gfx_ctx_wl_get_proc_address(const char *symbol)
{
   return NULL;
}

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

static float gfx_ctx_wl_get_refresh_rate(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl || !wl->current_output)
      return false;

   return (float) wl->current_output->refresh_rate / 1000.0f;
}

const gfx_ctx_driver_t gfx_ctx_vk_wayland = {
   gfx_ctx_wl_init,
   gfx_ctx_wl_destroy,
   gfx_ctx_wl_get_api,
   gfx_ctx_wl_bind_api,
   gfx_ctx_wl_set_swap_interval,
   gfx_ctx_wl_set_video_mode,
   gfx_ctx_wl_get_video_size,
   gfx_ctx_wl_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_wl_get_metrics,
   NULL,
   gfx_ctx_wl_update_title,
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
