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
#include <stdint.h>

#include <sys/system_properties.h>

#include <formats/image.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <retro_timers.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/vulkan_common.h"

#include "../../frontend/frontend_driver.h"
#include "../../frontend/drivers/platform_unix.h"
#include "../../verbosity.h"
#include "../../configuration.h"

typedef struct
{
   gfx_ctx_vulkan_data_t vk;
   unsigned width;
   unsigned height;
   int swap_interval;
} android_ctx_data_vk_t;

static void android_gfx_ctx_vk_destroy(void *data)
{
   android_ctx_data_vk_t *and         = (android_ctx_data_vk_t*)data;
   struct android_app *android_app = (struct android_app*)g_android;

   if (!and)
      return;

   vulkan_context_destroy(&and->vk, android_app->window);

   if (and->vk.context.queue_lock)
      slock_free(and->vk.context.queue_lock);

   free(data);
}

static void *android_gfx_ctx_vk_init(void *video_driver)
{
   struct android_app *android_app = (struct android_app*)g_android;
   android_ctx_data_vk_t *and  = (android_ctx_data_vk_t*)calloc(1, sizeof(*and));

   if (!android_app || !and)
      return false;

   if (!vulkan_context_init(&and->vk, VULKAN_WSI_ANDROID))
      goto error;

   slock_lock(android_app->mutex);
   if (!android_app->window)
   {
      slock_unlock(android_app->mutex);
      android_gfx_ctx_vk_destroy(and);
      return NULL;
   }

   slock_unlock(android_app->mutex);
   return and;

error:
   android_gfx_ctx_vk_destroy(and);

   return NULL;
}

static void android_gfx_ctx_vk_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   android_ctx_data_vk_t *and  = (android_ctx_data_vk_t*)data;

   *width  = and->width;
   *height = and->height;
}

static void android_gfx_ctx_vk_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   struct android_app *android_app = (struct android_app*)g_android;

   unsigned new_width       = 0;
   unsigned new_height      = 0;
   android_ctx_data_vk_t *and  = (android_ctx_data_vk_t*)data;

   *quit = false;

   if (android_app->content_rect.changed)
   {
      and->vk.need_new_swapchain = true;
      android_app->content_rect.changed = false;
   }

   /* Swapchains are recreated in set_resize as a
    * central place, so use that to trigger swapchain reinit. */
   *resize    = and->vk.need_new_swapchain;
   new_width  = android_app->content_rect.width;
   new_height = android_app->content_rect.height;

   if (new_width != *width || new_height != *height)
   {
      RARCH_LOG("[Android]: Resizing (%u x %u) -> (%u x %u).\n",
              *width, *height, new_width, new_height);

      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static bool android_gfx_ctx_vk_set_resize(void *data,
      unsigned width, unsigned height)
{
   android_ctx_data_vk_t        *and  = (android_ctx_data_vk_t*)data;
   struct android_app *android_app = (struct android_app*)g_android;

   and->width  = android_app->content_rect.width;
   and->height = android_app->content_rect.height;
   RARCH_LOG("[Android]: Native window size: %u x %u.\n", and->width, and->height);
   if (!vulkan_create_swapchain(&and->vk, and->width, and->height, and->swap_interval))
   {
      RARCH_ERR("[Android]: Failed to update swapchain.\n");
      return false;
   }

   if (and->vk.created_new_swapchain)
      vulkan_acquire_next_image(&and->vk);
   and->vk.context.invalid_swapchain = true;
   and->vk.need_new_swapchain        = false;

   return true;
}

static bool android_gfx_ctx_vk_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   struct android_app *android_app = (struct android_app*)g_android;
   android_ctx_data_vk_t *and = (android_ctx_data_vk_t*)data;

   and->width  = ANativeWindow_getWidth(android_app->window);
   and->height = ANativeWindow_getHeight(android_app->window);
   RARCH_LOG("[Android]: Native window size: %u x %u.\n",
         and->width, and->height);
   if (!vulkan_surface_create(&and->vk, VULKAN_WSI_ANDROID,
            NULL, android_app->window,
            and->width, and->height, and->swap_interval))
   {
      RARCH_ERR("[Android]: Failed to create surface.\n");
      return false;
   }

   return true;
}

static void android_gfx_ctx_vk_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   void *androidinput   = input_driver_init_wrap(&input_android, joypad_name);

   *input               = androidinput ? &input_android : NULL;
   *input_data          = androidinput;
}

static enum gfx_ctx_api android_gfx_ctx_vk_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool android_gfx_ctx_vk_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   if (api == GFX_CTX_VULKAN_API)
      return true;
   return false;
}

static bool android_gfx_ctx_vk_has_focus(void *data)
{
   bool                    focused = false;
   struct android_app *android_app = (struct android_app*)g_android;
   if (!android_app)
      return true;

   slock_lock(android_app->mutex);
   focused = !android_app->unfocused;
   slock_unlock(android_app->mutex);

   return focused;
}

static bool android_gfx_ctx_vk_suppress_screensaver(void *data, bool enable) { return false; }

static bool android_gfx_ctx_vk_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   static int dpi = -1;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
      case DISPLAY_METRIC_MM_HEIGHT:
         return false;
      case DISPLAY_METRIC_DPI:
         if (dpi == -1)
         {
            char density[PROP_VALUE_MAX];
            android_dpi_get_density(density, sizeof(density));
            if (string_is_empty(density))
               goto dpi_fallback;
            if ((dpi = atoi(density)) <= 0)
               goto dpi_fallback;
         }
         *value = (float)dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;

dpi_fallback:
   /* add a fallback in case the device doesn't report DPI.
    * Hopefully fixes issues with the moto G2. */
   dpi    = 90;
   *value = (float)dpi;
   return true;
}

static void android_gfx_ctx_vk_swap_buffers(void *data)
{
   android_ctx_data_vk_t *and  = (android_ctx_data_vk_t*)data;

   if (and->vk.context.has_acquired_swapchain)
   {
      and->vk.context.has_acquired_swapchain = false;
      if (and->vk.swapchain == VK_NULL_HANDLE)
      {
         retro_sleep(10);
      }
      else
         vulkan_present(&and->vk, and->vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&and->vk);
}

static void android_gfx_ctx_vk_set_swap_interval(void *data, int swap_interval)
{
   android_ctx_data_vk_t *and  = (android_ctx_data_vk_t*)data;

   if (and->swap_interval != swap_interval)
   {
      RARCH_LOG("[Vulkan]: Setting swap interval: %u.\n", swap_interval);
      and->swap_interval = swap_interval;
      if (and->vk.swapchain)
         and->vk.need_new_swapchain = true;
   }
}

static gfx_ctx_proc_t android_gfx_ctx_vk_get_proc_address(const char *symbol) { return NULL; }
static void android_gfx_ctx_vk_bind_hw_render(void *data, bool enable) { }

static void *android_gfx_ctx_vk_get_context_data(void *data)
{
   android_ctx_data_vk_t *and = (android_ctx_data_vk_t*)data;
   return &and->vk.context;
}

static uint32_t android_gfx_ctx_vk_get_flags(void *data)
{
   uint32_t flags = 0;

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static void android_gfx_ctx_vk_set_flags(void *data, uint32_t flags) { }

const gfx_ctx_driver_t gfx_ctx_vk_android = {
   android_gfx_ctx_vk_init,
   android_gfx_ctx_vk_destroy,
   android_gfx_ctx_vk_get_api,
   android_gfx_ctx_vk_bind_api,
   android_gfx_ctx_vk_set_swap_interval,
   android_gfx_ctx_vk_set_video_mode,
   android_gfx_ctx_vk_get_video_size,
   NULL,                                     /* get_refresh_rate */
   NULL,                                     /* get_video_output_size */
   NULL,                                     /* get_video_output_prev */
   NULL,                                     /* get_video_output_next */
   android_gfx_ctx_vk_get_metrics,
   NULL,
   NULL,                                     /* update_title */
   android_gfx_ctx_vk_check_window,
   android_gfx_ctx_vk_set_resize,
   android_gfx_ctx_vk_has_focus,
   android_gfx_ctx_vk_suppress_screensaver,
   false,                                    /* has_windowed */
   android_gfx_ctx_vk_swap_buffers,
   android_gfx_ctx_vk_input_driver,
   android_gfx_ctx_vk_get_proc_address,
   NULL,
   NULL,
   NULL,
   "vk_android",
   android_gfx_ctx_vk_get_flags,
   android_gfx_ctx_vk_set_flags,
   android_gfx_ctx_vk_bind_hw_render,
   android_gfx_ctx_vk_get_context_data,
   NULL                                      /* make_current */
};
