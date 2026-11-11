/*  RetroArch - A frontend for libretro.
 *
 *  Vulkan context for the Switch 2 (libounce, OunceVK): the surface is a
 *  VK_NN_vi_surface on the process's default NWindow.
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

#include <features/features_cpu.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_timers.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <switch.h>

#include "../common/switch_defines.h"
#include "../common/vulkan_common.h"
#include "../../frontend/frontend_driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"

/* TODO/FIXME - global referenced */
extern bool platform_switch_has_focus;
extern bool switch_ctx_get_metrics(void *data,
      enum display_metric_types type, float *value);

typedef struct
{
   gfx_ctx_vulkan_data_t vk;
   NWindow *win;
   int swap_interval;
   unsigned dims;                /* VIDEO_SCALE_PACK */
   unsigned refresh_rate_x1000;
} switch_vk_ctx_data_t;

/* The size the console's output has now: handheld or docked */
static unsigned switch_vk_ctx_mode_dims(void)
{
   switch (appletGetOperationMode())
   {
      case AppletOperationMode_Console:
         return VIDEO_SCALE_PACK(1920, 1080);
      case AppletOperationMode_Handheld:
      default:
         break;
   }
   return VIDEO_SCALE_PACK(1280, 720);
}

static void switch_vk_ctx_destroy(void *data)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;
   if (!ctx)
      return;

   vulkan_context_destroy(&ctx->vk, true);
#ifdef HAVE_THREADS
   if (ctx->vk.context.queue_lock)
      slock_free(ctx->vk.context.queue_lock);
#endif
   free(ctx);
}

static void switch_vk_ctx_get_video_size(void *data, unsigned *dims)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;
   *dims = ctx ? ctx->dims : switch_vk_ctx_mode_dims();
}

static float switch_vk_ctx_get_refresh_rate(void *data)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;
   return ctx ? ctx->refresh_rate_x1000 / 1000.0f : 60.0f;
}

static void *switch_vk_ctx_init(void *video_driver)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)
      calloc(1, sizeof(*ctx));
   if (!ctx)
      return NULL;

   ctx->win = nwindowGetDefault();
   ctx->dims = switch_vk_ctx_mode_dims();
   nwindowSetDimensions(ctx->win, VIDEO_SCALE_W(ctx->dims), VIDEO_SCALE_H(ctx->dims));

   if (!vulkan_context_init(&ctx->vk, VULKAN_WSI_SWITCH))
   {
      RARCH_ERR("[Vulkan] Failed to create Vulkan context.\n");
      goto error;
   }

   return ctx;

error:
   switch_vk_ctx_destroy(ctx);
   return NULL;
}

static void switch_vk_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *dims)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;
   unsigned new_dims         = switch_vk_ctx_mode_dims();

   *resize = (ctx->vk.flags & VK_DATA_FLAG_NEED_NEW_SWAPCHAIN) ? true : false;

   if (new_dims != ctx->dims)
   {
      ctx->dims = new_dims;
      nwindowSetDimensions(ctx->win, VIDEO_SCALE_W(new_dims), VIDEO_SCALE_H(new_dims));
      *resize   = true;
   }
   if (*dims != ctx->dims)
   {
      *dims   = ctx->dims;
      *resize = true;
   }

   *quit = false;
}

static bool switch_vk_ctx_set_resize(void *data, unsigned dims)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;

   ctx->dims = dims;

   if (!vulkan_create_swapchain(&ctx->vk, dims, ctx->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to update swapchain.\n");
      return false;
   }

   if (ctx->vk.flags & VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN)
   {
      vulkan_acquire_next_image(&ctx->vk);
      ctx->vk.context.flags |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
   }
   ctx->vk.flags &= ~VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   return true;
}

static bool switch_vk_ctx_set_video_mode(void *data,
      unsigned dims, bool fullscreen)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;

   ctx->dims = switch_vk_ctx_mode_dims();
   nwindowSetDimensions(ctx->win, VIDEO_SCALE_W(ctx->dims), VIDEO_SCALE_H(ctx->dims));

   if (!vulkan_surface_create(&ctx->vk, VULKAN_WSI_SWITCH,
            NULL, ctx->win, ctx->dims, ctx->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to create the Switch surface.\n");
      switch_vk_ctx_destroy(data);
      return false;
   }

   /* the swapchain may have come out another size */
   if (ctx->vk.context.swapchain_dims)
      ctx->dims = ctx->vk.context.swapchain_dims;
   ctx->refresh_rate_x1000 = 60000;

   return true;
}

static void switch_vk_ctx_input_driver(void *data, const char *name) { }

static enum gfx_ctx_api switch_vk_ctx_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool switch_vk_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return api == GFX_CTX_VULKAN_API;
}

static void switch_vk_ctx_set_flags(void *data, uint32_t flags) { }
static bool switch_vk_ctx_has_focus(void *data) { return platform_switch_has_focus; }
static bool switch_vk_ctx_suppress_screensaver(void *data, bool enable) { return false; }
static gfx_ctx_proc_t switch_vk_ctx_get_proc_address(const char *symbol) { return NULL; }

static void switch_vk_ctx_set_swap_interval(void *data, int swap_interval)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;

   if (ctx->swap_interval != swap_interval)
   {
      ctx->swap_interval = swap_interval;
      if (ctx->vk.swapchain)
         ctx->vk.flags |= VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   }
}

static bool switch_vk_ctx_presentable(void *data)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;
   return ctx && ctx->vk.swapchain != VK_NULL_HANDLE;
}

static void switch_vk_ctx_swap_buffers(void *data)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;

   if (ctx->vk.context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
   {
      ctx->vk.context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      if (ctx->vk.swapchain != VK_NULL_HANDLE)
         vulkan_present(&ctx->vk, ctx->vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&ctx->vk);
}

static uint32_t switch_vk_ctx_get_flags(void *data)
{
   uint32_t flags = 0;
#if defined(HAVE_SLANG) && (defined(HAVE_SPIRV_CROSS) || defined(HAVE_RSLC))
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   return flags;
}

static void *switch_vk_ctx_get_context_data(void *data)
{
   switch_vk_ctx_data_t *ctx = (switch_vk_ctx_data_t*)data;
   return &ctx->vk.context;
}

const gfx_ctx_driver_t gfx_ctx_switch_vk = {
   switch_vk_ctx_init,
   switch_vk_ctx_destroy,
   switch_vk_ctx_get_api,
   switch_vk_ctx_bind_api,
   switch_vk_ctx_set_swap_interval,
   switch_vk_ctx_set_video_mode,
   switch_vk_ctx_get_video_size,
   switch_vk_ctx_get_refresh_rate,
   NULL,                                        /* get_video_output_size */
   NULL,                                        /* get_video_output_prev */
   NULL,                                        /* get_video_output_next */
   switch_ctx_get_metrics,
   NULL,
   NULL,                                        /* update_title */
   switch_vk_ctx_check_window,
   switch_vk_ctx_set_resize,
   switch_vk_ctx_has_focus,
   switch_vk_ctx_suppress_screensaver,
   false,                                       /* has_windowed */
   switch_vk_ctx_swap_buffers,
   switch_vk_ctx_input_driver,
   switch_vk_ctx_get_proc_address,
   NULL,
   NULL,
   NULL,
   "vk_switch",
   switch_vk_ctx_get_flags,
   switch_vk_ctx_set_flags,
   NULL,
   switch_vk_ctx_get_context_data,
   NULL,                                        /* make_current */
   NULL,                                        /* create_surface */
   NULL,                                        /* destroy_surface */
   switch_vk_ctx_presentable
};
