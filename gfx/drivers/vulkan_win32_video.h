/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Included by vulkan.c on desktop Windows. */

#define VK_WIN32_VIDEO_IMPORTS 4

typedef struct vk_win32_video_import
{
   uint64_t source, ready, done;
   VkImage image;
   VkDeviceMemory memory;
   VkImageView view;
   VkSemaphore wait_sem, done_sem;
   unsigned generation, dims, pixfmt;
} vk_win32_video_import_t;

static bool vulkan_win32_video_matches(const vk_win32_video_import_t *p,
      const video_frame_import_t *frame)
{
   return p->source == frame->image && p->ready == frame->wait
      && p->done == frame->done && p->generation == frame->layout
      && p->dims == frame->dims && p->pixfmt == frame->pixfmt;
}

static void vulkan_win32_video_free(VkDevice device, vk_win32_video_import_t *p)
{
   vkDestroyImageView(device, p->view, NULL);
   vkDestroyImage(device, p->image, NULL);
   vkFreeMemory(device, p->memory, NULL);
   vkDestroySemaphore(device, p->wait_sem, NULL);
   vkDestroySemaphore(device, p->done_sem, NULL);
   memset(p, 0, sizeof(*p));
}

static bool vulkan_win32_video_semaphore(VkDevice device,
      PFN_vkImportSemaphoreWin32HandleKHR import_sem, uint64_t handle,
      VkSemaphore *out)
{
   VkSemaphoreTypeCreateInfo type;
   VkSemaphoreCreateInfo info;
   VkImportSemaphoreWin32HandleInfoKHR imported;
   memset(&type, 0, sizeof(type));
   memset(&info, 0, sizeof(info));
   memset(&imported, 0, sizeof(imported));
   type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
   type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
   info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   info.pNext = &type;
   if (vkCreateSemaphore(device, &info, NULL, out) != VK_SUCCESS)
      return false;
   imported.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
   imported.semaphore = *out;
   imported.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
   imported.handle = (HANDLE)(uintptr_t)handle;
   return import_sem(device, &imported) == VK_SUCCESS;
}

/* Called only after this stream slot's previous submission has completed. */
static bool vulkan_win32_video_get(vk_t *vk, vk_win32_video_import_t *p,
      const video_frame_import_t *frame, video_frame_import_t *view)
{
   VkDevice device = vk->context->device;
   if (!frame->image || !frame->wait || !frame->done
         || frame->array_slice || frame->crop_x || frame->crop_y)
      return false;
   if (!vulkan_win32_video_matches(p, frame))
   {
      VkExternalMemoryImageCreateInfo external;
      VkImageCreateInfo image;
      VkMemoryRequirements requirements;
      VkMemoryWin32HandlePropertiesKHR properties;
      VkMemoryDedicatedAllocateInfo dedicated;
      VkImportMemoryWin32HandleInfoKHR imported;
      VkMemoryAllocateInfo allocate;
      VkImageViewCreateInfo image_view;
      PFN_vkGetDeviceProcAddr get_proc = (PFN_vkGetDeviceProcAddr)
         vulkan_symbol_wrapper_instance_proc_addr()(vk->context->instance,
               "vkGetDeviceProcAddr");
      PFN_vkGetMemoryWin32HandlePropertiesKHR get_properties;
      PFN_vkImportSemaphoreWin32HandleKHR import_sem;
      unsigned index;
      if (!get_proc)
         return false;
      get_properties = (PFN_vkGetMemoryWin32HandlePropertiesKHR)
         get_proc(device, "vkGetMemoryWin32HandlePropertiesKHR");
      import_sem = (PFN_vkImportSemaphoreWin32HandleKHR)
         get_proc(device, "vkImportSemaphoreWin32HandleKHR");
      if (!get_properties || !import_sem)
         return false;
      vulkan_win32_video_free(device, p);
      memset(&external, 0, sizeof(external));
      memset(&image, 0, sizeof(image));
      memset(&properties, 0, sizeof(properties));
      memset(&dedicated, 0, sizeof(dedicated));
      memset(&imported, 0, sizeof(imported));
      memset(&allocate, 0, sizeof(allocate));
      memset(&image_view, 0, sizeof(image_view));
      external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
      external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
      image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      image.pNext = &external;
      image.imageType = VK_IMAGE_TYPE_2D;
      image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
      image.extent.width = VIDEO_SCALE_W(frame->dims);
      image.extent.height = VIDEO_SCALE_H(frame->dims);
      image.extent.depth = image.mipLevels = image.arrayLayers = 1;
      image.samples = VK_SAMPLE_COUNT_1_BIT;
      image.tiling = VK_IMAGE_TILING_OPTIMAL;
      image.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      if (vkCreateImage(device, &image, NULL, &p->image) != VK_SUCCESS)
         goto error;
      vkGetImageMemoryRequirements(device, p->image, &requirements);
      properties.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
      if (get_properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
               (HANDLE)(uintptr_t)frame->image, &properties) != VK_SUCCESS)
         goto error;
      for (index = 0; index < vk->context->memory_properties.memoryTypeCount; index++)
         if (requirements.memoryTypeBits & properties.memoryTypeBits & (1u << index))
            break;
      if (index == vk->context->memory_properties.memoryTypeCount)
         goto error;
      dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
      dedicated.image = p->image;
      imported.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
      imported.pNext = &dedicated;
      imported.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
      imported.handle = (HANDLE)(uintptr_t)frame->image;
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &imported;
      allocate.allocationSize = requirements.size;
      allocate.memoryTypeIndex = index;
      if (vkAllocateMemory(device, &allocate, NULL, &p->memory) != VK_SUCCESS
            || vkBindImageMemory(device, p->image, p->memory, 0) != VK_SUCCESS)
         goto error;
      image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      image_view.image = p->image;
      image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
      image_view.format = image.format;
      image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_view.subresourceRange.levelCount = image_view.subresourceRange.layerCount = 1;
      if (vkCreateImageView(device, &image_view, NULL, &p->view) != VK_SUCCESS
            || !vulkan_win32_video_semaphore(device, import_sem, frame->wait, &p->wait_sem)
            || !vulkan_win32_video_semaphore(device, import_sem, frame->done, &p->done_sem))
         goto error;
      p->source = frame->image; p->ready = frame->wait; p->done = frame->done;
      p->generation = frame->layout; p->dims = frame->dims; p->pixfmt = frame->pixfmt;
   }
   *view = *frame;
   memcpy(&view->image, &p->image, sizeof(p->image));
   memcpy(&view->plane_views[0], &p->view, sizeof(p->view));
   view->plane_views[1] = view->plane_views[0];
   memcpy(&view->wait, &p->wait_sem, sizeof(p->wait_sem));
   memcpy(&view->done, &p->done_sem, sizeof(p->done_sem));
   view->layout = VK_IMAGE_LAYOUT_GENERAL;
   view->array_slice = view->crop_x = view->crop_y = 0;
   return true;
error:
   vulkan_win32_video_free(device, p);
   return false;
}
