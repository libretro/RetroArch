/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation,
 *  either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Vulkan context driver for a headset's own output on Android: OpenXR
 * runs in the video frame through vulkan_openxr, and the window is
 * images nothing presents, shown in the headset as the desktop shows
 * its window's. */

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <jni.h>
#include <android/native_activity.h>
#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#ifdef HAVE_OPENGLES
#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif
#endif
#include "../common/vulkan_common.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "../../frontend/drivers/platform_unix.h"
#include "../../verbosity.h"
#include "../common/vulkan_openxr.h"
#include "../video_driver.h"

/* As many as a swapchain would give. */
#define ANDROID_VK_OPENXR_IMAGES 3

typedef struct android_vk_openxr
{
   gfx_ctx_vulkan_data_t vk;
   struct android_app *android_app;
   XrLoaderInitInfoAndroidKHR loader;
   XrInstanceCreateInfoAndroidKHR instance_info;
   VkImage images[ANDROID_VK_OPENXR_IMAGES];
   VkDeviceMemory memory[ANDROID_VK_OPENXR_IMAGES];
   unsigned dims;
   bool ended;
} android_vk_openxr_t;

static android_vk_openxr_t android_vk_openxr_ctx;

/* Exit or loss: the headset was all there was. */
static void android_vk_openxr_ended(void *user)
{
   ((android_vk_openxr_t*)user)->ended = true;
}

static void android_vk_openxr_window_free(android_vk_openxr_t *xr)
{
   unsigned i;
   vulkan_context_t *ctx = &xr->vk.context;
   if (!ctx->device)
      return;
   /* The driver's frames are the only users of the window. The XR
    * thread still runs and submits here, so not a device drain. */
   vulkan_context_finish_frames(&xr->vk);
   for (i = 0; i < ANDROID_VK_OPENXR_IMAGES; i++)
   {
      if (xr->images[i])
         vkDestroyImage(ctx->device, xr->images[i], NULL);
      if (xr->memory[i])
         vkFreeMemory(ctx->device, xr->memory[i], NULL);
      xr->images[i]            = VK_NULL_HANDLE;
      xr->memory[i]            = VK_NULL_HANDLE;
      ctx->swapchain_images[i] = VK_NULL_HANDLE;
   }
}

/* The window's next image, as an acquire gives one: the slot's last
 * frame has finished and its fence is reset. */
static void android_vk_openxr_next_image(android_vk_openxr_t *xr)
{
   vulkan_context_advance_frame(&xr->vk);
   xr->vk.context.current_swapchain_index =
      xr->vk.context.current_frame_index;
}

/* The window: images the driver draws its frame and UI in, as it
 * would a swapchain's, which nothing presents. Its format is one the
 * headset takes, or whose sRGB twin it takes. */
static bool android_vk_openxr_window_create(android_vk_openxr_t *xr)
{
   static const VkFormat formats[2] = {
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
   static const VkFormat srgb[2]    = {
      VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB };
   unsigned i, w = 0, h = 0;
   VkFormat format       = formats[0];
   vulkan_context_t *ctx = &xr->vk.context;

   for (i = 0; i < 2; i++)
      if (     vulkan_openxr_supports_format(ctx->xr, srgb[i])
            || vulkan_openxr_supports_format(ctx->xr, formats[i]))
      {
         format = formats[i];
         break;
      }
   vulkan_openxr_eye_size(ctx->xr, &w, &h);
   if (!w)
      w = 1280;
   /* 16:9 at an eye's width: the menu's screen and screenshots. */
   h = w * 9 / 16;

   for (i = 0; i < ANDROID_VK_OPENXR_IMAGES; i++)
   {
      VkImageCreateInfo ici;
      VkMemoryRequirements req;
      VkMemoryAllocateInfo mai;
      ici.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      ici.pNext                 = NULL;
      ici.flags                 = 0;
      ici.queueFamilyIndexCount = 0;
      ici.pQueueFamilyIndices   = NULL;
      ici.imageType             = VK_IMAGE_TYPE_2D;
      ici.format                = format;
      ici.extent.width          = w;
      ici.extent.height         = h;
      ici.extent.depth          = 1;
      ici.mipLevels             = 1;
      ici.arrayLayers           = 1;
      ici.samples               = VK_SAMPLE_COUNT_1_BIT;
      ici.tiling                = VK_IMAGE_TILING_OPTIMAL;
      ici.usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
         | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
         | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      ici.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
      ici.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
      if (vkCreateImage(ctx->device, &ici, NULL, &xr->images[i])
            != VK_SUCCESS)
         return false;
      vkGetImageMemoryRequirements(ctx->device, xr->images[i], &req);
      mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      mai.pNext           = NULL;
      mai.allocationSize  = req.size;
      mai.memoryTypeIndex = vulkan_find_memory_type_fallback(
            &ctx->memory_properties, req.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
      if (     vkAllocateMemory(ctx->device, &mai, NULL, &xr->memory[i])
            != VK_SUCCESS
            || vkBindImageMemory(ctx->device, xr->images[i],
               xr->memory[i], 0) != VK_SUCCESS)
         return false;
      ctx->swapchain_images[i] = xr->images[i];
   }
   ctx->num_swapchain_images = ANDROID_VK_OPENXR_IMAGES;
   ctx->swapchain_format     = format;
   ctx->swapchain_dims       = VIDEO_SCALE_PACK(w, h);
   ctx->flags               |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
   xr->dims                  = ctx->swapchain_dims;
   /* As a new swapchain's first acquire: every frame's slot has a
    * fence. */
   android_vk_openxr_next_image(xr);
   RARCH_LOG("[XR] The window is %ux%u, shown in the headset.\n", w, h);
   return true;
}

static void android_vk_openxr_teardown(android_vk_openxr_t *xr)
{
   android_vk_openxr_window_free(xr);
   /* OpenXR goes with the device. */
   vulkan_context_destroy(&xr->vk, true);
   vulkan_openxr_set_platform(NULL);
   memset(xr, 0, sizeof(*xr));
}

static void *android_vk_openxr_gfx_ctx_init(void *video_driver)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   vulkan_openxr_platform_t platform;

   /* Kept across a reinit, session and all. */
   if (xr->vk.context.xr && xr->vk.context.device != VK_NULL_HANDLE)
   {
      xr->vk.context.flags |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
      vulkan_openxr_host_register(xr->vk.context.xr);
      return xr;
   }

   memset(xr, 0, sizeof(*xr));
   if (!(xr->android_app = (struct android_app*)g_android))
      return NULL;

   xr->loader.type                       = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
   xr->loader.applicationVM              = xr->android_app->activity->vm;
   xr->loader.applicationContext         = xr->android_app->activity->clazz;
   xr->instance_info.type                = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR;
   xr->instance_info.applicationVM       = xr->android_app->activity->vm;
   xr->instance_info.applicationActivity = xr->android_app->activity->clazz;

   memset(&platform, 0, sizeof(platform));
   platform.runtime.loader        =
      (const XrLoaderInitInfoBaseHeaderKHR*)&xr->loader;
   platform.runtime.instance_next = &xr->instance_info;
   platform.runtime.instance_ext  =
      XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
   platform.ended                 = android_vk_openxr_ended;
   platform.user                  = xr;
   vulkan_openxr_set_platform(&platform);
   xr->vk.openxr_headset          = true;

   /* No runtime, headset or session: vk_android follows in the list. */
   if (     !vulkan_context_init(&xr->vk, VULKAN_WSI_ANDROID)
         || !xr->vk.context.xr
         || !vulkan_context_create_android_surface(&xr->vk,
            android_app_window(xr->android_app))
         || !vulkan_context_init_headless_device(&xr->vk)
         || !xr->vk.context.xr
         || !android_vk_openxr_window_create(xr))
   {
      android_vk_openxr_teardown(xr);
      return NULL;
   }
   return xr;
}

static enum gfx_ctx_api android_vk_openxr_gfx_ctx_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool android_vk_openxr_gfx_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return api == GFX_CTX_VULKAN_API;
}

static void android_vk_openxr_gfx_ctx_get_video_size(void *data, unsigned *dims)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   *dims = xr ? xr->dims : 0;
}

static bool android_vk_openxr_gfx_ctx_set_video_mode(void *data,
      unsigned dims, bool fullscreen)
{
   return true;
}

static bool android_vk_openxr_gfx_ctx_set_resize(void *data,
      unsigned dims)
{
   return true;
}

static void android_vk_openxr_gfx_ctx_set_flags(void *data, uint32_t flags)
{
}

static void android_vk_openxr_gfx_ctx_check_window(void *data,
      bool *quit, bool *resize, unsigned *dims)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   if (!xr)
      return;
   *quit   = xr->ended;
   *resize = false;
}

/* Nothing presents the window, but every submission on a slot is
 * followed by this, as by a desktop's acquire. */
static void android_vk_openxr_gfx_ctx_swap_buffers(void *data)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   if (xr)
      android_vk_openxr_next_image(xr);
}

static void android_vk_openxr_gfx_ctx_destroy(void *data)
{
   android_vk_openxr_t *xr        = (android_vk_openxr_t*)data;
   video_driver_state_t *video_st = video_state_get_ptr();

   if (!xr)
      return;
   if ((uint32_t)retro_atomic_load_relaxed_int(&video_st->flags)
         & VIDEO_FLAG_CACHE_CONTEXT)
   {
      RARCH_DBG("[XR] CACHE_CONTEXT set - preserving OpenXR/Vulkan context\n");
      return;
   }
   android_vk_openxr_teardown(xr);
}

static bool android_vk_openxr_gfx_ctx_has_focus(void *data)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   return xr && xr->vk.context.xr && vulkan_openxr_focused(xr->vk.context.xr);
}

static bool android_vk_openxr_gfx_ctx_suppress_screensaver(void *data, bool enable)
{
   return false; /* not meaningful under a VR compositor */
}

static void android_vk_openxr_gfx_ctx_input_driver(void *data,
      const char *joypad_name)
{
   input_driver_video_window(INPUT_WINDOW_PLATFORM, NULL);
}

static gfx_ctx_proc_t android_vk_openxr_gfx_ctx_get_proc_address(const char *symbol)
{
   return NULL; /* Vulkan uses vkGetInstanceProcAddr/vkGetDeviceProcAddr, not this */
}

static void android_vk_openxr_gfx_ctx_show_mouse(void *data, bool state) { }

static uint32_t android_vk_openxr_gfx_ctx_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_HEADSET_INLINE);
   return flags;
}

static void *android_vk_openxr_gfx_ctx_get_context_data(void *data)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   return xr ? &xr->vk.context : NULL;
}

static void android_vk_openxr_gfx_ctx_release_current(void *data)
{
}

const gfx_ctx_driver_t gfx_ctx_android_vk_openxr = {
   android_vk_openxr_gfx_ctx_init,
   android_vk_openxr_gfx_ctx_destroy,
   android_vk_openxr_gfx_ctx_get_api,
   android_vk_openxr_gfx_ctx_bind_api,
   NULL, /* swap_interval: the headset paces the frame */
   android_vk_openxr_gfx_ctx_set_video_mode,
   android_vk_openxr_gfx_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   NULL, /* update_window_title */
   android_vk_openxr_gfx_ctx_check_window,
   android_vk_openxr_gfx_ctx_set_resize,
   android_vk_openxr_gfx_ctx_has_focus,
   android_vk_openxr_gfx_ctx_suppress_screensaver,
   true, /* has_windowed: XR presentation, no Android window */
   android_vk_openxr_gfx_ctx_swap_buffers,
   android_vk_openxr_gfx_ctx_input_driver,
   android_vk_openxr_gfx_ctx_get_proc_address,
   NULL, /* image_buffer_init */
   NULL, /* image_buffer_write */
   android_vk_openxr_gfx_ctx_show_mouse,
   "android_vk_openxr",
   android_vk_openxr_gfx_ctx_get_flags,
   android_vk_openxr_gfx_ctx_set_flags,
   NULL, /* bind_hw_render */
   android_vk_openxr_gfx_ctx_get_context_data,
   NULL, /* make_current */
   NULL, /* create_surface */
   NULL, /* destroy_surface */
   NULL, /* presentable */
   NULL, /* last_present_time */
   android_vk_openxr_gfx_ctx_release_current
};
