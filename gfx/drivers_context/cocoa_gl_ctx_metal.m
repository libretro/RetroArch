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

static void *cocoagl_gfx_ctx_init(video_frame_info_t *video_info, void *video_driver)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)
   calloc(1, sizeof(cocoa_ctx_data_t));

   if (!cocoa_ctx)
      return NULL;

   switch (cocoagl_api)
   {
#if defined(HAVE_COCOATOUCH)
      case GFX_CTX_OPENGL_ES_API:
         // setViewType is not (yet?) defined for iOS
         // [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL_ES];
         break;
#elif defined(HAVE_COCOA_METAL)
      case GFX_CTX_OPENGL_API:
         [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL];
         break;
#endif
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         [apple_platform setViewType:APPLE_VIEW_TYPE_VULKAN];
         if (!vulkan_context_init(&cocoa_ctx->vk, VULKAN_WSI_MVK_MACOS))
            goto error;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return cocoa_ctx;

error:
   free(cocoa_ctx);
   return NULL;
}

static bool cocoagl_gfx_ctx_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height, bool fullscreen)
{
#if defined(HAVE_COCOA_METAL)
   NSView *g_view              = apple_platform.renderView;
#elif defined(HAVE_COCOA)
   CocoaView *g_view           = (CocoaView*)nsview_get_ptr();
#endif
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   cocoa_ctx->width            = width;
   cocoa_ctx->height           = height;

   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      {
#if defined(HAVE_COCOA_METAL)
         if ([g_view respondsToSelector: @selector(setWantsBestResolutionOpenGLSurface:)])
            [g_view setWantsBestResolutionOpenGLSurface:YES];

         NSOpenGLPixelFormatAttribute attributes [] = {
            NSOpenGLPFAColorSize,
            24,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAllowOfflineRenderers,
            NSOpenGLPFADepthSize,
            (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
            0,                                /* profile */
            0,                                /* profile enum */
            (NSOpenGLPixelFormatAttribute)0
         };

#if MAC_OS_X_VERSION_10_7
         if (g_major == 3 && (g_minor >= 1 && g_minor <= 3))
         {
            attributes[6] = NSOpenGLPFAOpenGLProfile;
            attributes[7] = NSOpenGLProfileVersion3_2Core;
         }
#endif

#if MAC_OS_X_VERSION_10_10
         if (g_major == 4 && g_minor == 1)
         {
            attributes[6] = NSOpenGLPFAOpenGLProfile;
            attributes[7] = NSOpenGLProfileVersion4_1Core;
         }
#endif

         g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
         if (g_format == nil)
         {
            /* NSOpenGLFPAAllowOfflineRenderers is
             not supported on this OS version. */
            attributes[3] = (NSOpenGLPixelFormatAttribute)0;
            g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
         }
#endif

         if (g_use_hw_ctx)
            g_hw_ctx  = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
         g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:(g_use_hw_ctx) ? g_hw_ctx : nil];
         [g_context setView:g_view];
#else
         if (g_use_hw_ctx)
            g_hw_ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
         g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
         g_view.context = g_context;
#endif

         [g_context makeCurrentContext];
         break;
      }
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         RARCH_LOG("[macOS]: Native window size: %u x %u.\n", cocoa_ctx->width, cocoa_ctx->height);
         if (!vulkan_surface_create(&cocoa_ctx->vk,
                  VULKAN_WSI_MVK_MACOS, NULL,
                  (BRIDGE void *)g_view, cocoa_ctx->width, cocoa_ctx->height,
                  cocoa_ctx->swap_interval))
         {
            RARCH_ERR("[macOS]: Failed to create surface.\n");
            return false;
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

#if defined(HAVE_COCOA_METAL)
   static bool has_went_fullscreen = false;
   /* TODO: Screen mode support. */

   if (fullscreen)
   {
      if (!has_went_fullscreen)
      {
         [g_view enterFullScreenMode:(BRIDGE NSScreen *)get_chosen_screen() withOptions:nil];
         cocoagl_gfx_ctx_show_mouse(data, false);
      }
   }
   else
   {
      if (has_went_fullscreen)
      {
         [g_view exitFullScreenModeWithOptions:nil];
         [[g_view window] makeFirstResponder:g_view];
         cocoagl_gfx_ctx_show_mouse(data, true);
      }

      [[g_view window] setContentSize:NSMakeSize(width, height)];
   }

   has_went_fullscreen = fullscreen;
#endif

   /* TODO: Maybe iOS users should be able to show/hide the status bar here? */

   return true;
}

#ifdef HAVE_VULKAN
static void *cocoagl_gfx_ctx_get_context_data(void *data)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   return &cocoa_ctx->vk.context;
}
#endif

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
