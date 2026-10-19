#if defined(_WIN32) && !defined(__WINRT__)
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#include <dxgiformat.h>
#include "win_dxgi_present.h"
#include "win_vulkan_dxgi.h"
#define vkGetInstanceProcAddr vulkan_symbol_wrapper_instance_proc_addr()

struct ra_vulkan_dxgi
{
   ra_dxgi_bridge_t *bridge;
   VkDevice device;
   VkQueue queue;
   void *queue_lock;
   VkCommandPool pool;
   VkFence resize_fence;
   VkImage images[2];
   VkDeviceMemory memory[2];
   VkSemaphore ready, done;
   VkCommandBuffer acquire[2], release[2];
   uint64_t ready_value;
   int submitted;
   unsigned width, height, family;
   VkFormat format;
   VkPhysicalDevice gpu;
};

static VkResult ra_vulkan_dxgi_submit(ra_vulkan_dxgi_t *p,
      const VkSubmitInfo *submit, VkFence fence)
{
   VkResult result;
#ifdef HAVE_THREADS
   if (p->queue_lock) slock_lock((slock_t *)p->queue_lock);
#endif
   result = vkQueueSubmit(p->queue, 1, submit, fence);
   if (result == VK_SUCCESS) p->submitted = 1;
#ifdef HAVE_THREADS
   if (p->queue_lock) slock_unlock((slock_t *)p->queue_lock);
#endif
   return result;
}

int ra_vulkan_dxgi_presentable(ra_vulkan_dxgi_t *p)
{
   return ra_dxgi_bridge_presentable(p->bridge);
}

static void ra_vulkan_dxgi_destroy_images(ra_vulkan_dxgi_t *p)
{
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      if (p->images[i]) vkDestroyImage(p->device, p->images[i], NULL);
      if (p->memory[i]) vkFreeMemory(p->device, p->memory[i], NULL);
      p->images[i] = VK_NULL_HANDLE; p->memory[i] = VK_NULL_HANDLE;
   }
}

static VkResult ra_vulkan_dxgi_drain(ra_vulkan_dxgi_t *p);

void ra_vulkan_dxgi_destroy(ra_vulkan_dxgi_t *p)
{
   if (!p) return;
   if (p->submitted)
      ra_vulkan_dxgi_drain(p);
   ra_dxgi_bridge_destroy(p->bridge);
   ra_vulkan_dxgi_destroy_images(p);
   if (p->resize_fence) vkDestroyFence(p->device, p->resize_fence, NULL);
   if (p->pool) vkDestroyCommandPool(p->device, p->pool, NULL);
   if (p->ready) vkDestroySemaphore(p->device, p->ready, NULL);
   if (p->done) vkDestroySemaphore(p->device, p->done, NULL);
   free(p);
}

#define CHECK_VK(expr) do { if ((expr) != VK_SUCCESS) goto error; } while (0)

static int ra_vulkan_dxgi_create_images(ra_vulkan_dxgi_t *p,
      unsigned width, unsigned height, VkFormat format)
{
   VkPhysicalDevice gpu = p->gpu;
   VkDevice device = p->device;
   unsigned family = p->family, i;
   VkPhysicalDeviceMemoryProperties memory_properties;
   PFN_vkGetMemoryWin32HandlePropertiesKHR get_memory_properties =
      (PFN_vkGetMemoryWin32HandlePropertiesKHR)vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandlePropertiesKHR");
   PFN_vkImportSemaphoreWin32HandleKHR import_semaphore =
      (PFN_vkImportSemaphoreWin32HandleKHR)vkGetDeviceProcAddr(device, "vkImportSemaphoreWin32HandleKHR");
   vkGetPhysicalDeviceMemoryProperties(gpu, &memory_properties);
   for (i = 0; i < 2; i++)
   {
      VkExternalMemoryImageCreateInfo external_image;
      VkImageCreateInfo image;
      VkMemoryRequirements requirements;
      VkMemoryWin32HandlePropertiesKHR handle_properties;
      VkImportMemoryWin32HandleInfoKHR import_memory;
      VkMemoryDedicatedAllocateInfo dedicated;
      VkMemoryAllocateInfo allocation;
      VkSemaphoreCreateInfo semaphore;
      VkImportSemaphoreWin32HandleInfoKHR import;
      VkCommandBufferAllocateInfo command;
      unsigned type, j;
      external_image.sType               = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
      external_image.pNext               = NULL;
      external_image.handleTypes         = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;

      image.sType                        = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      image.pNext                        = &external_image;
      image.flags                        = 0;
      image.imageType                    = VK_IMAGE_TYPE_2D;
      image.format                       = format;
      image.extent.width                 = width;
      image.extent.height                = height;
      image.extent.depth                 = 1;
      image.mipLevels                    = 1;
      image.arrayLayers                  = 1;
      image.samples                      = VK_SAMPLE_COUNT_1_BIT;
      image.tiling                       = VK_IMAGE_TILING_OPTIMAL;
      image.usage                        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      image.sharingMode                  = VK_SHARING_MODE_EXCLUSIVE;
      image.queueFamilyIndexCount        = 0;
      image.pQueueFamilyIndices          = NULL;
      image.initialLayout                = VK_IMAGE_LAYOUT_UNDEFINED;
      CHECK_VK(vkCreateImage(device, &image, NULL, &p->images[i]));
      vkGetImageMemoryRequirements(device, p->images[i], &requirements);

      handle_properties.sType            = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
      handle_properties.pNext            = NULL;
      handle_properties.memoryTypeBits   = 0;
      CHECK_VK(get_memory_properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
               ra_dxgi_bridge_resource(p->bridge, i), &handle_properties));
      for (type = 0; type < memory_properties.memoryTypeCount; type++)
         if ((requirements.memoryTypeBits & handle_properties.memoryTypeBits) & (1u << type))
            break;
      if (type == memory_properties.memoryTypeCount)
         goto error;

      import_memory.sType                = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
      import_memory.pNext                = NULL;
      import_memory.handleType           = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
      import_memory.handle               = ra_dxgi_bridge_resource(p->bridge, i);
      import_memory.name                 = NULL;

      dedicated.sType                    = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
      dedicated.pNext                    = &import_memory;
      dedicated.image                    = p->images[i];
      dedicated.buffer                   = VK_NULL_HANDLE;

      allocation.sType                   = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocation.pNext                   = &dedicated;
      allocation.allocationSize          = requirements.size;
      allocation.memoryTypeIndex         = type;
      CHECK_VK(vkAllocateMemory(device, &allocation, NULL, &p->memory[i]));
      CHECK_VK(vkBindImageMemory(device, p->images[i], p->memory[i], 0));
      if (!(i ? p->done : p->ready))
      {
         semaphore.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
         semaphore.pNext                 = NULL;
         semaphore.flags                 = 0;
         CHECK_VK(vkCreateSemaphore(device, &semaphore, NULL, i ? &p->done : &p->ready));

         import.sType                    = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
         import.pNext                    = NULL;
         import.semaphore                = i ? p->done : p->ready;
         import.flags                    = 0;
         import.handleType               = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
         import.handle                   = i ? ra_dxgi_bridge_done_fence(p->bridge) : ra_dxgi_bridge_ready_fence(p->bridge);
         import.name                     = NULL;
         CHECK_VK(import_semaphore(device, &import));
      }

      command.sType                      = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      command.pNext                      = NULL;
      command.commandPool                = p->pool;
      command.level                      = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      command.commandBufferCount         = 1;
      for (j = 0; j < 2; j++)
      {
         VkCommandBufferBeginInfo begin;
         VkImageMemoryBarrier barrier;
         VkCommandBuffer *buffer = j ? &p->release[i] : &p->acquire[i];
         if (*buffer == VK_NULL_HANDLE)
            CHECK_VK(vkAllocateCommandBuffers(device, &command, buffer));

         begin.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
         begin.pNext                     = NULL;
         begin.flags                     = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
         begin.pInheritanceInfo          = NULL;
         CHECK_VK(vkBeginCommandBuffer(*buffer, &begin));

         barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
         barrier.pNext                           = NULL;
         barrier.srcAccessMask                   = j ? VK_ACCESS_MEMORY_WRITE_BIT : 0;
         barrier.dstAccessMask                   = j ? 0 : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
         barrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
         barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
         barrier.srcQueueFamilyIndex             = j ? family : VK_QUEUE_FAMILY_EXTERNAL;
         barrier.dstQueueFamilyIndex             = j ? VK_QUEUE_FAMILY_EXTERNAL : family;
         barrier.image                           = p->images[i];
         barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
         barrier.subresourceRange.baseMipLevel   = 0;
         barrier.subresourceRange.levelCount     = 1;
         barrier.subresourceRange.baseArrayLayer = 0;
         barrier.subresourceRange.layerCount     = 1;
         vkCmdPipelineBarrier(*buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
         CHECK_VK(vkEndCommandBuffer(*buffer));
      }
   }
   p->width  = width;
   p->height = height;
   p->format = format;
   return 1;
error:
   return 0;
}

/* A fence, not vkQueueWaitIdle: queue_lock is held for the submit
 * only, so a core in lock_queue is never parked behind the drain. */
static VkResult ra_vulkan_dxgi_drain(ra_vulkan_dxgi_t *p)
{
   VkResult result;
   if (!p->resize_fence)
   {
      VkFenceCreateInfo info;
      info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      result     = vkCreateFence(p->device, &info, NULL, &p->resize_fence);
      if (result != VK_SUCCESS)
         return result;
   }
#ifdef HAVE_THREADS
   if (p->queue_lock) slock_lock((slock_t *)p->queue_lock);
#endif
   result = vkQueueSubmit(p->queue, 0, NULL, p->resize_fence);
   if (result == VK_SUCCESS)
      p->submitted = 1;
#ifdef HAVE_THREADS
   if (p->queue_lock)
      slock_unlock((slock_t *)p->queue_lock);
#endif
   if (result != VK_SUCCESS)
      return result;
   result = vkWaitForFences(p->device, 1, &p->resize_fence, VK_TRUE, UINT64_MAX);
   if (result != VK_SUCCESS)
      return result;
   return vkResetFences(p->device, 1, &p->resize_fence);
}

int ra_vulkan_dxgi_resize(ra_vulkan_dxgi_t *p,
      unsigned width, unsigned height, VkFormat format)
{
   unsigned dxgi_format;
   VkResult result;
   if (!p || !width || !height || format != p->format)
      return 0;
   if (p->width == width && p->height == height)
      return !ra_dxgi_bridge_failed(p->bridge);
   result = ra_vulkan_dxgi_drain(p);
   if (result != VK_SUCCESS)
      return 0;
   if (vkResetCommandPool(p->device, p->pool, 0) != VK_SUCCESS)
      return 0;
   ra_vulkan_dxgi_destroy_images(p);
   if (format == VK_FORMAT_R8G8B8A8_UNORM)
      dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
   else if (format == VK_FORMAT_B8G8R8A8_UNORM)
      dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
   else if (format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      dxgi_format = DXGI_FORMAT_R10G10B10A2_UNORM;
   else
      dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
   if (!ra_dxgi_bridge_resize(p->bridge, width, height, dxgi_format))
      return 0;
   return ra_vulkan_dxgi_create_images(p, width, height, format);
}

ra_vulkan_dxgi_t *ra_vulkan_dxgi_create(VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device, VkQueue queue, unsigned family,
      HWND window, unsigned width, unsigned height, VkFormat format, void *queue_lock)
{
   ra_vulkan_dxgi_t *p;
   VkPhysicalDeviceIDProperties id;
   VkPhysicalDeviceProperties2 properties;
   VkPhysicalDeviceExternalSemaphoreInfo semaphore_info;
   VkExternalSemaphoreProperties semaphore_properties;
   VkPhysicalDeviceExternalImageFormatInfo external_format_info;
   VkPhysicalDeviceImageFormatInfo2 format_info;
   VkExternalImageFormatProperties external_format_properties;
   VkImageFormatProperties2 format_properties;
   VkCommandPoolCreateInfo pool_info;
   PFN_vkGetPhysicalDeviceProperties2 get_properties;
   PFN_vkGetPhysicalDeviceExternalSemaphoreProperties get_semaphore_properties;
   PFN_vkGetPhysicalDeviceImageFormatProperties2 get_format_properties;
   PFN_vkGetMemoryWin32HandlePropertiesKHR get_memory_properties;
   PFN_vkImportSemaphoreWin32HandleKHR import_semaphore;
   unsigned dxgi_format;

   if (format == VK_FORMAT_R8G8B8A8_UNORM) dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
   else if (format == VK_FORMAT_B8G8R8A8_UNORM) dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
   else if (format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) dxgi_format = DXGI_FORMAT_R10G10B10A2_UNORM;
   else if (format == VK_FORMAT_R16G16B16A16_SFLOAT) dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
   else return NULL;
   get_properties = (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
   get_semaphore_properties = (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
   get_format_properties = (PFN_vkGetPhysicalDeviceImageFormatProperties2)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties2KHR");
   get_memory_properties = (PFN_vkGetMemoryWin32HandlePropertiesKHR)vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandlePropertiesKHR");
   import_semaphore = (PFN_vkImportSemaphoreWin32HandleKHR)vkGetDeviceProcAddr(device, "vkImportSemaphoreWin32HandleKHR");
   if (!get_properties || !get_semaphore_properties || !get_format_properties
         || !get_memory_properties || !import_semaphore) return NULL;
   /* Filled by the driver: only the chain and what is read back is set */
   id.sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
   id.pNext                         = NULL;
   id.deviceLUIDValid               = VK_FALSE;
   properties.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   properties.pNext                 = &id;
   get_properties(gpu, &properties);
   if (!id.deviceLUIDValid)
      return NULL;

   semaphore_info.sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
   semaphore_info.pNext             = NULL;
   semaphore_info.handleType        = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
   semaphore_properties.sType       = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
   semaphore_properties.pNext       = NULL;
   semaphore_properties.exportFromImportedHandleTypes = 0;
   semaphore_properties.compatibleHandleTypes         = 0;
   semaphore_properties.externalSemaphoreFeatures     = 0;
   get_semaphore_properties(gpu, &semaphore_info, &semaphore_properties);
   if (!(semaphore_properties.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT))
      return NULL;

   external_format_info.sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
   external_format_info.pNext      = NULL;
   external_format_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
   format_info.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
   format_info.pNext               = &external_format_info;
   format_info.format              = format;
   format_info.type                = VK_IMAGE_TYPE_2D;
   format_info.tiling              = VK_IMAGE_TILING_OPTIMAL;
   format_info.usage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   format_info.flags               = 0;
   external_format_properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
   external_format_properties.pNext = NULL;
   external_format_properties.externalMemoryProperties.externalMemoryFeatures        = 0;
   external_format_properties.externalMemoryProperties.exportFromImportedHandleTypes = 0;
   external_format_properties.externalMemoryProperties.compatibleHandleTypes         = 0;
   format_properties.sType         = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
   format_properties.pNext         = &external_format_properties;
   if (get_format_properties(gpu, &format_info, &format_properties) != VK_SUCCESS
         || !(external_format_properties.externalMemoryProperties.externalMemoryFeatures
            & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) return NULL;
   p = (ra_vulkan_dxgi_t *)calloc(1, sizeof(*p));
   if (!p) return NULL;
   p->device     = device;
   p->queue      = queue;
   p->queue_lock = queue_lock;
   p->gpu        = gpu;
   p->family     = family;
   p->bridge     = ra_dxgi_bridge_create(window, width, height, dxgi_format, id.deviceLUID);
   if (!p->bridge)
      goto error;
   pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool_info.pNext            = NULL;
   pool_info.flags            = 0;
   pool_info.queueFamilyIndex = family;
   CHECK_VK(vkCreateCommandPool(device, &pool_info, NULL, &p->pool));
   if (!ra_vulkan_dxgi_create_images(p, width, height, format))
      goto error;
   return p;
error:
   ra_vulkan_dxgi_destroy(p);
   return NULL;
}

void ra_vulkan_dxgi_hdr_metadata(ra_vulkan_dxgi_t *p, float peak)
{
   ra_dxgi_bridge_hdr_metadata(p->bridge, peak);
}

void *ra_vulkan_dxgi_device(ra_vulkan_dxgi_t *p)
{
   return p ? ra_dxgi_bridge_device(p->bridge) : NULL;
}

int64_t ra_vulkan_dxgi_present_time(ra_vulkan_dxgi_t *p)
{
   return p ? ra_dxgi_bridge_present_time(p->bridge) : 0;
}

VkImage ra_vulkan_dxgi_image(ra_vulkan_dxgi_t *p, unsigned index)
{ return index < 2 ? p->images[index] : VK_NULL_HANDLE; }

VkResult ra_vulkan_dxgi_acquire(ra_vulkan_dxgi_t *p,
      VkSemaphore semaphore, VkFence fence, unsigned *index, unsigned interval)
{
   VkD3D12FenceSubmitInfoKHR values;
   VkSubmitInfo submit;
   VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   uint64_t wait_value, signal_value = 0;
   unsigned acquired = ra_dxgi_bridge_index(p->bridge, interval);
   if (acquired >= 2)
      return ra_dxgi_bridge_failed(p->bridge) ? VK_ERROR_OUT_OF_DATE_KHR : VK_NOT_READY;
   *index = acquired;
   wait_value   = ra_dxgi_bridge_last_use(p->bridge, *index);

   values.sType = VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR;
   values.pNext                      = NULL;
   values.waitSemaphoreValuesCount   = 1;
   values.pWaitSemaphoreValues       = &wait_value;
   values.signalSemaphoreValuesCount = semaphore ? 1 : 0;
   values.pSignalSemaphoreValues     = &signal_value;

   submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.pNext                = &values;
   submit.waitSemaphoreCount   = 1;
   submit.pWaitSemaphores      = &p->done;
   submit.pWaitDstStageMask    = &stage;
   submit.commandBufferCount   = 1;
   submit.pCommandBuffers      = &p->acquire[*index];
   submit.signalSemaphoreCount = semaphore ? 1 : 0;
   submit.pSignalSemaphores    = &semaphore;

   return ra_vulkan_dxgi_submit(p, &submit, fence);
}

VkResult ra_vulkan_dxgi_present(ra_vulkan_dxgi_t *p, unsigned index,
      unsigned count, const VkSemaphore *wait, unsigned interval)
{
   unsigned i;
   VkD3D12FenceSubmitInfoKHR values;
   VkSubmitInfo submit;
   VkPipelineStageFlags stages[16];
   uint64_t wait_values[16];
   VkResult result;
   if (count > 16 || index >= 2)
      return VK_ERROR_INITIALIZATION_FAILED;
   for (i = 0; i < count; i++)
   {
      stages[i]      = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      wait_values[i] = 0;
   }
   ++p->ready_value;

   values.sType = VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR;
   values.pNext                      = NULL;
   values.waitSemaphoreValuesCount   = count;
   values.pWaitSemaphoreValues       = wait_values;
   values.signalSemaphoreValuesCount = 1;
   values.pSignalSemaphoreValues     = &p->ready_value;

   submit.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.pNext                  = &values;
   submit.waitSemaphoreCount     = count;
   submit.pWaitSemaphores        = wait;
   submit.pWaitDstStageMask      = stages;
   submit.commandBufferCount     = 1;
   submit.pCommandBuffers        = &p->release[index];
   submit.signalSemaphoreCount   = 1;
   submit.pSignalSemaphores      = &p->ready;

   result = ra_vulkan_dxgi_submit(p, &submit, VK_NULL_HANDLE);
   if (result != VK_SUCCESS)
      return result;
   return ra_dxgi_bridge_present(p->bridge, p->ready_value, interval)
      ? VK_SUCCESS : VK_ERROR_OUT_OF_DATE_KHR;
}

#endif
