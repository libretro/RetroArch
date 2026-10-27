/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <retro_assert.h>
#include <features/features_cpu.h>
#include <dynamic/dylib.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <retro_math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "vulkan_common.h"
#if defined(ANDROID) && defined(HAVE_THREADS)
#include "../../record/record_android_capture_device.h"
#endif
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
#include "vulkan_android_device.h"
#include "vulkan_android_cache.h"
#endif
#ifdef HAVE_TEMPORAL_UPSCALER
#include "vulkan_upscaler.h"
#include "../video_upscaler.h"
#endif
#if defined(HAVE_RMP4) || defined(HAVE_RWEBM)
#include "vulkan_video_decode.h"
#endif
#include "vulkan_video_caps.h"
#include "vulkan_memory_type.h"
#include "vulkan_surface_format.h"
#include "vulkan_swapchain_extent.h"
#include "../include/vulkan/vulkan.h"
#include "vksym.h"
#ifdef HAVE_OPENXR
#include "vulkan_openxr.h"
#endif
#include <libretro_vulkan.h>

#ifdef HAVE_SDL3
/* Must come after the Vulkan headers so SDL_vulkan.h picks up the
 * real Vk* types instead of forward-declaring its own. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#endif

#include "../../verbosity.h"
#include "../../configuration.h"

#ifdef _WIN32
#include <windows.h>

#include <compat/strl.h>
#endif

#ifdef HAVE_OPENXR
#include <openxr/openxr_platform.h>
#endif

#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_NV 0x10DE
#define VENDOR_ID_INTEL 0x8086

#ifdef __APPLE__
#if VK_HEADER_VERSION < 216
/* Portability enumeration extension for MoltenVK through Vulkan loader.
 * These may not be defined in older Vulkan headers. */
#ifndef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#define VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME "VK_KHR_portability_enumeration"
#endif
#ifndef VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
#define VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR 0x00000001
#endif
#endif
#endif

#if defined(_WIN32) || defined(__APPLE__) || (defined(__linux__) && !defined(ANDROID))
#define VULKAN_EMULATE_MAILBOX
#endif

/* TODO/FIXME - static globals
 * WARNING: These globals prevent safe use of multiple concurrent
 * Vulkan contexts (multi-window, etc.). The cached_device_vk
 * mechanism assumes single-context ownership. */
static dylib_t                       vulkan_library;
static VkInstance                    cached_instance_vk;
#if defined(_WIN32) && !defined(__WINRT__)
#include "win_vulkan_dxgi.h"
#endif

static VkDevice                      cached_device_vk;
static VkPhysicalDevice              cached_gpu_vk;
static int                           cached_gpu_index_vk;
static retro_vulkan_destroy_device_t cached_destroy_device_vk;
static bool                          cached_instance_hdr_vk;
static bool                          cached_timeline_vk;
/* The video decode queues a cached device was made with */
static uint32_t                      cached_video_vk[6];
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
static bool cached_android_images_vk, cached_android_legacy_vk;
static unsigned cached_android_descriptors_vk;
#endif
#ifdef HAVE_OPENXR
/* Kept with a cached device: the runtime made it. */
static vulkan_openxr_t              *cached_xr;
#endif

#ifdef __APPLE__
/* On Apple platforms the Vulkan implementation is provided by MoltenVK
 * (loaded dynamically, either directly or through the Vulkan loader).
 * The version string is captured once, when the physical device is
 * selected, and cached here so that the System Information menu can
 * report it without needing a live Vulkan context. Empty until a
 * context has been brought up at least once. */
static char                          moltenvk_version_str[64];

const char *vulkan_get_moltenvk_version(void)
{
   return moltenvk_version_str;
}
#endif

#if 0
#define WSI_HARDENING_TEST
#endif

#ifdef WSI_HARDENING_TEST
static unsigned wsi_harden_counter         = 0;
static unsigned wsi_harden_counter2        = 0;

static void trigger_spurious_error_vkresult(VkResult *res)
{
   ++wsi_harden_counter;
   if ((wsi_harden_counter & 15) == 12)
      *res = VK_ERROR_OUT_OF_DATE_KHR;
   else if ((wsi_harden_counter & 31) == 13)
      *res = VK_ERROR_OUT_OF_DATE_KHR;
   else if ((wsi_harden_counter & 15) == 6)
      *res = VK_ERROR_SURFACE_LOST_KHR;
}

static bool trigger_spurious_error(void)
{
   ++wsi_harden_counter2;
   return ((wsi_harden_counter2 & 15) == 9) || ((wsi_harden_counter2 & 15) == 10);
}
#endif

#ifdef VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_cb(
      VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
      VkDebugUtilsMessageTypeFlagsEXT msg_type,
      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
      void *pUserData)
{
   const char *severity = "";
   const char *type     = "";

   switch (msg_severity)
   {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
         severity = "ERROR";
         break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
         severity = "WARNING";
         break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
         severity = "INFO";
         break;
      default:
         severity = "VERBOSE";
         break;
   }

   switch (msg_type)
   {
      case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
         type = "Validation";
         break;
      case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
         type = "Performance";
         break;
      default:
         type = "General";
         break;
   }

   switch (msg_severity)
   {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
         RARCH_ERR("[Vulkan] %s %s: %s.\n", severity, type, pCallbackData->pMessage);
         break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
         RARCH_WARN("[Vulkan] %s %s: %s.\n", severity, type, pCallbackData->pMessage);
         break;
      default:
         RARCH_LOG("[Vulkan] %s %s: %s.\n", severity, type, pCallbackData->pMessage);
         break;
   }
   return VK_FALSE;
}
#endif

/* What the mailbox waits on is a swapchain image coming free, which the
 * display paces, so its bound is in refresh periods. A finite one is
 * also what lets the thread check the DEAD flag and exit: sthread_join
 * cannot interrupt an acquire, so this is what teardown costs, and
 * teardown is on the fast-forward release edge. Floored and capped so a
 * missing or absurd reported rate still leaves a usable bound. */
#define VULKAN_MAILBOX_ACQUIRE_FRAMES   8
#define VULKAN_MAILBOX_TIMEOUT_MIN_US   16000
#define VULKAN_MAILBOX_TIMEOUT_MAX_US   500000

static int64_t vulkan_mailbox_timeout_us(void)
{
   float  hz = video_driver_get_refresh_rate();
   double us = (hz > 1.0f)
      ? (double)VULKAN_MAILBOX_ACQUIRE_FRAMES * 1000000.0 / (double)hz
      : (double)VULKAN_MAILBOX_TIMEOUT_MAX_US;
   if (us < (double)VULKAN_MAILBOX_TIMEOUT_MIN_US)
      return VULKAN_MAILBOX_TIMEOUT_MIN_US;
   if (us > (double)VULKAN_MAILBOX_TIMEOUT_MAX_US)
      return VULKAN_MAILBOX_TIMEOUT_MAX_US;
   return (int64_t)us;
}

static void vulkan_emulated_mailbox_deinit(
      struct vulkan_emulated_mailbox *mailbox)
{
   if (mailbox->thread)
   {
      retro_atomic_store_release_int(&mailbox->dead, 1);
      retro_eventcount_notify(&mailbox->work);
      /* Wait for the background thread to see the dead flag.
       * Its acquire and fence waits are finite, so it will
       * unblock and exit the loop. */
      sthread_join(mailbox->thread);
   }

   /* Safe on ones never brought up: the struct starts zeroed. */
   retro_eventcount_free(&mailbox->answered);
   retro_eventcount_free(&mailbox->work);

   memset(mailbox, 0, sizeof(*mailbox));
}

/* The presenting thread. Asks for an image unless a request is already
 * out. */
static void vulkan_emulated_mailbox_request(
      struct vulkan_emulated_mailbox *mailbox)
{
   if (!mailbox->has_pending_request)
   {
      retro_atomic_store_release_int(&mailbox->request, 1);
      retro_eventcount_notify(&mailbox->work);
   }
   mailbox->has_pending_request = true;
}

static VkResult vulkan_emulated_mailbox_acquire_next_image(
      struct vulkan_emulated_mailbox *mailbox,
      unsigned *index)
{
   VkResult res                    = VK_TIMEOUT;

   vulkan_emulated_mailbox_request(mailbox);

   if (retro_atomic_load_acquire_int(&mailbox->acquired))
   {
      res                          = mailbox->result;
      *index                       = mailbox->index;
      mailbox->has_pending_request = false;
      retro_atomic_store_release_int(&mailbox->acquired, 0);
   }

   return res;
}

static VkResult vulkan_emulated_mailbox_acquire_next_image_blocking(
      struct vulkan_emulated_mailbox *mailbox,
      unsigned *index)
{
   VkResult res = VK_SUCCESS;
   retro_time_t deadline;

   vulkan_emulated_mailbox_request(mailbox);

   /* One deadline for the whole wait, not one per iteration: a wake
    * that is not the answer would otherwise re-arm the full timeout,
    * and enough of them would be a bound on nothing. */
   deadline = cpu_features_get_time_usec() + mailbox->timeout_us;
   for (;;)
   {
      retro_time_t now;
      int key = retro_eventcount_prepare_wait(&mailbox->answered);

      if (retro_atomic_load_acquire_int(&mailbox->acquired))
      {
         retro_eventcount_cancel_wait(&mailbox->answered);
         break;
      }
      /* A finite wait also covers a background thread still
       * waiting on its acquire or its fence. */
      now = cpu_features_get_time_usec();
      if (now >= deadline)
      {
         retro_eventcount_cancel_wait(&mailbox->answered);
         return VK_TIMEOUT;
      }
      retro_eventcount_commit_wait_timeout(&mailbox->answered, key,
            (int64_t)(deadline - now));
   }

   if ((res = mailbox->result) == VK_SUCCESS)
      *index                    = mailbox->index;
   mailbox->has_pending_request = false;
   retro_atomic_store_release_int(&mailbox->acquired, 0);

   return res;
}

/* The acquire thread: an answer is ready. result and index are
 * written before this. */
static void vulkan_emulated_mailbox_answer(
      struct vulkan_emulated_mailbox *mailbox)
{
   retro_atomic_store_release_int(&mailbox->acquired, 1);
   retro_eventcount_notify(&mailbox->answered);
}

static void vulkan_emulated_mailbox_loop(void *userdata)
{
   VkFence fence;
   VkFenceCreateInfo info;
   struct vulkan_emulated_mailbox *mailbox =
      (struct vulkan_emulated_mailbox*)userdata;

   if (!mailbox)
      return;

   info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   info.pNext             = NULL;
   info.flags             = 0;

   vkCreateFence(mailbox->device, &info, NULL, &fence);

   for (;;)
   {
      /* Sleep until there is a request, or the teardown. The second
       * look, between announcing the wait and committing to it, is
       * what keeps a request made in that window from being slept
       * through. */
      for (;;)
      {
         int key = retro_eventcount_prepare_wait(&mailbox->work);
         if (     retro_atomic_load_acquire_int(&mailbox->dead)
               || retro_atomic_load_acquire_int(&mailbox->request))
         {
            retro_eventcount_cancel_wait(&mailbox->work);
            break;
         }
         retro_eventcount_commit_wait(&mailbox->work, key);
      }

      if (retro_atomic_load_acquire_int(&mailbox->dead))
         break;

      retro_atomic_store_release_int(&mailbox->request, 0);

      /* Use a finite timeout so the thread can regularly check
       * for the dead flag and exit promptly during teardown.
       * UINT64_MAX would block forever, causing sthread_join
       * in vulkan_emulated_mailbox_deinit to deadlock. */
      mailbox->result          = vkAcquireNextImageKHR(
            mailbox->device, mailbox->swapchain,
            (uint64_t)mailbox->timeout_us * 1000,
            VK_NULL_HANDLE, fence, &mailbox->index);

      /* VK_SUBOPTIMAL_KHR can be returned on Android 10
       * when prerotate is not dealt with.
       * It can also be returned by WSI when the surface
       * is _temporarily_ suboptimal.
       * This is not an error we need to care about,
       * and we'll treat it as SUCCESS. */
      if (mailbox->result == VK_SUBOPTIMAL_KHR)
         mailbox->result = VK_SUCCESS;

      if (mailbox->result == VK_SUCCESS)
      {
         /* The image is already ours; VK_TIMEOUT only means the
          * presentation engine has not released it yet. It cannot be
          * handed back, and acquiring again with this fence pending is
          * invalid, so keep waiting, checking dead between waits. */
         bool dead = false;
         VkResult wait_res;

         while ((wait_res = vkWaitForFences(mailbox->device, 1, &fence, true,
                  (uint64_t)mailbox->timeout_us * 1000)) == VK_TIMEOUT)
         {
            dead = retro_atomic_load_acquire_int(&mailbox->dead) != 0;
            if (dead)
               break;
#ifdef __APPLE__
            /* See vulkan_acquire_next_image(). */
            break;
#endif
         }

         if (dead)
         {
            /* A pending fence must not be destroyed, but a driver
             * that never signals must not hang the join either. */
            if (vkWaitForFences(mailbox->device, 1, &fence, true,
                     (uint64_t)mailbox->timeout_us * 1000) == VK_TIMEOUT)
               RARCH_ERR("[Vulkan] Mailbox acquire fence still pending at teardown.\n");
            break;
         }

#ifdef __APPLE__
         /* The image may never come back: have the swapchain rebuilt,
          * on a new fence, as the old image may still signal this one. */
         if (wait_res == VK_TIMEOUT)
         {
            vkDestroyFence(mailbox->device, fence, NULL);
            vkCreateFence(mailbox->device, &info, NULL, &fence);
            mailbox->result = VK_ERROR_OUT_OF_DATE_KHR;
            vulkan_emulated_mailbox_answer(mailbox);
            continue;
         }
#endif

         /* A lost device reaches the main thread as an error, not an image. */
         mailbox->result = wait_res;
         vkResetFences(mailbox->device, 1, &fence);

         vulkan_emulated_mailbox_answer(mailbox);
      }
      else if (   mailbox->result == VK_TIMEOUT
               || mailbox->result == VK_NOT_READY)
      {
         /* No image available this round.
          * Check dead, re-arm the request the loop cleared,
          * then loop back to try again. */
         if (retro_atomic_load_acquire_int(&mailbox->dead))
            break;
         retro_atomic_store_release_int(&mailbox->request, 1);
      }
      else
      {
         /* VK_ERROR_OUT_OF_DATE_KHR, VK_ERROR_DEVICE_LOST, etc.
          * Propagate to the main thread via acquired + result.
          * The caller (non-blocking acquire) will return this error. */
         vkResetFences(mailbox->device, 1, &fence);
         vulkan_emulated_mailbox_answer(mailbox);
      }
   }

   vkDestroyFence(mailbox->device, fence, NULL);
}

static bool vulkan_emulated_mailbox_init(
      struct vulkan_emulated_mailbox *mailbox,
      VkDevice device,
      VkSwapchainKHR swapchain)
{
   /* Zeroed first, so that the error path can free an eventcount that
    * was never brought up. */
   memset(mailbox, 0, sizeof(*mailbox));
   mailbox->device              = device;
   mailbox->swapchain           = swapchain;
   mailbox->timeout_us          = vulkan_mailbox_timeout_us();
   mailbox->result              = VK_SUCCESS;
   retro_atomic_int_init(&mailbox->request, 0);
   retro_atomic_int_init(&mailbox->acquired, 0);
   retro_atomic_int_init(&mailbox->dead, 0);

   if (!retro_eventcount_init(&mailbox->work))
      goto error;
   if (!retro_eventcount_init(&mailbox->answered))
      goto error;
   if (!(mailbox->thread    = sthread_create(vulkan_emulated_mailbox_loop,
               mailbox)))
      goto error;
   return true;

error:
   /* Tear down anything we managed to allocate before failing.
    * vulkan_emulated_mailbox_deinit() is null-safe and ends with
    * a memset, so the struct is left in the same shape a caller
    * would see after a successful init+deinit cycle -- callers
    * that ignore our return value (the two sites in
    * vulkan_create_swapchain) will then take the
    * mailbox.swapchain == VK_NULL_HANDLE branch in
    * vulkan_acquire_next_image and skip the emulated path
    * cleanly instead of using a mailbox that never came up. */
   vulkan_emulated_mailbox_deinit(mailbox);
   return false;
}

static void vulkan_debug_mark_object(VkDevice device,
      VkObjectType object_type, uint64_t object_handle, const char *name, unsigned count)
{
   if (vkSetDebugUtilsObjectNameEXT)
   {
      char merged_name[1024];
      VkDebugUtilsObjectNameInfoEXT info;
      /* strlcpy() returns the length of the SOURCE, not the number of
       * bytes copied, so a name longer than the buffer would put
       * merged_name + _len past the end and underflow the remaining
       * size to near SIZE_MAX.  Every caller in tree passes a short
       * literal, so this is not reachable today -- but the next one
       * need not. */
      size_t _len                        = strlcpy(merged_name, name, sizeof(merged_name));
      if (_len >= sizeof(merged_name))
         _len                            = sizeof(merged_name) - 1;
      snprintf(merged_name + _len, sizeof(merged_name) - _len, " (%u)", count);

      info.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      info.pNext                         = NULL;
      info.objectType                    = object_type;
      info.objectHandle                  = object_handle;
      info.pObjectName                   = merged_name;
      vkSetDebugUtilsObjectNameEXT(device, &info);
   }
}

static bool vulkan_load_instance_symbols(gfx_ctx_vulkan_data_t *vk)
{
   if (!vulkan_symbol_wrapper_load_core_instance_symbols(vk->context.instance))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkDestroySurfaceKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceSupportKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
   return true;
}

static bool vulkan_load_device_symbols(gfx_ctx_vulkan_data_t *vk)
{
   if (!vulkan_symbol_wrapper_load_core_device_symbols(vk->context.device))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkCreateSwapchainKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkDestroySwapchainKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkGetSwapchainImagesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkAcquireNextImageKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkQueuePresentKHR);
   return true;
}

static bool vulkan_find_extensions(const char * const *exts, unsigned num_exts,
      const VkExtensionProperties *properties, unsigned property_count)
{
   unsigned i, ext;
   bool found;
   for (ext = 0; ext < num_exts; ext++)
   {
      found = false;
      for (i = 0; i < property_count; i++)
      {
         if (string_is_equal(exts[ext], properties[i].extensionName))
         {
            found = true;
            break;
         }
      }

      if (!found)
         return false;
   }
   return true;
}

static bool vulkan_find_instance_extensions(
      const char **enabled, unsigned *inout_enabled_count,
      const char **exts, unsigned num_exts,
      const char **optional_exts, unsigned num_optional_exts)
{
   uint32_t property_count;
   unsigned i;
   unsigned count                    = *inout_enabled_count;
   bool ret                          = true;
   VkExtensionProperties *properties = NULL;

   if (vkEnumerateInstanceExtensionProperties(NULL, &property_count, NULL) != VK_SUCCESS)
      return false;

   if (!(properties = (VkExtensionProperties*)malloc(property_count *
               sizeof(*properties))))
   {
      ret = false;
      goto end;
   }

   if (vkEnumerateInstanceExtensionProperties(NULL, &property_count, properties) != VK_SUCCESS)
   {
      ret = false;
      goto end;
   }

   if (!vulkan_find_extensions(exts, num_exts, properties, property_count))
   {
      RARCH_ERR("[Vulkan] Could not find required instance extensions. Will attempt without them.\n");
      ret = false;
      goto end;
   }

   memcpy((void*)(enabled + count), exts, num_exts * sizeof(*exts));
   count += num_exts;


   for (i = 0; i < num_exts; i++)
   {
      if (vulkan_find_extensions(&exts[i], 1, properties, property_count))
         RARCH_DBG("[Vulkan] Instance extension supported: %s.\n", exts[i]);
      else
         RARCH_DBG("[Vulkan] Instance extension NOT supported: %s.\n", exts[i]);
   }

   for (i = 0; i < num_optional_exts; i++)
      if (vulkan_find_extensions(&optional_exts[i], 1, properties, property_count))
         enabled[count++] = optional_exts[i];

end:
   free(properties);
   *inout_enabled_count = count;
   return ret;
}

static bool vulkan_find_device_extensions(VkPhysicalDevice gpu,
      const char **enabled, unsigned *inout_enabled_count,
      const char **exts, unsigned num_exts,
      const char **optional_exts, unsigned num_optional_exts)
{
   uint32_t property_count;
   unsigned i;
   unsigned count                    = *inout_enabled_count;
   bool ret                          = true;
   VkExtensionProperties *properties = NULL;

   if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &property_count, NULL) != VK_SUCCESS)
      return false;

   if (!(properties = (VkExtensionProperties*)malloc(property_count *
               sizeof(*properties))))
   {
      ret = false;
      goto end;
   }

   if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &property_count, properties) != VK_SUCCESS)
   {
      ret = false;
      goto end;
   }

   if (!vulkan_find_extensions(exts, num_exts, properties, property_count))
   {
      RARCH_ERR("[Vulkan] Could not find device extension. Will attempt without it.\n");
      ret = false;
      goto end;
   }

   /* Required extensions: presence already validated by the
    * vulkan_find_extensions() check above. Append in one shot. */
   memcpy((void*)(enabled + count), exts, num_exts * sizeof(*exts));
   for (i = 0; i < num_exts; i++)
      RARCH_DBG("[Vulkan] Device extension supported: %s.\n", exts[i]);
   count += num_exts;

   for (i = 0; i < num_optional_exts; i++)
   {
      if (vulkan_find_extensions(&optional_exts[i], 1, properties, property_count))
      {
         RARCH_DBG("[Vulkan] Optional device extension supported: %s.\n", optional_exts[i]);
         enabled[count++] = optional_exts[i];
      }
      else
      {
         RARCH_DBG("[Vulkan] Optional device extension NOT supported: %s.\n", optional_exts[i]);
      }
   }

end:
   free(properties);
   *inout_enabled_count = count;
   return ret;
}

#ifdef HAVE_OPENXR
/* Appends name unless the list has it. */
static void vulkan_append_ext(const char **list, uint32_t *count,
      uint32_t cap, const char *name)
{
   uint32_t i;
   for (i = 0; i < *count; i++)
      if (string_is_equal(list[i], name))
         return;
   if (*count < cap)
      list[(*count)++] = name;
}

/* Whether a queue family init_device() can use presents to the window. */
static bool vulkan_context_gpu_presents(gfx_ctx_vulkan_data_t *vk,
      VkPhysicalDevice gpu)
{
   uint32_t i;
   uint32_t count                 = 0;
   bool found                     = false;
   VkQueueFamilyProperties *props = NULL;
   vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, NULL);
   if (!count || !(props = (VkQueueFamilyProperties*)
            malloc(count * sizeof(*props))))
      return false;
   vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, props);
   for (i = 0; i < count && !found; i++)
   {
      VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
      VkBool32 supported    = VK_FALSE;
      if ((props[i].queueFlags & required) != required)
         continue;
      vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, vk->vk_surface,
            &supported);
      found = (supported == VK_TRUE);
   }
   free(props);
   return found;
}

/* The headset's GPU, which must be one the loader lists and able to
 * present to the window; otherwise the headset is dropped. */
static bool vulkan_context_openxr_gpu(gfx_ctx_vulkan_data_t *vk,
      const VkPhysicalDevice *gpus, uint32_t gpu_count)
{
   uint32_t i;
   VkPhysicalDevice gpu = vulkan_openxr_gpu(vk->context.xr,
         vk->context.instance);
   for (i = 0; i < gpu_count; i++)
   {
      if (gpus[i] != gpu)
         continue;
      /* A display (KMS) surface is made after this first pick; the
       * pick runs again in init_device() once it exists. */
      if (     vk->vk_surface != VK_NULL_HANDLE
            && !vulkan_context_gpu_presents(vk, gpu))
      {
         RARCH_WARN("[OpenXR] The runtime's GPU #%u cannot present to the window.\n",
               (unsigned)i);
         break;
      }
      RARCH_LOG("[Vulkan] Using the headset's GPU #%u: \"%s\".\n",
            (unsigned)i, vk->gpu_list->elems[i].data);
      vk->context.gpu       = gpu;
      /* The runtime's pick, not GPU Index's: no index to set back. */
      vk->context.gpu_index = 0;
      return true;
   }
   if (gpu != VK_NULL_HANDLE && i == gpu_count)
      RARCH_WARN("[OpenXR] The runtime's GPU is not one Vulkan lists.\n");
   vulkan_openxr_drop(vk->context.xr);
   vk->context.xr = NULL;
   return false;
}
#endif

static bool vulkan_context_init_gpu(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;
   uint32_t gpu_count               = 0;
   VkPhysicalDevice *gpus           = NULL;
   union string_list_elem_attr attr = {0};
   settings_t *settings             = config_get_ptr();
   int gpu_index                    = settings->ints.vulkan_gpu_index;

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, NULL) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan] Failed to enumerate physical devices.\n");
      return false;
   }

   if (!(gpus = (VkPhysicalDevice*)calloc(gpu_count, sizeof(*gpus))))
   {
      RARCH_ERR("[Vulkan] Failed to enumerate physical devices.\n");
      return false;
   }

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, gpus) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan] Failed to enumerate physical devices.\n");
      free(gpus);
      return false;
   }

   if (gpu_count < 1)
   {
      RARCH_ERR("[Vulkan] Failed to enumerate Vulkan physical device.\n");
      free(gpus);
      return false;
   }

   if (vk->gpu_list)
      string_list_free(vk->gpu_list);

   vk->gpu_list = string_list_new();

   for (i = 0; i < gpu_count; i++)
   {
      VkPhysicalDeviceProperties gpu_properties;

      vkGetPhysicalDeviceProperties(gpus[i],
            &gpu_properties);

      RARCH_LOG("[Vulkan] Found GPU #%d: \"%s\".\n", i, gpu_properties.deviceName);

      string_list_append(vk->gpu_list, gpu_properties.deviceName, attr);
   }

   video_driver_set_gpu_api_devices(GFX_CTX_VULKAN_API, vk->gpu_list);

   /* A cached device is reused as is, so it keeps the GPU it was made on. */
   if (cached_device_vk && cached_gpu_vk)
   {
      vk->context.gpu       = cached_gpu_vk;
      vk->context.gpu_index = cached_gpu_index_vk;
      free(gpus);
      return true;
   }

#ifdef HAVE_OPENXR
   if (vk->context.xr && vulkan_context_openxr_gpu(vk, gpus, gpu_count))
   {
      free(gpus);
      return true;
   }
#endif

   /* The device the index was chosen as, wherever the list now puts it */
   gpu_index = video_driver_gpu_index_resolve(GFX_CTX_VULKAN_API,
         gpu_index, vk->gpu_list);

   if (0 <= gpu_index && gpu_index < (int)gpu_count)
   {
      RARCH_LOG("[Vulkan] Using GPU #%d: \"%s\".\n", gpu_index, vk->gpu_list->elems[gpu_index].data);
      vk->context.gpu       = gpus[gpu_index];
      vk->context.gpu_index = gpu_index;
   }
   else
   {
      RARCH_WARN("[Vulkan] Invalid GPU index %d, using first device found.\n", gpu_index);
      vk->context.gpu = gpus[0];
   }

   free(gpus);

#ifdef __APPLE__
   /* Capture the MoltenVK version from the selected physical device.
    * MoltenVK encodes its version into VkPhysicalDeviceProperties's
    * driverVersion field as a decimal (major * 10000 + minor * 100 +
    * patch) and derives the string it logs to the console the exact same
    * way, so decode it identically here. This uses only core Vulkan 1.0
    * data that is always populated; the legacy vkGetVersionStringsMVK
    * entry point is no longer vended through the Vulkan loader, and the
    * VK_KHR_driver_properties driverInfo string is not reliably filled
    * for a standalone query. */
   if (!moltenvk_version_str[0] && vk->context.gpu)
   {
      VkPhysicalDeviceProperties props;
      unsigned dv, major, minor, patch;
      vkGetPhysicalDeviceProperties(vk->context.gpu, &props);
      dv    = (unsigned)props.driverVersion;
      major = dv / 10000;
      minor = (dv % 10000) / 100;
      patch = dv % 100;
      snprintf(moltenvk_version_str, sizeof(moltenvk_version_str),
            "%u.%u.%u", major, minor, patch);
      RARCH_LOG("[Vulkan] MoltenVK version: %s.\n", moltenvk_version_str);
   }
#endif

   return true;
}

static const char *vulkan_device_extensions[]  = {
   "VK_KHR_swapchain"
};

static const char *vulkan_optional_device_extensions[] = {
   "VK_KHR_sampler_mirror_clamp_to_edge",
   "VK_EXT_full_screen_exclusive",
   "VK_KHR_portability_subset",
   /* Display timestamps for the presenter's repeat cadence; absent on
    * most Windows drivers, present on Android and Mesa. */
   "VK_GOOGLE_display_timing",
   /* What imported frames (a hardware decoder's) synchronise with */
   "VK_KHR_timeline_semaphore",
   VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
   /* The low-latency release (NVIDIA Reflex); present_id is what the
    * extension requires beside the timeline semaphore above. Must
    * stay ahead of the Win32 entries vulkan_optional_device_count()
    * counts off the end. */
   VK_KHR_PRESENT_ID_EXTENSION_NAME,
   VK_NV_LOW_LATENCY_2_EXTENSION_NAME,
   VK_AMD_ANTI_LAG_EXTENSION_NAME
#ifdef VULKAN_HDR_SWAPCHAIN
   /* Lets the app signal SMPTE-2086 mastering-display metadata to the
    * compositor via vkSetHdrMetadataEXT. Optional: if absent (common on
    * older NVIDIA Linux and pre-25.1 Mesa) the metadata call is skipped and
    * HDR still works via the colour space alone. */
   , "VK_EXT_hdr_metadata"
#endif
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
   , "VK_ANDROID_external_memory_android_hardware_buffer"
   , "VK_EXT_queue_family_foreign"
   , "VK_KHR_external_semaphore_fd"
   , "VK_KHR_external_memory"
   , "VK_KHR_external_semaphore"
   , "VK_KHR_sampler_ycbcr_conversion"
   , "VK_KHR_bind_memory2"
   , "VK_KHR_maintenance1"
   , "VK_KHR_get_memory_requirements2"
   , "VK_KHR_dedicated_allocation"
   , "VK_KHR_maintenance6"
#endif
#if defined(_WIN32) && !defined(__WINRT__)
   , "VK_KHR_external_memory"
   , "VK_KHR_external_memory_win32"
   , "VK_KHR_external_semaphore"
   , "VK_KHR_external_semaphore_win32"
   , "VK_KHR_dedicated_allocation"
   , "VK_KHR_get_memory_requirements2"
#endif
};


/* Hardware video decode: codec-aware decode queues, transfer queues to copy
 * the pictures out on, the extensions and the synchronization2 feature
 * they need. On a device of our own with timeline semaphores only. */
#define VULKAN_VIDEO_MAX_LANES 2

/* Copies of any even offset and size: a family that moves whole
 * blocks only (some DMA queues) cannot copy a cropped picture out. */
#define VULKAN_VIDEO_GRANULARITY_1(q) \
   (     (q).queueFamilyProperties.minImageTransferGranularity.width  == 1 \
      && (q).queueFamilyProperties.minImageTransferGranularity.height == 1 \
      && (q).queueFamilyProperties.minImageTransferGranularity.depth  == 1)

typedef struct vulkan_video_probe
{
   uint32_t decode_family;
   uint32_t xfer_family;
   uint32_t xfer_first;
   uint32_t lanes;
   uint32_t codecs;
} vulkan_video_probe_t;

static bool vulkan_video_probe(gfx_ctx_vulkan_data_t *vk,
      vulkan_video_probe_t *out, uint32_t allowed)
{
   PFN_vkGetPhysicalDeviceQueueFamilyProperties2 get_families;
   PFN_vkGetPhysicalDeviceFeatures2 get_features;
   VkPhysicalDeviceSynchronization2FeaturesKHR sync2;
   VkPhysicalDeviceVideoDecodeVP9FeaturesKHR vp9;
   VkPhysicalDeviceFeatures2 f2;
   VkQueueFamilyProperties2 *qp         = NULL;
   VkQueueFamilyVideoPropertiesKHR *vp  = NULL;
   VkExtensionProperties *props         = NULL;
   uint32_t inst_version                = VK_API_VERSION_1_0;
   uint32_t gfx                         = vk->context.graphics_queue_index;
   uint32_t n                           = 0;
   uint32_t nprops                      = 0;
   uint32_t dec_count                   = 0;
   uint32_t xfer_count                  = 0;
   uint32_t i;
   int dec                              = -1;
   int xfer                             = -1;
   bool ok                              = false;

   memset(out, 0, sizeof(*out));
   if (!(vk->context.flags & VK_CTX_FLAG_TIMELINE_SEMAPHORE))
      return false;
   if (vkEnumerateInstanceVersion)
      vkEnumerateInstanceVersion(&inst_version);
   if (     inst_version < VK_API_VERSION_1_1
         || vk->context.gpu_properties.apiVersion < VK_API_VERSION_1_1)
      return false;
   get_families = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)
      vulkan_symbol_wrapper_instance_proc_addr()(vk->context.instance,
            "vkGetPhysicalDeviceQueueFamilyProperties2");
   get_features = (PFN_vkGetPhysicalDeviceFeatures2)
      vulkan_symbol_wrapper_instance_proc_addr()(vk->context.instance,
            "vkGetPhysicalDeviceFeatures2");
   if (!get_families || !get_features)
      return false;

   if (     vkEnumerateDeviceExtensionProperties(vk->context.gpu, NULL,
               &nprops, NULL) != VK_SUCCESS
         || !nprops
         || !(props = (VkExtensionProperties*)malloc(
               nprops * sizeof(*props)))
         || vkEnumerateDeviceExtensionProperties(vk->context.gpu, NULL,
               &nprops, props) != VK_SUCCESS
         || !(out->codecs = vulkan_video_supported_codecs(props, nprops)))
      goto end;

   sync2.sType        =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
   vp9.sType          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_DECODE_VP9_FEATURES_KHR;
   vp9.pNext          = NULL;
   vp9.videoDecodeVP9 = VK_FALSE;
   out->codecs &= allowed;
#ifndef HAVE_RAV1
   out->codecs &= ~VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
#endif
#ifndef HAVE_RVP9
   out->codecs &= ~VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
#endif
   sync2.pNext            = (out->codecs & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR) ? &vp9 : NULL;
   sync2.synchronization2 = VK_FALSE;
   f2.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   f2.pNext               = &sync2;
   get_features(vk->context.gpu, &f2);
   if (!sync2.synchronization2)
      goto end;
   if (!vp9.videoDecodeVP9) out->codecs &= ~VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;

   get_families(vk->context.gpu, &n, NULL);
   if (     !n
         || !(qp = (VkQueueFamilyProperties2*)calloc(n, sizeof(*qp)))
         || !(vp = (VkQueueFamilyVideoPropertiesKHR*)calloc(n, sizeof(*vp))))
      goto end;
   for (i = 0; i < n; i++)
   {
      qp[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
      qp[i].pNext = &vp[i];
      vp[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
   }
   get_families(vk->context.gpu, &n, qp);

   /* a decode family of its own: the driver's queue is not to share */
   dec = vulkan_video_select_decode_family(qp, vp, n, gfx, out->codecs, &out->codecs);
   if (dec >= 0) dec_count = qp[dec].queueFamilyProperties.queueCount;
   if (dec < 0)
      goto end;

   /* a transfer family of its own, else a compute one, else a second
    * queue beside the driver's */
   for (i = 0; i < n && xfer < 0; i++)
   {
      VkQueueFlags f = qp[i].queueFamilyProperties.queueFlags;
      if (     i != (uint32_t)dec && i != gfx
            && (f & VK_QUEUE_TRANSFER_BIT)
            && !(f & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
            && VULKAN_VIDEO_GRANULARITY_1(qp[i])
            && qp[i].queueFamilyProperties.queueCount)
      {
         xfer       = (int)i;
         xfer_count = qp[i].queueFamilyProperties.queueCount;
      }
   }
   for (i = 0; i < n && xfer < 0; i++)
   {
      VkQueueFlags f = qp[i].queueFamilyProperties.queueFlags;
      if (     i != (uint32_t)dec && i != gfx
            && (f & VK_QUEUE_COMPUTE_BIT)
            && VULKAN_VIDEO_GRANULARITY_1(qp[i])
            && qp[i].queueFamilyProperties.queueCount)
      {
         xfer       = (int)i;
         xfer_count = qp[i].queueFamilyProperties.queueCount;
      }
   }
   if (xfer < 0 && gfx < n && qp[gfx].queueFamilyProperties.queueCount > 1)
   {
      xfer             = (int)gfx;
      xfer_count       = qp[gfx].queueFamilyProperties.queueCount - 1;
      out->xfer_first  = 1;
   }
   if (xfer < 0)
      goto end;

   out->decode_family = (uint32_t)dec;
   out->xfer_family   = (uint32_t)xfer;
   out->lanes         = dec_count < xfer_count ? dec_count : xfer_count;
   if (out->lanes > VULKAN_VIDEO_MAX_LANES)
      out->lanes = VULKAN_VIDEO_MAX_LANES;
   ok = true;

end:
   free(props);
   free(qp);
   free(vp);
   if (!ok)
      memset(out, 0, sizeof(*out));
   return ok;
}

static void vulkan_low_latency_note_exts(gfx_ctx_vulkan_data_t *vk,
      const char * const *exts, unsigned count);
static const void *vulkan_anti_lag_chain(gfx_ctx_vulkan_data_t *vk,
      VkPhysicalDevice gpu, VkPhysicalDeviceAntiLagFeaturesAMD *feat,
      const void *next);
static void vulkan_low_latency_device_init(gfx_ctx_vulkan_data_t *vk);
static void vulkan_low_latency_device_deinit(gfx_ctx_vulkan_data_t *vk);
#ifdef VULKAN_DXGI_PACING
static void vulkan_dxgi_pacing_deinit(gfx_ctx_vulkan_data_t *vk);
#endif
static void vulkan_low_latency_marker(gfx_ctx_vulkan_data_t *vk,
      uint64_t frame_id, unsigned marker);

static unsigned vulkan_optional_device_count(
      const gfx_ctx_vulkan_data_t *vk, bool own_device)
{
   unsigned count = ARRAY_SIZE(vulkan_optional_device_extensions);
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
   if (!own_device)
      while (count && vulkan_android_device_extension(
                        vulkan_optional_device_extensions[count - 1]))
         count--;
#else
   (void)own_device;
#endif
#if defined(_WIN32) && !defined(__WINRT__)
#if !defined(HAVE_RAV1) || (!defined(HAVE_D3D11) && !defined(HAVE_D3D12))
   if (!vk->context.swapchain_settings.dxgi_present)
      count -= 6;
#else
   (void)vk;
#endif
#else
   (void)vk;
#endif
   return count;
}

/* vkCreateDevice, or the runtime's, which adds what it needs. A device
 * the runtime refuses drops the headset, not the device. */
/* The temporal upscaler's device extensions, the ones @gpu has, when
 * the setting is on: a device made without them cannot run it */
#ifdef HAVE_TEMPORAL_UPSCALER
#define VULKAN_UPSCALER_EXTS VULKAN_UPSCALER_MAX_EXTS
static enum video_temporal_upscaler vulkan_upscaler_kind(char *dir, size_t len)
{
   enum video_temporal_upscaler kind = video_upscaler_wanted()
      ? (enum video_temporal_upscaler)config_get_ptr()->uints.video_temporal_upscaler
      : VIDEO_TEMPORAL_UPSCALER_OFF;
   video_upscaler_dir(kind, dir, len);
   return kind;
}

static unsigned vulkan_upscaler_device_exts(gfx_ctx_vulkan_data_t *vk,
      VkPhysicalDevice gpu, const char **list, unsigned count, unsigned cap)
{
   char dir[DIR_MAX_LENGTH];
   enum video_temporal_upscaler kind = vulkan_upscaler_kind(dir, sizeof(dir));
   if (!kind)
      return count;
   return vulkan_upscaler_device_extensions(kind, dir, gpu,
         vulkan_symbol_wrapper_instance_proc_addr(), vk->context.instance,
         list, count, cap);
}

/* The features the upscaler's library needs, chained ahead of a
 * device's @pnext */
static const void *vulkan_upscaler_features(gfx_ctx_vulkan_data_t *vk,
      VkPhysicalDevice gpu, const void *pnext)
{
   char dir[DIR_MAX_LENGTH];
   enum video_temporal_upscaler kind = vulkan_upscaler_kind(dir, sizeof(dir));
   if (!kind)
      return pnext;
   return vulkan_upscaler_device_features(kind, dir, gpu,
         vulkan_symbol_wrapper_instance_proc_addr(), vk->context.instance,
         pnext);
}
#else
#define VULKAN_UPSCALER_EXTS 0
#define vulkan_upscaler_device_exts(vk, gpu, list, count, cap) (count)
#define vulkan_upscaler_features(vk, gpu, pnext) (pnext)
#endif

static VkResult vulkan_context_create_device(gfx_ctx_vulkan_data_t *vk,
      VkPhysicalDevice gpu, const VkDeviceCreateInfo *info,
      VkDevice *device)
{
   unsigned i;
   const VkBaseInStructure *chain;
   VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unified;
   VkPhysicalDeviceFeatures2 features;
   PFN_vkGetPhysicalDeviceFeatures2 get;
   VkDeviceCreateInfo local         = *info;
   bool enabled                     = false;
   unified.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR;
   unified.pNext                    = NULL;
   unified.unifiedImageLayouts      = VK_FALSE;
   unified.unifiedImageLayoutsVideo = VK_FALSE;
   for (chain = (const VkBaseInStructure*)info->pNext; chain; chain = chain->pNext)
      if (chain->sType == unified.sType)
      {
         enabled = ((const VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR*)chain)->unifiedImageLayoutsVideo != 0;
         break;
      }
   if (!chain)
      for (i = 0; i < info->enabledExtensionCount; i++)
         if (!strcmp(info->ppEnabledExtensionNames[i], VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME))
         {
            get = (PFN_vkGetPhysicalDeviceFeatures2)
               vulkan_symbol_wrapper_instance_proc_addr()(vk->context.instance,
                     "vkGetPhysicalDeviceFeatures2");
            memset(&features, 0, sizeof(features));
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features.pNext = &unified;
            if (get)
               get(gpu, &features);
            if (unified.unifiedImageLayoutsVideo)
            {
               unified.pNext = (void*)local.pNext;
               local.pNext   = &unified;
               enabled       = true;
            }
            break;
         }
   local.pNext = vulkan_upscaler_features(vk, gpu, local.pNext);
   info = &local;
   vk->context.flags &= ~VK_CTX_FLAG_VIDEO_GENERAL;
   if (enabled)
      vk->context.flags |= VK_CTX_FLAG_VIDEO_GENERAL;
#ifdef HAVE_OPENXR
   if (vk->context.xr && vulkan_openxr_uses_enable2(vk->context.xr))
   {
      VkResult res = vulkan_openxr_create_device(vk->context.xr,
            vulkan_symbol_wrapper_instance_proc_addr(), gpu, info, device);
      if (res == VK_SUCCESS)
         return res;
      vulkan_openxr_drop(vk->context.xr);
      vk->context.xr = NULL;
   }
#endif
   return vkCreateDevice(gpu, info, NULL, device);
}

#ifdef HAVE_OPENXR
/* RetroArch's device extensions, and the runtime's for a core that
 * makes the device with create_device (v1). */
static void vulkan_context_openxr_device_exts(gfx_ctx_vulkan_data_t *vk,
      const char **list, uint32_t *count, uint32_t cap)
{
   unsigned i, n;
   const char *xr_exts[VULKAN_OPENXR_MAX_EXTS];
   for (i = 0; i < ARRAY_SIZE(vulkan_device_extensions); i++)
      vulkan_append_ext(list, count, cap, vulkan_device_extensions[i]);
   if (!vk->context.xr)
      return;
   n = vulkan_openxr_device_extensions(vk->context.xr, xr_exts,
         VULKAN_OPENXR_MAX_EXTS);
   for (i = 0; i < n; i++)
      vulkan_append_ext(list, count, cap, xr_exts[i]);
}
#endif

/* Whether a core device's chain asks for timeline semaphores, through
 * either struct that can; silent when neither is in it. */
static bool vulkan_chain_names_timeline(const void *pnext, bool *enabled)
{
   const VkBaseInStructure *chain;
   for (chain = (const VkBaseInStructure*)pnext; chain; chain = chain->pNext)
   {
      if (chain->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR)
      {
         *enabled = ((const VkPhysicalDeviceTimelineSemaphoreFeaturesKHR*)chain)->timelineSemaphore != 0;
         return true;
      }
      if (chain->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
      {
         *enabled = ((const VkPhysicalDeviceVulkan12Features*)chain)->timelineSemaphore != 0;
         return true;
      }
   }
   return false;
}

static VkDevice vulkan_context_create_device_wrapper(
      VkPhysicalDevice gpu, void *opaque,
      const VkDeviceCreateInfo *create_info)
{
   VkResult res;
   gfx_ctx_vulkan_data_t *vk      = (gfx_ctx_vulkan_data_t*)opaque;
   VkDeviceCreateInfo info        = *create_info;
   VkDevice device                = VK_NULL_HANDLE;
   VkPhysicalDeviceTimelineSemaphoreFeaturesKHR ll_timeline;
   VkPhysicalDeviceAntiLagFeaturesAMD anti_lag_features;
   bool timeline_named            = false;
   unsigned device_extensions_cap = info.enabledExtensionCount +
               ARRAY_SIZE(vulkan_device_extensions) +
               ARRAY_SIZE(vulkan_optional_device_extensions) +
               ARRAY_SIZE(vulkan_video_device_extensions) +
               VULKAN_UPSCALER_EXTS;
   const char **device_extensions = (const char **)malloc(
         device_extensions_cap * sizeof(const char *));

   memcpy((void*)device_extensions, info.ppEnabledExtensionNames, info.enabledExtensionCount * sizeof(const char *));
   info.ppEnabledExtensionNames = device_extensions;

   if (!(vulkan_find_device_extensions(gpu,
         device_extensions, &info.enabledExtensionCount,
         vulkan_device_extensions, ARRAY_SIZE(vulkan_device_extensions),
         vulkan_optional_device_extensions,
         vulkan_optional_device_count(vk, false))))
   {
      RARCH_ERR("[Vulkan] Could not find required device extensions.\n");
      free((void*)device_extensions);
      return VK_NULL_HANDLE;
   }

   vk->context.gpu = gpu;
   vkGetPhysicalDeviceProperties(gpu, &vk->context.gpu_properties);
   info.enabledExtensionCount = vulkan_upscaler_device_exts(vk, gpu,
         device_extensions, info.enabledExtensionCount, device_extensions_cap);
   vulkan_low_latency_note_exts(vk, device_extensions,
         info.enabledExtensionCount);
   /* Reserve video queues without sharing the core's graphics queue. */
   if (info.queueCreateInfoCount == 1)
   {
      VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline;
      VkPhysicalDeviceSynchronization2FeaturesKHR sync2;
      VkPhysicalDeviceVideoDecodeVP9FeaturesKHR vp9;
      VkPhysicalDeviceFeatures2 f2;
      const VkBaseInStructure *chain;
      const VkPhysicalDeviceTimelineSemaphoreFeaturesKHR *requested_timeline = NULL;
      const VkPhysicalDeviceSynchronization2FeaturesKHR *requested_sync2 = NULL;
      const VkPhysicalDeviceVideoDecodeVP9FeaturesKHR *requested_vp9 = NULL;
      PFN_vkGetPhysicalDeviceFeatures2 get_features;
      vulkan_video_probe_t video;
      VkDeviceCreateInfo vinfo = info;
      VkDeviceQueueCreateInfo qi[3];
      float priorities[3] = {1.0f, 1.0f, 1.0f};
      unsigned j, nq = 1, next = info.enabledExtensionCount;
      bool timeline_ext = false;
      for (j = 0; j < next; j++)
         if (!strcmp(device_extensions[j], VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
            timeline_ext = true;
      memset(&timeline, 0, sizeof(timeline));
      timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
      memset(&vp9, 0, sizeof(vp9));
      vp9.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_DECODE_VP9_FEATURES_KHR;
      memset(&sync2, 0, sizeof(sync2));
      sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
      memset(&f2, 0, sizeof(f2));
      f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      f2.pNext = &timeline;
      get_features = (PFN_vkGetPhysicalDeviceFeatures2)
         vulkan_symbol_wrapper_instance_proc_addr()(vk->context.instance,
               "vkGetPhysicalDeviceFeatures2");
      if (get_features && timeline_ext)
         get_features(gpu, &f2);
      for (chain = (const VkBaseInStructure*)info.pNext; chain; chain = chain->pNext)
      {
         if (chain->sType == timeline.sType)
            requested_timeline = (const VkPhysicalDeviceTimelineSemaphoreFeaturesKHR*)chain;
         if (chain->sType == vp9.sType)
            requested_vp9 = (const VkPhysicalDeviceVideoDecodeVP9FeaturesKHR*)chain;
         if (chain->sType == sync2.sType)
            requested_sync2 = (const VkPhysicalDeviceSynchronization2FeaturesKHR*)chain;
      }
      if (requested_timeline)
         timeline.timelineSemaphore = requested_timeline->timelineSemaphore;
      vk->context.graphics_queue_index = info.pQueueCreateInfos[0].queueFamilyIndex;
      if (timeline.timelineSemaphore)
         vk->context.flags |= VK_CTX_FLAG_TIMELINE_SEMAPHORE;
      if (     (!requested_sync2 || requested_sync2->synchronization2)
            && vulkan_video_probe(vk, &video, requested_vp9 && !requested_vp9->videoDecodeVP9
               ? ~(uint32_t)VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR : UINT32_MAX))
      {
         qi[0] = info.pQueueCreateInfos[0];
         if (video.xfer_family == qi[0].queueFamilyIndex)
         {
            video.xfer_first = qi[0].queueCount;
            /* The probe reserves after queue 0; only that core layout fits. */
            if (qi[0].queueCount != 1)
               video.lanes = 0;
            qi[0].queueCount += video.lanes;
            qi[0].pQueuePriorities = priorities;
         }
         qi[nq] = qi[0];
         qi[nq].queueFamilyIndex = video.decode_family;
         qi[nq].queueCount = video.lanes;
         qi[nq++].pQueuePriorities = priorities;
         if (video.xfer_family != qi[0].queueFamilyIndex)
         {
            qi[nq] = qi[0];
            qi[nq].queueFamilyIndex = video.xfer_family;
            qi[nq].queueCount = video.lanes;
            qi[nq++].pQueuePriorities = priorities;
         }
         for (j = 0; j < ARRAY_SIZE(vulkan_video_device_extensions); j++)
         {
            unsigned k;
            if (!vulkan_video_extension_enabled(j, video.codecs)) continue;
            for (k = 0; k < next; k++)
               if (!strcmp(device_extensions[k], vulkan_video_device_extensions[j]))
                  break;
            if (k == next)
               device_extensions[next++] = vulkan_video_device_extensions[j];
         }
         if (!requested_timeline)
         {
            timeline.pNext = (void*)vinfo.pNext;
            vinfo.pNext = &timeline;
         }
         if (!requested_sync2)
         {
            sync2.synchronization2 = VK_TRUE;
            sync2.pNext = (void*)vinfo.pNext;
            vinfo.pNext = &sync2;
         }
         if ((video.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR) && !requested_vp9)
         {
            vp9.videoDecodeVP9 = VK_TRUE;
            vp9.pNext = (void*)vinfo.pNext;
            vinfo.pNext = &vp9;
         }
         vinfo.pQueueCreateInfos = qi;
         vinfo.queueCreateInfoCount = nq;
         vinfo.enabledExtensionCount = next;
         if (video.lanes && vulkan_context_create_device(vk, gpu, &vinfo, &device) == VK_SUCCESS)
         {
            vk->context.video_decode_family = video.decode_family;
            vk->context.video_xfer_family = video.xfer_family;
            vk->context.video_xfer_first = video.xfer_first;
            vk->context.video_lanes = video.lanes;
            vk->context.video_codec_operations = video.codecs;
            RARCH_LOG("[Vulkan] Core device: %u hardware video decode lane(s).\n", video.lanes);
            free((void*)device_extensions);
            return device;
         }
      }
      vk->context.flags &= ~VK_CTX_FLAG_TIMELINE_SEMAPHORE;
   }
   /* The low-latency release waits on a timeline semaphore: one the
    * core asked for itself is noted, one it did not is asked for on
    * its behalf where the GPU has the feature. */
   if (vk->low_latency.supported)
   {
      if (vulkan_chain_names_timeline(info.pNext, &timeline_named))
      {
         if (timeline_named)
            vk->context.flags |= VK_CTX_FLAG_TIMELINE_SEMAPHORE;
      }
      else
      {
         VkPhysicalDeviceFeatures2 f2;
         PFN_vkGetPhysicalDeviceFeatures2 get_features =
            (PFN_vkGetPhysicalDeviceFeatures2)
            vulkan_symbol_wrapper_instance_proc_addr()(vk->context.instance,
                  "vkGetPhysicalDeviceFeatures2");
         ll_timeline.sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
         ll_timeline.pNext             = NULL;
         ll_timeline.timelineSemaphore = VK_FALSE;
         f2.sType                      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
         f2.pNext                      = &ll_timeline;
         if (get_features)
            get_features(gpu, &f2);
         if (ll_timeline.timelineSemaphore)
         {
            ll_timeline.pNext  = (void*)info.pNext;
            info.pNext         = &ll_timeline;
            vk->context.flags |= VK_CTX_FLAG_TIMELINE_SEMAPHORE;
         }
      }
   }
   info.pNext = vulkan_anti_lag_chain(vk, gpu, &anti_lag_features, info.pNext);
   if ((res = vulkan_context_create_device(vk, gpu, &info, &device)) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan] Failed to create device (%d).\n", res);
      device = VK_NULL_HANDLE;
   }

   free((void*)device_extensions);
   return device;
}

static bool vulkan_context_init_device(gfx_ctx_vulkan_data_t *vk)
{
   uint32_t queue_count;
   unsigned i;
   const char *enabled_device_extensions[ARRAY_SIZE(vulkan_device_extensions)
      + ARRAY_SIZE(vulkan_optional_device_extensions)
      + ARRAY_SIZE(vulkan_video_device_extensions)
      + VULKAN_UPSCALER_EXTS];
   VkDeviceCreateInfo device_info;
   VkDeviceQueueCreateInfo queue_info;
   static const float one                  = 1.0f;
   static const float prios[VULKAN_VIDEO_MAX_LANES + 1] = { 1.0f, 1.0f, 1.0f };
   vulkan_video_probe_t video;
   bool found_queue                        = false;
#ifdef VULKAN_HDR_SWAPCHAIN
   bool hdr_metadata_enabled               = false;
#endif
   video_driver_state_t *video_st          = video_state_get_ptr();

#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
   vulkan_android_device_plan_t android_plan;
#endif
   VkPhysicalDeviceFeatures features       = { false };
   VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline;
   VkPhysicalDeviceAntiLagFeaturesAMD anti_lag_features;

   unsigned enabled_device_extension_count = 0;

   struct retro_hw_render_context_negotiation_interface_vulkan
                                    *iface = (struct retro_hw_render_context_negotiation_interface_vulkan*)
                                    video_st->hw_render_context_negotiation;

#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
   memset(&android_plan, 0, sizeof(android_plan));
   vk->context.flags &= ~VK_CTX_FLAG_ANDROID_IMAGES;
   vk->context.android_sampler_descriptors = 0;
   vk->context.android_legacy_descriptor_budget = false;
#endif
   queue_info.sType                        = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue_info.pNext                        = NULL;
   queue_info.flags                        = 0;
   queue_info.queueFamilyIndex             = 0;
   queue_info.queueCount                   = 0;
   queue_info.pQueuePriorities             = NULL;

   device_info.sType                       = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   device_info.pNext                       = NULL;
   device_info.flags                       = 0;
   device_info.queueCreateInfoCount        = 0;
   device_info.pQueueCreateInfos           = NULL;
   device_info.enabledLayerCount           = 0;
   device_info.ppEnabledLayerNames         = NULL;
   device_info.enabledExtensionCount       = 0;
   device_info.ppEnabledExtensionNames     = NULL;
   device_info.pEnabledFeatures            = NULL;

   if (iface)
   {
      if (iface->interface_type != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
      {
         RARCH_WARN("[Vulkan] Got HW context negotiation interface, but it's the wrong API.\n");
         iface = NULL;
      }
      else if (iface->interface_version == 0)
      {
         RARCH_WARN("[Vulkan] Got HW context negotiation interface, but it's the wrong interface version.\n");
         iface = NULL;
      }
      else
         RARCH_LOG("[Vulkan] Got HW context negotiation interface %u.\n", iface->interface_version);
   }

   /* Set below for a device made here; a core's has none of ours */
   vk->context.flags &= ~VK_CTX_FLAG_TIMELINE_SEMAPHORE;
   vk->context.video_lanes         = 0;
   vk->context.video_codec_operations = 0;
   vk->context.video_decode_family = 0;
   vk->context.video_xfer_family   = 0;
   vk->context.video_xfer_first    = 0;

   if (!vulkan_context_init_gpu(vk))
      return false;

   /* pEnabledFeatures is an opt-in request list: enable individual
    * features here only when a code path needs them. Blanket-enabling
    * everything the GPU reports (notably robustBufferAccess) changes
    * shader UBO read semantics on some drivers. */

   vk->low_latency.core_device = false;
   vk->low_latency.noted       = false;
   if (!cached_device_vk && iface && iface->create_device)
   {
      struct retro_vulkan_context context = { 0 };

      bool ret = false;

      if (     (iface->interface_version >= 2)
            &&  iface->create_device2)
      {
         ret = iface->create_device2(&context, vk->context.instance,
               vk->context.gpu,
               vk->vk_surface,
               vulkan_symbol_wrapper_instance_proc_addr(),
               vulkan_context_create_device_wrapper, vk);

         if (!ret)
         {
            RARCH_WARN("[Vulkan] Failed to create_device2 on provided VkPhysicalDevice, letting core decide which GPU to use.\n");
#ifdef HAVE_OPENXR
            /* The headset's GPU was refused. */
            if (vk->context.xr)
            {
               vulkan_openxr_drop(vk->context.xr);
               vk->context.xr = NULL;
            }
#endif
            vk->context.gpu = VK_NULL_HANDLE;
            ret = iface->create_device2(&context, vk->context.instance,
                  vk->context.gpu,
                  vk->vk_surface,
                  vulkan_symbol_wrapper_instance_proc_addr(),
                  vulkan_context_create_device_wrapper, vk);
         }
      }
      else
      {
         const char *dev_exts[ARRAY_SIZE(vulkan_device_extensions)
            + VULKAN_UPSCALER_EXTS
#ifdef HAVE_OPENXR
            + VULKAN_OPENXR_MAX_EXTS
#endif
            ];
         uint32_t num_dev_exts = 0;
#ifdef HAVE_OPENXR
         vulkan_context_openxr_device_exts(vk, dev_exts, &num_dev_exts,
               ARRAY_SIZE(dev_exts));
#else
         for (i = 0; i < ARRAY_SIZE(vulkan_device_extensions); i++)
            dev_exts[num_dev_exts++] = vulkan_device_extensions[i];
#endif
         /* A core making the device itself (v1) gets them as required:
          * only those the GPU has are listed */
         if (vk->context.gpu != VK_NULL_HANDLE)
            num_dev_exts = vulkan_upscaler_device_exts(vk, vk->context.gpu,
                  dev_exts, num_dev_exts, ARRAY_SIZE(dev_exts));
         ret = iface->create_device(&context, vk->context.instance,
               vk->context.gpu,
               vk->vk_surface,
               vulkan_symbol_wrapper_instance_proc_addr(),
               dev_exts,
               num_dev_exts,
               NULL,
               0,
               &features);
      }

      if (ret)
      {
         if (vk->context.gpu != VK_NULL_HANDLE && context.gpu != vk->context.gpu)
            RARCH_ERR("[Vulkan] Got unexpected VkPhysicalDevice, despite RetroArch using explicit physical device.\n");

         vk->context.destroy_device       = iface->destroy_device;
         /* Made through create_device2's wrapper, the device has
          * RetroArch's extensions where the GPU offers them */
         vk->low_latency.core_device      = !vk->low_latency.noted;

         vk->context.device               = context.device;
         vk->context.queue                = context.queue;
         vk->context.gpu                  = context.gpu;
         vk->context.graphics_queue_index = context.queue_family_index;
         vk->context.queue                = context.queue;

         if (context.presentation_queue != context.queue)
         {
            RARCH_ERR("[Vulkan] Present queue != graphics queue. This is currently not supported.\n");
            return false;
         }
      }
      else
      {
         RARCH_WARN("[Vulkan] Failed to create device with negotiation interface. Falling back to default path.\n");
#ifdef HAVE_OPENXR
         /* The runtime's lists were for the core's device. */
         if (vk->context.xr && !vulkan_openxr_uses_enable2(vk->context.xr))
         {
            vulkan_openxr_drop(vk->context.xr);
            vk->context.xr = NULL;
         }
#endif
      }
   }

   vkGetPhysicalDeviceProperties(vk->context.gpu,
         &vk->context.gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(vk->context.gpu,
         &vk->context.memory_properties);

   {
      /* Said once, so a log shows where the per-frame buffers live. */
      uint32_t types[2];
      const VkPhysicalDeviceMemoryProperties *mp = &vk->context.memory_properties;
      if (vulkan_cpu_write_memory_types(mp, ~0u,
               vulkan_find_memory_type(mp, ~0u,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), types) == 2)
         RARCH_LOG("[Vulkan] CPU-written buffers in device-local host-visible memory (type %u, %u MB heap).\n",
               (unsigned)types[0],
               (unsigned)(mp->memoryHeaps[mp->memoryTypes[types[0]].heapIndex].size >> 20));
   }

#ifdef VULKAN_EMULATE_MAILBOX
#if defined(_WIN32)
   /* Win32 windowed mode seems to deal just fine with toggling VSync.
    * Fullscreen however ... */
   if (vk->flags & VK_DATA_FLAG_FULLSCREEN)
      vk->flags |=  VK_DATA_FLAG_EMULATE_MAILBOX;
   else
      vk->flags &= ~VK_DATA_FLAG_EMULATE_MAILBOX;
#else
   vk->flags |=  VK_DATA_FLAG_EMULATE_MAILBOX;
#endif
#endif

   /* If we're emulating mailbox, stick to using fences rather than semaphores.
    * Avoids some really weird driver bugs. Resolved either way rather than
    * left to the caller's zeroing: where mailbox emulation is compiled in
    * this is the only writer. */
   if (      (vk->flags & VK_DATA_FLAG_EMULATE_MAILBOX)
         || (vk->context.gpu_properties.deviceType
            != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))
      vk->flags &= ~VK_DATA_FLAG_USE_WSI_SEMAPHORE;
   else
      vk->flags |=  VK_DATA_FLAG_USE_WSI_SEMAPHORE;
   RARCH_LOG("[Vulkan] Using %s for WSI acquire%s.\n",
         (vk->flags & VK_DATA_FLAG_USE_WSI_SEMAPHORE)
         ? "semaphores" : "fences",
         (vk->flags & VK_DATA_FLAG_EMULATE_MAILBOX)
         ? ", mailbox emulation available" : "");

   {
      char version_str[128];
      size_t _len            = snprintf(version_str      , sizeof(version_str)      , "%u", VK_VERSION_MAJOR(vk->context.gpu_properties.apiVersion));
      version_str[  _len]    = '.';
      version_str[++_len]    = '\0';
      _len                  += snprintf(version_str + _len, sizeof(version_str) - _len, "%u", VK_VERSION_MINOR(vk->context.gpu_properties.apiVersion));
      version_str[  _len]    = '.';
      version_str[++_len]    = '\0';
      snprintf(version_str + _len, sizeof(version_str) - _len, "%u", VK_VERSION_PATCH(vk->context.gpu_properties.apiVersion));
      video_driver_set_gpu_api_version_string(version_str);
   }

   if (vk->context.device == VK_NULL_HANDLE)
   {
      VkQueueFamilyProperties *queue_properties = NULL;
      vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
            &queue_count, NULL);

      if (queue_count < 1)
      {
         RARCH_ERR("[Vulkan] Invalid number of queues detected.\n");
         return false;
      }

      if (!(queue_properties = (VkQueueFamilyProperties*)malloc(queue_count *
                  sizeof(*queue_properties))))
         return false;

      vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
            &queue_count, queue_properties);

      for (i = 0; i < queue_count; i++)
      {
         VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
         VkBool32 supported    = VK_FALSE;
#ifdef HAVE_OPENXR
         /* Headless (XR): no surface, nothing to present to. */
         if (vk->vk_surface == VK_NULL_HANDLE)
            supported = VK_TRUE;
         else
#endif
         vkGetPhysicalDeviceSurfaceSupportKHR(
               vk->context.gpu, i,
               vk->vk_surface, &supported);
         if (supported && ((queue_properties[i].queueFlags & required) == required))
         {
            vk->context.graphics_queue_index = i;
            RARCH_LOG("[Vulkan] Queue family %u supports %u sub-queues.\n",
                  i, queue_properties[i].queueCount);
            found_queue = true;
            break;
         }
      }

      free(queue_properties);

      if (!found_queue)
      {
         RARCH_ERR("[Vulkan] Did not find suitable graphics queue.\n");
         return false;
      }

      if (!(vulkan_find_device_extensions(vk->context.gpu,
              enabled_device_extensions, &enabled_device_extension_count,
              vulkan_device_extensions, ARRAY_SIZE(vulkan_device_extensions),
              vulkan_optional_device_extensions,
         vulkan_optional_device_count(vk, true))))
      {
          RARCH_ERR("[Vulkan] Could not find required device extensions.\n");
          return false;
      }
      enabled_device_extension_count = vulkan_upscaler_device_exts(vk,
            vk->context.gpu, enabled_device_extensions,
            enabled_device_extension_count,
            ARRAY_SIZE(enabled_device_extensions));

      vk->fse_supported = false;
      for (i = 0; i < enabled_device_extension_count; i++)
      {
         if (!strcmp(enabled_device_extensions[i], "VK_EXT_full_screen_exclusive"))
         {
            vk->fse_supported = true;
            break;
         }
      }
      for (i = 0; i < enabled_device_extension_count; i++)
      {
         if (!strcmp(enabled_device_extensions[i], "VK_GOOGLE_display_timing"))
         {
            vk->display_timing_supported = true;
            break;
         }
      }
      vulkan_low_latency_note_exts(vk, enabled_device_extensions,
            enabled_device_extension_count);

#ifdef VULKAN_HDR_SWAPCHAIN
      /* Note whether the extension was enabled; the actual entrypoint is
       * loaded below, after the device exists. */
      vk->set_hdr_metadata = NULL;
      for (i = 0; i < enabled_device_extension_count; i++)
      {
         if (!strcmp(enabled_device_extensions[i], "VK_EXT_hdr_metadata"))
         {
            hdr_metadata_enabled = true;
            break;
         }
      }
#endif

      queue_info.queueFamilyIndex         = vk->context.graphics_queue_index;
      queue_info.queueCount               = 1;
      queue_info.pQueuePriorities         = &one;

      /* Timeline semaphores, where the extension came with the feature */
      timeline.sType             =
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
      timeline.pNext             = NULL;
      timeline.timelineSemaphore = VK_FALSE;
      for (i = 0; i < enabled_device_extension_count; i++)
         if (!strcmp(enabled_device_extensions[i],
                  VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
         {
            PFN_vkGetPhysicalDeviceFeatures2 get_features2 =
               (PFN_vkGetPhysicalDeviceFeatures2)
               vulkan_symbol_wrapper_instance_proc_addr()(
                     vk->context.instance, "vkGetPhysicalDeviceFeatures2");
            uint32_t inst_version = VK_API_VERSION_1_0;
            /* A 1.1 command: the instance is made 1.1 where the loader
             * is (vulkan_context_init) */
            if (vkEnumerateInstanceVersion)
               vkEnumerateInstanceVersion(&inst_version);
            if (     get_features2
                  && inst_version >= VK_API_VERSION_1_1
                  && vk->context.gpu_properties.apiVersion
                     >= VK_API_VERSION_1_1)
            {
               VkPhysicalDeviceFeatures2 f2;
               f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
               f2.pNext = &timeline;
               get_features2(vk->context.gpu, &f2);
            }
            break;
         }
      if (timeline.timelineSemaphore)
      {
         device_info.pNext  = &timeline;
         vk->context.flags |= VK_CTX_FLAG_TIMELINE_SEMAPHORE;
      }
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
      {
         PFN_vkGetInstanceProcAddr proc = vulkan_symbol_wrapper_instance_proc_addr();
         uint32_t version = VK_API_VERSION_1_0;
         if (vkEnumerateInstanceVersion)
            vkEnumerateInstanceVersion(&version);
         if (version > vk->context.gpu_properties.apiVersion)
            version = vk->context.gpu_properties.apiVersion;
         if (version > VK_API_VERSION_1_1)
            version = VK_API_VERSION_1_1;
         if (proc && vulkan_android_device_plan_query(vk->context.gpu, version,
                  enabled_device_extensions, enabled_device_extension_count,
                  (PFN_vkGetPhysicalDeviceFeatures2)proc(vk->context.instance,
                        "vkGetPhysicalDeviceFeatures2"),
                  (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)proc(
                        vk->context.instance, "vkGetPhysicalDeviceExternalSemaphoreProperties"),
                  (PFN_vkGetPhysicalDeviceProperties2)proc(vk->context.instance,
                        "vkGetPhysicalDeviceProperties2"), &android_plan))
            device_info.pNext = vulkan_android_device_plan_chain(
                  &android_plan, device_info.pNext);
      }
#endif
      device_info.pNext = vulkan_anti_lag_chain(vk, vk->context.gpu,
            &anti_lag_features, device_info.pNext);

      device_info.queueCreateInfoCount    = 1;
      device_info.pQueueCreateInfos       = &queue_info;
      device_info.enabledExtensionCount   = enabled_device_extension_count;
      device_info.ppEnabledExtensionNames = enabled_device_extensions;
      device_info.pEnabledFeatures        = &features;

      if (cached_device_vk)
      {
         vk->context.device = cached_device_vk;
         cached_device_vk   = NULL;
         if (cached_timeline_vk)
            vk->context.flags |=  VK_CTX_FLAG_TIMELINE_SEMAPHORE;
         else
            vk->context.flags &= ~VK_CTX_FLAG_TIMELINE_SEMAPHORE;
         vk->context.video_decode_family = cached_video_vk[0];
         vk->context.video_xfer_family   = cached_video_vk[1];
         vk->context.video_xfer_first    = cached_video_vk[2];
         vk->context.video_lanes         = cached_video_vk[3];
         vk->context.video_codec_operations = cached_video_vk[4];
         if (cached_video_vk[5]) vk->context.flags |= VK_CTX_FLAG_VIDEO_GENERAL;
         else vk->context.flags &= ~VK_CTX_FLAG_VIDEO_GENERAL;
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
         if (cached_android_images_vk)
            vk->context.flags |= VK_CTX_FLAG_ANDROID_IMAGES;
         vk->context.android_sampler_descriptors = cached_android_descriptors_vk;
         vk->context.android_legacy_descriptor_budget = cached_android_legacy_vk;
#endif
         cached_gpu_vk      = VK_NULL_HANDLE;

         if (cached_destroy_device_vk)
         {
            vk->context.destroy_device = cached_destroy_device_vk;
            cached_destroy_device_vk   = NULL;
         }

         video_driver_cache_context_ack_set();
         RARCH_LOG("[Vulkan] Using cached Vulkan context.\n");
      }
      else
      {
         VkDevice dev = VK_NULL_HANDLE;
         /* With the video decode queues where the GPU has them; a
          * device that cannot be made so is made without */
         if (vulkan_video_probe(vk, &video, UINT32_MAX))
         {
            VkDeviceQueueCreateInfo qi[3];
            VkDeviceCreateInfo vinfo = device_info;
            VkPhysicalDeviceSynchronization2FeaturesKHR sync2;
            VkPhysicalDeviceVideoDecodeVP9FeaturesKHR vp9;
            unsigned j, nq = 1, next = enabled_device_extension_count;

            for (j = 0; j < ARRAY_SIZE(vulkan_video_device_extensions); j++)
               if (vulkan_video_extension_enabled(j, video.codecs))
                  enabled_device_extensions[next++] = vulkan_video_device_extensions[j];
            qi[0]               = queue_info;
            qi[0].pQueuePriorities = prios;
            if (video.xfer_family == vk->context.graphics_queue_index)
               qi[0].queueCount = 1 + video.lanes;
            qi[nq]                  = queue_info;
            qi[nq].queueFamilyIndex = video.decode_family;
            qi[nq].queueCount       = video.lanes;
            qi[nq].pQueuePriorities = prios;
            nq++;
            if (video.xfer_family != vk->context.graphics_queue_index)
            {
               qi[nq]                  = queue_info;
               qi[nq].queueFamilyIndex = video.xfer_family;
               qi[nq].queueCount       = video.lanes;
               qi[nq].pQueuePriorities = prios;
               nq++;
            }
            sync2.sType            =
               VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
            sync2.pNext            = (void*)device_info.pNext;
            sync2.synchronization2 = VK_TRUE;
            vinfo.pNext                   = &sync2;
            if (video.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
            {
               memset(&vp9, 0, sizeof(vp9));
               vp9.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_DECODE_VP9_FEATURES_KHR;
               vp9.videoDecodeVP9 = VK_TRUE; vp9.pNext = (void*)vinfo.pNext;
               vinfo.pNext = &vp9;
            }
            vinfo.queueCreateInfoCount    = nq;
            vinfo.pQueueCreateInfos       = qi;
            vinfo.enabledExtensionCount   = next;
            if (vulkan_context_create_device(vk, vk->context.gpu,
                     &vinfo, &dev) == VK_SUCCESS)
            {
               vk->context.video_decode_family = video.decode_family;
               vk->context.video_xfer_family   = video.xfer_family;
               vk->context.video_xfer_first    = video.xfer_first;
               vk->context.video_lanes         = video.lanes;
               vk->context.video_codec_operations = video.codecs;
               RARCH_LOG("[Vulkan] Video decode: family %u, copies on family %u from queue %u, %u at once.\n",
                     video.decode_family, video.xfer_family,
                     video.xfer_first, video.lanes);
            }
            else
            {
               dev = VK_NULL_HANDLE;
               RARCH_WARN("[Vulkan] Device with video decode refused; made without.\n");
            }
         }
         if (     dev == VK_NULL_HANDLE
               && vulkan_context_create_device(vk, vk->context.gpu,
                  &device_info, &dev) != VK_SUCCESS)
         {
            dev = VK_NULL_HANDLE;
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
            if (vulkan_android_device_plan_disable(&android_plan, &device_info.pNext,
                     enabled_device_extensions, &enabled_device_extension_count))
            {
               device_info.enabledExtensionCount = enabled_device_extension_count;
               if (vulkan_context_create_device(vk, vk->context.gpu,
                        &device_info, &dev) != VK_SUCCESS)
                  dev = VK_NULL_HANDLE;
            }
#endif
            if (dev == VK_NULL_HANDLE)
            {
               RARCH_ERR("[Vulkan] Failed to create device.\n");
               return false;
            }
         }
         vk->context.device = dev;
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
         if (android_plan.supported)
         {
            vk->context.flags |= VK_CTX_FLAG_ANDROID_IMAGES;
            vk->context.android_sampler_descriptors = android_plan.sampler_descriptors;
            vk->context.android_legacy_descriptor_budget = android_plan.legacy_descriptor_budget;
         }
#endif
      }
   }

   if (!vulkan_load_device_symbols(vk))
   {
      RARCH_ERR("[Vulkan] Failed to load device symbols.\n");
      return false;
   }

#ifdef VK_USE_PLATFORM_WIN32_KHR
   /* Resolve the explicit acquire/release now the device exists. Only
    * used under VIDEO_FSE_FORCED; NULL otherwise and never called. */
   vk->fse_acquire = NULL;
   vk->fse_release = NULL;
   if (vk->fse_supported)
   {
      vk->fse_acquire = vkGetDeviceProcAddr(vk->context.device,
               "vkAcquireFullScreenExclusiveModeEXT");
      vk->fse_release = vkGetDeviceProcAddr(vk->context.device,
               "vkReleaseFullScreenExclusiveModeEXT");
   }
   vk->display_timing_query = NULL;
   vk->present_id           = 0;
   if (vk->display_timing_supported)
      vk->display_timing_query = vkGetDeviceProcAddr(vk->context.device,
               "vkGetPastPresentationTimingGOOGLE");
#endif

   vulkan_low_latency_device_init(vk);

#ifdef VULKAN_HDR_SWAPCHAIN
   /* Now that the device exists, resolve vkSetHdrMetadataEXT if the
    * extension was enabled above. Stays NULL (call skipped) otherwise. */
   if (hdr_metadata_enabled)
      vk->set_hdr_metadata = (PFN_vkSetHdrMetadataEXT)
         vkGetDeviceProcAddr(vk->context.device, "vkSetHdrMetadataEXT");
#endif

   if (vk->context.queue == VK_NULL_HANDLE)
   {
      vkGetDeviceQueue(vk->context.device,
            vk->context.graphics_queue_index, 0, &vk->context.queue);
   }

#ifdef HAVE_THREADS
   vk->context.queue_lock = slock_new();
   if (!vk->context.queue_lock)
   {
      RARCH_ERR("[Vulkan] Failed to create queue lock.\n");
      return false;
   }
#endif

#ifdef HAVE_OPENXR
   if (vk->context.xr && !vulkan_openxr_start(vk->context.xr,
            vk->context.instance, vk->context.gpu, vk->context.device,
            vk->context.graphics_queue_index, vk->context.queue_lock))
   {
      /* The device was made for the runtime: rebuild video without it,
       * unless the core keeps its context, which a reinit would reuse. */
      if (video_st->hw_render.cache_context || vk->openxr_headset)
         vulkan_openxr_drop(vk->context.xr);
      else
      {
         vulkan_openxr_drop_and_reinit(vk->context.xr);
         video_driver_modify_disp_flags(VIDEO_FLAG_DRIVER_REINIT, 0);
      }
      vk->context.xr = NULL;
   }
#endif

   return true;
}

#ifdef HAVE_OPENXR
bool vulkan_context_init_headless_device(gfx_ctx_vulkan_data_t *vk)
{
   return vulkan_context_init_device(vk);
}
#endif

#if defined(HAVE_OPENXR) && defined(ANDROID)
bool vulkan_context_create_android_surface(gfx_ctx_vulkan_data_t *vk,
      void *window)
{
   VkAndroidSurfaceCreateInfoKHR surf_info;
   PFN_vkCreateAndroidSurfaceKHR create;

   if (!vk || !window || vk->context.instance == VK_NULL_HANDLE)
   {
      RARCH_ERR("[Vulkan] Cannot create OpenXR core surface: Vulkan instance or Android window is missing.\n");
      return false;
   }

   if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance,
            "vkCreateAndroidSurfaceKHR", create))
   {
      RARCH_ERR("[Vulkan] vkCreateAndroidSurfaceKHR is unavailable for OpenXR core negotiation.\n");
      return false;
   }

   surf_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
   surf_info.pNext  = NULL;
   surf_info.flags  = 0;
   surf_info.window = (ANativeWindow*)window;

   if (create(vk->context.instance, &surf_info, NULL, &vk->vk_surface)
         != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan] Failed to create Android surface for OpenXR core negotiation.\n");
      return false;
   }

   return true;
}
#endif

#ifdef VULKAN_HDR_SWAPCHAIN
#define VULKAN_COLORSPACE_EXTENSION_NAME "VK_EXT_swapchain_colorspace"
#endif

static const char *vulkan_optional_instance_extensions[] = {
#ifdef __APPLE__
   VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
#ifdef _WIN32
   "VK_KHR_get_surface_capabilities2",
#endif
#ifdef VULKAN_HDR_SWAPCHAIN
   VULKAN_COLORSPACE_EXTENSION_NAME
#endif
#if defined(_WIN32) && !defined(__WINRT__)
   , "VK_KHR_get_physical_device_properties2"
   , "VK_KHR_external_memory_capabilities"
   , "VK_KHR_external_semaphore_capabilities"
#endif
};


static unsigned vulkan_optional_instance_count(const gfx_ctx_vulkan_data_t *vk)
{
   unsigned count = ARRAY_SIZE(vulkan_optional_instance_extensions);
#if defined(_WIN32) && !defined(__WINRT__)
   if (!vk->context.swapchain_settings.dxgi_present)
      count -= 3;
#else
   (void)vk;
#endif
   return count;
}

/* vkCreateInstance, or the runtime's, which adds what it needs. An
 * instance the runtime refuses drops the headset. */
static VkResult vulkan_context_create_instance(gfx_ctx_vulkan_data_t *vk,
      const VkInstanceCreateInfo *info, VkInstance *instance)
{
#ifdef HAVE_OPENXR
   if (vk->context.xr && vulkan_openxr_uses_enable2(vk->context.xr))
   {
      VkResult res = vulkan_openxr_create_instance(vk->context.xr,
            vulkan_symbol_wrapper_instance_proc_addr(), info, instance);
      if (res == VK_SUCCESS)
         return res;
      vulkan_openxr_drop(vk->context.xr);
      vk->context.xr = NULL;
   }
#endif
   return vkCreateInstance(info, NULL, instance);
}

static VkInstance vulkan_context_create_instance_wrapper(void *opaque, const VkInstanceCreateInfo *create_info)
{
   VkResult res;
   uint32_t i;
   gfx_ctx_vulkan_data_t *vk        = (gfx_ctx_vulkan_data_t *)opaque;
   VkInstanceCreateInfo info        = *create_info;
   VkInstance instance              = VK_NULL_HANDLE;
   /* Room for VK_KHR_surface, WSI, SDL3, the debug extension and a headset runtime's. */
   const char *required_extensions[32];
   uint32_t required_extension_count = 0;
   unsigned instance_extensions_cap = info.enabledExtensionCount
                                                          + ARRAY_SIZE(required_extensions)
                                                          + ARRAY_SIZE(vulkan_optional_instance_extensions)
                                                          + VULKAN_UPSCALER_EXTS;
   const char **instance_extensions = (const char**)malloc(instance_extensions_cap * sizeof(const char *));
   const char **instance_layers     = (const char**)malloc((info.enabledLayerCount     + 1)                * sizeof(const char *));

   /* Both mallocs must have succeeded before the memcpy / field
    * assignments below dereference the buffers.  The 'end' label
    * free()s both pointers; free(NULL) is a no-op, so if only one
    * of the two succeeded it still gets reclaimed.  'instance' was
    * pre-initialised to VK_NULL_HANDLE at the top of the function,
    * which is the correct error-return value for this API. */
   if (!instance_extensions || !instance_layers)
   {
      RARCH_ERR("[Vulkan] Out of memory allocating instance extension / layer arrays.\n");
      goto end;
   }

   /* A caller enabling no extensions or no layers legitimately
    * passes NULL with a zero count; memcpy's second argument is
    * declared non-NULL even for n == 0, so guard each copy. */
   if (info.enabledExtensionCount)
      memcpy((void*)instance_extensions, info.ppEnabledExtensionNames, info.enabledExtensionCount * sizeof(const char *));
   if (info.enabledLayerCount)
      memcpy((void*)instance_layers,     info.ppEnabledLayerNames,     info.enabledLayerCount     * sizeof(const char *));
   info.ppEnabledExtensionNames     = instance_extensions;
   info.ppEnabledLayerNames         = instance_layers;

   required_extensions[required_extension_count++] = "VK_KHR_surface";

   switch (vk->wsi_type)
   {
      case VULKAN_WSI_WAYLAND:
         required_extensions[required_extension_count++] = "VK_KHR_wayland_surface";
         break;
      case VULKAN_WSI_ANDROID:
         required_extensions[required_extension_count++] = "VK_KHR_android_surface";
         break;
      case VULKAN_WSI_WIN32:
         required_extensions[required_extension_count++] = "VK_KHR_win32_surface";
         break;
      case VULKAN_WSI_XLIB:
         required_extensions[required_extension_count++] = "VK_KHR_xlib_surface";
         break;
      case VULKAN_WSI_XCB:
         required_extensions[required_extension_count++] = "VK_KHR_xcb_surface";
         break;
      case VULKAN_WSI_MIR:
         required_extensions[required_extension_count++] = "VK_KHR_mir_surface";
         break;
      case VULKAN_WSI_DISPLAY:
         required_extensions[required_extension_count++] = "VK_KHR_display";
         break;
      case VULKAN_WSI_MVK_MACOS:
      case VULKAN_WSI_MVK_IOS:
         required_extensions[required_extension_count++] = "VK_EXT_metal_surface";
         break;
#ifdef HAVE_SDL3
      case VULKAN_WSI_SDL3:
      {
         Uint32 sdl_ext_count = 0;
         const char * const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
         for (i = 0; sdl_extensions && i < sdl_ext_count; i++)
         {
            /* VK_KHR_surface already added above. */
            if (string_is_equal(sdl_extensions[i], "VK_KHR_surface"))
               continue;
            if (required_extension_count < ARRAY_SIZE(required_extensions))
               required_extensions[required_extension_count++] = sdl_extensions[i];
            else
               RARCH_WARN("[Vulkan] Dropping SDL3 instance extension \"%s\": list full.\n", sdl_extensions[i]);
         }
         break;
      }
#endif
      case VULKAN_WSI_NONE:
      default:
         break;
   }

#ifdef VULKAN_DEBUG
   instance_layers[info.enabledLayerCount++]         = "VK_LAYER_KHRONOS_validation";
   if (required_extension_count < ARRAY_SIZE(required_extensions))
      required_extensions[required_extension_count++] = "VK_EXT_debug_utils";
   else
      RARCH_WARN("[Vulkan] Dropping VK_EXT_debug_utils: extension list full.\n");
#endif

#ifdef HAVE_OPENXR
   /* XR_KHR_vulkan_enable lists what its instance needs. */
   if (vk->context.xr && !vulkan_openxr_uses_enable2(vk->context.xr))
   {
      const char *xr_exts[VULKAN_OPENXR_MAX_EXTS];
      unsigned n = vulkan_openxr_instance_extensions(vk->context.xr,
            xr_exts, VULKAN_OPENXR_MAX_EXTS);
      for (i = 0; i < n; i++)
         vulkan_append_ext(required_extensions, &required_extension_count,
               ARRAY_SIZE(required_extensions), xr_exts[i]);
   }
#endif

   if (!(vulkan_find_instance_extensions(
            instance_extensions, &info.enabledExtensionCount,
            required_extensions, required_extension_count,
            vulkan_optional_instance_extensions,
            vulkan_optional_instance_count(vk))))
   {
      RARCH_ERR("[Vulkan] Instance does not support required extensions.\n");
      goto end;
   }
#ifdef HAVE_TEMPORAL_UPSCALER
   {
      char dir[DIR_MAX_LENGTH];
      enum video_temporal_upscaler kind = vulkan_upscaler_kind(dir, sizeof(dir));
      if (kind)
         info.enabledExtensionCount = vulkan_upscaler_instance_extensions(
               kind, dir, instance_extensions, info.enabledExtensionCount,
               instance_extensions_cap);
   }
#endif

#ifdef VULKAN_HDR_SWAPCHAIN
   /* Check if HDR colorspace extension was enabled */
   vk->context.flags &= ~VK_CTX_FLAG_HDR_SUPPORT;
   for (i = 0; i < info.enabledExtensionCount; i++)
   {
      if (string_is_equal(instance_extensions[i], VULKAN_COLORSPACE_EXTENSION_NAME))
      {
         vk->context.flags |= VK_CTX_FLAG_HDR_SUPPORT;
         break;
      }
   }
#endif

#ifdef __APPLE__
   /* Check if portability enumeration was enabled (needed for Vulkan loader to find MoltenVK) */
   for (i = 0; i < info.enabledExtensionCount; i++)
   {
      if (string_is_equal(instance_extensions[i], VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
      {
         info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
         break;
      }
   }
#endif

   if (info.pApplicationInfo)
   {
      uint32_t supported_instance_version = VK_API_VERSION_1_0;
      if (!vkEnumerateInstanceVersion || vkEnumerateInstanceVersion(&supported_instance_version) != VK_SUCCESS)
         supported_instance_version = VK_API_VERSION_1_0;

      if (supported_instance_version < info.pApplicationInfo->apiVersion)
      {
         RARCH_ERR("[Vulkan] Core requests apiVersion %u.%u, but it is not supported by loader.\n",
               VK_VERSION_MAJOR(info.pApplicationInfo->apiVersion),
               VK_VERSION_MINOR(info.pApplicationInfo->apiVersion));
         goto end;
      }
   }

   if ((res = vulkan_context_create_instance(vk, &info, &instance)) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan] Failed to create Vulkan instance (%d).\n", res);
      RARCH_ERR("[Vulkan] If VULKAN_DEBUG=1 is enabled, make sure Vulkan validation layers are installed.\n");
      for (i = 0; i < info.enabledLayerCount; i++)
         RARCH_ERR("[Vulkan] Core explicitly enables layer (%s), this might be cause of failure.\n", info.ppEnabledLayerNames[i]);
      instance = VK_NULL_HANDLE;
      goto end;
   }

end:
   free((void*)instance_extensions);
   free((void*)instance_layers);
   return instance;
}

static bool vulkan_update_display_mode(
      unsigned *dims,
      const VkDisplayModePropertiesKHR *mode,
      const struct vulkan_display_surface_info *info)
{
   unsigned vis_w = mode->parameters.visibleRegion.width;
   unsigned vis_h = mode->parameters.visibleRegion.height;
   int want_w     = (int)VIDEO_SCALE_W(info->dims);
   int want_h     = (int)VIDEO_SCALE_H(info->dims);

   if (!want_w || !want_h)
   {
      /* Strategy here is to pick something which is largest resolution. */
      if (vis_w * vis_h <= VIDEO_SCALE_W(*dims) * VIDEO_SCALE_H(*dims))
         return false;
   }
   else
   {
      /* For particular resolutions, find the closest. */
      int dx     = want_w - (int)vis_w;
      int dy     = want_h - (int)vis_h;
      int old_dx = want_w - (int)VIDEO_SCALE_W(*dims);
      int old_dy = want_h - (int)VIDEO_SCALE_H(*dims);
      if (     dx * dx + dy * dy >= old_dx * old_dx + old_dy * old_dy
            || abs((int)info->refresh_rate_x1000
               - (int)mode->parameters.refreshRate) >= 1000)
         return false;
   }

   *dims = VIDEO_SCALE_PACK(vis_w, vis_h);
   return true;
}

static bool vulkan_create_display_surface(gfx_ctx_vulkan_data_t *vk,
      unsigned *dims,
      const struct vulkan_display_surface_info *info)
{
   unsigned dpy, i, j;
   VkDisplaySurfaceCreateInfoKHR create_info;
   bool ret                                  = true;
   uint32_t display_count                    = 0;
   uint32_t plane_count                      = 0;
   VkDisplayPropertiesKHR *displays          = NULL;
   VkDisplayPlanePropertiesKHR *planes       = NULL;
   uint32_t mode_count                       = 0;
   VkDisplayModePropertiesKHR *modes         = NULL;
   uint32_t best_plane                       = UINT32_MAX;
   VkDisplayPlaneAlphaFlagBitsKHR alpha_mode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
   VkDisplayModeKHR best_mode                = VK_NULL_HANDLE;
   /* Monitor index starts on 1, 0 is auto. */
   unsigned monitor_index                    = info->monitor_index;
   unsigned saved_dims                       = *dims;

   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetPhysicalDeviceDisplayPropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayPlaneSupportedDisplaysKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayModePropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDisplayModeKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayPlaneCapabilitiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDisplayPlaneSurfaceKHR);

#define GOTO_FAIL() do { \
   ret = false; \
   goto end; \
} while(0)

   if (vkGetPhysicalDeviceDisplayPropertiesKHR(vk->context.gpu, &display_count, NULL) != VK_SUCCESS)
      GOTO_FAIL();
   if (!(displays = (VkDisplayPropertiesKHR*)calloc(display_count, sizeof(*displays))))
      GOTO_FAIL();
   if (vkGetPhysicalDeviceDisplayPropertiesKHR(vk->context.gpu, &display_count, displays) != VK_SUCCESS)
      GOTO_FAIL();

   if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk->context.gpu, &plane_count, NULL) != VK_SUCCESS)
      GOTO_FAIL();
   if (!(planes = (VkDisplayPlanePropertiesKHR*)calloc(plane_count, sizeof(*planes))))
      GOTO_FAIL();
   if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk->context.gpu, &plane_count, planes) != VK_SUCCESS)
      GOTO_FAIL();

   if (monitor_index > display_count)
   {
      RARCH_WARN("[Vulkan] Monitor index is out of range, using automatic display.\n");
      monitor_index = 0;
   }

retry:
   for (dpy = 0; dpy < display_count; dpy++)
   {
      VkDisplayKHR display;
      if (monitor_index != 0 && (monitor_index - 1) != dpy)
         continue;

      display    = displays[dpy].display;
      best_mode  = VK_NULL_HANDLE;
      best_plane = UINT32_MAX;

      if (vkGetDisplayModePropertiesKHR(vk->context.gpu,
            display, &mode_count, NULL) != VK_SUCCESS)
         GOTO_FAIL();

      if (!(modes = (VkDisplayModePropertiesKHR*)calloc(mode_count, sizeof(*modes))))
         GOTO_FAIL();

      if (vkGetDisplayModePropertiesKHR(vk->context.gpu,
            display, &mode_count, modes) != VK_SUCCESS)
         GOTO_FAIL();

      for (i = 0; i < mode_count; i++)
      {
         const VkDisplayModePropertiesKHR *mode = &modes[i];
         if (vulkan_update_display_mode(dims, mode, info))
            best_mode = modes[i].displayMode;
      }

      free(modes);
      modes      = NULL;
      mode_count = 0;

      if (best_mode == VK_NULL_HANDLE)
         continue;

      for (i = 0; i < plane_count; i++)
      {
         uint32_t supported_count = 0;
         VkDisplayKHR *supported  = NULL;
         VkDisplayPlaneCapabilitiesKHR plane_caps;
         vkGetDisplayPlaneSupportedDisplaysKHR(vk->context.gpu, i, &supported_count, NULL);
         if (!supported_count)
            continue;

         if (!(supported = (VkDisplayKHR*)calloc(supported_count,
                     sizeof(*supported))))
            GOTO_FAIL();

         vkGetDisplayPlaneSupportedDisplaysKHR(vk->context.gpu, i, &supported_count,
               supported);

         for (j = 0; j < supported_count; j++)
         {
            if (supported[j] == display)
            {
               if (best_plane == UINT32_MAX)
                  best_plane = j;
               break;
            }
         }

         free(supported);
         supported = NULL;

         if (j == supported_count)
            continue;

         if (   (planes[i].currentDisplay == VK_NULL_HANDLE)
             || (planes[i].currentDisplay == display))
            best_plane = j;
         else
            continue;

         vkGetDisplayPlaneCapabilitiesKHR(vk->context.gpu,
               best_mode, i, &plane_caps);

         if (    plane_caps.supportedAlpha
               & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR)
         {
            best_plane = j;
            alpha_mode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
            goto out;
         }
      }
   }
out:

   if (     (best_plane    == UINT32_MAX)
         && (monitor_index != 0))
   {
      RARCH_WARN("[Vulkan] Could not find suitable surface for monitor index: %u.\n",
            monitor_index);
      RARCH_WARN("[Vulkan] Retrying first suitable monitor.\n");
      monitor_index = 0;
      best_mode = VK_NULL_HANDLE;
      *dims = saved_dims;
      goto retry;
   }

   if (best_mode == VK_NULL_HANDLE)
      GOTO_FAIL();
   if (best_plane == UINT32_MAX)
      GOTO_FAIL();

   create_info.sType              = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
   create_info.pNext              = NULL;
   create_info.flags              = 0;
   create_info.displayMode        = best_mode;
   create_info.planeIndex         = best_plane;
   create_info.planeStackIndex    = planes[best_plane].currentStackIndex;
   create_info.transform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   create_info.globalAlpha        = 1.0f;
   create_info.alphaMode          = alpha_mode;
   create_info.imageExtent.width  = VIDEO_SCALE_W(*dims);
   create_info.imageExtent.height = VIDEO_SCALE_H(*dims);

   if (vkCreateDisplayPlaneSurfaceKHR(vk->context.instance,
            &create_info, NULL, &vk->vk_surface) != VK_SUCCESS)
      GOTO_FAIL();

end:
   free(displays);
   free(planes);
   free(modes);
   return ret;
}

/* Waits, by fence, for every frame this context submitted - the only
 * work that references the swapchain images from this side. This is
 * what a swapchain teardown or rebuild has to outlive; the rest of the
 * device's work is a hardware core's, has nothing to do with the
 * swapchain, and is not drained for it. Needs no queue lock. */
static void vulkan_context_wait_frames(gfx_ctx_vulkan_data_t *vk)
{
   VkFence fences[VULKAN_MAX_SWAPCHAIN_IMAGES + 1];
   unsigned count     = 0;
   bool reset_present = false;
   unsigned i;

   if (vk->context.device == VK_NULL_HANDLE)
      return;
   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (     vk->context.swapchain_fences_signalled[i]
            && vk->context.swapchain_fences[i] != VK_NULL_HANDLE)
         fences[count++] = vk->context.swapchain_fences[i];
   }

   /* The presents are not on those fences. vkQueuePresentKHR returns
    * before the presentation engine has waited on the frame's
    * swapchain semaphore, and that wait is a queue operation, behind
    * the last frame's fence rather than under it. Destroying the
    * semaphore, or the swapchain, while that wait is pending is what
    * Mali's Android WSI answered with a failed
    * QueueSignalReleaseImageANDROID on every present after (#19601).
    * An empty submission behind the presents signals its fence once
    * they have executed; the rest of the device is still not
    * drained. */
   if (vk->context.present_pending)
   {
      VkFence present_fence = vk->context.present_fence;

      if (present_fence == VK_NULL_HANDLE)
      {
         VkFenceCreateInfo fence_info;
         fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
         fence_info.pNext = NULL;
         fence_info.flags = 0;
         if (vkCreateFence(vk->context.device, &fence_info, NULL,
                  &present_fence) != VK_SUCCESS)
            present_fence = VK_NULL_HANDLE;
         vk->context.present_fence = present_fence;
      }

      if (present_fence != VK_NULL_HANDLE)
      {
         VkResult res;
#ifdef HAVE_THREADS
         slock_lock(vk->context.queue_lock);
#endif
         res = vulkan_queue_signal_fence(vk->context.queue, present_fence);
#ifdef HAVE_THREADS
         slock_unlock(vk->context.queue_lock);
#endif
         if (res == VK_SUCCESS)
         {
            fences[count++] = present_fence;
            reset_present    = true;
         }
      }
      vk->context.present_pending = false;
   }

   if (count)
      vkWaitForFences(vk->context.device, count, fences, VK_TRUE,
            UINT64_MAX);
   if (reset_present)
      vkResetFences(vk->context.device, 1, &vk->context.present_fence);
}

static void vulkan_destroy_swapchain(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;
   unsigned j;

#ifdef VK_USE_PLATFORM_WIN32_KHR
   /* Exclusive mode is bound to the swapchain; release it first. */
   if (vk->fse_acquired && vk->fse_release && vk->swapchain != VK_NULL_HANDLE)
      ((PFN_vkReleaseFullScreenExclusiveModeEXT)vk->fse_release)(
            vk->context.device, vk->swapchain);
   vk->fse_acquired = false;
#endif

#if defined(_WIN32) && !defined(__WINRT__)
   if (vk->dxgi_presenter)
   {
      ra_vulkan_dxgi_destroy((ra_vulkan_dxgi_t *)vk->dxgi_presenter);
      vk->dxgi_presenter = NULL;
      vk->context.swapchain_external = false;
      vk->context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      memset(vk->context.swapchain_images, 0, sizeof(vk->context.swapchain_images));
   }
#endif

   vulkan_emulated_mailbox_deinit(&vk->mailbox);
   if (vk->swapchain != VK_NULL_HANDLE)
   {
      /* Our frames are the only submissions that touch its images.
       * The context teardown drains the device before it gets here;
       * a rebuild after an out-of-date acquire, or a resize, does
       * not, and a threaded hardware core's work goes on. */
      vulkan_context_wait_frames(vk);
      vulkan_low_latency_retire(vk, vk->swapchain);
      memset(vk->context.swapchain_images, 0, sizeof(vk->context.swapchain_images));
      vk->swapchain                      = VK_NULL_HANDLE;
      vk->context.flags                 &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
   }

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (vk->context.swapchain_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_semaphores[i], NULL);
      if (vk->context.swapchain_fences[i] != VK_NULL_HANDLE)
         vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
      if (vk->context.swapchain_recycled_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_recycled_semaphores[i], NULL);
      for (j = 0; j < vk->context.swapchain_num_wait_semaphores[i]; j++)
         if (vk->context.swapchain_wait_semaphores[i][j] != VK_NULL_HANDLE)
            vkDestroySemaphore(vk->context.device,
                  vk->context.swapchain_wait_semaphores[i][j], NULL);
      vk->context.swapchain_num_wait_semaphores[i] = 0;
      if (vk->context.swapchain_stale_acquire_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_stale_acquire_semaphores[i], NULL);
      vk->context.swapchain_stale_acquire_semaphores[i] = VK_NULL_HANDLE;
   }
   for (; i < 2 * VULKAN_MAX_SWAPCHAIN_IMAGES + 1; i++)
      if (vk->context.swapchain_recycled_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_recycled_semaphores[i], NULL);
   vk->context.num_stale_acquire_semaphores = 0;

   if (vk->context.swapchain_acquire_semaphore != VK_NULL_HANDLE)
      vkDestroySemaphore(vk->context.device,
            vk->context.swapchain_acquire_semaphore, NULL);
   vk->context.swapchain_acquire_semaphore = VK_NULL_HANDLE;

   memset(vk->context.swapchain_semaphores, 0,
         sizeof(vk->context.swapchain_semaphores));
   memset(vk->context.swapchain_recycled_semaphores, 0,
         sizeof(vk->context.swapchain_recycled_semaphores));
   memset(vk->context.swapchain_wait_semaphores, 0,
         sizeof(vk->context.swapchain_wait_semaphores));
   memset(vk->context.swapchain_fences, 0,
         sizeof(vk->context.swapchain_fences));
   vk->context.num_recycled_acquire_semaphores = 0;
}

bool vulkan_surface_destroy(gfx_ctx_vulkan_data_t *vk)
{
   if (!vk || !vk->context.instance)
      return false;

   vulkan_destroy_swapchain(vk);

   if (vk->vk_surface != VK_NULL_HANDLE)
   {
      vkDestroySurfaceKHR(vk->context.instance,
            vk->vk_surface, NULL);
      vk->vk_surface = VK_NULL_HANDLE;
   }

   return true;
}

static void vulkan_acquire_clear_fences(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;
   for (i = 0; i < vk->context.num_swapchain_images; i++)
   {
      if (vk->context.swapchain_fences[i])
      {
         vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
         vk->context.swapchain_fences[i]        = VK_NULL_HANDLE;
      }
      vk->context.swapchain_fences_signalled[i] = false;

      /* The device was drained before this (swapchain teardown), so
       * every waited semaphore is consumed and goes back to the
       * pool - the stale ones too. */
      {
         struct vulkan_context *ctx = &vk->context;
         unsigned j;
         for (j = 0; j < ctx->swapchain_num_wait_semaphores[i]; j++)
         {
            VkSemaphore sem = ctx->swapchain_wait_semaphores[i][j];
            if (sem == VK_NULL_HANDLE)
               continue;
            assert(ctx->num_recycled_acquire_semaphores < 2 * VULKAN_MAX_SWAPCHAIN_IMAGES + 1);
            ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++] = sem;
            ctx->swapchain_wait_semaphores[i][j] = VK_NULL_HANDLE;
         }
         ctx->swapchain_num_wait_semaphores[i] = 0;
      }
   }
   {
      struct vulkan_context *ctx = &vk->context;
      for (i = 0; i < ctx->num_stale_acquire_semaphores; i++)
      {
         assert(ctx->num_recycled_acquire_semaphores < 2 * VULKAN_MAX_SWAPCHAIN_IMAGES + 1);
         ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++] =
            ctx->swapchain_stale_acquire_semaphores[i];
         ctx->swapchain_stale_acquire_semaphores[i] = VK_NULL_HANDLE;
      }
      ctx->num_stale_acquire_semaphores = 0;
   }

   vk->context.current_frame_index = 0;
}

static VkSemaphore vulkan_get_wsi_acquire_semaphore(struct vulkan_context *ctx)
{
   VkSemaphore sem;

   if (ctx->num_recycled_acquire_semaphores == 0)
   {
      VkSemaphoreCreateInfo sem_info;

      sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      sem_info.pNext = NULL;
      sem_info.flags = 0;
      vkCreateSemaphore(ctx->device, &sem_info, NULL,
            &ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++]);
   }

   sem               =
      ctx->swapchain_recycled_semaphores[--ctx->num_recycled_acquire_semaphores];
   ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores] =
      VK_NULL_HANDLE;
   return sem;
}

static void vulkan_acquire_wait_fences(gfx_ctx_vulkan_data_t *vk)
{
   unsigned index;
   VkFence *next_fence             = NULL;

   /* Decouples the frame fence index from swapchain index. */
   vk->context.current_frame_index =
       (vk->context.current_frame_index + 1) %
       vk->context.num_swapchain_images;

   index                           = vk->context.current_frame_index;
   if (*(next_fence = &vk->context.swapchain_fences[index]) != VK_NULL_HANDLE)
   {
      if (vk->context.swapchain_fences_signalled[index])
         vkWaitForFences(vk->context.device, 1, next_fence, true, UINT64_MAX);
      vkResetFences(vk->context.device, 1, next_fence);
   }
   else
   {
      VkFenceCreateInfo fence_info;
      fence_info.sType                = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence_info.pNext                = NULL;
      fence_info.flags                = 0;
      vkCreateFence(vk->context.device, &fence_info, NULL, next_fence);
   }
   vk->context.swapchain_fences_signalled[index] = false;

   /* The fence covered the submission that waited on these, so
    * their signals are consumed and they can be acquired with. */
   {
      struct vulkan_context *ctx = &vk->context;
      unsigned j;
      for (j = 0; j < ctx->swapchain_num_wait_semaphores[index]; j++)
      {
         VkSemaphore sem = ctx->swapchain_wait_semaphores[index][j];
         if (sem == VK_NULL_HANDLE)
            continue;
         assert(ctx->num_recycled_acquire_semaphores < 2 * VULKAN_MAX_SWAPCHAIN_IMAGES + 1);
         ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++] = sem;
         ctx->swapchain_wait_semaphores[index][j] = VK_NULL_HANDLE;
      }
      ctx->swapchain_num_wait_semaphores[index] = 0;
   }
}

#ifdef HAVE_OPENXR
void vulkan_context_advance_frame(gfx_ctx_vulkan_data_t *vk)
{
   if (!vk || !vk->context.num_swapchain_images)
      return;

   vulkan_acquire_wait_fences(vk);
}
#endif

#if defined(HAVE_OPENXR) && defined(ANDROID)
void vulkan_context_finish_frames(gfx_ctx_vulkan_data_t *vk)
{
   if (vk)
      vulkan_context_wait_frames(vk);
}
#endif

unsigned vulkan_context_take_acquire_waits(struct vulkan_context *ctx,
      unsigned frame_index, VkSemaphore *sems,
      VkPipelineStageFlags *stages, VkPipelineStageFlags stage)
{
   unsigned n = 0;
   unsigned i;

   if (     (ctx->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && ctx->swapchain_acquire_semaphore != VK_NULL_HANDLE)
   {
      sems[n]                                = ctx->swapchain_acquire_semaphore;
      stages[n]                              = stage;
      ctx->swapchain_wait_semaphores[frame_index][n] = sems[n];
      ctx->swapchain_acquire_semaphore       = VK_NULL_HANDLE;
      n++;
   }
   for (i = 0; i < ctx->num_stale_acquire_semaphores; i++)
   {
      sems[n]                                = ctx->swapchain_stale_acquire_semaphores[i];
      stages[n]                              = stage;
      ctx->swapchain_wait_semaphores[frame_index][n] = sems[n];
      ctx->swapchain_stale_acquire_semaphores[i]     = VK_NULL_HANDLE;
      n++;
   }
   ctx->num_stale_acquire_semaphores            = 0;
   ctx->swapchain_num_wait_semaphores[frame_index] = n;
   return n;
}

static void vulkan_create_wait_fences(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;
   VkFenceCreateInfo fence_info;

   fence_info.sType                = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.pNext                = NULL;
   fence_info.flags                = 0;

   for (i = 0; i < vk->context.num_swapchain_images; i++)
   {
      if (!vk->context.swapchain_fences[i])
         vkCreateFence(vk->context.device, &fence_info, NULL,
               &vk->context.swapchain_fences[i]);
   }

   vk->context.current_frame_index = 0;
}

void vulkan_debug_mark_buffer(VkDevice device, VkBuffer buffer)
{
   /* a chain's objects may be made on the task thread while a frame
    * makes its own */
   static retro_atomic_int_t object_count;
   vulkan_debug_mark_object(device, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, "RetroArch buffer",
         (unsigned)retro_atomic_fetch_add_int(&object_count, 1) + 1);
}

void vulkan_debug_mark_image(VkDevice device, VkImage image)
{
   /* a chain's objects may be made on the task thread while a frame
    * makes its own */
   static retro_atomic_int_t object_count;
   vulkan_debug_mark_object(device, VK_OBJECT_TYPE_IMAGE, (uint64_t)image, "RetroArch image",
         (unsigned)retro_atomic_fetch_add_int(&object_count, 1) + 1);
}

void vulkan_debug_mark_memory(VkDevice device, VkDeviceMemory memory)
{
   /* a chain's objects may be made on the task thread while a frame
    * makes its own */
   static retro_atomic_int_t object_count;
   vulkan_debug_mark_object(device, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)memory, "RetroArch memory",
         (unsigned)retro_atomic_fetch_add_int(&object_count, 1) + 1);
}

/* A surface for @surface of a window system's @type, which
 * VULKAN_WSI_DISPLAY is not: that one needs the context's GPU. */
static bool vulkan_wsi_surface_create(VkInstance instance,
      enum vulkan_wsi_type type, void *display, void *surface,
      VkSurfaceKHR *out)
{
   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
#ifdef HAVE_WAYLAND
         {
            VkWaylandSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateWaylandSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateWaylandSurfaceKHR", create))
               return false;

            surf_info.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext   = NULL;
            surf_info.flags   = 0;
            surf_info.display = (struct wl_display*)display;
            surf_info.surface = (struct wl_surface*)surface;

            if (create(instance,
                     &surf_info, NULL, out) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_ANDROID:
#ifdef ANDROID
         {
            VkAndroidSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateAndroidSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateAndroidSurfaceKHR", create))
               return false;

            surf_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext  = NULL;
            surf_info.flags  = 0;
            surf_info.window = (ANativeWindow*)surface;

            if (create(instance,
                     &surf_info, NULL, out) != VK_SUCCESS)
            {
               RARCH_ERR("[Vulkan] Failed to create Android surface.\n");
               return false;
            }
            RARCH_LOG("[Vulkan] Created Android surface: %llu.\n",
                  (unsigned long long)*out);
         }
#endif
         break;
      case VULKAN_WSI_WIN32:
#ifdef _WIN32
         {
            VkWin32SurfaceCreateInfoKHR surf_info;
            PFN_vkCreateWin32SurfaceKHR create;

            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateWin32SurfaceKHR", create))
               return false;

            surf_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext     = NULL;
            surf_info.flags     = 0;
            surf_info.hinstance = *(const HINSTANCE*)display;
            surf_info.hwnd      = *(const HWND*)surface;

            if (create(instance,
                     &surf_info, NULL, out) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XLIB:
#ifdef HAVE_XLIB
         {
            VkXlibSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateXlibSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateXlibSurfaceKHR", create))
               return false;

            surf_info.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext  = NULL;
            surf_info.flags  = 0;
            surf_info.dpy    = (Display*)display;
            surf_info.window = *(const Window*)surface;

            if (create(instance,
                     &surf_info, NULL, out)
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XCB:
#ifdef HAVE_X11
#ifdef HAVE_XCB
         {
            VkXcbSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateXcbSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateXcbSurfaceKHR", create))
               return false;

            surf_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext      = NULL;
            surf_info.flags      = 0;
            /* The caller's connection, which need not be the one
             * its window was made on. */
            surf_info.connection = (xcb_connection_t*)display;
            surf_info.window     = *(const xcb_window_t*)surface;

            if (create(instance,
                     &surf_info, NULL, out)
                  != VK_SUCCESS)
               return false;
         }
#endif
#endif
         break;
      case VULKAN_WSI_MIR:
#ifdef HAVE_MIR
         {
            VkMirSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateMirSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateMirSurfaceKHR", create))
               return false;

            surf_info.sType      = VK_STRUCTURE_TYPE_MIR_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext      = NULL;
            surf_info.connection = display;
            surf_info.mirSurface = surface;

            if (create(instance,
                     &surf_info, NULL, out)
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_MVK_MACOS:
      case VULKAN_WSI_MVK_IOS:
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
         {
            VkMetalSurfaceCreateInfoEXT surf_info;
            PFN_vkCreateMetalSurfaceEXT create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateMetalSurfaceEXT", create))
               return false;

            surf_info.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
            surf_info.pNext  = NULL;
            surf_info.flags  = 0;
            surf_info.pLayer = surface;

            if (create(instance, &surf_info, NULL, out)
                != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_SDL3:
#ifdef HAVE_SDL3
         /* The SDL3 context driver passes its SDL_Window through the
          * 'surface' parameter; SDL picks the platform surface path. */
         if (!SDL_Vulkan_CreateSurface((SDL_Window*)surface, instance, NULL, out))
         {
            RARCH_ERR("[Vulkan] Failed to create SDL3 surface: %s.\n",
                  SDL_GetError());
            return false;
         }
#endif
         break;
      case VULKAN_WSI_DISPLAY:
      case VULKAN_WSI_NONE:
      default:
         return false;
   }

   return true;
}

bool vulkan_surface_create(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type,
      void *display, void *surface,
      unsigned dims, int8_t swap_interval)
{
   if (type == VULKAN_WSI_DISPLAY)
   {
      /* We need to decide on GPU here to be able to query support. */
      if (!vulkan_context_init_gpu(vk))
         return false;
      if (!vulkan_create_display_surface(vk,
               &dims,
               (const struct vulkan_display_surface_info*)display))
         return false;
   }
   else
   {
#if defined(_WIN32) && !defined(__WINRT__)
      if (type == VULKAN_WSI_WIN32)
         vk->dxgi_window = *(const HWND*)surface;
#endif
      if (!vulkan_wsi_surface_create(vk->context.instance, type,
               display, surface, &vk->vk_surface))
         return false;
   }

   /* Must create device after surface since we need to be able to query queues
    * to use for presentation. When replacing a lost surface, retain the
    * existing device and verify that its queue can present to the new one. */
   if (vk->context.device == VK_NULL_HANDLE)
   {
      if (!vulkan_context_init_device(vk))
         goto error_surface;
   }
   else
   {
      VkResult res;
      VkBool32 supported = VK_FALSE;

      res = vkGetPhysicalDeviceSurfaceSupportKHR(
            vk->context.gpu,
            vk->context.graphics_queue_index,
            vk->vk_surface, &supported);
      if (res != VK_SUCCESS || !supported)
      {
         RARCH_ERR("[Vulkan] Existing queue cannot present to replacement surface (err = %d).\n",
               (int)res);
         goto error_surface;
      }
   }

   if (!vulkan_create_swapchain(vk, dims, swap_interval))
      goto error_swapchain;

   vulkan_acquire_next_image(vk);
   return true;

error_swapchain:
   vulkan_destroy_swapchain(vk);
error_surface:
   if (vk->vk_surface != VK_NULL_HANDLE)
   {
      vkDestroySurfaceKHR(vk->context.instance,
            vk->vk_surface, NULL);
      vk->vk_surface = VK_NULL_HANDLE;
   }
   return false;
}

#ifdef HAVE_VIDEO_WINDOWS
/* How long a teardown waits for the presentation engine to give back an
 * image it was asked for: it may never, for a surface going away. */
#define VULKAN_OUTPUT_FINISH_NS 100000000

static void vulkan_output_destroy_set(VkDevice device,
      VkSwapchainKHR swapchain, VkImageView *views,
      VkFramebuffer *framebuffers, VkSemaphore *semaphores,
      unsigned num_images)
{
   unsigned i;
   for (i = 0; i < num_images; i++)
   {
      if (framebuffers[i] != VK_NULL_HANDLE)
         vkDestroyFramebuffer(device, framebuffers[i], NULL);
      if (views[i] != VK_NULL_HANDLE)
         vkDestroyImageView(device, views[i], NULL);
      if (semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(device, semaphores[i], NULL);
      framebuffers[i] = VK_NULL_HANDLE;
      views[i]        = VK_NULL_HANDLE;
      semaphores[i]   = VK_NULL_HANDLE;
   }
   if (swapchain != VK_NULL_HANDLE)
      vkDestroySwapchainKHR(device, swapchain, NULL);
}

/* Destroys each retired swapchain whose fence has signalled. True when
 * none is left. */
static bool vulkan_output_sweep(vulkan_output_t *output)
{
   unsigned i;
   bool clear      = true;
   VkDevice device = output->owner->device;
   for (i = 0; i < VULKAN_OUTPUT_RETIRED_MAX; i++)
   {
      struct vulkan_output_retired *r = &output->retired[i];
      if (!r->pending)
         continue;
      if (vkGetFenceStatus(device, r->fence) == VK_NOT_READY)
      {
         clear = false;
         continue;
      }
      vulkan_output_destroy_set(device, r->swapchain, r->views,
            r->framebuffers, r->semaphores, r->num_images);
      vkResetFences(device, 1, &r->fence);
      r->swapchain  = VK_NULL_HANDLE;
      r->num_images = 0;
      r->pending    = false;
   }
   return clear;
}

/* The current swapchain into a free retired slot, behind an empty
 * submission: its fence signals once everything queued so far has run,
 * the frames that drew into the swapchain and the waits of its
 * presents among it (see vulkan_context_wait_frames()). False when
 * both slots are busy. */
static bool vulkan_output_release(vulkan_output_t *output)
{
   unsigned i;
   VkResult res;
   struct vulkan_output_retired *r = NULL;
   vulkan_context_t *ctx           = output->owner;

   if (output->swapchain == VK_NULL_HANDLE)
      return true;
   for (i = 0; i < VULKAN_OUTPUT_RETIRED_MAX; i++)
   {
      if (!output->retired[i].pending)
      {
         r = &output->retired[i];
         break;
      }
   }
   if (!r)
      return false;

   r->swapchain  = output->swapchain;
   r->num_images = output->num_images;
   for (i = 0; i < output->num_images; i++)
   {
      r->views[i]        = output->views[i];
      r->framebuffers[i] = output->framebuffers[i];
      r->semaphores[i]   = output->semaphores[i];
   }
   output->swapchain  = VK_NULL_HANDLE;
   output->num_images = 0;
   output->flags     &= ~(VK_OUTPUT_FLAG_ACQUIRED | VK_OUTPUT_FLAG_READY);
   memset(output->images,       0, sizeof(output->images));
   memset(output->views,        0, sizeof(output->views));
   memset(output->framebuffers, 0, sizeof(output->framebuffers));
   memset(output->semaphores,   0, sizeof(output->semaphores));

#ifdef HAVE_THREADS
   slock_lock(ctx->queue_lock);
#endif
   res = vulkan_queue_signal_fence(ctx->queue, r->fence);
#ifdef HAVE_THREADS
   slock_unlock(ctx->queue_lock);
#endif
   /* Nothing to wait for on a queue that takes nothing: a lost device */
   if (res != VK_SUCCESS)
   {
      vulkan_output_destroy_set(ctx->device, r->swapchain, r->views,
            r->framebuffers, r->semaphores, r->num_images);
      r->swapchain  = VK_NULL_HANDLE;
      r->num_images = 0;
      return true;
   }
   r->pending = true;
   return true;
}

bool vulkan_output_resize(vulkan_output_t *output, unsigned dims)
{
   unsigned i;
   VkResult res;
   uint32_t count;
   uint32_t num_images;
   VkExtent2D extent;
   VkImageViewCreateInfo view;
   VkFramebufferCreateInfo fb;
   VkSemaphoreCreateInfo sem;
   VkSwapchainCreateInfoKHR info;
   VkSurfaceCapabilitiesKHR caps;
   VkSurfaceFormatKHR formats[64];
   VkPresentModeKHR modes[16];
   VkSwapchainKHR swapchain       = VK_NULL_HANDLE;
   VkPresentModeKHR present_mode  = VK_PRESENT_MODE_FIFO_KHR;
   bool format_ok                 = false;
   bool use_requested_extent      = false;
   vulkan_context_t *ctx          = output->owner;

   if (output->flags & VK_OUTPUT_FLAG_LOST)
      return false;
   vulkan_output_sweep(output);
   /* An image the presentation engine may still read can't go yet */
   if (     (output->flags & VK_OUTPUT_FLAG_ACQUIRED)
         && !(output->flags & VK_OUTPUT_FLAG_READY))
      return false;
   if (output->swapchain != VK_NULL_HANDLE)
   {
      for (i = 0; i < VULKAN_OUTPUT_RETIRED_MAX; i++)
         if (!output->retired[i].pending)
            break;
      if (i == VULKAN_OUTPUT_RETIRED_MAX)
         return false;
   }

   if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->gpu,
            output->surface, &caps) != VK_SUCCESS)
   {
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }
#ifdef ANDROID
   use_requested_extent = dims != 0;
#endif
   extent = vulkan_swapchain_extent(&caps,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims),
         use_requested_extent);
   if (!extent.width || !extent.height)
   {
      output->flags |= VK_OUTPUT_FLAG_RECREATE;
      return false;
   }

   /* The main output's format, so its render pass and the chains'
    * pipelines draw here too */
   count = sizeof(formats) / sizeof(formats[0]);
   vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->gpu, output->surface,
         &count, formats);
   if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
      format_ok = true;
   for (i = 0; i < count; i++)
      if (     formats[i].format     == ctx->swapchain_format
            && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
         format_ok = true;
   if (!format_ok)
   {
      RARCH_ERR("[Vulkan] A second output's surface has no format %d.\n",
            (int)ctx->swapchain_format);
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }

   count = sizeof(modes) / sizeof(modes[0]);
   vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->gpu, output->surface,
         &count, modes);
   for (i = 0; i < count; i++)
      if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
         present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

   /* One beyond the least, so a zero-timeout acquire finds one free */
   num_images = caps.minImageCount + 1;
   if (caps.maxImageCount && num_images > caps.maxImageCount)
      num_images = caps.maxImageCount;
   if (num_images > VULKAN_MAX_SWAPCHAIN_IMAGES)
      num_images = VULKAN_MAX_SWAPCHAIN_IMAGES;

   info.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   info.pNext                 = NULL;
   info.flags                 = 0;
   info.surface               = output->surface;
   info.minImageCount         = num_images;
   info.imageFormat           = ctx->swapchain_format;
   info.imageColorSpace       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
   info.imageExtent           = extent;
   info.imageArrayLayers      = 1;
   info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices   = NULL;
   info.preTransform          = (caps.supportedTransforms
         & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;
   if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
      info.compositeAlpha     = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
      info.compositeAlpha     = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
   else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
      info.compositeAlpha     = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
   else
      info.compositeAlpha     = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
   info.presentMode           = present_mode;
   info.clipped               = VK_TRUE;
   info.oldSwapchain          = output->swapchain;

   res = vkCreateSwapchainKHR(ctx->device, &info, NULL, &swapchain);
   /* The old one is retired by the call whether or not it worked */
   vulkan_output_release(output);
   if (res != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan] Could not make a second output's swapchain "
            "(err = %d).\n", (int)res);
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }
   output->swapchain = swapchain;

   count = 0;
   vkGetSwapchainImagesKHR(ctx->device, swapchain, &count, NULL);
   if (!count || count > VULKAN_MAX_SWAPCHAIN_IMAGES)
   {
      RARCH_ERR("[Vulkan] A second output's swapchain has %u images.\n",
            (unsigned)count);
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }
   vkGetSwapchainImagesKHR(ctx->device, swapchain, &count, output->images);
   output->num_images = count;

   view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext                           = NULL;
   view.flags                           = 0;
   view.image                           = VK_NULL_HANDLE;
   view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view.format                          = ctx->swapchain_format;
   view.components.r                    = VK_COMPONENT_SWIZZLE_R;
   view.components.g                    = VK_COMPONENT_SWIZZLE_G;
   view.components.b                    = VK_COMPONENT_SWIZZLE_B;
   view.components.a                    = VK_COMPONENT_SWIZZLE_A;
   view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.baseMipLevel   = 0;
   view.subresourceRange.levelCount     = 1;
   view.subresourceRange.baseArrayLayer = 0;
   view.subresourceRange.layerCount     = 1;
   fb.sType                             = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
   fb.pNext                             = NULL;
   fb.flags                             = 0;
   fb.renderPass                        = output->render_pass;
   fb.attachmentCount                   = 1;
   fb.pAttachments                      = NULL;
   fb.width                             = extent.width;
   fb.height                            = extent.height;
   fb.layers                            = 1;
   sem.sType                            = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   sem.pNext                            = NULL;
   sem.flags                            = 0;
   for (i = 0; i < count; i++)
   {
      view.image      = output->images[i];
      if (vkCreateImageView(ctx->device, &view, NULL,
               &output->views[i]) != VK_SUCCESS)
         break;
      fb.pAttachments = &output->views[i];
      if (vkCreateFramebuffer(ctx->device, &fb, NULL,
               &output->framebuffers[i]) != VK_SUCCESS)
         break;
      if (vkCreateSemaphore(ctx->device, &sem, NULL,
               &output->semaphores[i]) != VK_SUCCESS)
         break;
   }
   if (i < count)
   {
      RARCH_ERR("[Vulkan] Could not make a second output's image %u.\n", i);
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }

   output->format       = ctx->swapchain_format;
   output->present_mode = present_mode;
   output->dims         = VIDEO_SCALE_PACK(extent.width, extent.height);
   output->flags       &= ~VK_OUTPUT_FLAG_RECREATE;
   return true;
}

bool vulkan_output_create(vulkan_output_t *output,
      vulkan_context_t *owner, enum vulkan_wsi_type type,
      void *display, void *surface, unsigned dims)
{
   unsigned i;
   VkFenceCreateInfo fence_info;
   VkBool32 supported = VK_FALSE;

   output->owner      = owner;
   if (     !vulkan_wsi_surface_create(owner->instance, type, display,
            surface, &output->surface)
         || output->surface == VK_NULL_HANDLE)
   {
      RARCH_ERR("[Vulkan] Could not make a second output's surface.\n");
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }
   if (     vkGetPhysicalDeviceSurfaceSupportKHR(owner->gpu,
            owner->graphics_queue_index, output->surface, &supported)
            != VK_SUCCESS
         || !supported)
   {
      RARCH_ERR("[Vulkan] The device's queue cannot present to a second "
            "output.\n");
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }

   fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.pNext = NULL;
   fence_info.flags = 0;
   if (vkCreateFence(owner->device, &fence_info, NULL,
            &output->acquire_fence) != VK_SUCCESS)
   {
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }
   for (i = 0; i < VULKAN_OUTPUT_RETIRED_MAX; i++)
   {
      if (vkCreateFence(owner->device, &fence_info, NULL,
               &output->retired[i].fence) != VK_SUCCESS)
      {
         output->flags |= VK_OUTPUT_FLAG_LOST;
         return false;
      }
   }

   /* A zero extent is made later, by the acquire */
   return vulkan_output_resize(output, dims)
      || !(output->flags & VK_OUTPUT_FLAG_LOST);
}

bool vulkan_output_acquire(vulkan_output_t *output)
{
   VkResult res;
   VkDevice device = output->owner->device;

   if (output->flags & VK_OUTPUT_FLAG_LOST)
      return false;
   vulkan_output_sweep(output);
   if (!(output->flags & VK_OUTPUT_FLAG_ACQUIRED))
   {
      if (     (   (output->flags & VK_OUTPUT_FLAG_RECREATE)
                || output->swapchain == VK_NULL_HANDLE)
            && !vulkan_output_resize(output, output->dims))
         return false;
      if (output->flags & VK_OUTPUT_FLAG_FENCE_USED)
      {
         vkResetFences(device, 1, &output->acquire_fence);
         output->flags &= ~VK_OUTPUT_FLAG_FENCE_USED;
      }
      /* A fence, not a semaphore: nothing the presentation engine
       * signals ever goes into the queue, which the main output's
       * frames share. */
      res = vkAcquireNextImageKHR(device, output->swapchain, 0,
            VK_NULL_HANDLE, output->acquire_fence, &output->index);
      if (     res == VK_ERROR_OUT_OF_DATE_KHR
            && vulkan_output_resize(output, output->dims))
         res = vkAcquireNextImageKHR(device, output->swapchain, 0,
               VK_NULL_HANDLE, output->acquire_fence, &output->index);
      if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
      {
         if (res == VK_ERROR_OUT_OF_DATE_KHR)
            output->flags |= VK_OUTPUT_FLAG_RECREATE;
         else if (res != VK_NOT_READY && res != VK_TIMEOUT)
            output->flags |= VK_OUTPUT_FLAG_LOST;
         return false;
      }
      output->flags |= VK_OUTPUT_FLAG_ACQUIRED | VK_OUTPUT_FLAG_FENCE_USED;
   }
   if (     !(output->flags & VK_OUTPUT_FLAG_READY)
         && vkGetFenceStatus(device, output->acquire_fence) == VK_SUCCESS)
      output->flags |= VK_OUTPUT_FLAG_READY;
   return (output->flags & VK_OUTPUT_FLAG_READY) != 0;
}

bool vulkan_output_present(vulkan_output_t *output)
{
   VkPresentInfoKHR present;
   VkResult err;
   VkResult result         = VK_SUCCESS;
   uint32_t index          = output->index;
   vulkan_context_t *ctx   = output->owner;

   if (!(output->flags & VK_OUTPUT_FLAG_READY))
      return false;

   present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   present.pNext              = NULL;
   present.waitSemaphoreCount = 1;
   present.pWaitSemaphores    = &output->semaphores[index];
   present.swapchainCount     = 1;
   present.pSwapchains        = &output->swapchain;
   present.pImageIndices      = &index;
   present.pResults           = &result;
#ifdef HAVE_THREADS
   slock_lock(ctx->queue_lock);
#endif
   err = vkQueuePresentKHR(ctx->queue, &present);
#ifdef HAVE_THREADS
   slock_unlock(ctx->queue_lock);
#endif
   output->flags &= ~(VK_OUTPUT_FLAG_ACQUIRED | VK_OUTPUT_FLAG_READY);

   /* As the main output: one rebuild for suboptimal, then the surface's
    * standing answer (Android without prerotate) is kept */
   if (err == VK_SUBOPTIMAL_KHR || result == VK_SUBOPTIMAL_KHR)
   {
      if (!(output->flags & VK_OUTPUT_FLAG_SUBOPTIMAL_REBUILT))
      {
         output->flags |= VK_OUTPUT_FLAG_SUBOPTIMAL_REBUILT
                        | VK_OUTPUT_FLAG_RECREATE;
         vulkan_output_resize(output, output->dims);
      }
      return true;
   }
   if (err == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
   {
      output->flags |= VK_OUTPUT_FLAG_RECREATE;
      vulkan_output_resize(output, output->dims);
      return true;
   }
   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      output->flags |= VK_OUTPUT_FLAG_LOST;
      return false;
   }
   output->flags &= ~VK_OUTPUT_FLAG_SUBOPTIMAL_REBUILT;
   return true;
}

bool vulkan_output_retire(vulkan_output_t *output)
{
   unsigned i;
   VkDevice device;

   if (!output->owner)
      return true;
   device = output->owner->device;
   if (     (output->flags & VK_OUTPUT_FLAG_ACQUIRED)
         && !(output->flags & VK_OUTPUT_FLAG_READY))
   {
      if (vkGetFenceStatus(device, output->acquire_fence) == VK_NOT_READY)
         return false;
      output->flags |= VK_OUTPUT_FLAG_READY;
   }
   if (!vulkan_output_release(output))
   {
      vulkan_output_sweep(output);
      return false;
   }
   if (!vulkan_output_sweep(output))
      return false;

   if (     output->acquire_fence != VK_NULL_HANDLE
         && !(output->flags & VK_OUTPUT_FLAG_FENCE_PENDING))
      vkDestroyFence(device, output->acquire_fence, NULL);
   for (i = 0; i < VULKAN_OUTPUT_RETIRED_MAX; i++)
      if (output->retired[i].fence != VK_NULL_HANDLE)
         vkDestroyFence(device, output->retired[i].fence, NULL);
   if (output->surface != VK_NULL_HANDLE)
      vkDestroySurfaceKHR(output->owner->instance, output->surface, NULL);
   memset(output, 0, sizeof(*output));
   return true;
}

void vulkan_output_finish(vulkan_output_t *output)
{
   unsigned i;
   VkDevice device;

   if (!output->owner)
      return;
   device = output->owner->device;
   if (     (output->flags & VK_OUTPUT_FLAG_ACQUIRED)
         && !(output->flags & VK_OUTPUT_FLAG_READY))
   {
      if (vkWaitForFences(device, 1, &output->acquire_fence, VK_TRUE,
               VULKAN_OUTPUT_FINISH_NS) != VK_SUCCESS)
         output->flags |= VK_OUTPUT_FLAG_FENCE_PENDING;
      output->flags |= VK_OUTPUT_FLAG_READY;
   }
   for (i = 0; i < VULKAN_OUTPUT_RETIRED_MAX; i++)
      if (output->retired[i].pending)
         vkWaitForFences(device, 1, &output->retired[i].fence, VK_TRUE,
               UINT64_MAX);
   vulkan_output_sweep(output);
   vulkan_output_release(output);
   for (i = 0; i < VULKAN_OUTPUT_RETIRED_MAX; i++)
      if (output->retired[i].pending)
         vkWaitForFences(device, 1, &output->retired[i].fence, VK_TRUE,
               UINT64_MAX);
}
#endif

uint32_t vulkan_find_memory_type(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if (     (device_reqs & (1u << i))
            && (mem_props->memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   RARCH_ERR("[Vulkan] Failed to find valid memory type. This should never happen.");
   abort();
}

VkResult vulkan_queue_signal_fence(VkQueue queue, VkFence fence)
{
   VkSubmitInfo submit;
   submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.pNext                = NULL;
   submit.waitSemaphoreCount   = 0;
   submit.pWaitSemaphores      = NULL;
   submit.pWaitDstStageMask    = NULL;
   submit.commandBufferCount   = 0;
   submit.pCommandBuffers      = NULL;
   submit.signalSemaphoreCount = 0;
   submit.pSignalSemaphores    = NULL;
   return vkQueueSubmit(queue, 1, &submit, fence);
}

VkResult vulkan_allocate_cpu_write_memory(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t type_bits, const VkMemoryAllocateInfo *alloc,
      VkDeviceMemory *memory)
{
   uint32_t types[2];
   unsigned i;
   VkResult res = VK_ERROR_OUT_OF_DEVICE_MEMORY;
   VkMemoryAllocateInfo info = *alloc;
   const unsigned n = vulkan_cpu_write_memory_types(mem_props, type_bits,
         alloc->memoryTypeIndex, types);

   /* The device-local type first where it is the whole of video memory;
    * a refusal there falls through to the caller's own choice. */
   for (i = 0; i < n; i++)
   {
      info.memoryTypeIndex = types[i];
      if ((res = vkAllocateMemory(device, &info, NULL, memory)) == VK_SUCCESS)
         return res;
   }
   *memory = VK_NULL_HANDLE;
   return res;
}

uint32_t vulkan_find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs_first,
      uint32_t host_reqs_second)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if (     (device_reqs & (1u << i))
            && (mem_props->memoryTypes[i].propertyFlags & host_reqs_first) == host_reqs_first)
         return i;
   }

   if (host_reqs_first == 0)
   {
      RARCH_ERR("[Vulkan] Failed to find valid memory type. This should never happen.");
      abort();
   }

   return vulkan_find_memory_type_fallback(mem_props,
         device_reqs, host_reqs_second, 0);
}

/* The stale list is full: consume every signal on it, so the
 * semaphores can be destroyed and the list starts again.
 *
 * A pending acquire signal is consumed by a submission that waits on
 * it, and a batch may wait without running anything. So one batch
 * waits on the whole list, with a fence, and the fence is waited on
 * here. queue_lock is held for the submit call and not a moment
 * longer.
 *
 * This was vkDeviceWaitIdle followed by destroying one semaphore. That
 * is a use of every queue of the device made without queue_lock, while
 * a hardware core may be submitting on the queue from its own thread;
 * it waited for the core's work as well as ours; and an acquire is not
 * queue work, so it did not even wait for the signal it then destroyed
 * the semaphore under. */
static void vulkan_consume_stale_acquires(gfx_ctx_vulkan_data_t *vk)
{
   VkSubmitInfo submit;
   VkFenceCreateInfo fence_info;
   VkPipelineStageFlags stages[VULKAN_MAX_SWAPCHAIN_IMAGES];
   struct vulkan_context *ctx = &vk->context;
   VkFence fence              = VK_NULL_HANDLE;
   VkResult res;
   unsigned i;

   fence_info.sType            = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.pNext            = NULL;
   fence_info.flags            = 0;
   if (vkCreateFence(ctx->device, &fence_info, NULL, &fence) != VK_SUCCESS)
      fence = VK_NULL_HANDLE;

   for (i = 0; i < ctx->num_stale_acquire_semaphores; i++)
      stages[i]                = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

   submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.pNext                = NULL;
   submit.waitSemaphoreCount   = ctx->num_stale_acquire_semaphores;
   submit.pWaitSemaphores      = ctx->swapchain_stale_acquire_semaphores;
   submit.pWaitDstStageMask    = stages;
   submit.commandBufferCount   = 0;
   submit.pCommandBuffers      = NULL;
   submit.signalSemaphoreCount = 0;
   submit.pSignalSemaphores    = NULL;

#ifdef HAVE_THREADS
   slock_lock(ctx->queue_lock);
#endif
   res = (fence != VK_NULL_HANDLE)
      ? vkQueueSubmit(ctx->queue, 1, &submit, fence)
      : VK_ERROR_OUT_OF_HOST_MEMORY;
#ifdef HAVE_THREADS
   slock_unlock(ctx->queue_lock);
#endif

   if (res == VK_SUCCESS)
   {
      vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, UINT64_MAX);
      for (i = 0; i < ctx->num_stale_acquire_semaphores; i++)
         vkDestroySemaphore(ctx->device,
               ctx->swapchain_stale_acquire_semaphores[i], NULL);
   }
   /* else: nothing waits on them and their signals stay pending.
    * They cannot be destroyed or acquired with again; the handles are
    * dropped, and the device teardown takes them. */
   if (fence != VK_NULL_HANDLE)
      vkDestroyFence(ctx->device, fence, NULL);

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
      ctx->swapchain_stale_acquire_semaphores[i] = VK_NULL_HANDLE;
   ctx->num_stale_acquire_semaphores = 0;
}

void vulkan_acquire_next_image(gfx_ctx_vulkan_data_t *vk)
{
   unsigned index;
   VkFenceCreateInfo fence_info;
   VkSemaphoreCreateInfo sem_info;
   VkResult err                   = VK_SUCCESS;
   VkFence fence                  = VK_NULL_HANDLE;
   VkSemaphore semaphore          = VK_NULL_HANDLE;
   bool is_retrying               = false;

   fence_info.sType               = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.pNext               = NULL;
   fence_info.flags               = 0;

   sem_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   sem_info.pNext                 = NULL;
   sem_info.flags                 = 0;

retry:
   if (!vulkan_has_swapchain(vk))
   {
      /* We don't have a swapchain, try to create one now. */
      if (!vulkan_create_swapchain(vk, vk->context.swapchain_dims,
               vk->context.swap_interval))
      {
#ifdef VULKAN_DEBUG
         RARCH_ERR("[Vulkan] Failed to create new swapchain.\n");
#endif
         /* No sleep here. A window with no swapchain is paced by the
          * runloop, which asks the context driver whether it has
          * anything to present (gfx_ctx_driver_t::presentable) and
          * waits a frame when it has not - one throttle, at the layer
          * that knows what else is already holding the loop. A sleep
          * in here would stack on top of it.
          *
          * The frame is still drawn, minimised or not, so it moves on
          * to the next frame slot and waits for that slot's last
          * submission, as an acquire does: the frame re-records the
          * slot's command buffer and rewrites its buffers and
          * descriptors, and the slot's fence is what says the GPU is
          * done with them. */
         if (vk->context.num_swapchain_images)
            vulkan_acquire_wait_fences(vk);
         return;
      }

      if (!vulkan_has_swapchain(vk))
      {
         /* We still don't have a swapchain, so just fake it ... */
         vk->context.current_swapchain_index = 0;
         vk->context.current_frame_index     = 0;
         vulkan_acquire_clear_fences(vk);
         vulkan_acquire_wait_fences(vk);
         vk->context.flags                  |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
         return;
      }
   }

   retro_assert(!(vk->context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN));

   if (vk->flags & VK_DATA_FLAG_EMULATING_MAILBOX)
   {
      /* Non-blocking acquire. If we don't get a swapchain frame right away,
       * just skip rendering to the swapchain this frame, similar to what
       * MAILBOX would do. */
      if (vk->mailbox.swapchain == VK_NULL_HANDLE)
         err   = VK_ERROR_OUT_OF_DATE_KHR;
      else
         err   = vulkan_emulated_mailbox_acquire_next_image(
               &vk->mailbox, &vk->context.current_swapchain_index);
   }
   else
   {
      if ((vk->flags & VK_DATA_FLAG_USE_WSI_SEMAPHORE)
            || vk->context.swapchain_external)
          semaphore = vulkan_get_wsi_acquire_semaphore(&vk->context);
      else
          vkCreateFence(vk->context.device, &fence_info, NULL, &fence);

#if defined(_WIN32) && !defined(__WINRT__)
      if (vk->dxgi_presenter)
      {
         err = ra_vulkan_dxgi_acquire((ra_vulkan_dxgi_t *)vk->dxgi_presenter,
               semaphore, fence, &vk->context.current_swapchain_index,
               vk->context.swap_interval > 0 ? (unsigned)vk->context.swap_interval : 0);
      }
      else
#endif
         err = vkAcquireNextImageKHR(vk->context.device,
               vk->swapchain, UINT64_MAX,
               semaphore, fence, &vk->context.current_swapchain_index);
   }

#ifdef __APPLE__
   /* MoltenVK signals this fence from the drawable's presented handler,
    * which macOS drops for a window hidden with presents in flight:
    * rebuild the swapchain rather than wait forever. MoltenVK holds its
    * own reference, so the fence is destroyed below as usual. */
   if (     (err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR)
         && fence != VK_NULL_HANDLE
         && vkWaitForFences(vk->context.device, 1, &fence, true,
               (uint64_t)vk->acquire_timeout_us * 1000) == VK_TIMEOUT)
      err = VK_ERROR_OUT_OF_DATE_KHR;
#endif

   if (err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR)
   {
#ifndef __APPLE__
      if (fence != VK_NULL_HANDLE)
         vkWaitForFences(vk->context.device, 1, &fence, true, UINT64_MAX);
#endif
      vk->context.flags |= VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;

      if (vk->context.swapchain_acquire_semaphore)
      {
         /* The previous acquire was never submitted against: its
          * signal is still pending, so it can neither be acquired
          * with again nor destroyed. It goes on the stale list, and
          * the next submission waits on it along with its own
          * acquire - that consumes the signal, and it recycles with
          * that frame. When frames have gone unsubmitted for a whole
          * swapchain's worth the list is full, and it is emptied by
          * the same means: see vulkan_consume_stale_acquires(). */
         VkSemaphore old_sem                     = vk->context.swapchain_acquire_semaphore;
         vk->context.swapchain_acquire_semaphore = semaphore;
         if (vk->context.num_stale_acquire_semaphores == VULKAN_MAX_SWAPCHAIN_IMAGES)
            vulkan_consume_stale_acquires(vk);
         vk->context.swapchain_stale_acquire_semaphores[
            vk->context.num_stale_acquire_semaphores++] = old_sem;
      }
      else
         vk->context.swapchain_acquire_semaphore = semaphore;
   }
   else
   {
      vk->context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      if (semaphore)
      {
         struct vulkan_context *ctx = &vk->context;
         VkSemaphore sem            = semaphore;
         assert(ctx->num_recycled_acquire_semaphores < VULKAN_MAX_SWAPCHAIN_IMAGES);
         ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++] = sem;
      }
   }

#ifdef WSI_HARDENING_TEST
   trigger_spurious_error_vkresult(&err);
#endif

#if defined(_WIN32) && !defined(__WINRT__)
   if (vk->dxgi_presenter && err != VK_SUCCESS && err != VK_SUBOPTIMAL_KHR
         && err != VK_NOT_READY && err != VK_TIMEOUT)
      vk->dxgi_failed = true;
#endif

   if (fence != VK_NULL_HANDLE)
      vkDestroyFence(vk->context.device, fence, NULL);

   switch (err)
   {
      case VK_NOT_READY:
      case VK_TIMEOUT:
      case VK_SUBOPTIMAL_KHR:
         /* Do nothing. */
         break;
      case VK_ERROR_OUT_OF_DATE_KHR:
#ifdef VK_USE_PLATFORM_WIN32_KHR
      case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
         /* Alt-tab or a mode switch under Forced negotiation. Same
          * recovery as out-of-date: rebuild and re-acquire. */
#endif
         /* Throw away the old swapchain and try again - once. A
          * swapchain that is out of date the moment it is made is a
          * window still changing under us; the frame goes without an
          * image, exactly as when no swapchain could be made, and the
          * next frame's acquire builds a new one. That used to be a
          * 10 ms sleep and another go, repeated for as long as the
          * window kept moving, on the thread that draws. */
         vulkan_destroy_swapchain(vk);
         vulkan_acquire_clear_fences(vk);
         if (!is_retrying)
         {
            is_retrying = true;
            goto retry;
         }
         vk->context.current_swapchain_index = 0;
         vk->context.current_frame_index     = 0;
         vulkan_acquire_wait_fences(vk);
         vk->context.flags                  |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
         return;
      default:
         if (err != VK_SUCCESS)
         {
            /* We are screwed, don't try anymore. Maybe it will work later. */
            vulkan_destroy_swapchain(vk);
            RARCH_ERR("[Vulkan] Failed to acquire from swapchain (err = %d).\n",
                  (int)err);
            if (err == VK_ERROR_DEVICE_LOST)
               video_driver_modify_disp_flags(VIDEO_FLAG_GPU_DEVICE_LOST, 0);
            if (err == VK_ERROR_SURFACE_LOST_KHR)
               RARCH_ERR("[Vulkan] Got VK_ERROR_SURFACE_LOST_KHR.\n");
            /* Force driver to reset swapchain image handles. */
            vk->context.flags |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
            vulkan_acquire_clear_fences(vk);
            return;
         }
         break;
   }

   index = vk->context.current_swapchain_index;
   if (vk->context.swapchain_semaphores[index] == VK_NULL_HANDLE)
      vkCreateSemaphore(vk->context.device, &sem_info,
            NULL, &vk->context.swapchain_semaphores[index]);
   vulkan_acquire_wait_fences(vk);
}

#ifdef VULKAN_HDR_SWAPCHAIN
bool vulkan_is_hdr10_format(VkFormat format)
{
   return
   (
         format == VK_FORMAT_A2B10G10R10_UNORM_PACK32
      || format == VK_FORMAT_A2R10G10B10_UNORM_PACK32
   );
}
#endif /* VULKAN_HDR_SWAPCHAIN */

bool vulkan_create_swapchain(gfx_ctx_vulkan_data_t *vk,
      unsigned dims, int8_t swap_interval)
{
   unsigned i;
   uint32_t format_count;
   uint32_t desired_swapchain_images;
   VkSurfaceCapabilitiesKHR surface_properties;
   VkSurfaceFormatKHR formats[256];
   VkPresentModeKHR present_modes[16];
   VkExtent2D swapchain_size;
   VkSurfaceFormatKHR format;
   VkSwapchainKHR old_swapchain;
   VkSwapchainCreateInfoKHR info;
   VkSwapchainLatencyCreateInfoNV latency_info;
   VkSurfaceTransformFlagBitsKHR pre_transform;
   uint32_t present_mode_count             = 0;
   VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
   VkCompositeAlphaFlagBitsKHR composite   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   /* Never the settings: this can run on the thread that draws */
   const vulkan_swapchain_settings_t *ss   = &vk->context.swapchain_settings;
   bool vsync                              = ss->vsync;
   bool adaptive_vsync                     = ss->adaptive_vsync;
   bool reuse_swapchain;
#ifdef ANDROID
   bool use_requested_extent              = vk->wsi_type == VULKAN_WSI_ANDROID;
#else
   bool use_requested_extent              = false;
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
   bool video_windowed_fullscreen          = ss->windowed_fullscreen;
   /* Relaxed: ALLOWED is a hint and the driver may decline - and on
    * NVIDIA it does, leaving the swapchain on DWM's independent-flip
    * path with the setting silently inert (PresentMon reports
    * "Hardware Composed: Independent Flip" in both windowed-fullscreen
    * states). Forced: APPLICATION_CONTROLLED, followed by an explicit
    * vkAcquireFullScreenExclusiveModeEXT once the swapchain exists. */
   bool fse_forced                         =
         !video_windowed_fullscreen
      && ss->fse_negotiation == VIDEO_FSE_FORCED;
   HMONITOR hmonitor;
   /* Assigned rather than initialised: the exclusive mode depends on
    * two settings read above, and C89 wants an initialiser it can
    * compute at load time. */
   VkSurfaceFullScreenExclusiveInfoEXT fse_info;
   VkSurfaceFullScreenExclusiveWin32InfoEXT fse_win32_info;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
   fse_info.sType                          =
      VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
   fse_info.pNext                          = NULL;
   fse_info.fullScreenExclusive            = video_windowed_fullscreen
      || !(vk->flags & VK_DATA_FLAG_FULLSCREEN)
      ? VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT
      : (fse_forced
            ? VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT
            : VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT);
   fse_win32_info.sType                    =
      VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
   fse_win32_info.pNext                    = NULL;
   fse_win32_info.hmonitor                 = NULL;
#endif

   format.format                           = VK_FORMAT_UNDEFINED;
   format.colorSpace                       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->context.gpu,
         vk->vk_surface, &surface_properties);

   /* Skip creation when window is minimized */
   if (   !surface_properties.currentExtent.width
       && !surface_properties.currentExtent.height)
      return false;

   if (     (swap_interval == 0)
         && (vk->flags & VK_DATA_FLAG_EMULATE_MAILBOX)
         && vsync)
   {
      RARCH_LOG("[Vulkan] swap_interval 0 (vsync off) overridden to %d because "
            "VK_DATA_FLAG_EMULATE_MAILBOX requires non-zero swap_interval.\n",
            adaptive_vsync ? -1 : 1);
      swap_interval  =  (adaptive_vsync) ? -1 : 1;
      vk->flags     |=  VK_DATA_FLAG_EMULATING_MAILBOX;
   }
   else
      vk->flags     &= ~VK_DATA_FLAG_EMULATING_MAILBOX;

   /* Resolve the present mode before deciding whether a new swapchain
    * is needed at all.  A swap_interval change only matters to the
    * swapchain through the present mode it resolves to; on a surface
    * that offers FIFO alone every interval resolves to FIFO, and
    * intervals above 1 are emulated by frame duplication in the video
    * driver, never by the swapchain. */
   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, NULL);
   if (present_mode_count < 1 || present_mode_count > 16)
   {
      RARCH_ERR("[Vulkan] Bogus present modes found.\n");
      return false;
   }
   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, present_modes);

   for (i = 0; i < present_mode_count; i++)
      vk->context.present_modes[i] = present_modes[i];

   /* Settled here, for whoever asks the context for its flags */
   {
      int relaxed = 0;
      for (i = 0; i < present_mode_count; i++)
      {
         if (present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
         {
            relaxed = 1;
            break;
         }
      }
      retro_atomic_store_release_int(
            &vk->context.supports_adaptive_vsync, relaxed);
   }

   /* Prefer IMMEDIATE without vsync */
   for (i = 0; i < present_mode_count; i++)
   {
      if (     !swap_interval
            && !vsync
            && present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
      {
         swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
         break;
      }

      if (     swap_interval < 0
            && present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
      {
         swapchain_present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
         break;
      }
   }

   /* If still in FIFO with no swap interval, try MAILBOX */
   for (i = 0; i < present_mode_count; i++)
   {
      if (     !swap_interval
            && swapchain_present_mode == VK_PRESENT_MODE_FIFO_KHR
            && present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
      {
         swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
         break;
      }
   }

#if defined(_WIN32) && !defined(__WINRT__)
   if (vk->dxgi_presenter)
      vk->flags &= ~VK_DATA_FLAG_EMULATING_MAILBOX;
#endif
   reuse_swapchain = vulkan_has_swapchain(vk)
         && (!(vk->context.flags & VK_CTX_FLAG_INVALID_SWAPCHAIN))
         &&   (vk->context.swapchain_dims == dims)
         &&   (   (vk->context.swap_interval          == swap_interval)
               || (vk->context.swapchain_present_mode == swapchain_present_mode));

   if (     reuse_swapchain
         && ((vk->flags & VK_DATA_FLAG_EMULATING_MAILBOX) != 0)
               == (vk->mailbox.swapchain != VK_NULL_HANDLE))
   {
      vk->context.swap_interval = swap_interval;
      vk->flags &= ~VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;
      return true;
   }

   /* Replacing the swapchain or changing the acquire path needs the
    * old submissions drained before their fences are cleared. */
   vulkan_context_wait_frames(vk);
#if defined(_WIN32) && !defined(__WINRT__)
   if (!(ss->dxgi_present && !vk->dxgi_failed && vk->dxgi_presenter
            && vk->wsi_type == VULKAN_WSI_WIN32))
#endif
      vulkan_acquire_clear_fences(vk);

   vk->flags |= VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;

   if (reuse_swapchain)
   {
      /* Do not bother creating a swapchain redundantly.  The interval
       * still takes effect: the driver reads it for frame duplication,
       * and the mailbox emulation below is keyed off it. */
      vk->context.swap_interval = swap_interval;
#ifdef VULKAN_DEBUG
      RARCH_DBG("[Vulkan] Do not need to re-create swapchain.\n");
#endif
      vulkan_create_wait_fences(vk);

      if (     (vk->flags & VK_DATA_FLAG_EMULATING_MAILBOX)
            && (vk->mailbox.swapchain == VK_NULL_HANDLE))
      {
         if (!vulkan_emulated_mailbox_init(
               &vk->mailbox, vk->context.device, vk->swapchain))
         {
            RARCH_WARN("[Vulkan] Failed to initialize emulated mailbox -- "
                  "falling back to blocking acquire.\n");
            vk->flags &= ~VK_DATA_FLAG_EMULATING_MAILBOX;
         }
         vk->flags                &= ~VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;
         return true;
      }
      else if (
               (!(vk->flags & VK_DATA_FLAG_EMULATING_MAILBOX))
            &&   (vk->mailbox.swapchain != VK_NULL_HANDLE))
      {
         VkResult res = VK_SUCCESS;
         /* We are tearing down, and entering a state
          * where we are supposed to have
          * acquired an image, so block until we have acquired. */
         if (! (vk->context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN))
            if (vk->mailbox.swapchain != VK_NULL_HANDLE)
               res = vulkan_emulated_mailbox_acquire_next_image_blocking(
                     &vk->mailbox,
                     &vk->context.current_swapchain_index);

         vulkan_emulated_mailbox_deinit(&vk->mailbox);

         if (res == VK_SUCCESS)
         {
            vk->context.flags |=  VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
            vk->flags         &= ~VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;
            return true;
         }

         /* We failed for some reason, so create a new swapchain. */
         vk->context.flags    &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      }
      else
      {
         vk->flags &= ~VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;
#ifdef VULKAN_HDR_SWAPCHAIN
         video_driver_set_hdr_output_mode(
               vk->context.swapchain_colour_space == VK_COLOR_SPACE_HDR10_ST2084_EXT
               ? 1 : (vk->context.swapchain_colour_space == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
               ? 2 : 0));
#else
         video_driver_set_hdr_output_mode(0);
#endif
         return true;
      }
   }

   vulkan_emulated_mailbox_deinit(&vk->mailbox);

   vk->context.swap_interval = swap_interval;

   /* Present mode logging */
   if (vk->swapchain == VK_NULL_HANDLE)
   {
      for (i = 0; i < present_mode_count; i++)
      {
         switch (present_modes[i])
         {
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
               RARCH_DBG("[Vulkan] Swapchain supports present mode: IMMEDIATE.\n");
               break;
            case VK_PRESENT_MODE_MAILBOX_KHR:
               RARCH_DBG("[Vulkan] Swapchain supports present mode: MAILBOX.\n");
               break;
            case VK_PRESENT_MODE_FIFO_KHR:
               RARCH_DBG("[Vulkan] Swapchain supports present mode: FIFO.\n");
               break;
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
               RARCH_DBG("[Vulkan] Swapchain supports present mode: FIFO_RELAXED.\n");
               break;
            default:
               break;
         }
      }
   }
   else
   {
      switch (swapchain_present_mode)
      {
         case VK_PRESENT_MODE_IMMEDIATE_KHR:
            RARCH_DBG("[Vulkan] Creating swapchain with present mode: IMMEDIATE.\n");
            break;
         case VK_PRESENT_MODE_MAILBOX_KHR:
            RARCH_DBG("[Vulkan] Creating swapchain with present mode: MAILBOX.\n");
            break;
         case VK_PRESENT_MODE_FIFO_KHR:
            RARCH_DBG("[Vulkan] Creating swapchain with present mode: FIFO.\n");
            break;
         case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            RARCH_DBG("[Vulkan] Creating swapchain with present mode: FIFO_RELAXED.\n");
            break;
         default:
            break;
      }
   }

   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, NULL);
   if (format_count > ARRAY_SIZE(formats))
      format_count = ARRAY_SIZE(formats);
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, formats);

#if defined(VK_USE_PLATFORM_WIN32_KHR) && defined(VULKAN_HDR_SWAPCHAIN)
   /* Hybrid GPU workaround (e.g. Nvidia Optimus / AMD switchable).
    * The display is often physically connected to the integrated GPU,
    * so the discrete GPU may not report HDR surface formats even though
    * the display supports HDR.  Query all other physical devices for
    * HDR formats and merge them in — on Windows the DWM compositor
    * handles presentation, so HDR formats reported by ANY device for
    * this surface will work. */
   if (vk->context.flags & VK_CTX_FLAG_HDR_SUPPORT)
   {
      bool have_hdr = false;
      for (i = 0; i < format_count && !have_hdr; i++)
      {
         if (  (  vulkan_is_hdr10_format(formats[i].format)
               && formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
            || (  formats[i].format     == VK_FORMAT_R16G16B16A16_SFLOAT
               && formats[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT))
            have_hdr = true;
      }

      if (!have_hdr)
      {
         uint32_t gpu_count = 0;
         VkPhysicalDevice gpus_all[8];

         vkEnumeratePhysicalDevices(vk->context.instance, &gpu_count, NULL);
         if (gpu_count > ARRAY_SIZE(gpus_all))
            gpu_count = ARRAY_SIZE(gpus_all);
         vkEnumeratePhysicalDevices(vk->context.instance, &gpu_count, gpus_all);

         for (i = 0; i < gpu_count; i++)
         {
            uint32_t j, other_count = 0;
            VkSurfaceFormatKHR other_formats[256];
            VkPhysicalDeviceProperties props;

            if (gpus_all[i] == vk->context.gpu)
               continue;

            vkGetPhysicalDeviceSurfaceFormatsKHR(
                  gpus_all[i], vk->vk_surface, &other_count, NULL);
            if (other_count == 0)
               continue;
            if (other_count > ARRAY_SIZE(other_formats))
               other_count = ARRAY_SIZE(other_formats);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                  gpus_all[i], vk->vk_surface, &other_count, other_formats);

            vkGetPhysicalDeviceProperties(gpus_all[i], &props);

            for (j = 0; j < other_count; j++)
            {
               bool is_hdr_fmt =
                  (  vulkan_is_hdr10_format(other_formats[j].format)
                  && other_formats[j].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
                  || (  other_formats[j].format     == VK_FORMAT_R16G16B16A16_SFLOAT
                     && other_formats[j].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);

               if (is_hdr_fmt && format_count < ARRAY_SIZE(formats))
               {
                  RARCH_LOG("[Vulkan] HDR format %u/colorspace %u found on GPU \"%s\", "
                        "merging into surface format list.\n",
                        other_formats[j].format,
                        other_formats[j].colorSpace,
                        props.deviceName);
                  formats[format_count++] = other_formats[j];
                  have_hdr = true;
               }
            }

            if (have_hdr)
               break; /* Found HDR formats from another GPU */
         }
      }
   }
#endif

   format.format = VK_FORMAT_UNDEFINED;
   if (     format_count == 1
         && (formats[0].format == VK_FORMAT_UNDEFINED))
   {
      format        = formats[0];
      format.format = VK_FORMAT_B8G8R8A8_UNORM;
   }
   else
   {
      if (format_count == 0)
      {
         RARCH_ERR("[Vulkan] Surface has no formats.\n");
         return false;
      }

#ifdef VULKAN_HDR_SWAPCHAIN
      if (vk->context.flags & VK_CTX_FLAG_HDR_SUPPORT)
      {
         unsigned video_hdr_mode = ss->hdr_mode;

         /* Advertise HDR capabilities to the menu based on which
          * surface formats the driver actually enumerates.
          * The colorspace extension alone is not enough — some
          * drivers expose the extension without any HDR surface
          * formats. */
         {
            uint32_t hdr_flags = 0;
            for (i = 0; i < format_count; i++)
            {
               if (  vulkan_is_hdr10_format(formats[i].format)
                  && formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
                  hdr_flags |= VIDEO_FLAG_HDR10_SUPPORT;
               if (  formats[i].format     == VK_FORMAT_R16G16B16A16_SFLOAT
                  && formats[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
                  hdr_flags |= VIDEO_FLAG_SCRGB_SUPPORT;
            }
            if (hdr_flags & (VIDEO_FLAG_HDR10_SUPPORT | VIDEO_FLAG_SCRGB_SUPPORT))
               hdr_flags |= VIDEO_FLAG_HDR_SUPPORT;
            video_driver_modify_disp_flags(hdr_flags,
                  VIDEO_FLAG_HDR_SUPPORT | VIDEO_FLAG_HDR10_SUPPORT | VIDEO_FLAG_SCRGB_SUPPORT);
         }

         /* Clamp the selected mode if the surface doesn't support it */
         {
            uint32_t disp_flags = video_driver_get_disp_flags();
            if (video_hdr_mode == 2 && !(disp_flags & VIDEO_FLAG_SCRGB_SUPPORT))
            {
               RARCH_WARN("[Vulkan] scRGB not available on this surface, falling back.\n");
               video_hdr_mode = (disp_flags & VIDEO_FLAG_HDR10_SUPPORT) ? 1 : 0;
            }
            if (video_hdr_mode == 1 && !(disp_flags & VIDEO_FLAG_HDR10_SUPPORT))
            {
               RARCH_WARN("[Vulkan] HDR10 not available on this surface, falling back.\n");
               video_hdr_mode = 0;
            }
         }

         if (video_hdr_mode > 0)
            vk->context.flags |=  VK_CTX_FLAG_HDR_ENABLE;
         else
            vk->context.flags &= ~VK_CTX_FLAG_HDR_ENABLE;

         if (video_hdr_mode == 2)
         {
            /* scRGB mode: always use R16G16B16A16_SFLOAT + extended linear sRGB */
            for (i = 0; i < format_count; i++)
            {
               if (  formats[i].format     == VK_FORMAT_R16G16B16A16_SFLOAT
                  && formats[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
               {
                  format = formats[i];
                  vk->context.flags |= VK_CTX_FLAG_HDR_SCRGB;
                  RARCH_LOG("[Vulkan] Selecting R16G16B16A16_SFLOAT swapchain with scRGB colour space.\n");
                  break;
               }
            }

            if (format.format != VK_FORMAT_R16G16B16A16_SFLOAT)
            {
               RARCH_WARN("[Vulkan] R16G16B16A16_SFLOAT + scRGB not available, falling back to HDR10.\n");
               vk->context.flags &= ~VK_CTX_FLAG_HDR_SCRGB;
               /* Fall through to HDR10 format search below */
            }
         }

         if (video_hdr_mode == 1
            || (video_hdr_mode == 2 && format.format != VK_FORMAT_R16G16B16A16_SFLOAT))
         {
            /* HDR10 mode: always A2B10G10R10 + ST.2084 PQ.
             * Shaders that output RGBA16F with PQ data get quantised
             * to 10-bit by the swapchain — R16G16B16A16_SFLOAT is not
             * paired with HDR10_ST2084 on any known implementation. */
            vk->context.flags &= ~VK_CTX_FLAG_HDR_SCRGB;

            for (i = 0; i < format_count; i++)
            {
               if (     (vulkan_is_hdr10_format(formats[i].format))
                     && (formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT))
               {
                  format = formats[i];
                  break;
               }
            }
         }

         if (  !vulkan_is_hdr10_format(format.format)
            && format.format != VK_FORMAT_R16G16B16A16_SFLOAT)
            vk->context.flags &= ~VK_CTX_FLAG_HDR_ENABLE;
      }
      else
      {
         vk->context.flags &= ~VK_CTX_FLAG_HDR_ENABLE;
      }

      if (!(vk->context.flags & VK_CTX_FLAG_HDR_ENABLE))
#endif /* VULKAN_HDR_SWAPCHAIN */
      {
         /* A 10-bit SDR swapchain is the useful state for shader chains
          * that darken heavily (CRT beam profiles, aperture grilles): it
          * removes the final-pass quantisation without dragging in the
          * whole HDR pipeline.  Opt-in, since it is not free on every
          * compositor, and fall back to 8-bit when unavailable. */
         if (ss->bit_depth == 2)
         {
            for (i = 0; i < format_count; i++)
            {
               if (     (   formats[i].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32
                         || formats[i].format == VK_FORMAT_A2R10G10B10_UNORM_PACK32)
                     && (formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
               {
                  format = formats[i];
                  break;
               }
            }

            if (format.format == VK_FORMAT_UNDEFINED)
               RARCH_WARN("[Vulkan] 10-bit SDR swapchain requested but not"
                     " available, falling back to 8-bit.\n");
            else
               RARCH_LOG("[Vulkan] Using 10-bit SDR swapchain format %u.\n",
                     format.format);
         }

         if (format.format == VK_FORMAT_UNDEFINED)
         {
            int sdr8 = vulkan_sdr8_surface_format(formats, format_count);
            if (sdr8 >= 0)
               format = formats[sdr8];
         }
      }

      if (format.format == VK_FORMAT_UNDEFINED)
         format = formats[0];
   }

#ifdef WSI_HARDENING_TEST
   if (trigger_spurious_error())
   {
      surface_properties.maxImageExtent.width = 0;
      surface_properties.maxImageExtent.height = 0;
      surface_properties.minImageExtent.width = 0;
      surface_properties.minImageExtent.height = 0;
   }
#endif

   swapchain_size = vulkan_swapchain_extent(&surface_properties,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims),
         use_requested_extent);

   if (     (swapchain_size.width  == 0)
         && (swapchain_size.height == 0))
   {
      /* Cannot create swapchain yet, try again later. */
#if defined(_WIN32) && !defined(__WINRT__)
      if (vk->dxgi_presenter)
         vulkan_destroy_swapchain(vk);
#endif
      vulkan_low_latency_retire(vk, vk->swapchain);
      vk->swapchain                    = VK_NULL_HANDLE;
      vk->context.swapchain_dims       = dims;
      vk->context.num_swapchain_images = 1;

      memset(vk->context.swapchain_images, 0, sizeof(vk->context.swapchain_images));
      RARCH_DBG("[Vulkan] Cannot create a swapchain yet. Will try again later...\n");
      return true;
   }

   /* Unless we have other reasons to clamp, we should prefer 3 images.
    * We hard sync against the swapchain, so if we have 2 images,
    * we would be unable to overlap CPU and GPU, which can get very slow
    * for GPU-rendered cores. */
   desired_swapchain_images    = ss->max_images;

   /* We don't clamp the number of images requested to what is reported
    * as supported by the implementation in surface_properties.minImageCount,
    * because MESA always reports a minImageCount of 4, but 3 and 2 work
    * perfectly well, even if it's out of spec. */

   if (     (surface_properties.maxImageCount > 0)
         && (desired_swapchain_images > surface_properties.maxImageCount))
      desired_swapchain_images = surface_properties.maxImageCount;

   /* Clamp up to minImageCount to satisfy the spec requirement.
    * Some drivers (MESA) are lenient, but Vulkan requires
    * minImageCount >= max(minImageCount, 1). */
   if (desired_swapchain_images < surface_properties.minImageCount)
      desired_swapchain_images = surface_properties.minImageCount;

   /* Cap our request to what we can actually hold. Per-image arrays
    * (swapchain_images, swapchain_fences, the various semaphore
    * arrays, vk->swapchain[], readback.staging[]) are all sized to
    * VULKAN_MAX_SWAPCHAIN_IMAGES at compile time. The post-create
    * fill below also clamps defensively in case a driver returns
    * more images than requested. */
   if (desired_swapchain_images > VULKAN_MAX_SWAPCHAIN_IMAGES)
      desired_swapchain_images = VULKAN_MAX_SWAPCHAIN_IMAGES;

   if (surface_properties.supportedTransforms
         & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      pre_transform            = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   else
      pre_transform            = surface_properties.currentTransform;

   if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
      composite                = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
      composite                = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
      composite                = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
      composite                = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

   old_swapchain               = vk->swapchain;

#if defined(_WIN32) && !defined(__WINRT__)
   if (ss->dxgi_present && !vk->dxgi_failed && vk->wsi_type == VULKAN_WSI_WIN32)
   {
      VkFormat import_format = format.format;
      bool srgb = import_format == VK_FORMAT_R8G8B8A8_SRGB
         || import_format == VK_FORMAT_B8G8R8A8_SRGB;
      if (import_format == VK_FORMAT_R8G8B8A8_SRGB)
         import_format = VK_FORMAT_R8G8B8A8_UNORM;
      else if (import_format == VK_FORMAT_B8G8R8A8_SRGB)
         import_format = VK_FORMAT_B8G8R8A8_UNORM;
      if (vk->dxgi_presenter && ra_vulkan_dxgi_resize(
               (ra_vulkan_dxgi_t *)vk->dxgi_presenter,
               swapchain_size.width, swapchain_size.height, import_format))
      {
         if (vk->context.swapchain_dims == VIDEO_SCALE_PACK(
                  swapchain_size.width, swapchain_size.height)
               && vk->context.swapchain_format == import_format)
         {
            vk->context.swap_interval = swap_interval;
            vk->flags &= ~VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;
            return true;
         }
         /* The resize drained submissions; keep their synchronization objects. */
      }
      else
         vulkan_destroy_swapchain(vk);
      old_swapchain = VK_NULL_HANDLE;
      if (!vk->dxgi_presenter && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            && (import_format == VK_FORMAT_R8G8B8A8_UNORM
               || import_format == VK_FORMAT_B8G8R8A8_UNORM))
         vk->dxgi_presenter = ra_vulkan_dxgi_create(vk->context.instance,
               vk->context.gpu, vk->context.device, vk->context.queue,
               vk->context.graphics_queue_index, (HWND)vk->dxgi_window,
               swapchain_size.width, swapchain_size.height, import_format,
               vk->context.queue_lock);
#ifdef VULKAN_HDR_SWAPCHAIN
      else if (!vk->dxgi_presenter && format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
                 && format.format == VK_FORMAT_R16G16B16A16_SFLOAT)
         vk->dxgi_presenter = ra_vulkan_dxgi_create(vk->context.instance,
               vk->context.gpu, vk->context.device, vk->context.queue,
               vk->context.graphics_queue_index, (HWND)vk->dxgi_window,
               swapchain_size.width, swapchain_size.height, format.format,
               vk->context.queue_lock);
#endif
#ifdef VULKAN_HDR_SWAPCHAIN
      else if (!vk->dxgi_presenter && format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT
                 && format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      {
         vk->dxgi_presenter = ra_vulkan_dxgi_create(vk->context.instance,
               vk->context.gpu, vk->context.device, vk->context.queue,
               vk->context.graphics_queue_index, (HWND)vk->dxgi_window,
               swapchain_size.width, swapchain_size.height, format.format,
               vk->context.queue_lock);
         if (vk->dxgi_presenter)
            ra_vulkan_dxgi_hdr_metadata((ra_vulkan_dxgi_t *)vk->dxgi_presenter,
                  ss->display_peak);
      }
#endif
      if (vk->dxgi_presenter)
      {
         VkSemaphoreCreateInfo semaphore;
         memset(&semaphore, 0, sizeof(semaphore));
         semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
         RARCH_LOG("[Vulkan] DXGI presentation active.\n");
         vk->context.swapchain_external = true;
         vk->context.flags &= ~VK_CTX_FLAG_SWAPCHAIN_IS_SRGB;
         if (srgb)
            vk->context.flags |= VK_CTX_FLAG_SWAPCHAIN_IS_SRGB;
         vk->context.num_swapchain_images = 2;
         vk->context.swapchain_format = import_format;
         vk->context.swapchain_dims = VIDEO_SCALE_PACK(swapchain_size.width, swapchain_size.height);
         vk->context.swapchain_present_mode = swapchain_present_mode;
#ifdef VULKAN_HDR_SWAPCHAIN
         vk->context.swapchain_colour_space = format.colorSpace;
#endif
         vk->flags &= ~VK_DATA_FLAG_EMULATING_MAILBOX;
         for (i = 0; i < 2; i++)
         {
            vk->context.swapchain_images[i] = ra_vulkan_dxgi_image(
                  (ra_vulkan_dxgi_t *)vk->dxgi_presenter, i);
            if (vk->context.swapchain_semaphores[i] == VK_NULL_HANDLE
                  && vkCreateSemaphore(vk->context.device, &semaphore, NULL,
                     &vk->context.swapchain_semaphores[i]) != VK_SUCCESS)
            {
               vulkan_destroy_swapchain(vk);
               goto dxgi_unavailable;
            }
         }
         vk->context.flags |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
         vk->context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
         vk->context.current_swapchain_index = 0;
         vulkan_create_wait_fences(vk);
         vk->flags &= ~VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;
#ifdef VULKAN_HDR_SWAPCHAIN
         video_driver_set_hdr_output_mode(
               vk->context.swapchain_colour_space == VK_COLOR_SPACE_HDR10_ST2084_EXT
               ? 1 : (vk->context.swapchain_colour_space == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
               ? 2 : 0));
#else
         video_driver_set_hdr_output_mode(0);
#endif
         return true;
      }
dxgi_unavailable:
      vk->dxgi_failed = true;
      RARCH_WARN("[Vulkan] DXGI presentation unavailable; using native presentation.\n");
   }
#endif

   info.sType                  = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   info.pNext                  = NULL;
   info.flags                  = 0;
   info.surface                = vk->vk_surface;
   info.minImageCount          = desired_swapchain_images;
   info.imageFormat            = format.format;
   info.imageColorSpace        = format.colorSpace;
   info.imageExtent.width      = swapchain_size.width;
   info.imageExtent.height     = swapchain_size.height;
   info.imageArrayLayers       = 1;
   info.imageUsage             =  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                | VK_IMAGE_USAGE_SAMPLED_BIT;
   info.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount  = 0;
   info.pQueueFamilyIndices    = NULL;
   info.preTransform           = pre_transform;
   info.compositeAlpha         = composite;
   info.presentMode            = swapchain_present_mode;
   info.clipped                = VK_TRUE;

   /* TODO/FIXME:
    * Weird shenanigans necessary for Apple otherwise the following happens:
    * The menu sometimes refuses to display, but still responds to input.
    * This happens about 1/5 times on macOS but 100% in the quick menu on iOS
    */
#ifdef __APPLE__
   info.oldSwapchain           = NULL;
   vulkan_low_latency_retire(vk, old_swapchain);
   old_swapchain               = VK_NULL_HANDLE;
#else
   info.oldSwapchain           = old_swapchain;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
   if (vk->fse_supported)
   {
      hmonitor                = MonitorFromWindow(GetActiveWindow(), MONITOR_DEFAULTTONEAREST);
      fse_win32_info.hmonitor = hmonitor;
      fse_info.pNext          = &fse_win32_info;
      info.pNext              = &fse_info;
   }
#endif
   /* Only a swapchain made with this can be put in low-latency mode;
    * whether it is, is set on it later. */
   if (vk->low_latency.supported)
   {
      latency_info.sType             = VK_STRUCTURE_TYPE_SWAPCHAIN_LATENCY_CREATE_INFO_NV;
      latency_info.pNext             = info.pNext;
      latency_info.latencyModeEnable = VK_TRUE;
      info.pNext                     = &latency_info;
   }

   {
      VkResult res = vkCreateSwapchainKHR(vk->context.device,
               &info, NULL, &vk->swapchain);

#ifndef __APPLE__
      /* oldSwapchain is retired by the call whether or not the new one
       * was created, so nothing is lost by dropping it and asking again
       * without the handoff.  Some display WSIs (PowerVR on
       * VK_KHR_display) refuse to build a replacement while the old
       * swapchain still holds the plane, and succeed once it is gone. */
      if (res != VK_SUCCESS && old_swapchain != VK_NULL_HANDLE)
      {
         RARCH_WARN("[Vulkan] Swapchain replacement failed (err = %d); "
               "retrying without oldSwapchain.\n", (int)res);
         vulkan_low_latency_retire(vk, old_swapchain);
         old_swapchain     = VK_NULL_HANDLE;
         info.oldSwapchain = VK_NULL_HANDLE;
         vk->swapchain     = VK_NULL_HANDLE;
         res               = vkCreateSwapchainKHR(vk->context.device,
               &info, NULL, &vk->swapchain);
      }
#endif

      if (res != VK_SUCCESS)
      {
         RARCH_ERR("[Vulkan] Failed to create swapchain (err = %d).\n",
               (int)res);
         /* Leave the context in the same "no swapchain yet" state the
          * zero-extent path uses, so the acquire path retries the
          * create on a later frame instead of presenting to a retired
          * or undefined handle. */
         vulkan_low_latency_retire(vk, old_swapchain);
         vk->swapchain                    = VK_NULL_HANDLE;
         vk->context.num_swapchain_images = 1;
         memset(vk->context.swapchain_images, 0,
               sizeof(vk->context.swapchain_images));
         vk->context.flags               |=  VK_CTX_FLAG_INVALID_SWAPCHAIN;
         vk->context.flags               &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
         return false;
      }
   }

#ifdef VK_USE_PLATFORM_WIN32_KHR
   vk->fse_acquired = false;
   if (     fse_forced && vk->fse_supported && vk->fse_acquire
         && (vk->flags & VK_DATA_FLAG_FULLSCREEN))
   {
      VkResult res = ((PFN_vkAcquireFullScreenExclusiveModeEXT)
            vk->fse_acquire)(vk->context.device, vk->swapchain);
      if (res == VK_SUCCESS)
      {
         vk->fse_acquired = true;
         RARCH_LOG("[Vulkan] Exclusive fullscreen acquired.\n");
      }
      else
         /* Not fatal: the swapchain still works on the compositor
          * path, which is where Relaxed would have left it anyway. */
         RARCH_WARN("[Vulkan] Exclusive fullscreen requested but "
               "vkAcquireFullScreenExclusiveModeEXT returned %d; "
               "continuing without it.\n", (int)res);
   }
#endif

   /* See TODO/FIXME note above - part of the same rubber bandaid hack 'fix' */
#ifndef __APPLE__
   vulkan_low_latency_retire(vk, old_swapchain);
#endif

   /* A new swapchain, in no mode yet; the handle may be a reused one */
   vk->low_latency.swapchain          = VK_NULL_HANDLE;
   vk->low_latency.mode               = 0;

   vk->context.swapchain_dims         = VIDEO_SCALE_PACK(swapchain_size.width,
         swapchain_size.height);
   vk->context.swapchain_present_mode = swapchain_present_mode;
#ifdef VULKAN_HDR_SWAPCHAIN
   vk->context.swapchain_colour_space = format.colorSpace;
#endif /* VULKAN_HDR_SWAPCHAIN */

   /* Make sure we create a backbuffer format that is as we expect. */
   switch (format.format)
   {
      case VK_FORMAT_B8G8R8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8A8_UNORM;
         vk->context.flags            |= VK_CTX_FLAG_SWAPCHAIN_IS_SRGB;
         break;

      case VK_FORMAT_R8G8B8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8A8_UNORM;
         vk->context.flags            |= VK_CTX_FLAG_SWAPCHAIN_IS_SRGB;
         break;

      case VK_FORMAT_R8G8B8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8_UNORM;
         vk->context.flags            |= VK_CTX_FLAG_SWAPCHAIN_IS_SRGB;
         break;

      case VK_FORMAT_B8G8R8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8_UNORM;
         vk->context.flags            |= VK_CTX_FLAG_SWAPCHAIN_IS_SRGB;
         break;

      default:
         vk->context.swapchain_format  = format.format;
         break;
   }

   vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, NULL);

   /* Even after capping minImageCount above, drivers may legally
    * return more images than requested. Clamp before the fill call
    * so we don't write past swapchain_images[] and so every
    * downstream loop bounded by num_swapchain_images stays inside
    * its compile-time-sized array. */
   if (vk->context.num_swapchain_images > VULKAN_MAX_SWAPCHAIN_IMAGES)
   {
      RARCH_WARN("[Vulkan] Swapchain returned %u images, clamping to %u.\n",
            vk->context.num_swapchain_images,
            (unsigned)VULKAN_MAX_SWAPCHAIN_IMAGES);
      vk->context.num_swapchain_images = VULKAN_MAX_SWAPCHAIN_IMAGES;
   }

   vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, vk->context.swapchain_images);

   if (old_swapchain == VK_NULL_HANDLE)
      RARCH_LOG("[Vulkan] Got %u swapchain images.\n",
            vk->context.num_swapchain_images);

   /* Pre-create the per-image present-side semaphores up-front for every
    * image in the new swapchain.  vulkan_acquire_next_image only allocates
    * swapchain_semaphores[index] for the image actually returned by
    * vkAcquireNextImageKHR, leaving slots for not-yet-acquired images at
    * VK_NULL_HANDLE.  On the swapchain recreate path the prior teardown
    * memsets the array to zero, and at least one path through the recreate
    * can reach vulkan_present with current_swapchain_index pointing at an
    * image whose slot has not yet been re-populated.  NVIDIA real-FSE on
    * Win11 then segfaults inside vkQueuePresentKHR when handed
    * VK_NULL_HANDLE in pWaitSemaphores. */
   {
      VkSemaphoreCreateInfo sem_info_pre;
      unsigned sem_idx;
      sem_info_pre.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      sem_info_pre.pNext = NULL;
      sem_info_pre.flags = 0;
      for (sem_idx = 0; sem_idx < vk->context.num_swapchain_images; sem_idx++)
      {
         if (vk->context.swapchain_semaphores[sem_idx] == VK_NULL_HANDLE)
            vkCreateSemaphore(vk->context.device, &sem_info_pre,
                  NULL, &vk->context.swapchain_semaphores[sem_idx]);
      }
   }

   /* Force driver to reset swapchain image handles. */
   vk->context.flags                 |=  VK_CTX_FLAG_INVALID_SWAPCHAIN;
   vk->context.flags                 &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
   /* A replacement swapchain can have fewer images than its predecessor.
    * Do not retain an image index from the old swapchain while waiting for
    * the first acquire from the new one. */
   vk->context.current_swapchain_index = 0;
   vulkan_create_wait_fences(vk);
#ifdef __APPLE__
   vk->acquire_timeout_us              = vulkan_mailbox_timeout_us();
#endif

   if (vk->flags & VK_DATA_FLAG_EMULATING_MAILBOX)
   {
      if (!vulkan_emulated_mailbox_init(&vk->mailbox, vk->context.device, vk->swapchain))
      {
         RARCH_WARN("[Vulkan] Failed to initialize emulated mailbox -- "
               "falling back to blocking acquire.\n");
         vk->flags &= ~VK_DATA_FLAG_EMULATING_MAILBOX;
      }
   }

   /* This flag needs to be cleared otherwise elsewhere it can be perceived as if there's a new swapchain created everytime its being called */
   vk->flags &= ~VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;

#ifdef VULKAN_HDR_SWAPCHAIN
   /* Signal SMPTE-2086 mastering-display metadata to the compositor for an
    * HDR10 swapchain. Best-effort: only when VK_EXT_hdr_metadata was enabled
    * (set_hdr_metadata non-NULL) and the swapchain is an HDR10 surface.
    * Uses Rec.2020 primaries (matching the D3D path) and RetroArch's
    * configured output-luminance range. Touches no formats or pipelines, so
    * a wrong or ignored value at worst affects display tone mapping. */
   /* MoltenVK before 1.3.0 over-releases the autoreleased CAEDRMetadata
    * and NSData objects it creates inside MVKSwapchain::setHDRMetadataEXT()
    * (upstream commits 3b77dea and 8caa1d5, first shipped in 1.3.0); the
    * pending autoreleases then crash the main-thread pool drain shortly
    * after the call.  MoltenVK encodes driverVersion as
    * major * 10000 + minor * 100 + patch, so 1.3.0 is 10300.  Skipping
    * the call on affected versions only omits the SMPTE-2086 mastering
    * hint; the layer colour space and EDR flag are still derived from
    * the swapchain colour space by MoltenVK itself. */
   if (     vk->set_hdr_metadata
         && (vk->context.flags & VK_CTX_FLAG_HDR_ENABLE)
         && vulkan_is_hdr10_format(vk->context.swapchain_format)
         && !(   vk->wsi_type == VULKAN_WSI_MVK_MACOS
              && vk->context.gpu_properties.driverVersion < 10300))
   {
      VkHdrMetadataEXT meta;
      meta.sType                     = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
      meta.pNext                     = NULL;
      /* Rec.2020 display primaries and D65 white point. */
      meta.displayPrimaryRed.x       = 0.708f;
      meta.displayPrimaryRed.y       = 0.292f;
      meta.displayPrimaryGreen.x     = 0.170f;
      meta.displayPrimaryGreen.y     = 0.797f;
      meta.displayPrimaryBlue.x      = 0.131f;
      meta.displayPrimaryBlue.y      = 0.046f;
      meta.whitePoint.x              = 0.3127f;
      meta.whitePoint.y              = 0.3290f;
      /* 1000 nits unless Use Display Peak supplies the display's */
      meta.maxLuminance              = ss->display_peak > 0.0f
            ? ss->display_peak : 1000.0f;
      meta.minLuminance              = 0.001f;
      meta.maxContentLightLevel      = meta.maxLuminance;
      meta.maxFrameAverageLightLevel = meta.maxLuminance;
      vk->set_hdr_metadata(vk->context.device, 1, &vk->swapchain, &meta);
   }
#endif

#ifdef VULKAN_HDR_SWAPCHAIN
   video_driver_set_hdr_output_mode(
         vk->context.swapchain_colour_space == VK_COLOR_SPACE_HDR10_ST2084_EXT
         ? 1 : (vk->context.swapchain_colour_space == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
         ? 2 : 0));
#else
   video_driver_set_hdr_output_mode(0);
#endif
   return true;
}

/* The settings a swapchain is made from, as the context is made:
 * the main thread's, or the one it is waiting on.  Each frame takes
 * them over after that. */
static void vulkan_swapchain_settings_init(vulkan_swapchain_settings_t *ss)
{
   settings_t *settings    = config_get_ptr();
   ss->display_peak        = video_driver_hdr_metadata_peak(0.0f);
   ss->hdr_mode            = settings->uints.video_hdr_mode;
   ss->bit_depth           = settings->uints.video_swapchain_bit_depth;
   ss->max_images          = settings->uints.video_max_swapchain_images;
   ss->fse_negotiation     = settings->uints.video_fse_negotiation;
   ss->vsync               = settings->bools.video_vsync;
   ss->adaptive_vsync      = settings->bools.video_adaptive_vsync;
   ss->windowed_fullscreen = settings->bools.video_windowed_fullscreen;
   ss->dxgi_present        = settings->bools.video_dxgi_present;
}

#ifdef HAVE_OPENXR
/* Every instance extension XR_KHR_vulkan_enable names must exist, or
 * the instance itself would fail. */
static bool vulkan_context_openxr_instance_exts_ok(vulkan_openxr_t *xr)
{
   uint32_t count = 0;
   unsigned i, n;
   bool ok        = true;
   const char *xr_exts[VULKAN_OPENXR_MAX_EXTS];
   VkExtensionProperties *props;

   n = vulkan_openxr_instance_extensions(xr, xr_exts, VULKAN_OPENXR_MAX_EXTS);
   if (!n)
      return true;
   if (     vkEnumerateInstanceExtensionProperties(NULL, &count, NULL) != VK_SUCCESS
         || !count
         || !(props = (VkExtensionProperties*)malloc(count * sizeof(*props))))
      return false;
   if (vkEnumerateInstanceExtensionProperties(NULL, &count, props) != VK_SUCCESS)
      ok = false;
   for (i = 0; ok && i < n; i++)
      ok = vulkan_find_extensions(&xr_exts[i], 1, props, count);
   free(props);
   return ok;
}

/* Chosen before the instance exists: the runtime makes it, or lists
 * what it needs. A core making its own device with create_device (v1)
 * gets XR_KHR_vulkan_enable's lists; everything else goes through
 * XR_KHR_vulkan_enable2. */
static void vulkan_context_openxr_init(gfx_ctx_vulkan_data_t *vk,
      const struct retro_hw_render_context_negotiation_interface_vulkan *iface,
      uint32_t api_version)
{
   settings_t *settings = config_get_ptr();
   bool enable          = settings->bools.video_openxr_enable;
   bool enable1         = iface && iface->create_device
      && !(iface->interface_version >= 2 && iface->create_device2);

#ifdef ANDROID
   /* Only a headset's own context starts the runtime, and it needs no
    * setting to. */
   enable = vk->openxr_headset;
#endif

   /* A kept device keeps the runtime it was made for, while the runtime
    * keeps the instance. */
   if (cached_instance_vk)
   {
      if (cached_xr && enable && !vulkan_openxr_lost(cached_xr))
         vk->context.xr = cached_xr;
      else
      {
         vulkan_openxr_free(cached_xr);
         if (enable)
            vulkan_openxr_needs_reload();
      }
      cached_xr = NULL;
      return;
   }
   if (cached_xr)
   {
      vulkan_openxr_free(cached_xr);
      cached_xr = NULL;
   }
   if (!enable)
      return;
   vk->context.xr = vulkan_openxr_new(enable1, api_version);
   if (     vk->context.xr && enable1
         && !vulkan_context_openxr_instance_exts_ok(vk->context.xr))
   {
      vulkan_openxr_drop(vk->context.xr);
      vk->context.xr = NULL;
   }
}
#endif

bool vulkan_context_init(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type)
{
   VkApplicationInfo app;
   PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
   const char *prog_name          = NULL;
   video_driver_state_t *video_st = video_state_get_ptr();
   struct retro_hw_render_context_negotiation_interface_vulkan
                           *iface = (struct retro_hw_render_context_negotiation_interface_vulkan*)video_st->hw_render_context_negotiation;

   if (iface && iface->interface_type != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
   {
      RARCH_WARN("[Vulkan] Got HW context negotiation interface, but it's the wrong API.\n");
      iface = NULL;
   }

   if (iface && iface->interface_version == 0)
   {
      RARCH_WARN("[Vulkan] Got HW context negotiation interface, but it's the wrong interface version.\n");
      iface = NULL;
   }

   vk->wsi_type = type;
   vulkan_swapchain_settings_init(&vk->context.swapchain_settings);

   if (!vulkan_library)
   {
#ifdef _WIN32
      vulkan_library = dylib_load("vulkan-1.dll");
#elif __APPLE__
      /* allow overriding by environment variable; this means restart is required to change */
      if (!getenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS"))
      {
         settings_t *settings             = config_get_ptr();
         int use_mab                      = settings->bools.video_use_metal_arg_buffers;
         setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", use_mab ? "1" : "0", 1);
      }
      /* Try Vulkan loader first (enables validation layers if installed).
       * Falls back to MoltenVK directly if loader not available. The
       * loads are attempted in order without an availability check: a
       * dylib_load that cannot succeed on an older OS fails and falls
       * through exactly as the check would have, and a plain C file
       * has no business carrying clang's availability runtime. */
      vulkan_library = dylib_load("libvulkan.dylib");
      if (!vulkan_library)
         vulkan_library = dylib_load("MoltenVK");
      if (!vulkan_library)
         vulkan_library = dylib_load("MoltenVK-v1.2.7.framework");
#else
      vulkan_library = dylib_load("libvulkan.so.1");
      if (!vulkan_library)
         vulkan_library = dylib_load("libvulkan.so");
#endif
   }

   if (!vulkan_library)
   {
      RARCH_ERR("[Vulkan] Failed to open Vulkan loader.\n");
      return false;
   }

   RARCH_LOG("[Vulkan] Vulkan dynamic library loaded.\n");

   GetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dylib_proc(vulkan_library, "vkGetInstanceProcAddr");

   if (!GetInstanceProcAddr)
   {
      RARCH_ERR("[Vulkan] Failed to load vkGetInstanceProcAddr symbol, broken loader?\n");
      return false;
   }

   vulkan_symbol_wrapper_init(GetInstanceProcAddr);

   if (!vulkan_symbol_wrapper_load_global_symbols())
   {
      RARCH_ERR("[Vulkan] Failed to load global Vulkan symbols, broken loader?\n");
      return false;
   }

   prog_name              = msg_hash_to_str(MSG_PROGRAM);
   app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   app.pNext              = NULL;
   app.pApplicationName   = prog_name;
   app.applicationVersion = 0;
   app.pEngineName        = prog_name;
   app.engineVersion      = 0;
   app.apiVersion         = VK_API_VERSION_1_0;

   if (iface)
   {
      if (!iface->get_application_info && iface->interface_version >= 2)
      {
         RARCH_ERR("[Vulkan] Core did not provide application info as required by v2.\n");
         return false;
      }

      if (iface->get_application_info)
      {
         const VkApplicationInfo *app_info = iface->get_application_info();

         if (!app_info && iface->interface_version >= 2)
         {
            RARCH_ERR("[Vulkan] Core did not provide application info as required by v2.\n");
            return false;
         }

         if (app_info)
         {
            app = *app_info;
#ifdef VULKAN_DEBUG
            if (app.pApplicationName)
            {
               RARCH_LOG("[Vulkan] App: %s (version %u)\n",
                     app.pApplicationName, app.applicationVersion);
            }

            if (app.pEngineName)
            {
               RARCH_LOG("[Vulkan] Engine: %s (version %u)\n",
                     app.pEngineName, app.engineVersion);
            }
#endif
         }
      }
   }

   if (app.apiVersion < VK_API_VERSION_1_1)
   {
      /* Try to upgrade to at least Vulkan 1.1 so that we can more easily make use of advanced features.
       * Vulkan 1.0 drivers are completely irrelevant these days. */
      uint32_t supported;
      if (     vkEnumerateInstanceVersion
            && (vkEnumerateInstanceVersion(&supported) == VK_SUCCESS)
            && (supported >= VK_API_VERSION_1_1))
         app.apiVersion = VK_API_VERSION_1_1;
   }

#ifdef HAVE_OPENXR
   vulkan_context_openxr_init(vk, iface, app.apiVersion);
   /* A headset's own context without its runtime makes no instance
    * and takes no kept one: the flat context follows in the list, and
    * what is kept is that one's. */
   if (vk->openxr_headset && !vk->context.xr)
      return false;
#endif

   if (cached_instance_vk)
   {
      vk->context.instance = cached_instance_vk;
      cached_instance_vk   = NULL;
      /* Only the create wrapper sees the enabled extensions. */
      if (cached_instance_hdr_vk)
         vk->context.flags |=  VK_CTX_FLAG_HDR_SUPPORT;
      else
         vk->context.flags &= ~VK_CTX_FLAG_HDR_SUPPORT;
   }
   else
   {
      if (     iface
            && iface->interface_version >= 2
            && iface->create_instance)
         vk->context.instance = iface->create_instance(
               GetInstanceProcAddr, &app,
               vulkan_context_create_instance_wrapper, vk);
      else
      {
         VkInstanceCreateInfo info;
         info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
         info.pNext                   = NULL;
         info.flags                   = 0;
         info.pApplicationInfo        = &app;
         info.enabledLayerCount       = 0;
         info.ppEnabledLayerNames     = NULL;
         info.enabledExtensionCount   = 0;
         info.ppEnabledExtensionNames = NULL;
         vk->context.instance         = vulkan_context_create_instance_wrapper(vk, &info);
      }

      if (vk->context.instance == VK_NULL_HANDLE)
      {
         RARCH_ERR("[Vulkan] Failed to create Vulkan instance.\n");
         return false;
      }
   }

#ifdef VULKAN_DEBUG
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDebugUtilsMessengerEXT);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkDestroyDebugUtilsMessengerEXT);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkSetDebugUtilsObjectNameEXT);

   {
      VkDebugUtilsMessengerCreateInfoEXT info =
      { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
      info.messageSeverity =
           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
      info.messageType =
           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      info.pfnUserCallback = vulkan_debug_cb;

      if (vk->context.instance)
         vkCreateDebugUtilsMessengerEXT(vk->context.instance, &info, NULL,
               &vk->context.debug_callback);
   }
   RARCH_LOG("[Vulkan] Enabling Vulkan debug layers.\n");
#endif

   if (!vulkan_load_instance_symbols(vk))
   {
      RARCH_ERR("[Vulkan] Failed to load instance symbols.\n");
      return false;
   }

   return true;
}


void vulkan_context_destroy(gfx_ctx_vulkan_data_t *vk,
      bool destroy_surface)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   uint32_t video_st_flags        = 0;
#if defined(ANDROID) && defined(HAVE_THREADS)
   VkResult recording_idle = VK_NOT_READY;
#endif
#ifdef HAVE_OPENXR
   /* Its thread uses the queue, which is drained below. */
   vulkan_openxr_stop(vk->context.xr);
#endif
   if (!vk->context.instance)
   {
#ifdef HAVE_OPENXR
      vulkan_openxr_free(vk->context.xr);
      vk->context.xr = NULL;
#endif
      return;
   }

   if (vk->context.device)
   {
#ifdef __APPLE__
      /* MoltenVK before 1.2.11 idles without what is still on its
       * asynchronous submission queue; a fence behind that is not. */
      vk->context.present_pending = true;
      vulkan_context_wait_frames(vk);
#endif
#if defined(ANDROID) && defined(HAVE_THREADS)
      recording_idle =
#endif
         vkDeviceWaitIdle(vk->context.device);
   }
#if defined(ANDROID) && defined(HAVE_THREADS)
   /* Retire capture children before their device, including failed drains. */
   if (vk->context.device
         && !record_android_capture_device_prepare(vk->context.device,
               recording_idle == VK_SUCCESS
                  || recording_idle == VK_ERROR_DEVICE_LOST))
   {
      RARCH_ERR("[Vulkan] Recording children still owned; retaining device.\n");
      return;
   }
#endif
   /* Drained above; the fence would only be waited on again. */
   vk->context.present_pending = false;

   vulkan_destroy_swapchain(vk);
   vulkan_low_latency_device_deinit(vk);

   if (vk->context.present_fence != VK_NULL_HANDLE)
   {
      vkDestroyFence(vk->context.device, vk->context.present_fence, NULL);
      vk->context.present_fence = VK_NULL_HANDLE;
   }

   if (     destroy_surface
         && (vk->vk_surface != VK_NULL_HANDLE))
   {
      vkDestroySurfaceKHR(vk->context.instance,
            vk->vk_surface, NULL);
      vk->vk_surface = VK_NULL_HANDLE;
   }

#ifdef VULKAN_DEBUG
   if (vk->context.debug_callback)
      vkDestroyDebugUtilsMessengerEXT(vk->context.instance, vk->context.debug_callback, NULL);
#endif

   video_st_flags              = (uint32_t)retro_atomic_load_relaxed_int(&video_st->flags);

#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
   if (vulkan_android_cache_device_pending(vk->context.device))
      video_st_flags &= ~VIDEO_FLAG_CACHE_CONTEXT;
#endif
#if defined(ANDROID) && defined(HAVE_THREADS)
   if (record_android_capture_device_pending(vk->context.device))
      video_st_flags &= ~VIDEO_FLAG_CACHE_CONTEXT;
#endif
   if (video_st_flags & VIDEO_FLAG_CACHE_CONTEXT)
   {
      cached_device_vk         = vk->context.device;
      cached_gpu_vk            = vk->context.gpu;
      cached_gpu_index_vk      = vk->context.gpu_index;
      cached_instance_vk       = vk->context.instance;
      cached_destroy_device_vk = vk->context.destroy_device;
      cached_instance_hdr_vk   =
         (vk->context.flags & VK_CTX_FLAG_HDR_SUPPORT) ? true : false;
      cached_timeline_vk       =
         (vk->context.flags & VK_CTX_FLAG_TIMELINE_SEMAPHORE) ? true : false;
      cached_video_vk[0]       = vk->context.video_decode_family;
      cached_video_vk[1]       = vk->context.video_xfer_family;
      cached_video_vk[2]       = vk->context.video_xfer_first;
      cached_video_vk[3]       = vk->context.video_lanes;
      cached_video_vk[4]       = vk->context.video_codec_operations;
      cached_video_vk[5]       = vk->context.flags & VK_CTX_FLAG_VIDEO_GENERAL;
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
      cached_android_images_vk = !!(vk->context.flags & VK_CTX_FLAG_ANDROID_IMAGES);
      cached_android_descriptors_vk = vk->context.android_sampler_descriptors;
      cached_android_legacy_vk = vk->context.android_legacy_descriptor_budget;
#endif
#ifdef HAVE_OPENXR
      cached_xr                = vk->context.xr;
      vk->context.xr           = NULL;
#endif
   }
   else
   {
#ifdef HAVE_OPENXR
      /* The runtime's objects on the device go first. */
      vulkan_openxr_free(vk->context.xr);
      vk->context.xr = NULL;
#endif
      if (vk->context.device)
      {
#if defined(HAVE_RMP4) || defined(HAVE_RWEBM)
         vulkan_video_decode_deinit();
#endif
         /* Call the frontend's destroy_device callback BEFORE
          * vkDestroyDevice, so the frontend can clean up its
          * per-device resources while the device handle is still
          * valid. */
         if (vk->context.destroy_device)
            vk->context.destroy_device();
         vk->context.destroy_device = NULL;
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
         vulkan_android_cache_device_destroying(vk->context.device);
#endif
         vkDestroyDevice(vk->context.device, NULL);
#if defined(ANDROID) && defined(HAVE_THREADS)
         record_android_capture_device_destroyed(vk->context.device);
#endif
#if defined(ANDROID) \
      && (defined(HAVE_RMP4) || defined(HAVE_RVP9) || defined(HAVE_RAV1) \
            || defined(HAVE_RECORD_MEDIACODEC))
         vulkan_android_cache_device_destroyed(vk->context.device);
#endif
         vk->context.device = NULL;
      }

      if (vk->context.instance)
      {
         vkDestroyInstance(vk->context.instance, NULL);
         vk->context.instance = NULL;

         if (vulkan_library)
         {
            dylib_close(vulkan_library);
            vulkan_library = NULL;
         }
      }
   }

   video_driver_set_gpu_api_devices(GFX_CTX_VULKAN_API, NULL);
   if (vk->gpu_list)
   {
      string_list_free(vk->gpu_list);
      vk->gpu_list = NULL;
   }

#ifdef HAVE_THREADS
   /* vulkan_context_init_device() creates a fresh queue_lock on
    * every bring-up -- including the cached-context path, which
    * restores the device and then falls through to slock_new() like
    * any other init -- so the lock's lifetime ends here regardless
    * of whether the device itself is being cached.  Everything that
    * takes it (vulkan_present, the frame submission paths) is done
    * by this point: vkDeviceWaitIdle() ran at the top of this
    * function.  Freeing it here stops one slock leaking per driver
    * reinit -- every resolution change and fullscreen toggle. */
   if (vk->context.queue_lock)
   {
      slock_free(vk->context.queue_lock);
      vk->context.queue_lock = NULL;
   }
#endif
}

/* When the most recent present the driver has timed reached the display,
 * in microseconds on the platform's monotonic clock - the clock
 * cpu_features_get_time_usec() reads on the platforms that have this
 * extension. 0 without the extension or before the first timed present. */
retro_time_t vulkan_last_present_time(gfx_ctx_vulkan_data_t *vk)
{
   PFN_vkGetPastPresentationTimingGOOGLE query;
   VkPastPresentationTimingGOOGLE timings[8];
   uint32_t count = 8;
   uint32_t i;
   uint64_t latest = 0;

   if (!vk || !vk->display_timing_supported || !vk->display_timing_query
         || vk->swapchain == VK_NULL_HANDLE)
      return 0;
   query = (PFN_vkGetPastPresentationTimingGOOGLE)vk->display_timing_query;
   if (query(vk->context.device, vk->swapchain, &count, timings) < 0)
      return 0;
   for (i = 0; i < count; i++)
      if (timings[i].actualPresentTime > latest)
         latest = timings[i].actualPresentTime;
   return (retro_time_t)(latest / 1000);
}

/* Whether the device was made with VK_NV_low_latency2 and what it
 * needs, from the list a device was created with; resolves the entry
 * points once the device exists (vulkan_low_latency_device_init). */
static void vulkan_low_latency_note_exts(gfx_ctx_vulkan_data_t *vk,
      const char * const *exts, unsigned count)
{
   unsigned i;
   bool low_latency = false;
   bool present_id  = false;
   vk->low_latency.supported    = false;
   vk->low_latency.anti_lag_ext = false;
   vk->low_latency.noted        = true;
   for (i = 0; i < count; i++)
   {
      if (!strcmp(exts[i], VK_NV_LOW_LATENCY_2_EXTENSION_NAME))
         low_latency = true;
      else if (!strcmp(exts[i], VK_KHR_PRESENT_ID_EXTENSION_NAME))
         present_id  = true;
      else if (!strcmp(exts[i], VK_AMD_ANTI_LAG_EXTENSION_NAME))
         vk->low_latency.anti_lag_ext = true;
   }
   vk->low_latency.supported = low_latency && present_id;
}

/* VK_AMD_anti_lag's feature, chained in front of @next where the GPU
 * has it and the device has the extension (and not low_latency2,
 * which is used instead). The feature struct must outlive the device
 * creation call. Returns the chain to create with. */
static const void *vulkan_anti_lag_chain(gfx_ctx_vulkan_data_t *vk,
      VkPhysicalDevice gpu, VkPhysicalDeviceAntiLagFeaturesAMD *feat,
      const void *next)
{
   const VkBaseInStructure *chain;
   VkPhysicalDeviceFeatures2 f2;
   PFN_vkGetPhysicalDeviceFeatures2 get_features;
   vk->low_latency.anti_lag = false;
   if (!vk->low_latency.anti_lag_ext || vk->low_latency.supported)
      return next;
   /* A core that asked for it itself */
   for (chain = (const VkBaseInStructure*)next; chain; chain = chain->pNext)
      if (chain->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ANTI_LAG_FEATURES_AMD)
      {
         vk->low_latency.anti_lag =
            ((const VkPhysicalDeviceAntiLagFeaturesAMD*)chain)->antiLag != 0;
         return next;
      }
   get_features = (PFN_vkGetPhysicalDeviceFeatures2)
      vulkan_symbol_wrapper_instance_proc_addr()(vk->context.instance,
            "vkGetPhysicalDeviceFeatures2");
   if (!get_features)
      return next;
   feat->sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ANTI_LAG_FEATURES_AMD;
   feat->pNext   = NULL;
   feat->antiLag = VK_FALSE;
   f2.sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   f2.pNext      = feat;
   get_features(gpu, &f2);
   if (!feat->antiLag)
      return next;
   feat->pNext              = (void*)next;
   vk->low_latency.anti_lag = true;
   return feat;
}

static void vulkan_low_latency_device_init(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   VkSemaphoreTypeCreateInfo type_info;
   VkSemaphoreCreateInfo info;
   uint32_t count = 0;
   VkExtensionProperties *props;
   unsigned i;

   ll->set_sleep_mode  = NULL;
   ll->sleep           = NULL;
   ll->set_marker      = NULL;
   ll->wait_semaphores = NULL;
   ll->semaphore       = VK_NULL_HANDLE;
   ll->swapchain       = VK_NULL_HANDLE;
   ll->sleep_value     = 0;
   ll->frame_id        = 0;
   ll->min_interval_us = 0;
   ll->spec_version    = 0;
   ll->num_retired     = 0;
   ll->timeouts        = 0;
   ll->mode            = 0;
   ll->stage           = 0;
   ll->failed          = false;
   ll->pub_table[0]    = VK_NULL_HANDLE;
   ll->pub_table[1]    = VK_NULL_HANDLE;
   ll->pub_pending     = VK_NULL_HANDLE;
   ll->publish_pending = false;
   retro_atomic_store_release_size(&ll->pub_gen, 0);
   retro_atomic_store_release_int(&ll->want, 0);
   retro_atomic_store_release_int(&ll->answer, VIDEO_DRIVER_PACING_REASON_OFF);
   retro_atomic_store_release_int(&ll->in_call, 0);
   retro_atomic_store_release_int(&ll->stalled, 0);
   retro_atomic_store_release_int(&ll->amd_mode, 0);
   retro_atomic_store_release_int(&ll->amd_max_fps, 0);
   ll->anti_lag_update = NULL;
   if (ll->anti_lag && !ll->supported)
   {
      ll->anti_lag_update = vkGetDeviceProcAddr(vk->context.device,
            "vkAntiLagUpdateAMD");
      if (ll->anti_lag_update)
         RARCH_LOG("[Vulkan] Driver Pacing available (VK_AMD_anti_lag).\n");
   }
   if (!ll->anti_lag_update)
      ll->anti_lag = false;

   if (!ll->supported)
      return;
   /* The sleep signals a timeline semaphore and nothing else can
    * wait for it. */
   if (!(vk->context.flags & VK_CTX_FLAG_TIMELINE_SEMAPHORE))
   {
      ll->supported = false;
      return;
   }

   ll->set_sleep_mode  = vkGetDeviceProcAddr(vk->context.device,
         "vkSetLatencySleepModeNV");
   ll->sleep           = vkGetDeviceProcAddr(vk->context.device,
         "vkLatencySleepNV");
   ll->set_marker      = vkGetDeviceProcAddr(vk->context.device,
         "vkSetLatencyMarkerNV");
   ll->wait_semaphores = vkGetDeviceProcAddr(vk->context.device,
         "vkWaitSemaphoresKHR");
   if (!ll->wait_semaphores)
      ll->wait_semaphores = vkGetDeviceProcAddr(vk->context.device,
            "vkWaitSemaphores");
   if (     !ll->set_sleep_mode || !ll->sleep || !ll->set_marker
         || !ll->wait_semaphores)
   {
      ll->supported = false;
      return;
   }

   type_info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
   type_info.pNext         = NULL;
   type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
   type_info.initialValue  = 0;
   info.sType              = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   info.pNext              = &type_info;
   info.flags              = 0;
   if (vkCreateSemaphore(vk->context.device, &info, NULL,
            &ll->semaphore) != VK_SUCCESS)
   {
      ll->semaphore = VK_NULL_HANDLE;
      ll->supported = false;
      return;
   }

   /* Revision 3 is where explicit submission attribution starts being
    * honoured; the implicit rules are what this backend relies on, so
    * the revision is logged for the record, not acted on. */
   if (     vkEnumerateDeviceExtensionProperties(vk->context.gpu, NULL,
               &count, NULL) == VK_SUCCESS
         && count
         && (props = (VkExtensionProperties*)malloc(
               count * sizeof(*props))))
   {
      if (vkEnumerateDeviceExtensionProperties(vk->context.gpu, NULL,
               &count, props) == VK_SUCCESS)
         for (i = 0; i < count; i++)
            if (!strcmp(props[i].extensionName,
                     VK_NV_LOW_LATENCY_2_EXTENSION_NAME))
            {
               ll->spec_version = props[i].specVersion;
               break;
            }
      free(props);
   }
   RARCH_LOG("[Vulkan] Low-latency release available (VK_NV_low_latency2 revision %u).\n",
         (unsigned)ll->spec_version);
}

static void vulkan_low_latency_device_deinit(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   unsigned i;
#ifdef VULKAN_DXGI_PACING
   vulkan_dxgi_pacing_deinit(vk);
#endif
   /* The device is idle and the frame loop is this thread: whatever
    * was kept for a marker in flight can go. */
   for (i = 0; i < ll->num_retired; i++)
      vkDestroySwapchainKHR(vk->context.device, ll->retired[i], NULL);
   ll->num_retired = 0;
   /* A sleep the driver never signalled may still signal: its
    * semaphore is left to the device. */
   if (ll->semaphore != VK_NULL_HANDLE && !ll->failed)
      vkDestroySemaphore(vk->context.device, ll->semaphore, NULL);
   ll->semaphore       = VK_NULL_HANDLE;
   ll->supported       = false;
   ll->anti_lag        = false;
   ll->anti_lag_update = NULL;
   ll->swapchain       = VK_NULL_HANDLE;
   ll->mode            = 0;
}

/* Whether the frame loop is outside a marker call now, in the sense
 * that matters: its store to in_call is fenced before its read of
 * pub_gen, and the presenter's last publication fenced before this
 * read, so a 0 here means any call it enters from now on reads the
 * current publication. */
static bool vulkan_low_latency_reader_out(vulkan_low_latency_t *ll)
{
   retro_atomic_thread_fence_seq_cst();
   return retro_atomic_load_acquire_int(&ll->in_call) == 0;
}

/* The swapchain the frame loop's markers may name: the one the mode is
 * on, or none. Two slots under a generation; a slot is rewritten only
 * while the frame loop is out, so no call reads a slot being written,
 * and a publication that finds it in a call waits for the next frame
 * (the old swapchain stays alive meanwhile, see retire). */
static void vulkan_low_latency_publish_tick(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   size_t gen;
   if (!ll->publish_pending)
      return;
   if (!vulkan_low_latency_reader_out(ll))
      return;
   gen                    = retro_atomic_load_relaxed_size(&ll->pub_gen) + 1;
   ll->pub_table[gen & 1] = ll->pub_pending;
   retro_atomic_store_release_size(&ll->pub_gen, gen);
   ll->publish_pending    = false;
}

static void vulkan_low_latency_publish(gfx_ctx_vulkan_data_t *vk,
      VkSwapchainKHR swapchain)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   ll->pub_pending     = swapchain;
   ll->publish_pending = true;
   vulkan_low_latency_publish_tick(vk);
}

/* Retired swapchains go once nothing pending points at them and the
 * frame loop is out of any call that could. */
static void vulkan_low_latency_retire_tick(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   unsigned i;
   vulkan_low_latency_publish_tick(vk);
   if (!ll->num_retired || ll->publish_pending)
      return;
   if (!vulkan_low_latency_reader_out(ll))
      return;
   for (i = 0; i < ll->num_retired; i++)
      vkDestroySwapchainKHR(vk->context.device, ll->retired[i], NULL);
   ll->num_retired = 0;
}

/* A swapchain goes: destroyed now unless the frame loop may be inside
 * a marker call with it, then kept until it is seen outside one. */
void vulkan_low_latency_retire(gfx_ctx_vulkan_data_t *vk,
      VkSwapchainKHR swapchain)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   if (swapchain == VK_NULL_HANDLE)
      return;
   if (ll->swapchain == swapchain)
   {
      ll->swapchain = VK_NULL_HANDLE;
      ll->mode      = 0;
      vulkan_low_latency_publish(vk, VK_NULL_HANDLE);
   }
   if (!ll->supported)
   {
      vkDestroySwapchainKHR(vk->context.device, swapchain, NULL);
      return;
   }
   if (ll->num_retired == ARRAY_SIZE(ll->retired))
   {
      /* Four replacements inside one marker call: the oldest has been
       * out of reach for all of them. */
      vkDestroySwapchainKHR(vk->context.device, ll->retired[0], NULL);
      memmove(ll->retired, ll->retired + 1,
            (ll->num_retired - 1) * sizeof(ll->retired[0]));
      ll->num_retired--;
   }
   ll->retired[ll->num_retired++] = swapchain;
   vulkan_low_latency_retire_tick(vk);
}

/* Puts the mode on the current swapchain, or takes it off: with the
 * mode's fields cleared, as pSleepModeInfo is not optional and the
 * driver reads it either way. */
static bool vulkan_low_latency_apply(gfx_ctx_vulkan_data_t *vk,
      unsigned mode, unsigned min_interval_us)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   VkLatencySleepModeInfoNV mode_info;
   VkResult res;
   mode_info.sType             = VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV;
   mode_info.pNext             = NULL;
   mode_info.lowLatencyMode    = mode ? VK_TRUE : VK_FALSE;
   mode_info.lowLatencyBoost   = (mode == VIDEO_DRIVER_PACING_BOOST) ? VK_TRUE : VK_FALSE;
   mode_info.minimumIntervalUs = mode ? min_interval_us : 0;
   res = ((PFN_vkSetLatencySleepModeNV)ll->set_sleep_mode)(
         vk->context.device, vk->swapchain, &mode_info);
   if (res != VK_SUCCESS)
   {
      ll->failed    = true;
      ll->mode      = 0;
      ll->swapchain = VK_NULL_HANDLE;
      vulkan_low_latency_publish(vk, VK_NULL_HANDLE);
      return false;
   }
   if ((mode != 0) != (ll->mode != 0))
      vulkan_low_latency_publish(vk, mode ? vk->swapchain : VK_NULL_HANDLE);
   ll->mode            = (uint8_t)mode;
   ll->min_interval_us = min_interval_us;
   ll->swapchain       = mode ? vk->swapchain : VK_NULL_HANDLE;
   return true;
}

/* One vkAntiLagUpdateAMD: @stage the stage, or ~0u for a mode update
 * with no frame (the mode taken off) */
static void vulkan_anti_lag_update(gfx_ctx_vulkan_data_t *vk,
      unsigned mode, unsigned stage, uint64_t frame_index)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   VkAntiLagPresentationInfoAMD info;
   VkAntiLagDataAMD data;
   info.sType        = VK_STRUCTURE_TYPE_ANTI_LAG_PRESENTATION_INFO_AMD;
   info.pNext        = NULL;
   info.stage        = (VkAntiLagStageAMD)stage;
   info.frameIndex   = frame_index;
   data.sType        = VK_STRUCTURE_TYPE_ANTI_LAG_DATA_AMD;
   data.pNext        = NULL;
   data.mode         = mode ? VK_ANTI_LAG_MODE_ON_AMD : VK_ANTI_LAG_MODE_OFF_AMD;
   data.maxFPS       = mode
      ? (uint32_t)retro_atomic_load_acquire_int(&ll->amd_max_fps) : 0;
   data.pPresentationInfo = (stage == ~0u) ? NULL : &info;
   ((PFN_vkAntiLagUpdateAMD)ll->anti_lag_update)(vk->context.device, &data);
}

/* The presenter's side of a mode request on VK_AMD_anti_lag: the mode
 * is the device's, so it is recorded for the frame loop's INPUT update
 * and taken off with one update of its own. Boost has nothing to ask
 * for here and reads as On. */
static unsigned vulkan_anti_lag_apply_want(gfx_ctx_vulkan_data_t *vk,
      unsigned mode)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   if (!mode)
   {
      if (ll->mode)
      {
         retro_atomic_store_release_int(&ll->amd_mode, 0);
         vulkan_anti_lag_update(vk, 0, ~0u, 0);
      }
      ll->mode = 0;
      return VIDEO_DRIVER_PACING_REASON_OFF;
   }
   if (vk->swapchain == VK_NULL_HANDLE || vk->context.swapchain_external)
      return VIDEO_DRIVER_PACING_REASON_SWAPCHAIN;
   ll->mode = (uint8_t)mode;
   retro_atomic_store_release_int(&ll->amd_mode, (int)mode);
   return VIDEO_DRIVER_PACING_REASON_NONE;
}

#ifdef VULKAN_DXGI_PACING
/* COM's first three slots, for the reference held on the device */
typedef struct
{
   struct
   {
      void *query_interface;
      unsigned long (__stdcall *add_ref)(void *self);
      unsigned long (__stdcall *release)(void *self);
   } *lpVtbl;
} vulkan_com_object_t;

/* The D3D12 device the DXGI presenter presents with, referenced until
 * the context goes so a sleep on it never outlives it. NULL with no
 * presenter, or with more devices than are kept. */
static void *vulkan_dxgi_pacing_device(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   void *device;
   unsigned i;
   if (!vk->dxgi_presenter)
      return NULL;
   if (!(device = ra_vulkan_dxgi_device(
               (ra_vulkan_dxgi_t*)vk->dxgi_presenter)))
      return NULL;
   for (i = 0; i < ll->dxgi_num_held; i++)
      if (ll->dxgi_held[i] == device)
         return device;
   if (ll->dxgi_num_held == ARRAY_SIZE(ll->dxgi_held))
      return NULL;
   ((vulkan_com_object_t*)device)->lpVtbl->add_ref(device);
   ll->dxgi_held[ll->dxgi_num_held++] = device;
   return device;
}

/* The presenter's side of a mode request while presenting through
 * DXGI: the mode goes on the presenting device through NVAPI, and the
 * frame loop is shown that device while it is in effect. */
static unsigned vulkan_dxgi_pacing_apply_want(gfx_ctx_vulkan_data_t *vk,
      unsigned mode)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   void *device             = vulkan_dxgi_pacing_device(vk);
   unsigned answer;
   if (!device)
   {
      retro_atomic_store_release_size(&ll->dxgi_pub, 0);
      return mode ? VIDEO_DRIVER_PACING_REASON_SWAPCHAIN
                  : VIDEO_DRIVER_PACING_REASON_OFF;
   }
   if (!mode)
      retro_atomic_store_release_int(&ll->stalled, 0);
   else if (retro_atomic_load_acquire_int(&ll->stalled))
   {
      /* The frame loop's sleep failed: off until asked for anew */
      retro_atomic_store_release_size(&ll->dxgi_pub, 0);
      d3d_driver_pacing_set(&ll->dxgi, device, 0);
      return VIDEO_DRIVER_PACING_REASON_FAILED;
   }
   answer = d3d_driver_pacing_set(&ll->dxgi, device, mode);
   /* No NVAPI (Anti-Lag, say), where Vulkan's own present would have
    * a backend: the present is what is in the way */
   if (     answer == VIDEO_DRIVER_PACING_REASON_DEVICE
         && (ll->supported || ll->anti_lag))
      answer = VIDEO_DRIVER_PACING_REASON_SWAPCHAIN;
   retro_atomic_store_release_size(&ll->dxgi_pub,
         (mode && answer == VIDEO_DRIVER_PACING_REASON_NONE)
         ? (size_t)device : 0);
   return answer;
}

/* The device the presenter put the mode on, when it presents there */
static void *vulkan_dxgi_pacing_on(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   return (vk->dxgi_presenter && ll->dxgi.mode && !ll->dxgi.failed)
      ? ll->dxgi.device : NULL;
}

/* The context goes: the mode comes off and the devices are let go */
static void vulkan_dxgi_pacing_deinit(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   unsigned i;
   retro_atomic_store_release_size(&ll->dxgi_pub, 0);
   if (ll->dxgi.device)
      d3d_driver_pacing_free(&ll->dxgi, ll->dxgi.device);
   for (i = 0; i < ll->dxgi_num_held; i++)
      ((vulkan_com_object_t*)ll->dxgi_held[i])->lpVtbl->release(
            ll->dxgi_held[i]);
   ll->dxgi_num_held = 0;
   memset(&ll->dxgi, 0, sizeof(ll->dxgi));
}
#endif

/* The presenter's side of a mode request. */
static unsigned vulkan_low_latency_apply_want(gfx_ctx_vulkan_data_t *vk,
      unsigned mode)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
#ifdef VULKAN_DXGI_PACING
   if (vk->dxgi_presenter)
      return vulkan_dxgi_pacing_apply_want(vk, mode);
   /* Native again: the mode left on the DXGI presenter's device goes */
   if (ll->dxgi.mode && ll->dxgi.device)
   {
      retro_atomic_store_release_size(&ll->dxgi_pub, 0);
      d3d_driver_pacing_set(&ll->dxgi, ll->dxgi.device, 0);
   }
#endif
   if (!ll->supported && ll->anti_lag)
      return vulkan_anti_lag_apply_want(vk, mode);
   if (!ll->supported)
      return ll->core_device ? VIDEO_DRIVER_PACING_REASON_CORE
                             : VIDEO_DRIVER_PACING_REASON_DEVICE;
   /* A replaced swapchain took the mode with it; what is set is
    * what is set on this one. */
   if (ll->swapchain != vk->swapchain)
   {
      ll->swapchain = VK_NULL_HANDLE;
      ll->mode      = 0;
   }
   if (!mode)
   {
      if (ll->mode && !vulkan_low_latency_apply(vk, 0, 0))
         return VIDEO_DRIVER_PACING_REASON_FAILED;
      /* Asked for again is the way back from a failure */
      ll->failed = false;
      retro_atomic_store_release_int(&ll->stalled, 0);
      return VIDEO_DRIVER_PACING_REASON_OFF;
   }
   if (ll->failed || retro_atomic_load_acquire_int(&ll->stalled))
   {
      /* Nothing more is slept on a semaphore the driver still owes */
      if (ll->mode)
         vulkan_low_latency_apply(vk, 0, 0);
      return VIDEO_DRIVER_PACING_REASON_FAILED;
   }
   if (vk->swapchain == VK_NULL_HANDLE || vk->context.swapchain_external)
      return VIDEO_DRIVER_PACING_REASON_SWAPCHAIN;
   if (mode == ll->mode)
      return VIDEO_DRIVER_PACING_REASON_NONE;
   if (!vulkan_low_latency_apply(vk, mode, ll->min_interval_us))
      return VIDEO_DRIVER_PACING_REASON_FAILED;
   return VIDEO_DRIVER_PACING_REASON_NONE;
}

unsigned vulkan_low_latency_set(gfx_ctx_vulkan_data_t *vk, unsigned mode)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   unsigned answer;
   /* The presenter's thread is another one: the request is left for
    * it, and the answer is the one it last gave. A request it has not
    * seen yet reads as pending, never as in effect. */
   if (video_driver_thread_wrapper_active())
   {
      if ((unsigned)retro_atomic_load_relaxed_int(&ll->want) != mode)
      {
         retro_atomic_store_release_int(&ll->want, (int)mode);
         return mode ? VIDEO_DRIVER_PACING_REASON_PENDING
                     : VIDEO_DRIVER_PACING_REASON_OFF;
      }
      answer = (unsigned)retro_atomic_load_acquire_int(&ll->answer);
      if (mode && answer == VIDEO_DRIVER_PACING_REASON_OFF)
         return VIDEO_DRIVER_PACING_REASON_PENDING;
      return answer;
   }
   retro_atomic_store_relaxed_int(&ll->want, (int)mode);
   answer = vulkan_low_latency_apply_want(vk, mode);
   retro_atomic_store_relaxed_int(&ll->answer, (int)answer);
   return answer;
}

void vulkan_low_latency_frame_begin(gfx_ctx_vulkan_data_t *vk,
      uint64_t frame_id, unsigned min_interval_us)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   /* A request from the frame loop's thread, answered here */
   if (video_driver_thread_wrapper_active())
   {
      unsigned want = (unsigned)retro_atomic_load_acquire_int(&ll->want);
      retro_atomic_store_release_int(&ll->answer,
            (int)vulkan_low_latency_apply_want(vk, want));
   }
#ifdef VULKAN_DXGI_PACING
   {
      void *device = vulkan_dxgi_pacing_on(vk);
      if (device)
      {
         d3d_driver_pacing_frame_begin(&ll->dxgi, device, frame_id,
               min_interval_us);
         return;
      }
   }
#endif
   if (!ll->supported)
   {
      /* The frame the PRESENT update names, and the frame limit the
       * frame loop's next INPUT update carries */
      if (ll->anti_lag && ll->mode && frame_id && frame_id != ll->frame_id)
      {
         ll->frame_id    = frame_id;
         ll->stage       = 1;
         retro_atomic_store_release_int(&ll->amd_max_fps, min_interval_us
            ? (int)((1000000 + min_interval_us / 2) / min_interval_us) : 0);
      }
      return;
   }
   vulkan_low_latency_retire_tick(vk);
   if (!ll->mode || ll->failed || ll->swapchain != vk->swapchain)
      return;
   if (!frame_id || frame_id == ll->frame_id)
      return;
   if (     min_interval_us != ll->min_interval_us
         && !vulkan_low_latency_apply(vk, ll->mode, min_interval_us))
      return;
   ll->frame_id = frame_id;
   ll->stage    = 1;
   vulkan_low_latency_marker(vk, frame_id, VK_LATENCY_MARKER_SIMULATION_END_NV);
   vulkan_low_latency_marker(vk, frame_id, VK_LATENCY_MARKER_RENDERSUBMIT_START_NV);
}

void vulkan_low_latency_present_begin(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
#ifdef VULKAN_DXGI_PACING
   if (vk->dxgi_presenter)
   {
      void *device = vulkan_dxgi_pacing_on(vk);
      if (device)
         d3d_driver_pacing_present_begin(&ll->dxgi, device);
      return;
   }
#endif
   /* Anti-Lag names the frame's own present only; it has no notion of
    * the presents after it */
   if (ll->anti_lag && !ll->supported)
   {
      if (ll->mode && ll->frame_id && ll->stage == 1)
      {
         ll->stage = 2;
         vulkan_anti_lag_update(vk, ll->mode, VK_ANTI_LAG_STAGE_PRESENT_AMD,
               ll->frame_id);
      }
      return;
   }
   if (!ll->mode || ll->failed || ll->swapchain != vk->swapchain || !ll->frame_id)
      return;
   if (ll->stage == 1)
   {
      vulkan_low_latency_marker(vk, ll->frame_id, VK_LATENCY_MARKER_RENDERSUBMIT_END_NV);
      vulkan_low_latency_marker(vk, ll->frame_id, VK_LATENCY_MARKER_PRESENT_START_NV);
   }
   else if (ll->stage == 2)
      vulkan_low_latency_marker(vk, ll->frame_id,
            VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_START_NV);
}

void vulkan_low_latency_present_end(gfx_ctx_vulkan_data_t *vk)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   VkLatencySleepInfoNV sleep_info;
#ifdef VULKAN_DXGI_PACING
   if (vk->dxgi_presenter)
   {
      void *device = vulkan_dxgi_pacing_on(vk);
      if (device)
         d3d_driver_pacing_present_end(&ll->dxgi, device);
      return;
   }
#endif
   if (!ll->mode || ll->failed || ll->swapchain != vk->swapchain || !ll->frame_id)
      return;
   if (ll->stage == 2)
   {
      vulkan_low_latency_marker(vk, ll->frame_id,
            VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_END_NV);
      return;
   }
   if (ll->stage != 1)
      return;
   ll->stage = 2;
   vulkan_low_latency_marker(vk, ll->frame_id, VK_LATENCY_MARKER_PRESENT_END_NV);
   /* The one sleep between this present and the next: it signals the
    * frame's identity when the next frame may start, which is what
    * the frame loop waits for. */
   if (ll->frame_id <= ll->sleep_value)
      return;
   sleep_info.sType           = VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV;
   sleep_info.pNext           = NULL;
   sleep_info.signalSemaphore = ll->semaphore;
   sleep_info.value           = ll->frame_id;
   if (((PFN_vkLatencySleepNV)ll->sleep)(vk->context.device,
            vk->swapchain, &sleep_info) != VK_SUCCESS)
   {
      ll->failed = true;
      retro_atomic_store_release_int(&ll->answer, VIDEO_DRIVER_PACING_REASON_FAILED);
      return;
   }
   ll->sleep_value = ll->frame_id;
}

bool vulkan_low_latency_wait(gfx_ctx_vulkan_data_t *vk,
      uint64_t frame_id, uint64_t next_id, unsigned max_wait_us)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   VkSemaphoreWaitInfo wait_info;
   VkResult res;
#ifdef VULKAN_DXGI_PACING
   {
      /* Through DXGI, NvAPI_D3D_Sleep is the hold, on the device the
       * presenter shows */
      void *device = (void*)retro_atomic_load_acquire_size(&ll->dxgi_pub);
      if (device)
      {
         if (retro_atomic_load_acquire_int(&ll->answer)
               != VIDEO_DRIVER_PACING_REASON_NONE)
            return false;
         if (d3d_driver_pacing_sleep_on(device))
            return true;
         retro_atomic_store_release_int(&ll->stalled, 1);
         retro_atomic_store_release_int(&ll->answer,
               VIDEO_DRIVER_PACING_REASON_FAILED);
         return false;
      }
   }
#endif
   /* Anti-Lag's INPUT update is the hold, inside the driver, for the
    * frame about to sample its input; the device-wide call is safe
    * beside the presenter's on another thread. */
   if (ll->anti_lag && !ll->supported)
   {
      unsigned mode = (unsigned)retro_atomic_load_acquire_int(&ll->amd_mode);
      if (!mode || retro_atomic_load_acquire_int(&ll->answer)
            != VIDEO_DRIVER_PACING_REASON_NONE)
         return false;
      vulkan_anti_lag_update(vk, mode, VK_ANTI_LAG_STAGE_INPUT_AMD, next_id);
      return true;
   }
   (void)next_id;
   if (!ll->supported || ll->semaphore == VK_NULL_HANDLE)
      return false;
   if (retro_atomic_load_acquire_int(&ll->answer) != VIDEO_DRIVER_PACING_REASON_NONE)
      return false;
   if (!frame_id)
      return true;
   /* The hold: the driver signals when the next frame should start.
    * A signal that does not come within the bound is a driver that has
    * stopped pacing - a lost device, a sleeping display - or a frame
    * that never reached a present; a few in a row and this thread is
    * not held for it again until the mode is asked for anew. */
   wait_info.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
   wait_info.pNext          = NULL;
   wait_info.flags          = 0;
   wait_info.semaphoreCount = 1;
   wait_info.pSemaphores    = &ll->semaphore;
   wait_info.pValues        = &frame_id;
   res = ((PFN_vkWaitSemaphores)ll->wait_semaphores)(vk->context.device,
         &wait_info, (uint64_t)max_wait_us * 1000);
   if (res == VK_SUCCESS)
   {
      ll->timeouts = 0;
      return true;
   }
   /* The presenter's thread reads stalled where it answers the mode
    * request; this thread owns nothing else here. */
   if (res != VK_TIMEOUT || ++ll->timeouts >= 4)
   {
      ll->timeouts = 0;
      retro_atomic_store_release_int(&ll->stalled, 1);
      retro_atomic_store_release_int(&ll->answer, VIDEO_DRIVER_PACING_REASON_FAILED);
   }
   return false;
}

static void vulkan_low_latency_marker(gfx_ctx_vulkan_data_t *vk,
      uint64_t frame_id, unsigned marker)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   VkSetLatencyMarkerInfoNV info;
   if (!ll->mode || ll->failed || ll->swapchain != vk->swapchain || !frame_id)
      return;
   info.sType     = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV;
   info.pNext     = NULL;
   info.presentID = frame_id;
   info.marker    = (VkLatencyMarkerNV)marker;
   ((PFN_vkSetLatencyMarkerNV)ll->set_marker)(vk->context.device,
         vk->swapchain, &info);
}

void vulkan_low_latency_mark_main(gfx_ctx_vulkan_data_t *vk,
      uint64_t frame_id, unsigned marker)
{
   vulkan_low_latency_t *ll = &vk->low_latency;
   VkSetLatencyMarkerInfoNV info;
   VkSwapchainKHR swapchain;
   size_t gen;
#ifdef VULKAN_DXGI_PACING
   {
      void *device = (void*)retro_atomic_load_acquire_size(&ll->dxgi_pub);
      if (device)
      {
         d3d_driver_pacing_mark_on(device, frame_id, marker);
         return;
      }
   }
#endif
   if (!ll->supported || !frame_id)
      return;
   /* in_call before pub_gen, a full fence between, against the
    * presenter's publish, fence, read of in_call: see
    * vulkan_low_latency_reader_out */
   retro_atomic_store_release_int(&ll->in_call, 1);
   retro_atomic_thread_fence_seq_cst();
   gen       = retro_atomic_load_acquire_size(&ll->pub_gen);
   swapchain = ll->pub_table[gen & 1];
   if (swapchain != VK_NULL_HANDLE)
   {
      info.sType     = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV;
      info.pNext     = NULL;
      info.presentID = frame_id;
      info.marker    = (VkLatencyMarkerNV)marker;
      ((PFN_vkSetLatencyMarkerNV)ll->set_marker)(vk->context.device,
            swapchain, &info);
   }
   retro_atomic_store_release_int(&ll->in_call, 0);
}

void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index)
{
   VkPresentInfoKHR present;
   VkPresentTimesInfoGOOGLE times;
   VkPresentTimeGOOGLE ptime;
   VkResult result                 = VK_SUCCESS;
   VkResult err                    = VK_SUCCESS;

   present.sType                   = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   present.pNext                   = NULL;
   present.waitSemaphoreCount      = 1;
   present.pWaitSemaphores         = &vk->context.swapchain_semaphores[index];
   present.swapchainCount          = 1;
   present.pSwapchains             = &vk->swapchain;
   present.pImageIndices           = &index;
   present.pResults                = &result;

   /* An ID per present, so the timing query below can name it. */
   if (vk->display_timing_supported && vk->display_timing_query)
   {
      times.sType                = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE;
      times.pNext                = NULL;
      times.swapchainCount       = 1;
      times.pTimes               = &ptime;
      ptime.presentID            = ++vk->present_id;
      ptime.desiredPresentTime   = 0;
      present.pNext              = &times;
   }

   /* queue_lock is for the queue call and is dropped the moment it
    * returns. It used to be kept to the end of this function, across
    * the failure handling - and a failed present tears the swapchain
    * down, which waits for the present with an empty fenced submission
    * behind it (vulkan_context_wait_frames), which takes queue_lock.
    * Where the mutex is not recursive - everywhere but Windows - the
    * thread then waited on itself: an out-of-date present, as on a
    * resize, never returned. */
#if defined(_WIN32) && !defined(__WINRT__)
   if (vk->dxgi_presenter)
   {
      vulkan_low_latency_present_begin(vk);
      err = ra_vulkan_dxgi_present((ra_vulkan_dxgi_t *)vk->dxgi_presenter,
            index, present.waitSemaphoreCount, present.pWaitSemaphores,
            vk->context.swap_interval > 0 ? (unsigned)vk->context.swap_interval : 0);
      vulkan_low_latency_present_end(vk);
   }
   else
#endif
   {
      vulkan_low_latency_present_begin(vk);
#ifdef HAVE_THREADS
      slock_lock(vk->context.queue_lock);
#endif
      err = vkQueuePresentKHR(vk->context.queue, &present);
#ifdef HAVE_THREADS
      slock_unlock(vk->context.queue_lock);
#endif
      vulkan_low_latency_present_end(vk);
   }
   /* Queued whatever it returned: a failed present has still put its
    * semaphore wait on the queue, or may have, and the fence taken
    * before the next rebuild covers either. */
   vk->context.present_pending = true;

   /* The frame was shown, but the swapchain no longer matches the
    * surface: a Wayland WSI says this when the compositor's dma-buf
    * feedback changes under it - a window gone fullscreen is offered
    * the scanout tranche, and the images it made before, for being
    * composited, are not in it. Rebuilding is what lets the WSI
    * allocate from the new tranche, and so what gets the window onto
    * the display plane. One rebuild per answer: where the surface
    * stays suboptimal (Android without prerotate) the rebuilt swapchain
    * is kept until a present reports otherwise. */
   if (     (result == VK_SUBOPTIMAL_KHR || err == VK_SUBOPTIMAL_KHR)
         && !(vk->context.flags & VK_CTX_FLAG_SUBOPTIMAL_REBUILT))
   {
      RARCH_LOG("[Vulkan] Swapchain is suboptimal for the surface, rebuilding.\n");
      vk->context.flags                   |= VK_CTX_FLAG_SUBOPTIMAL_REBUILT;
      vk->context.swapchain_never_presented = 0;
      vulkan_destroy_swapchain(vk);
      return;
   }
   if (result == VK_SUBOPTIMAL_KHR)
      result = VK_SUCCESS;
   else if (err == VK_SUCCESS && result == VK_SUCCESS)
      vk->context.flags &= ~VK_CTX_FLAG_SUBOPTIMAL_REBUILT;
   if (err == VK_SUBOPTIMAL_KHR)
      err = VK_SUCCESS;

#ifdef WSI_HARDENING_TEST
   trigger_spurious_error_vkresult(&err);
#endif

   if (err == VK_SUCCESS && result == VK_SUCCESS)
      vk->context.swapchain_never_presented = 0;

   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      RARCH_LOG("[Vulkan] QueuePresent failed (err = %d, result = %d), destroying swapchain.\n",
            (int)err, (int)result);
      /* Swapchain after swapchain with nothing ever shown: the GPU the
       * index picked cannot present to this display, whatever it
       * answered when asked. Say so once rather than loop in silence. */
      if (++vk->context.swapchain_never_presented == 8)
      {
         struct string_list *gpus = video_driver_get_gpu_api_devices(
               GFX_CTX_VULKAN_API);
         int idx                  = vk->context.gpu_index;
         RARCH_ERR("[Vulkan] Nothing has reached the display through %u swapchains.\n",
               vk->context.swapchain_never_presented);
         if (gpus && idx > 0 && idx < (int)gpus->size)
            RARCH_ERR("[Vulkan] GPU #%d (\"%s\") cannot present here; set GPU Index back to 0 in Video -> Output.\n",
                  idx, gpus->elems[idx].data);
         else
            RARCH_ERR("[Vulkan] The GPU in use cannot present to this display.\n");
      }
      /* A lost device does not come back with a new swapchain: the
       * whole driver has to, and the runloop does that when it sees
       * the flag (after a TDR, a GPU reset).
       *
       * One VK_ERROR_DEVICE_LOST from a present is not taken as that.
       * NVIDIA's driver has been seen to answer the first present
       * after the swapchain is rebuilt for a new swap interval with
       * it - a moment after the driver starts, as on every fullscreen
       * toggle with the menu up - on a device that goes on to build
       * the next swapchain without complaint. Taking it at its word
       * rebuilt the whole video driver, window included; the new
       * device answered the same way, and the run loop's retries, each
       * later than the last, ended in giving up with a dead picture.
       *
       * So a present that fails this way costs a swapchain, as any
       * failed present does. The device is given up on when presents
       * have failed on swapchain after swapchain, or when the frame's
       * own submit says it is lost (vulkan_check_device_lost), which a
       * device that is really gone does on the next frame. */
      if (     (err == VK_ERROR_DEVICE_LOST || result == VK_ERROR_DEVICE_LOST)
            && vk->context.swapchain_never_presented >= 8)
         video_driver_modify_disp_flags(VIDEO_FLAG_GPU_DEVICE_LOST, 0);
#if defined(_WIN32) && !defined(__WINRT__)
      if (vk->dxgi_presenter)
         vk->dxgi_failed = true;
#endif
      vulkan_destroy_swapchain(vk);
   }
}
