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
} ohos_ctx_data_vk_t;

/* FORWARD DECLARATION */
bool ohos_display_get_metrics(void *data,
	enum display_metric_types type, float *value);
bool ohos_display_has_focus(void *data);

static void ohos_gfx_ctx_vk_destroy(void *data)
{
   ohos_ctx_data_vk_t *and         = (ohos_ctx_data_vk_t*)data;
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;

   if (!and)
      return;

   vulkan_context_destroy(&and->vk, ohos_app->window);

   if (and->vk.context.queue_lock)
      slock_free(and->vk.context.queue_lock);

   free(data);
}

static void *ohos_gfx_ctx_vk_init(void *video_driver)
{
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   ohos_ctx_data_vk_t *and  = (ohos_ctx_data_vk_t*)calloc(1, sizeof(*and));

   if (!ohos_app || !and)
      return NULL;

   if (!vulkan_context_init(&and->vk, VULKAN_WSI_OHOS))
   {
      ohos_gfx_ctx_vk_destroy(and);
      return NULL;
   }

   slock_lock(ohos_app->mutex);
   if (!ohos_app->window)
   {
      slock_unlock(ohos_app->mutex);
      ohos_gfx_ctx_vk_destroy(and);
      return NULL;
   }

   slock_unlock(ohos_app->mutex);
   return and;
}

static void ohos_gfx_ctx_vk_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   ohos_ctx_data_vk_t *and  = (ohos_ctx_data_vk_t*)data;

   *width  = and->width;
   *height = and->height;
}

static void ohos_gfx_ctx_vk_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   struct ohos_app *ohos_app      = (struct ohos_app*)g_ohos;
   unsigned new_width                   = 0;
   unsigned new_height                  = 0;
   ohos_ctx_data_vk_t *and           = (ohos_ctx_data_vk_t*)data;

   *quit                                = false;

   if (ohos_app->content_rect.changed)
   {
      and->vk.flags                    |= VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
      ohos_app->content_rect.changed = false;
   }

   /* Swapchains are recreated in set_resize as a
    * central place, so use that to trigger swapchain reinit. */
   *resize    = (and->vk.flags & VK_DATA_FLAG_NEED_NEW_SWAPCHAIN) ? true : false;
   new_width  = ohos_app->content_rect.width;
   new_height = ohos_app->content_rect.height;

   if (new_width != *width || new_height != *height)
   {
      RARCH_LOG("[Vulkan] Resizing (%ux%u) -> (%ux%u).\n",
              *width, *height, new_width, new_height);

      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static bool ohos_gfx_ctx_vk_set_resize(void *data,
      unsigned width, unsigned height)
{
   ohos_ctx_data_vk_t        *and  = (ohos_ctx_data_vk_t*)data;
   struct ohos_app *ohos_app    = (struct ohos_app*)g_ohos;

   and->width                         = ohos_app->content_rect.width;
   and->height                        = ohos_app->content_rect.height;
   RARCH_LOG("[Vulkan] Native window size: %ux%u.\n", and->width, and->height);
   if (!vulkan_create_swapchain(&and->vk, and->width, and->height, and->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to update swapchain.\n");
      return false;
   }

   if (and->vk.flags & VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN)
      vulkan_acquire_next_image(&and->vk);
   and->vk.context.flags             |=  VK_CTX_FLAG_INVALID_SWAPCHAIN;
   and->vk.flags                     &= ~VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;

   return true;
}

static bool ohos_gfx_ctx_vk_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   ohos_ctx_data_vk_t *and      = (ohos_ctx_data_vk_t*)data;
   and->width                      = ohos_app->content_rect.width;
   and->height                     = ohos_app->content_rect.height;
   if (!vulkan_surface_create(&and->vk, VULKAN_WSI_OHOS,
            NULL, ohos_app->window,
            and->width, and->height, and->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to create surface.\n");
      return false;
   }
   RARCH_LOG("[Vulkan] Native window size: %ux%u.\n",
         and->width, and->height);
   return true;
}

static void ohos_gfx_ctx_vk_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   void *ohosinput   = input_driver_init_wrap(&input_ohos, joypad_name);

   *input               = ohosinput ? &input_ohos : NULL;
   *input_data          = ohosinput;
}

static enum gfx_ctx_api ohos_gfx_ctx_vk_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool ohos_gfx_ctx_vk_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return (api == GFX_CTX_VULKAN_API);
}


static bool ohos_gfx_ctx_vk_suppress_screensaver(void *data, bool enable) { return false; }

static void ohos_gfx_ctx_vk_swap_buffers(void *data)
{
   ohos_ctx_data_vk_t *and  = (ohos_ctx_data_vk_t*)data;

   if (and->vk.context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
   {
      and->vk.context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      if (and->vk.swapchain == VK_NULL_HANDLE)
      {
         retro_sleep(10);
      }
      else
         vulkan_present(&and->vk, and->vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&and->vk);
}

static void ohos_gfx_ctx_vk_set_swap_interval(void *data, int swap_interval)
{
   ohos_ctx_data_vk_t *and  = (ohos_ctx_data_vk_t*)data;

   if (and->swap_interval != swap_interval)
   {
      RARCH_LOG("[Vulkan] Setting swap interval: %u.\n", swap_interval);
      and->swap_interval       = swap_interval;
      if (and->vk.swapchain)
         and->vk.flags        |= VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   }
}

static gfx_ctx_proc_t ohos_gfx_ctx_vk_get_proc_address(const char *symbol) { return NULL; }
static void ohos_gfx_ctx_vk_bind_hw_render(void *data, bool enable) { }

static void *ohos_gfx_ctx_vk_get_context_data(void *data)
{
   ohos_ctx_data_vk_t *and = (ohos_ctx_data_vk_t*)data;
   return &and->vk.context;
}

static uint32_t ohos_gfx_ctx_vk_get_flags(void *data)
{
   uint32_t flags = 0;

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static void ohos_gfx_ctx_vk_set_flags(void *data, uint32_t flags) { }

const gfx_ctx_driver_t gfx_ctx_vk_ohos = {
   ohos_gfx_ctx_vk_init,
   ohos_gfx_ctx_vk_destroy,
   ohos_gfx_ctx_vk_get_api,
   ohos_gfx_ctx_vk_bind_api,
   ohos_gfx_ctx_vk_set_swap_interval,
   ohos_gfx_ctx_vk_set_video_mode,
   ohos_gfx_ctx_vk_get_video_size,
   NULL,                                     /* get_refresh_rate */
   NULL,                                     /* get_video_output_size */
   NULL,                                     /* get_video_output_prev */
   NULL,                                     /* get_video_output_next */
   NULL, /* get_metrics - handled by display server */
   NULL,
   NULL,                                     /* update_title */
   ohos_gfx_ctx_vk_check_window,
   ohos_gfx_ctx_vk_set_resize,
   ohos_display_has_focus,
   ohos_gfx_ctx_vk_suppress_screensaver,
   false,                                    /* has_windowed */
   ohos_gfx_ctx_vk_swap_buffers,
   ohos_gfx_ctx_vk_input_driver,
   ohos_gfx_ctx_vk_get_proc_address,
   NULL,
   NULL,
   NULL,
   "vk_ohos",
   ohos_gfx_ctx_vk_get_flags,
   ohos_gfx_ctx_vk_set_flags,
   ohos_gfx_ctx_vk_bind_hw_render,
   ohos_gfx_ctx_vk_get_context_data,
   NULL,                                     /* make_current */
   NULL,                                     /* create_surface */
   NULL                                      /* destroy_surface */
};
