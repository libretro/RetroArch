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
#include <dynamic/dylib.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <retro_timers.h>
#include <retro_math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_X11
#ifdef HAVE_XCB
#include <X11/Xlib-xcb.h>
#endif
#endif

#include "vulkan_common.h"
#include "../include/vulkan/vulkan.h"
#include "vksym.h"
#include <libretro_vulkan.h>

#include "../../verbosity.h"
#include "../../configuration.h"

#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_NV 0x10DE
#define VENDOR_ID_INTEL 0x8086

#if defined(_WIN32)
#define VULKAN_EMULATE_MAILBOX
#endif

/* TODO/FIXME - static globals */
static dylib_t                       vulkan_library;
static VkInstance                    cached_instance_vk;
static VkDevice                      cached_device_vk;
static retro_vulkan_destroy_device_t cached_destroy_device_vk;

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
   if (     (msg_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
         && (msg_type     == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT))
   {
      RARCH_ERR("[Vulkan]: Validation Error: %s\n", pCallbackData->pMessage);
   }
   return VK_FALSE;
}
#endif

static void vulkan_emulated_mailbox_deinit(
      struct vulkan_emulated_mailbox *mailbox)
{
   if (mailbox->thread)
   {
      slock_lock(mailbox->lock);
      mailbox->flags |= VK_MAILBOX_FLAG_DEAD;
      scond_signal(mailbox->cond);
      slock_unlock(mailbox->lock);
      sthread_join(mailbox->thread);
   }

   if (mailbox->lock)
      slock_free(mailbox->lock);
   if (mailbox->cond)
      scond_free(mailbox->cond);

   memset(mailbox, 0, sizeof(*mailbox));
}

static VkResult vulkan_emulated_mailbox_acquire_next_image(
      struct vulkan_emulated_mailbox *mailbox,
      unsigned *index)
{
   VkResult res                    = VK_TIMEOUT;

   slock_lock(mailbox->lock);

   if (!(mailbox->flags & VK_MAILBOX_FLAG_HAS_PENDING_REQUEST))
   {
      mailbox->flags |= VK_MAILBOX_FLAG_REQUEST_ACQUIRE;
      scond_signal(mailbox->cond);
   }

   mailbox->flags |= VK_MAILBOX_FLAG_HAS_PENDING_REQUEST;

   if (mailbox->flags & VK_MAILBOX_FLAG_ACQUIRED)
   {
      res                          = mailbox->result;
      *index                       = mailbox->index;
      mailbox->flags              &= ~(VK_MAILBOX_FLAG_HAS_PENDING_REQUEST
                                     | VK_MAILBOX_FLAG_ACQUIRED);
   }

   slock_unlock(mailbox->lock);
   return res;
}

static VkResult vulkan_emulated_mailbox_acquire_next_image_blocking(
      struct vulkan_emulated_mailbox *mailbox,
      unsigned *index)
{
   VkResult res = VK_SUCCESS;

   slock_lock(mailbox->lock);

   if (!(mailbox->flags & VK_MAILBOX_FLAG_HAS_PENDING_REQUEST))
   {
      mailbox->flags |= VK_MAILBOX_FLAG_REQUEST_ACQUIRE;
      scond_signal(mailbox->cond);
   }

   mailbox->flags |= VK_MAILBOX_FLAG_HAS_PENDING_REQUEST;

   while (!(mailbox->flags & VK_MAILBOX_FLAG_ACQUIRED))
      scond_wait(mailbox->cond, mailbox->lock);

   if ((res = mailbox->result) == VK_SUCCESS)
      *index                    = mailbox->index;
   mailbox->flags              &= ~(VK_MAILBOX_FLAG_HAS_PENDING_REQUEST
                                  | VK_MAILBOX_FLAG_ACQUIRED);

   slock_unlock(mailbox->lock);
   return res;
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
      slock_lock(mailbox->lock);
      while (   !(mailbox->flags & VK_MAILBOX_FLAG_DEAD)
             && !(mailbox->flags & VK_MAILBOX_FLAG_REQUEST_ACQUIRE))
         scond_wait(mailbox->cond, mailbox->lock);

      if (mailbox->flags & VK_MAILBOX_FLAG_DEAD)
      {
         slock_unlock(mailbox->lock);
         break;
      }

      mailbox->flags &= ~VK_MAILBOX_FLAG_REQUEST_ACQUIRE;
      slock_unlock(mailbox->lock);

      mailbox->result          = vkAcquireNextImageKHR(
            mailbox->device, mailbox->swapchain, UINT64_MAX,
            VK_NULL_HANDLE, fence, &mailbox->index);

      /* VK_SUBOPTIMAL_KHR can be returned on Android 10
       * when prerotate is not dealt with.
       * It can also be returned by WSI when the surface
       * is _temorarily_ suboptimal.
       * This is not an error we need to care about,
       * and we'll treat it as SUCCESS. */
      if (mailbox->result == VK_SUBOPTIMAL_KHR)
         mailbox->result = VK_SUCCESS;

      if (mailbox->result == VK_SUCCESS)
      {
         vkWaitForFences(mailbox->device, 1, &fence, true, UINT64_MAX);
         vkResetFences(mailbox->device, 1, &fence);

         slock_lock(mailbox->lock);
         mailbox->flags |= VK_MAILBOX_FLAG_ACQUIRED;
         scond_signal(mailbox->cond);
         slock_unlock(mailbox->lock);
      }
      else
         vkResetFences(mailbox->device, 1, &fence);
   }

   vkDestroyFence(mailbox->device, fence, NULL);
}

static bool vulkan_emulated_mailbox_init(
      struct vulkan_emulated_mailbox *mailbox,
      VkDevice device,
      VkSwapchainKHR swapchain)
{
   mailbox->thread              = NULL;
   mailbox->lock                = NULL;
   mailbox->cond                = NULL;
   mailbox->device              = device;
   mailbox->swapchain           = swapchain;
   mailbox->index               = 0;
   mailbox->result              = VK_SUCCESS;
   mailbox->flags               = 0;

   if (!(mailbox->cond      = scond_new()))
      return false;
   if (!(mailbox->lock      = slock_new()))
      return false;
   if (!(mailbox->thread    = sthread_create(vulkan_emulated_mailbox_loop,
               mailbox)))
      return false;
   return true;
}

static void vulkan_debug_mark_object(VkDevice device,
      VkObjectType object_type, uint64_t object_handle, const char *name, unsigned count)
{
   if (vkSetDebugUtilsObjectNameEXT)
   {
      char merged_name[1024];
      VkDebugUtilsObjectNameInfoEXT info;
      size_t _len                        = strlcpy(merged_name, name, sizeof(merged_name));
      snprintf(merged_name + _len, sizeof(merged_name) - _len, " (%u)", count);

      info.sType                         = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      info.pNext                         = NULL;
      info.objectType                    = object_type;
      info.objectHandle                  = object_handle;
      info.pObjectName                   = merged_name;
      vkSetDebugUtilsObjectNameEXT(device, &info);
   }
}

static bool vulkan_buffer_chain_suballoc(struct vk_buffer_chain *chain,
      size_t size, struct vk_buffer_range *range)
{
   VkDeviceSize next_offset = chain->offset + size;
   if (next_offset <= chain->current->buffer.size)
   {
      range->data   = (uint8_t*)chain->current->buffer.mapped + chain->offset;
      range->buffer = chain->current->buffer.buffer;
      range->offset = chain->offset;
      chain->offset = (next_offset + chain->alignment - 1)
         & ~(chain->alignment - 1);

      return true;
   }

   return false;
}

static struct vk_buffer_node *vulkan_buffer_chain_alloc_node(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer_node *node = (struct vk_buffer_node*)
      malloc(sizeof(*node));
   if (!node)
      return NULL;

   node->buffer = vulkan_create_buffer(
         context, size, usage);
   node->next   = NULL;
   return node;
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
      RARCH_ERR("[Vulkan]: Could not find required instance extensions. Will attempt without them.\n");
      ret = false;
      goto end;
   }

   memcpy((void*)(enabled + count), exts, num_exts * sizeof(*exts));
   count += num_exts;

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
      RARCH_ERR("[Vulkan]: Could not find device extension. Will attempt without it.\n");
      ret = false;
      goto end;
   }

   memcpy((void*)(enabled + count), exts, num_exts * sizeof(*exts));
   count += num_exts;

   for (i = 0; i < num_optional_exts; i++)
      if (vulkan_find_extensions(&optional_exts[i], 1, properties, property_count))
         enabled[count++] = optional_exts[i];

end:
   free(properties);
   *inout_enabled_count = count;
   return ret;
}

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
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      return false;
   }

   if (!(gpus = (VkPhysicalDevice*)calloc(gpu_count, sizeof(*gpus))))
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      return false;
   }

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, gpus) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      free(gpus);
      return false;
   }

   if (gpu_count < 1)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate Vulkan physical device.\n");
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

      RARCH_LOG("[Vulkan]: Found GPU at index %d: \"%s\".\n", i, gpu_properties.deviceName);

      string_list_append(vk->gpu_list, gpu_properties.deviceName, attr);
   }

   video_driver_set_gpu_api_devices(GFX_CTX_VULKAN_API, vk->gpu_list);

   if (0 <= gpu_index && gpu_index < (int)gpu_count)
   {
      RARCH_LOG("[Vulkan]: Using GPU index %d.\n", gpu_index);
      vk->context.gpu = gpus[gpu_index];
   }
   else
   {
      RARCH_WARN("[Vulkan]: Invalid GPU index %d, using first device found.\n", gpu_index);
      vk->context.gpu = gpus[0];
   }

   free(gpus);
   return true;
}

static const char *vulkan_device_extensions[]  = {
   "VK_KHR_swapchain",
};

static const char *vulkan_optional_device_extensions[] = {
   "VK_KHR_sampler_mirror_clamp_to_edge",
};

static VkDevice vulkan_context_create_device_wrapper(
      VkPhysicalDevice gpu, void *opaque,
      const VkDeviceCreateInfo *create_info)
{
   VkResult res;
   VkDeviceCreateInfo info        = *create_info;
   VkDevice device                = VK_NULL_HANDLE;
   const char **device_extensions = (const char **)malloc(
         (info.enabledExtensionCount +
               ARRAY_SIZE(vulkan_device_extensions) +
               ARRAY_SIZE(vulkan_optional_device_extensions)) * sizeof(const char *));

   memcpy((void*)device_extensions, info.ppEnabledExtensionNames, info.enabledExtensionCount * sizeof(const char *));
   info.ppEnabledExtensionNames = device_extensions;

   if (!(vulkan_find_device_extensions(gpu,
         device_extensions, &info.enabledExtensionCount,
         vulkan_device_extensions, ARRAY_SIZE(vulkan_device_extensions),
         vulkan_optional_device_extensions,
         ARRAY_SIZE(vulkan_optional_device_extensions))))
   {
      RARCH_ERR("[Vulkan]: Could not find required device extensions.\n");
      return VK_NULL_HANDLE;
   }

   /* When we get around to using fancier features we can chain in PDF2 stuff. */
   if ((res = vkCreateDevice(gpu, &info, NULL, &device)) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to create device (%d).\n", res);
      device = VK_NULL_HANDLE;
      goto end;
   }

end:
   free((void*)device_extensions);
   return device;
}

static bool vulkan_context_init_device(gfx_ctx_vulkan_data_t *vk)
{
   uint32_t queue_count;
   unsigned i;
   const char *enabled_device_extensions[8];
   VkDeviceCreateInfo device_info;
   VkDeviceQueueCreateInfo queue_info;
   static const float one                  = 1.0f;
   bool found_queue                        = false;
   video_driver_state_t *video_st          = video_state_get_ptr();

   VkPhysicalDeviceFeatures features       = { false };

   unsigned enabled_device_extension_count = 0;

   struct retro_hw_render_context_negotiation_interface_vulkan
                                    *iface = (struct retro_hw_render_context_negotiation_interface_vulkan*)
                                    video_st->hw_render_context_negotiation;

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
         RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong API.\n");
         iface = NULL;
      }
      else if (iface->interface_version == 0)
      {
         RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong interface version.\n");
         iface = NULL;
      }
      else
         RARCH_LOG("[Vulkan]: Got HW context negotiation interface %u.\n", iface->interface_version);
   }

   if (!vulkan_context_init_gpu(vk))
      return false;

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
            RARCH_WARN("[Vulkan]: Failed to create_device2 on provided VkPhysicalDevice, letting core decide which GPU to use.\n");
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
         ret = iface->create_device(&context, vk->context.instance,
               vk->context.gpu,
               vk->vk_surface,
               vulkan_symbol_wrapper_instance_proc_addr(),
               vulkan_device_extensions,
               ARRAY_SIZE(vulkan_device_extensions),
               NULL,
               0,
               &features);
      }

      if (ret)
      {
         if (vk->context.gpu != VK_NULL_HANDLE && context.gpu != vk->context.gpu)
            RARCH_ERR("[Vulkan]: Got unexpected VkPhysicalDevice, despite RetroArch using explicit physical device.\n");

         vk->context.destroy_device       = iface->destroy_device;

         vk->context.device               = context.device;
         vk->context.queue                = context.queue;
         vk->context.gpu                  = context.gpu;
         vk->context.graphics_queue_index = context.queue_family_index;
         vk->context.queue                = context.queue;

         if (context.presentation_queue != context.queue)
         {
            RARCH_ERR("[Vulkan]: Present queue != graphics queue. This is currently not supported.\n");
            return false;
         }
      }
      else
      {
         RARCH_WARN("[Vulkan]: Failed to create device with negotiation interface. Falling back to default path.\n");
      }
   }

   if (cached_device_vk && cached_destroy_device_vk)
   {
      vk->context.destroy_device = cached_destroy_device_vk;
      cached_destroy_device_vk   = NULL;
   }

   vkGetPhysicalDeviceProperties(vk->context.gpu,
         &vk->context.gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(vk->context.gpu,
         &vk->context.memory_properties);

#ifdef VULKAN_EMULATE_MAILBOX
   /* Win32 windowed mode seems to deal just fine with toggling VSync.
    * Fullscreen however ... */
   if (vk->flags & VK_DATA_FLAG_FULLSCREEN)
      vk->flags |=  VK_DATA_FLAG_EMULATE_MAILBOX;
   else
      vk->flags &= ~VK_DATA_FLAG_EMULATE_MAILBOX;
#endif

   /* If we're emulating mailbox, stick to using fences rather than semaphores.
    * Avoids some really weird driver bugs. */
   if (!(vk->flags & VK_DATA_FLAG_EMULATE_MAILBOX))
   {
      if (vk->context.gpu_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
      {
         vk->flags |= VK_DATA_FLAG_USE_WSI_SEMAPHORE;
         RARCH_LOG("[Vulkan]: Using semaphores for WSI acquire.\n");
      }
      else
      {
         vk->flags &= ~VK_DATA_FLAG_USE_WSI_SEMAPHORE;
         RARCH_LOG("[Vulkan]: Using fences for WSI acquire.\n");
      }
   }

   RARCH_LOG("[Vulkan]: Using GPU: \"%s\".\n", vk->context.gpu_properties.deviceName);

   {
      char version_str[128];
      size_t len            = snprintf(version_str      , sizeof(version_str)      , "%u", VK_VERSION_MAJOR(vk->context.gpu_properties.apiVersion));
      version_str[  len]    = '.';
      version_str[++len]    = '\0';
      len                  += snprintf(version_str + len, sizeof(version_str) - len, "%u", VK_VERSION_MINOR(vk->context.gpu_properties.apiVersion));
      version_str[  len]    = '.';
      version_str[++len]    = '\0';
      snprintf(version_str + len, sizeof(version_str) - len, "%u", VK_VERSION_PATCH(vk->context.gpu_properties.apiVersion));
      video_driver_set_gpu_api_version_string(version_str);
   }

   if (vk->context.device == VK_NULL_HANDLE)
   {
      VkQueueFamilyProperties *queue_properties = NULL;
      vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
            &queue_count, NULL);

      if (queue_count < 1)
      {
         RARCH_ERR("[Vulkan]: Invalid number of queues detected.\n");
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
         vkGetPhysicalDeviceSurfaceSupportKHR(
               vk->context.gpu, i,
               vk->vk_surface, &supported);
         if (supported && ((queue_properties[i].queueFlags & required) == required))
         {
            vk->context.graphics_queue_index = i;
            RARCH_LOG("[Vulkan]: Queue family %u supports %u sub-queues.\n",
                  i, queue_properties[i].queueCount);
            found_queue = true;
            break;
         }
      }

      free(queue_properties);

      if (!found_queue)
      {
         RARCH_ERR("[Vulkan]: Did not find suitable graphics queue.\n");
         return false;
      }

      if (!(vulkan_find_device_extensions(vk->context.gpu,
              enabled_device_extensions, &enabled_device_extension_count,
              vulkan_device_extensions, ARRAY_SIZE(vulkan_device_extensions),
              vulkan_optional_device_extensions,
              ARRAY_SIZE(vulkan_optional_device_extensions))))
      {
          RARCH_ERR("[Vulkan]: Could not find required device extensions.\n");
          return false;
      }

      queue_info.queueFamilyIndex         = vk->context.graphics_queue_index;
      queue_info.queueCount               = 1;
      queue_info.pQueuePriorities         = &one;

      device_info.queueCreateInfoCount    = 1;
      device_info.pQueueCreateInfos       = &queue_info;
      device_info.enabledExtensionCount   = enabled_device_extension_count;
      device_info.ppEnabledExtensionNames = enabled_device_extensions;
      device_info.pEnabledFeatures        = &features;

      if (cached_device_vk)
      {
         vk->context.device = cached_device_vk;
         cached_device_vk   = NULL;

         video_st->flags   |= VIDEO_FLAG_CACHE_CONTEXT_ACK;
         RARCH_LOG("[Vulkan]: Using cached Vulkan context.\n");
      }
      else if (vkCreateDevice(vk->context.gpu, &device_info,
               NULL, &vk->context.device) != VK_SUCCESS)
      {
         RARCH_ERR("[Vulkan]: Failed to create device.\n");
         return false;
      }
   }

   if (!vulkan_load_device_symbols(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to load device symbols.\n");
      return false;
   }

   if (vk->context.queue == VK_NULL_HANDLE)
   {
      vkGetDeviceQueue(vk->context.device,
            vk->context.graphics_queue_index, 0, &vk->context.queue);
   }

#ifdef HAVE_THREADS
   vk->context.queue_lock = slock_new();
   if (!vk->context.queue_lock)
   {
      RARCH_ERR("[Vulkan]: Failed to create queue lock.\n");
      return false;
   }
#endif

   return true;
}

#ifdef VULKAN_HDR_SWAPCHAIN
#define VULKAN_COLORSPACE_EXTENSION_NAME "VK_EXT_swapchain_colorspace"
#endif

static const char *vulkan_optional_instance_extensions[] = {
#ifdef VULKAN_HDR_SWAPCHAIN
   VULKAN_COLORSPACE_EXTENSION_NAME
#endif
};

static VkInstance vulkan_context_create_instance_wrapper(void *opaque, const VkInstanceCreateInfo *create_info)
{
   VkResult res;
   uint32_t i, layer_count;
   VkLayerProperties properties[128];
   gfx_ctx_vulkan_data_t *vk        = (gfx_ctx_vulkan_data_t *)opaque;
   VkInstanceCreateInfo info        = *create_info;
   VkInstance instance              = VK_NULL_HANDLE;
   const char **instance_extensions = (const char**)malloc((info.enabledExtensionCount + 3
                                                          + ARRAY_SIZE(vulkan_optional_device_extensions)) * sizeof(const char *));
   const char **instance_layers     = (const char**)malloc((info.enabledLayerCount     + 1)                * sizeof(const char *));

   const char *required_extensions[3];
   uint32_t required_extension_count = 0;

   memcpy((void*)instance_extensions, info.ppEnabledExtensionNames, info.enabledExtensionCount * sizeof(const char *));
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
         required_extensions[required_extension_count++] = "VK_MVK_macos_surface";
         break;
      case VULKAN_WSI_MVK_IOS:
         required_extensions[required_extension_count++] = "VK_MVK_ios_surface";
         break;
      case VULKAN_WSI_NONE:
      default:
         break;
   }

#ifdef VULKAN_DEBUG
   instance_layers[info.enabledLayerCount++]         = "VK_LAYER_KHRONOS_validation";
   required_extensions[required_extension_count++] = "VK_EXT_debug_utils";
#endif

   layer_count = ARRAY_SIZE(properties);
   vkEnumerateInstanceLayerProperties(&layer_count, properties);

   if (!(vulkan_find_instance_extensions(
            instance_extensions, &info.enabledExtensionCount,
            required_extensions, required_extension_count,
            vulkan_optional_instance_extensions,
            ARRAY_SIZE(vulkan_optional_instance_extensions))))
   {
      RARCH_ERR("[Vulkan]: Instance does not support required extensions.\n");
      goto end;
   }

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

   if (info.pApplicationInfo)
   {
      uint32_t supported_instance_version = VK_API_VERSION_1_0;
      if (!vkEnumerateInstanceVersion || vkEnumerateInstanceVersion(&supported_instance_version) != VK_SUCCESS)
         supported_instance_version = VK_API_VERSION_1_0;

      if (supported_instance_version < info.pApplicationInfo->apiVersion)
      {
         RARCH_ERR("[Vulkan]: Core requests apiVersion %u.%u, but it is not supported by loader.\n",
               VK_VERSION_MAJOR(info.pApplicationInfo->apiVersion),
               VK_VERSION_MINOR(info.pApplicationInfo->apiVersion));
         goto end;
      }
   }

   if ((res = vkCreateInstance(&info, NULL, &instance)) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to create Vulkan instance (%d).\n", res);
      RARCH_ERR("[Vulkan]: If VULKAN_DEBUG=1 is enabled, make sure Vulkan validation layers are installed.\n");
      for (i = 0; i < info.enabledLayerCount; i++)
         RARCH_ERR("[Vulkan]: Core explicitly enables layer (%s), this might be cause of failure.\n", info.ppEnabledLayerNames[i]);
      instance = VK_NULL_HANDLE;
      goto end;
   }

end:
   free((void*)instance_extensions);
   free((void*)instance_layers);
   return instance;
}

static bool vulkan_update_display_mode(
      unsigned *width,
      unsigned *height,
      const VkDisplayModePropertiesKHR *mode,
      const struct vulkan_display_surface_info *info)
{
   unsigned visible_width  = mode->parameters.visibleRegion.width;
   unsigned visible_height = mode->parameters.visibleRegion.height;

   if (!info->width || !info->height)
   {
      /* Strategy here is to pick something which is largest resolution. */
      unsigned area = visible_width * visible_height;
      if (area > (*width) * (*height))
      {
         *width     = visible_width;
         *height    = visible_height;
         return true;
      }
   }
   else
   {
      unsigned visible_rate = mode->parameters.refreshRate;
      /* For particular resolutions, find the closest. */
      int delta_x           = (int)info->width  - (int)visible_width;
      int delta_y           = (int)info->height - (int)visible_height;
      int old_delta_x       = (int)info->width  - (int)*width;
      int old_delta_y       = (int)info->height - (int)*height;
      int delta_rate        = abs((int)info->refresh_rate_x1000 - (int)visible_rate);

      int dist              = delta_x     * delta_x     + delta_y     * delta_y;
      int old_dist          = old_delta_x * old_delta_x + old_delta_y * old_delta_y;

      if (dist < old_dist && delta_rate < 1000)
      {
         *width       = visible_width;
         *height      = visible_height;
         return true;
      }
   }

   return false;
}

static bool vulkan_create_display_surface(gfx_ctx_vulkan_data_t *vk,
      unsigned *width, unsigned *height,
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
   unsigned saved_width                      = *width;
   unsigned saved_height                     = *height;

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
      RARCH_WARN("Monitor index is out of range, using automatic display.\n");
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
         if (vulkan_update_display_mode(width, height, mode, info))
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
      RARCH_WARN("Could not find suitable surface for monitor index: %u.\n",
            monitor_index);
      RARCH_WARN("Retrying first suitable monitor.\n");
      monitor_index = 0;
      best_mode = VK_NULL_HANDLE;
      *width = saved_width;
      *height = saved_height;
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
   create_info.imageExtent.width  = *width;
   create_info.imageExtent.height = *height;

   if (vkCreateDisplayPlaneSurfaceKHR(vk->context.instance,
            &create_info, NULL, &vk->vk_surface) != VK_SUCCESS)
      GOTO_FAIL();

end:
   free(displays);
   free(planes);
   free(modes);
   return ret;
}

static void vulkan_destroy_swapchain(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;

   vulkan_emulated_mailbox_deinit(&vk->mailbox);
   if (vk->swapchain != VK_NULL_HANDLE)
   {
      vkDeviceWaitIdle(vk->context.device);
      vkDestroySwapchainKHR(vk->context.device, vk->swapchain, NULL);
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
      if (vk->context.swapchain_wait_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_wait_semaphores[i], NULL);
   }

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

      if (vk->context.swapchain_wait_semaphores[i])
      {
	      struct vulkan_context *ctx = &vk->context;
         VkSemaphore sem            = vk->context.swapchain_wait_semaphores[i];
         assert(ctx->num_recycled_acquire_semaphores < VULKAN_MAX_SWAPCHAIN_IMAGES);
         ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++] = sem;
      }
      vk->context.swapchain_wait_semaphores[i] = VK_NULL_HANDLE;
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

   if (vk->context.swapchain_wait_semaphores[index] != VK_NULL_HANDLE)
   {
      struct vulkan_context *ctx = &vk->context;
      VkSemaphore sem            = vk->context.swapchain_wait_semaphores[index];
      assert(ctx->num_recycled_acquire_semaphores < VULKAN_MAX_SWAPCHAIN_IMAGES);
      ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++] = sem;
   }
   vk->context.swapchain_wait_semaphores[index] = VK_NULL_HANDLE;
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

bool vulkan_buffer_chain_alloc(const struct vulkan_context *context,
      struct vk_buffer_chain *chain,
      size_t size, struct vk_buffer_range *range)
{
   if (!chain->head)
   {
      if (!(chain->head = vulkan_buffer_chain_alloc_node(context,
            chain->block_size, chain->usage)))
         return false;

      chain->current = chain->head;
      chain->offset  = 0;
   }

   if (!vulkan_buffer_chain_suballoc(chain, size, range))
   {
      /* We've exhausted the current chain, traverse list until we
       * can find a block we can use. Usually, we just step once. */
      while (chain->current->next)
      {
         chain->current = chain->current->next;
         chain->offset  = 0;
         if (vulkan_buffer_chain_suballoc(chain, size, range))
            return true;
      }

      /* We have to allocate a new node, might allocate larger
       * buffer here than block_size in case we have
       * a very large allocation. */
      if (size < chain->block_size)
         size        = chain->block_size;

      if (!(chain->current->next = vulkan_buffer_chain_alloc_node(
                  context, size, chain->usage)))
         return false;

      chain->current = chain->current->next;
      chain->offset  = 0;
      /* This cannot possibly fail. */
      retro_assert(vulkan_buffer_chain_suballoc(chain, size, range));
   }
   return true;
}


void vulkan_debug_mark_buffer(VkDevice device, VkBuffer buffer)
{
   static unsigned object_count;
   vulkan_debug_mark_object(device, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, "RetroArch buffer", ++object_count);
}

void vulkan_debug_mark_image(VkDevice device, VkImage image)
{
   static unsigned object_count;
   vulkan_debug_mark_object(device, VK_OBJECT_TYPE_IMAGE, (uint64_t)image, "RetroArch image", ++object_count);
}

void vulkan_debug_mark_memory(VkDevice device, VkDeviceMemory memory)
{
   static unsigned object_count;
   vulkan_debug_mark_object(device, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)memory, "RetroArch memory", ++object_count);
}

struct vk_buffer vulkan_create_buffer(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer buffer;
   VkMemoryRequirements mem_reqs;
   VkBufferCreateInfo info;
   VkMemoryAllocateInfo alloc;

   info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   info.pNext                 = NULL;
   info.flags                 = 0;
   info.size                  = size;
   info.usage                 = usage;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices   = NULL;
   vkCreateBuffer(context->device, &info, NULL, &buffer.buffer);
   vulkan_debug_mark_buffer(context->device, buffer.buffer);

   vkGetBufferMemoryRequirements(context->device, buffer.buffer, &mem_reqs);

   alloc.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext                = NULL;
   alloc.allocationSize       = mem_reqs.size;
   alloc.memoryTypeIndex      = vulkan_find_memory_type(
         &context->memory_properties,
         mem_reqs.memoryTypeBits,
           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   vkAllocateMemory(context->device, &alloc, NULL, &buffer.memory);
   vulkan_debug_mark_memory(context->device, buffer.memory);
   vkBindBufferMemory(context->device, buffer.buffer, buffer.memory, 0);

   buffer.size                = size;

   vkMapMemory(context->device,
         buffer.memory, 0, buffer.size, 0, &buffer.mapped);
   return buffer;
}

void vulkan_destroy_buffer(VkDevice device, struct vk_buffer *buffer)
{
   vkUnmapMemory(device, buffer->memory);
   vkFreeMemory(device, buffer->memory, NULL);

   vkDestroyBuffer(device, buffer->buffer, NULL);

   memset(buffer, 0, sizeof(*buffer));
}

struct vk_descriptor_pool *vulkan_alloc_descriptor_pool(
      VkDevice device,
      const struct vk_descriptor_manager *manager)
{
   unsigned i;
   VkDescriptorPoolCreateInfo pool_info;
   VkDescriptorSetAllocateInfo alloc_info;
   struct vk_descriptor_pool *pool =
      (struct vk_descriptor_pool*)malloc(sizeof(*pool));
   if (!pool)
      return NULL;

   pool_info.sType                 = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool_info.pNext                 = NULL;
   pool_info.flags                 = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   pool_info.maxSets               = VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS;
   pool_info.poolSizeCount         = manager->num_sizes;
   pool_info.pPoolSizes            = manager->sizes;

   pool->pool                      = VK_NULL_HANDLE;
   for (i = 0; i < VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS; i++)
      pool->sets[i]                = VK_NULL_HANDLE;
   pool->next                      = NULL;

   vkCreateDescriptorPool(device, &pool_info, NULL, &pool->pool);

   /* Just allocate all descriptor sets up front. */
   alloc_info.sType                = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   alloc_info.pNext                = NULL;
   alloc_info.descriptorPool       = pool->pool;
   alloc_info.descriptorSetCount   = 1;
   alloc_info.pSetLayouts          = &manager->set_layout;

   for (i = 0; i < VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS; i++)
      vkAllocateDescriptorSets(device, &alloc_info, &pool->sets[i]);

   return pool;
}

VkDescriptorSet vulkan_descriptor_manager_alloc(
      VkDevice device, struct vk_descriptor_manager *manager)
{
   if (manager->count >= VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS)
   {
      while (manager->current->next)
      {
         manager->current = manager->current->next;
         manager->count   = 0;
         return manager->current->sets[manager->count++];
      }

      manager->current->next = vulkan_alloc_descriptor_pool(device, manager);
      retro_assert(manager->current->next);

      manager->current = manager->current->next;
      manager->count   = 0;
   }
   return manager->current->sets[manager->count++];
}


bool vulkan_surface_create(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type,
      void *display, void *surface,
      unsigned width, unsigned height,
      unsigned swap_interval)
{
   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
#ifdef HAVE_WAYLAND
         {
            VkWaylandSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateWaylandSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateWaylandSurfaceKHR", create))
               return false;

            surf_info.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext   = NULL;
            surf_info.flags   = 0;
            surf_info.display = (struct wl_display*)display;
            surf_info.surface = (struct wl_surface*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_ANDROID:
#ifdef ANDROID
         {
            VkAndroidSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateAndroidSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateAndroidSurfaceKHR", create))
               return false;

            surf_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext  = NULL;
            surf_info.flags  = 0;
            surf_info.window = (ANativeWindow*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
            {
               RARCH_ERR("[Vulkan]: Failed to create Android surface.\n");
               return false;
            }
            RARCH_LOG("[Vulkan]: Created Android surface: %llu\n",
                  (unsigned long long)vk->vk_surface);
         }
#endif
         break;
      case VULKAN_WSI_WIN32:
#ifdef _WIN32
         {
            VkWin32SurfaceCreateInfoKHR surf_info;
            PFN_vkCreateWin32SurfaceKHR create;

            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateWin32SurfaceKHR", create))
               return false;

            surf_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext     = NULL;
            surf_info.flags     = 0;
            surf_info.hinstance = *(const HINSTANCE*)display;
            surf_info.hwnd      = *(const HWND*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XLIB:
#ifdef HAVE_XLIB
         {
            VkXlibSurfaceCreateInfoKHR surf_info;
            PFN_vkCreateXlibSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateXlibSurfaceKHR", create))
               return false;

            surf_info.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext  = NULL;
            surf_info.flags  = 0;
            surf_info.dpy    = (Display*)display;
            surf_info.window = *(const Window*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface)
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
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateXcbSurfaceKHR", create))
               return false;

            surf_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext      = NULL;
            surf_info.flags      = 0;
            surf_info.connection = XGetXCBConnection((Display*)display);
            surf_info.window     = *(const xcb_window_t*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface)
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
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateMirSurfaceKHR", create))
               return false;

            surf_info.sType      = VK_STRUCTURE_TYPE_MIR_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext      = NULL;
            surf_info.connection = display;
            surf_info.mirSurface = surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface)
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_DISPLAY:
         /* We need to decide on GPU here to be able to query support. */
         if (!vulkan_context_init_gpu(vk))
            return false;
         if (!vulkan_create_display_surface(vk,
                  &width, &height,
                  (const struct vulkan_display_surface_info*)display))
            return false;
         break;
      case VULKAN_WSI_MVK_MACOS:
#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
         {
            VkMacOSSurfaceCreateInfoMVK surf_info;
            PFN_vkCreateMacOSSurfaceMVK create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateMacOSSurfaceMVK", create))
               return false;

            surf_info.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
            surf_info.pNext = NULL;
            surf_info.flags = 0;
            surf_info.pView = surface;

            if (create(vk->context.instance, &surf_info, NULL, &vk->vk_surface)
                != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_MVK_IOS:
#ifdef HAVE_COCOATOUCH
         {
            VkIOSSurfaceCreateInfoMVK surf_info;
            PFN_vkCreateIOSSurfaceMVK create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateIOSSurfaceMVK", create))
               return false;

            surf_info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
            surf_info.pNext = NULL;
            surf_info.flags = 0;
            surf_info.pView = surface;

            if (create(vk->context.instance, &surf_info, NULL, &vk->vk_surface)
                != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_NONE:
      default:
         return false;
   }

   /* Must create device after surface since we need to be able to query queues to use for presentation. */
   if (!vulkan_context_init_device(vk))
      return false;

   if (!vulkan_create_swapchain(
            vk, width, height, swap_interval))
      return false;

   vulkan_acquire_next_image(vk);
   return true;
}

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

   RARCH_ERR("[Vulkan]: Failed to find valid memory type. This should never happen.");
   abort();
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
      RARCH_ERR("[Vulkan]: Failed to find valid memory type. This should never happen.");
      abort();
   }

   return vulkan_find_memory_type_fallback(mem_props,
         device_reqs, host_reqs_second, 0);
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
   if (vk->swapchain == VK_NULL_HANDLE)
   {
      /* We don't have a swapchain, try to create one now. */
      if (!vulkan_create_swapchain(vk, vk->context.swapchain_width,
               vk->context.swapchain_height, vk->context.swap_interval))
      {
#ifdef VULKAN_DEBUG
         RARCH_ERR("[Vulkan]: Failed to create new swapchain.\n");
#endif
         retro_sleep(20);
         return;
      }

      if (vk->swapchain == VK_NULL_HANDLE)
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
      if (vk->flags & VK_DATA_FLAG_USE_WSI_SEMAPHORE)
          semaphore = vulkan_get_wsi_acquire_semaphore(&vk->context);
      else
          vkCreateFence(vk->context.device, &fence_info, NULL, &fence);

      err = vkAcquireNextImageKHR(vk->context.device,
            vk->swapchain, UINT64_MAX,
            semaphore, fence, &vk->context.current_swapchain_index);
   }

   if (err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR)
   {
      if (fence != VK_NULL_HANDLE)
         vkWaitForFences(vk->context.device, 1, &fence, true, UINT64_MAX);
      vk->context.flags |= VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;

      if (vk->context.swapchain_acquire_semaphore)
      {
#ifdef HAVE_THREADS
         slock_lock(vk->context.queue_lock);
#endif
         vkDeviceWaitIdle(vk->context.device);
         vkDestroySemaphore(vk->context.device, vk->context.swapchain_acquire_semaphore, NULL);
#ifdef HAVE_THREADS
         slock_unlock(vk->context.queue_lock);
#endif
      }
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
         /* Throw away the old swapchain and try again. */
         vulkan_destroy_swapchain(vk);
         /* Swapchain out of date, trying to create new one ... */
         if (is_retrying)
         {
            retro_sleep(10);
         }
         else
            is_retrying = true;
         vulkan_acquire_clear_fences(vk);
         goto retry;
      default:
         if (err != VK_SUCCESS)
         {
            /* We are screwed, don't try anymore. Maybe it will work later. */
            vulkan_destroy_swapchain(vk);
            RARCH_ERR("[Vulkan]: Failed to acquire from swapchain (err = %d).\n",
                  (int)err);
            if (err == VK_ERROR_SURFACE_LOST_KHR)
               RARCH_ERR("[Vulkan]: Got VK_ERROR_SURFACE_LOST_KHR.\n");
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
      unsigned width, unsigned height,
      unsigned swap_interval)
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
   VkSurfaceTransformFlagBitsKHR pre_transform;
   uint32_t present_mode_count             = 0;
   VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
   VkCompositeAlphaFlagBitsKHR composite   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   settings_t                    *settings = config_get_ptr();
   bool vsync                              = settings->bools.video_vsync;

   format.format                           = VK_FORMAT_UNDEFINED;
   format.colorSpace                       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

   vkDeviceWaitIdle(vk->context.device);
   vulkan_acquire_clear_fences(vk);

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
      swap_interval  =  1;
      vk->flags     |=  VK_DATA_FLAG_EMULATING_MAILBOX;
   }
   else
      vk->flags     &= ~VK_DATA_FLAG_EMULATING_MAILBOX;

   vk->flags        |= VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN;

   if (       (vk->swapchain != VK_NULL_HANDLE)
         && (!(vk->context.flags & VK_CTX_FLAG_INVALID_SWAPCHAIN))
         &&   (vk->context.swapchain_width  == width)
         &&   (vk->context.swapchain_height == height)
         &&   (vk->context.swap_interval    == swap_interval))
   {
      /* Do not bother creating a swapchain redundantly. */
#ifdef VULKAN_DEBUG
      RARCH_DBG("[Vulkan]: Do not need to re-create swapchain.\n");
#endif
      vulkan_create_wait_fences(vk);

      if (     (vk->flags & VK_DATA_FLAG_EMULATING_MAILBOX)
            && (vk->mailbox.swapchain == VK_NULL_HANDLE))
      {
         vulkan_emulated_mailbox_init(
               &vk->mailbox, vk->context.device, vk->swapchain);
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
         return true;
      }
   }

   vulkan_emulated_mailbox_deinit(&vk->mailbox);

   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, NULL);
   if (present_mode_count < 1 || present_mode_count > 16)
   {
      RARCH_ERR("[Vulkan]: Bogus present modes found.\n");
      return false;
   }
   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, present_modes);

   vk->context.swap_interval = swap_interval;

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

   /* Present mode logging */
   if (vk->swapchain == VK_NULL_HANDLE)
   {
      for (i = 0; i < present_mode_count; i++)
      {
         switch (present_modes[i])
         {
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
               RARCH_DBG("[Vulkan]: Swapchain supports present mode: IMMEDIATE.\n");
               break;
            case VK_PRESENT_MODE_MAILBOX_KHR:
               RARCH_DBG("[Vulkan]: Swapchain supports present mode: MAILBOX.\n");
               break;
            case VK_PRESENT_MODE_FIFO_KHR:
               RARCH_DBG("[Vulkan]: Swapchain supports present mode: FIFO.\n");
               break;
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
               RARCH_DBG("[Vulkan]: Swapchain supports present mode: FIFO_RELAXED.\n");
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
            RARCH_DBG("[Vulkan]: Creating swapchain with present mode: IMMEDIATE.\n");
            break;
         case VK_PRESENT_MODE_MAILBOX_KHR:
            RARCH_DBG("[Vulkan]: Creating swapchain with present mode: MAILBOX.\n");
            break;
         case VK_PRESENT_MODE_FIFO_KHR:
            RARCH_DBG("[Vulkan]: Creating swapchain with present mode: FIFO.\n");
            break;
         case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            RARCH_DBG("[Vulkan]: Creating swapchain with present mode: FIFO_RELAXED.\n");
            break;
         default:
            break;
      }
   }

   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, NULL);
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, formats);

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
         RARCH_ERR("[Vulkan]: Surface has no formats.\n");
         return false;
      }

#ifdef VULKAN_HDR_SWAPCHAIN
      if (vk->context.flags & VK_CTX_FLAG_HDR_SUPPORT)
      {
         if (settings->bools.video_hdr_enable)
            vk->context.flags |=  VK_CTX_FLAG_HDR_ENABLE;
         else
            vk->context.flags &= ~VK_CTX_FLAG_HDR_ENABLE;

         video_driver_unset_hdr_support();

         for (i = 0; i < format_count; i++)
         {
            if (     (vulkan_is_hdr10_format(formats[i].format))
                  && (formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT))
            {
               format = formats[i];
               video_driver_set_hdr_support();
               break;
            }
         }

         if (!vulkan_is_hdr10_format(format.format))
            vk->context.flags &= ~VK_CTX_FLAG_HDR_ENABLE;
      }
      else
      {
         vk->context.flags &= ~VK_CTX_FLAG_HDR_ENABLE;
      }

      if (!(vk->context.flags & VK_CTX_FLAG_HDR_ENABLE))
#endif /* VULKAN_HDR_SWAPCHAIN */
      {
         for (i = 0; i < format_count; i++)
         {
            if (
                     formats[i].format == VK_FORMAT_R8G8B8A8_UNORM
                  || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM
                  || formats[i].format == VK_FORMAT_A8B8G8R8_UNORM_PACK32)
            {
               format = formats[i];
               break;
            }
         }
      }

      if (format.format == VK_FORMAT_UNDEFINED)
         format = formats[0];
   }

   if (surface_properties.currentExtent.width == UINT32_MAX)
   {
      swapchain_size.width     = width;
      swapchain_size.height    = height;
   }
   else
      swapchain_size           = surface_properties.currentExtent;

#ifdef WSI_HARDENING_TEST
   if (trigger_spurious_error())
   {
      surface_properties.maxImageExtent.width = 0;
      surface_properties.maxImageExtent.height = 0;
      surface_properties.minImageExtent.width = 0;
      surface_properties.minImageExtent.height = 0;
   }
#endif

   /* Clamp swapchain size to boundaries. */
   if (swapchain_size.width > surface_properties.maxImageExtent.width)
      swapchain_size.width = surface_properties.maxImageExtent.width;
   if (swapchain_size.width < surface_properties.minImageExtent.width)
      swapchain_size.width = surface_properties.minImageExtent.width;
   if (swapchain_size.height > surface_properties.maxImageExtent.height)
      swapchain_size.height = surface_properties.maxImageExtent.height;
   if (swapchain_size.height < surface_properties.minImageExtent.height)
      swapchain_size.height = surface_properties.minImageExtent.height;

   if (     (swapchain_size.width  == 0)
         && (swapchain_size.height == 0))
   {
      /* Cannot create swapchain yet, try again later. */
      if (vk->swapchain != VK_NULL_HANDLE)
         vkDestroySwapchainKHR(vk->context.device, vk->swapchain, NULL);
      vk->swapchain                    = VK_NULL_HANDLE;
      vk->context.swapchain_width      = width;
      vk->context.swapchain_height     = height;
      vk->context.num_swapchain_images = 1;

      memset(vk->context.swapchain_images, 0, sizeof(vk->context.swapchain_images));
      RARCH_DBG("[Vulkan]: Cannot create a swapchain yet. Will try again later ...\n");
      return true;
   }

   /* Unless we have other reasons to clamp, we should prefer 3 images.
    * We hard sync against the swapchain, so if we have 2 images,
    * we would be unable to overlap CPU and GPU, which can get very slow
    * for GPU-rendered cores. */
   desired_swapchain_images    = settings->uints.video_max_swapchain_images;

   /* We don't clamp the number of images requested to what is reported
    * as supported by the implementation in surface_properties.minImageCount,
    * because MESA always reports a minImageCount of 4, but 3 and 2 work
    * pefectly well, even if it's out of spec. */

   if (     (surface_properties.maxImageCount > 0)
         && (desired_swapchain_images > surface_properties.maxImageCount))
      desired_swapchain_images = surface_properties.maxImageCount;

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
   info.oldSwapchain           = old_swapchain;

   info.oldSwapchain = VK_NULL_HANDLE;
   if (old_swapchain != VK_NULL_HANDLE)
      vkDestroySwapchainKHR(vk->context.device, old_swapchain, NULL);

   if (vkCreateSwapchainKHR(vk->context.device,
            &info, NULL, &vk->swapchain) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to create swapchain.\n");
      return false;
   }

   vk->context.swapchain_width        = swapchain_size.width;
   vk->context.swapchain_height       = swapchain_size.height;
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
   vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, vk->context.swapchain_images);

   if (old_swapchain == VK_NULL_HANDLE)
      RARCH_LOG("[Vulkan]: Got %u swapchain images.\n",
            vk->context.num_swapchain_images);

   /* Force driver to reset swapchain image handles. */
   vk->context.flags                 |=  VK_CTX_FLAG_INVALID_SWAPCHAIN;
   vk->context.flags                 &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
   vulkan_create_wait_fences(vk);

   if (vk->flags & VK_DATA_FLAG_EMULATING_MAILBOX)
      vulkan_emulated_mailbox_init(&vk->mailbox, vk->context.device, vk->swapchain);

   return true;
}

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
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong API.\n");
      iface = NULL;
   }

   if (iface && iface->interface_version == 0)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong interface version.\n");
      iface = NULL;
   }

   vk->wsi_type = type;

   if (!vulkan_library)
   {
#ifdef _WIN32
      vulkan_library = dylib_load("vulkan-1.dll");
#elif __APPLE__
      if (__builtin_available(macOS 10.15, iOS 13, tvOS 12, *))
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
      RARCH_ERR("[Vulkan]: Failed to open Vulkan loader.\n");
      return false;
   }

   RARCH_LOG("[Vulkan]: Vulkan dynamic library loaded.\n");

   GetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dylib_proc(vulkan_library, "vkGetInstanceProcAddr");

   if (!GetInstanceProcAddr)
   {
      RARCH_ERR("[Vulkan]: Failed to load vkGetInstanceProcAddr symbol, broken loader?\n");
      return false;
   }

   vulkan_symbol_wrapper_init(GetInstanceProcAddr);

   if (!vulkan_symbol_wrapper_load_global_symbols())
   {
      RARCH_ERR("[Vulkan]: Failed to load global Vulkan symbols, broken loader?\n");
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
         RARCH_ERR("[Vulkan]: Core did not provide application info as required by v2.\n");
         return false;
      }

      if (iface->get_application_info)
      {
         const VkApplicationInfo *app_info = iface->get_application_info();

         if (!app_info && iface->interface_version >= 2)
         {
            RARCH_ERR("[Vulkan]: Core did not provide application info as required by v2.\n");
            return false;
         }

         if (app_info)
         {
            app = *app_info;
#ifdef VULKAN_DEBUG
            if (app.pApplicationName)
            {
               RARCH_LOG("[Vulkan]: App: %s (version %u)\n",
                     app.pApplicationName, app.applicationVersion);
            }

            if (app.pEngineName)
            {
               RARCH_LOG("[Vulkan]: Engine: %s (version %u)\n",
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

   if (cached_instance_vk)
   {
      vk->context.instance = cached_instance_vk;
      cached_instance_vk   = NULL;
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
         RARCH_ERR("Failed to create Vulkan instance.\n");
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
   RARCH_LOG("[Vulkan]: Enabling Vulkan debug layers.\n");
#endif

   if (!vulkan_load_instance_symbols(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to load instance symbols.\n");
      return false;
   }

   return true;
}


void vulkan_context_destroy(gfx_ctx_vulkan_data_t *vk,
      bool destroy_surface)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   uint32_t video_st_flags        = 0;
   if (!vk->context.instance)
      return;

   if (vk->context.device)
      vkDeviceWaitIdle(vk->context.device);

   vulkan_destroy_swapchain(vk);

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

   video_st_flags              = video_st->flags;

   if (video_st_flags & VIDEO_FLAG_CACHE_CONTEXT)
   {
      cached_device_vk         = vk->context.device;
      cached_instance_vk       = vk->context.instance;
      cached_destroy_device_vk = vk->context.destroy_device;
   }
   else
   {
      if (vk->context.device)
      {
         vkDestroyDevice(vk->context.device, NULL);
         vk->context.device = NULL;
      }

      if (vk->context.instance)
      {
         if (vk->context.destroy_device)
            vk->context.destroy_device();

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
}

void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index)
{
   VkPresentInfoKHR present;
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

   /* Better hope QueuePresent doesn't block D: */
#ifdef HAVE_THREADS
   slock_lock(vk->context.queue_lock);
#endif
   err = vkQueuePresentKHR(vk->context.queue, &present);

   /* VK_SUBOPTIMAL_KHR can be returned on
    * Android 10 when prerotate is not dealt with.
    * It can also be returned by WSI when the surface
    * is _temorarily_ suboptimal.
    * This is not an error we need to care about,
    * and we'll treat it as SUCCESS. */
   if (result == VK_SUBOPTIMAL_KHR)
      result = VK_SUCCESS;
   if (err == VK_SUBOPTIMAL_KHR)
      err = VK_SUCCESS;

#ifdef WSI_HARDENING_TEST
   trigger_spurious_error_vkresult(&err);
#endif

   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      RARCH_LOG("[Vulkan]: QueuePresent failed, destroying swapchain.\n");
      vulkan_destroy_swapchain(vk);
   }

#ifdef HAVE_THREADS
   slock_unlock(vk->context.queue_lock);
#endif
}

void vulkan_initialize_render_pass(VkDevice device, VkFormat format,
      VkRenderPass *render_pass)
{
   VkAttachmentReference color_ref;
   VkRenderPassCreateInfo rp_info;
   VkAttachmentDescription attachment;
   VkSubpassDescription subpass;

   rp_info.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   rp_info.pNext                = NULL;
   rp_info.flags                = 0;
   rp_info.attachmentCount      = 1;
   rp_info.pAttachments         = &attachment;
   rp_info.subpassCount         = 1;
   rp_info.pSubpasses           = &subpass;
   rp_info.dependencyCount      = 0;
   rp_info.pDependencies        = NULL;

   color_ref.attachment         = 0;
   color_ref.layout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   /* We will always write to the entire framebuffer,
    * so we don't really need to clear. */
   attachment.flags             = 0;
   attachment.format            = format;
   attachment.samples           = VK_SAMPLE_COUNT_1_BIT;
   attachment.loadOp            = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.storeOp           = VK_ATTACHMENT_STORE_OP_STORE;
   attachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   attachment.initialLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   attachment.finalLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   subpass.flags                     = 0;
   subpass.pipelineBindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.inputAttachmentCount      = 0;
   subpass.pInputAttachments         = NULL;
   subpass.colorAttachmentCount      = 1;
   subpass.pColorAttachments         = &color_ref;
   subpass.pResolveAttachments       = NULL;
   subpass.pDepthStencilAttachment   = NULL;
   subpass.preserveAttachmentCount   = 0;
   subpass.pPreserveAttachments      = NULL;

   vkCreateRenderPass(device, &rp_info, NULL, render_pass);
}

void vulkan_framebuffer_clear(VkImage image, VkCommandBuffer cmd)
{
   VkClearColorValue color;
   VkImageSubresourceRange range;

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
         image,
         VK_REMAINING_MIP_LEVELS,
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED);

   color.float32[0]     = 0.0f;
   color.float32[1]     = 0.0f;
   color.float32[2]     = 0.0f;
   color.float32[3]     = 0.0f;
   range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   range.baseMipLevel   = 0;
   range.levelCount     = 1;
   range.baseArrayLayer = 0;
   range.layerCount     = 1;

   vkCmdClearColorImage(cmd,
         image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         &color,
         1,
         &range);

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
         image,
         VK_REMAINING_MIP_LEVELS,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED);
}

void vulkan_framebuffer_generate_mips(
      VkFramebuffer framebuffer,
      VkImage image,
      struct Size2D size,
      VkCommandBuffer cmd,
      unsigned levels
      )
{
   unsigned i;
   /* This is run every frame, so make sure
    * we aren't opting into the "lazy" way of doing this. :) */
   VkImageMemoryBarrier barriers[2];

   /* First, transfer the input mip level to TRANSFER_SRC_OPTIMAL.
    * This should allow the surface to stay compressed.
    * All subsequent mip-layers are now transferred into DST_OPTIMAL from
    * UNDEFINED at this point.
    */

   /* Input */
   barriers[0].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barriers[0].pNext                           = NULL;
   barriers[0].srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   barriers[0].dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
   barriers[0].oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   barriers[0].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   barriers[0].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barriers[0].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barriers[0].image                           = image;
   barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   barriers[0].subresourceRange.baseMipLevel   = 0;
   barriers[0].subresourceRange.levelCount     = 1;
   barriers[0].subresourceRange.baseArrayLayer = 0;
   barriers[0].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

   /* The rest of the mip chain */
   barriers[1].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barriers[1].pNext                           = NULL;
   barriers[1].srcAccessMask                   = 0;
   barriers[1].dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
   barriers[1].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
   barriers[1].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barriers[1].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barriers[1].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barriers[1].image                           = image;
   barriers[1].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   barriers[1].subresourceRange.baseMipLevel   = 1;
   barriers[1].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
   barriers[1].subresourceRange.baseArrayLayer = 0;
   barriers[1].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

   vkCmdPipelineBarrier(cmd,
         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         0,
         0,
         NULL,
         0,
         NULL,
         2,
         barriers);

   for (i = 1; i < levels; i++)
   {
      unsigned src_width, src_height, target_width, target_height;
      VkImageBlit blit_region = {{0}};

      /* For subsequent passes, we have to transition
       * from DST_OPTIMAL to SRC_OPTIMAL,
       * but only do so one mip-level at a time. */
      if (i > 1)
      {
         barriers[0].srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
         barriers[0].dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
         barriers[0].subresourceRange.baseMipLevel = i - 1;
         barriers[0].subresourceRange.levelCount   = 1;
         barriers[0].oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
         barriers[0].newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

         vkCmdPipelineBarrier(cmd,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               0,
               0,
               NULL,
               0,
               NULL,
               1,
               barriers);
      }

      src_width                                 = MAX(size.width >> (i - 1), 1u);
      src_height                                = MAX(size.height >> (i - 1), 1u);
      target_width                              = MAX(size.width >> i, 1u);
      target_height                             = MAX(size.height >> i, 1u);

      blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      blit_region.srcSubresource.mipLevel       = i - 1;
      blit_region.srcSubresource.baseArrayLayer = 0;
      blit_region.srcSubresource.layerCount     = 1;
      blit_region.dstSubresource                = blit_region.srcSubresource;
      blit_region.dstSubresource.mipLevel       = i;
      blit_region.srcOffsets[1].x               = src_width;
      blit_region.srcOffsets[1].y               = src_height;
      blit_region.srcOffsets[1].z               = 1;
      blit_region.dstOffsets[1].x               = target_width;
      blit_region.dstOffsets[1].y               = target_height;
      blit_region.dstOffsets[1].z               = 1;

      vkCmdBlitImage(cmd,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit_region, VK_FILTER_LINEAR);
   }

   /* We are now done, and we have all mip-levels except
    * the last in TRANSFER_SRC_OPTIMAL,
    * and the last one still on TRANSFER_DST_OPTIMAL,
    * so do a final barrier which
    * moves everything to SHADER_READ_ONLY_OPTIMAL in
    * one go along with the execution barrier to next pass.
    * Read-to-read memory barrier, so only need execution
    * barrier for first transition.
    */
   barriers[0].srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
   barriers[0].dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
   barriers[0].subresourceRange.baseMipLevel = 0;
   barriers[0].subresourceRange.levelCount   = levels - 1;
   barriers[0].oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   barriers[0].newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

   /* This is read-after-write barrier. */
   barriers[1].srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
   barriers[1].dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
   barriers[1].subresourceRange.baseMipLevel = levels - 1;
   barriers[1].subresourceRange.levelCount   = 1;
   barriers[1].oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barriers[1].newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

   vkCmdPipelineBarrier(cmd,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         0,
         0,
         NULL,
         0,
         NULL,
         2, barriers);

   /* Next pass will wait for ALL_GRAPHICS_BIT, and since
    * we have dstStage as FRAGMENT_SHADER,
    * the dependency chain will ensure we don't start
    * next pass until the mipchain is complete. */
}

void vulkan_framebuffer_copy(VkImage image,
      struct Size2D size,
      VkCommandBuffer cmd,
      VkImage src_image, VkImageLayout src_layout)
{
   VkImageCopy region;

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(
         cmd,
         image,
         VK_REMAINING_MIP_LEVELS,
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED);

   region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.srcSubresource.mipLevel       = 0;
   region.srcSubresource.baseArrayLayer = 0;
   region.srcSubresource.layerCount     = 1;
   region.srcOffset.x                   = 0;
   region.srcOffset.y                   = 0;
   region.srcOffset.z                   = 0;
   region.dstSubresource                = region.srcSubresource;
   region.dstOffset.x                   = 0;
   region.dstOffset.y                   = 0;
   region.dstOffset.z                   = 0;
   region.extent.width                  = size.width;
   region.extent.height                 = size.height;
   region.extent.depth                  = 1;

   vkCmdCopyImage(cmd,
         src_image, src_layout,
         image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         1, &region);

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
         image,
         VK_REMAINING_MIP_LEVELS,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED);
}
