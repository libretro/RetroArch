/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
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

#include "cocoa_gl_shared.h"

static bool cocoagl_gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
#ifdef HAVE_VULKAN
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
#endif

   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         cocoa_ctx->width  = width;
         cocoa_ctx->height = height;

         if (vulkan_create_swapchain(&cocoa_ctx->vk,
                  width, height, cocoa_ctx->swap_interval))
         {
            cocoa_ctx->vk.context.invalid_swapchain = true;
            if (cocoa_ctx->vk.created_new_swapchain)
               vulkan_acquire_next_image(&cocoa_ctx->vk);
         }
         else
         {
            RARCH_ERR("[macOS/Vulkan]: Failed to update swapchain.\n");
            return false;
         }

         cocoa_ctx->vk.need_new_swapchain = false;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;
}

const gfx_ctx_driver_t gfx_ctx_cocoagl = {
   .init                 = cocoagl_gfx_ctx_init,
   .destroy              = cocoagl_gfx_ctx_destroy,
   .get_api              = cocoagl_gfx_ctx_get_api,
   .bind_api             = cocoagl_gfx_ctx_bind_api,
   .swap_interval        = cocoagl_gfx_ctx_swap_interval,
   .set_video_mode       = cocoagl_gfx_ctx_set_video_mode,
   .get_video_size       = cocoagl_gfx_ctx_get_video_size,
   .get_metrics          = cocoagl_gfx_ctx_get_metrics,
#if defined(HAVE_COCOA_METAL)
   .update_window_title  = cocoagl_gfx_ctx_update_title,
#endif
   .check_window         = cocoagl_gfx_ctx_check_window,
   .set_resize           = cocoagl_gfx_ctx_set_resize,
   .has_focus            = cocoagl_gfx_ctx_has_focus,
   .suppress_screensaver = cocoagl_gfx_ctx_suppress_screensaver,
#if !defined(HAVE_COCOATOUCH)
   .has_windowed         = cocoagl_gfx_ctx_has_windowed,
#endif
   .swap_buffers         = cocoagl_gfx_ctx_swap_buffers,
   .input_driver         = cocoagl_gfx_ctx_input_driver,
   .get_proc_address     = cocoagl_gfx_ctx_get_proc_address,
   .ident                = "macOS",
   .get_flags            = cocoagl_gfx_ctx_get_flags,
   .set_flags            = cocoagl_gfx_ctx_set_flags,
   .bind_hw_render       = cocoagl_gfx_ctx_bind_hw_render,
#if defined(HAVE_VULKAN)
   .get_context_data     = cocoagl_gfx_ctx_get_context_data,
#else
   .get_context_data     = NULL,
#endif
};
