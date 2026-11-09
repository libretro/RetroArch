/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_POOL_H
#define RARCH_VULKAN_ENCODE_POOL_H
#include "../include/vulkan/vulkan.h"
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
#define VULKAN_ENCODE_POOL_SLOTS 16

typedef struct vulkan_encode_dpb
{
   VkImage image[2];
   VkImageView view[2];
} vulkan_encode_dpb_t;
typedef struct vulkan_encode_pool_ops
{
   PFN_vkGetPhysicalDeviceMemoryProperties memory_properties;
   PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR video_formats;
   PFN_vkGetPhysicalDeviceImageFormatProperties2 image_properties;
   PFN_vkGetPhysicalDeviceFormatProperties2 format_properties;
   PFN_vkCreateImage create_image;
   PFN_vkDestroyImage destroy_image;
   PFN_vkGetImageMemoryRequirements image_memory;
   PFN_vkBindImageMemory bind_image;
   PFN_vkCreateImageView create_view;
   PFN_vkDestroyImageView destroy_view;
   PFN_vkCreateBuffer create_buffer;
   PFN_vkDestroyBuffer destroy_buffer;
   PFN_vkGetBufferMemoryRequirements buffer_memory;
   PFN_vkBindBufferMemory bind_buffer;
   PFN_vkMapMemory map;
   PFN_vkUnmapMemory unmap;
   PFN_vkAllocateMemory allocate;
   PFN_vkFreeMemory free_memory;
   PFN_vkCreateCommandPool create_commands;
   PFN_vkDestroyCommandPool destroy_commands;
   PFN_vkAllocateCommandBuffers allocate_command;
   PFN_vkCreateFence create_fence;
   PFN_vkDestroyFence destroy_fence;
   PFN_vkCreateQueryPool create_queries;
   PFN_vkDestroyQueryPool destroy_queries;
   PFN_vkCreateSemaphore create_semaphore;
   PFN_vkDestroySemaphore destroy_semaphore;
} vulkan_encode_pool_ops_t;
typedef struct vulkan_encode_buffer
{
   void *mapped;
   VkBuffer buffer;
   VkDeviceMemory memory;
   VkDeviceSize size;
   VkMemoryPropertyFlags properties;
} vulkan_encode_buffer_t;
typedef struct vulkan_encode_pool_slot
{
   vulkan_encode_buffer_t output, conversion;
   VkImage image;
   VkDeviceMemory memory;
   VkImageView view, planes[2];
   VkCommandBuffer command;
   VkFence fence;
} vulkan_encode_pool_slot_t;
typedef struct vulkan_encode_pool_config
{
   const VkVideoProfileInfoKHR *profile;
   const VkVideoCapabilitiesKHR *capabilities;
   VkPhysicalDevice physical;
   VkDevice device;
   VkExtent2D extent;
   VkFormat format;
   size_t packet_size;
   unsigned slots, encode_family, producer_family;
   /* Enabled on the device, not merely physically available. */
   bool storage_image_extended_formats;
} vulkan_encode_pool_config_t;
typedef struct vulkan_encode_pool
{
   const vulkan_encode_pool_ops_t *ops;
   VkDevice device;
   vulkan_encode_pool_slot_t slot[VULKAN_ENCODE_POOL_SLOTS];
   vulkan_encode_dpb_t dpb;
   VkDeviceMemory dpb_memory[2];
   VkCommandPool commands;
   VkQueryPool queries;
   VkSemaphore ready, done;
   VkExtent2D extent;
   VkFormat format;
   unsigned count;
   bool storage_images;
} vulkan_encode_pool_t;

/* Worker startup, no submissions. Device enables video encode, timeline and
 * both queue families. NV12 only. Config/profile/caps are borrowed during
 * construction; device/dispatch through free. Resources stay on one device.
 * Storage plane views are preferred; otherwise conversion uses a GPU
 * packed buffer followed by an image copy. Output is coherently mapped, cached
 * where available. Frame paths allocate nothing. */
vulkan_encode_pool_t *vulkan_encode_pool_new(
      const vulkan_encode_pool_config_t *config,
      const vulkan_encode_pool_ops_t *ops);
/* After producers, commands and every borrowed packet/view retire. Device
 * remains valid. All allocated command buffers belong to the command pool. */
void vulkan_encode_pool_free(vulkan_encode_pool_t *pool);
RETRO_END_DECLS
#endif
