/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_PRIME_IMAGE_H
#define RARCH_VULKAN_PRIME_IMAGE_H
#include "vulkan_prime_format.h"
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct vulkan_prime_plane
{
   VkDeviceSize offset, pitch, allocation_size;
   int fd;
} vulkan_prime_plane_t;
typedef struct vulkan_prime_image_ops
{
   PFN_vkCreateImage create_image;
   PFN_vkDestroyImage destroy_image;
   PFN_vkGetImageMemoryRequirements2 requirements;
   PFN_vkGetMemoryFdPropertiesKHR fd_properties;
   PFN_vkAllocateMemory allocate;
   PFN_vkFreeMemory free_memory;
   PFN_vkBindImageMemory2 bind;
   PFN_vkCreateImageView create_view;
   PFN_vkDestroyImageView destroy_view;
   int (*duplicate)(int);
   bool (*size)(int, VkDeviceSize*);
   void (*close)(int);
} vulkan_prime_image_ops_t;
typedef struct vulkan_prime_image
{
   VkDeviceMemory memory[4];
   VkImage image;
   VkImageView view;
} vulkan_prime_image_t;
/* Cold Linux CLOEXEC duplication and DMA-BUF llseek size discovery. */
bool vulkan_prime_image_bind_io(vulkan_prime_image_ops_t *ops);
/* Cold renderer-owner cache population. out starts empty; plan is the exact
 * successful format query for these planes on the matching physical device.
 * Input FDs are borrowed. Every allocation imports its own CLOEXEC duplicate;
 * only successful Vulkan allocation consumes that duplicate. No pixels read.
 * Producer completion must precede this call. Foreign ownership and GPU use
 * retirement remain caller responsibilities; no queue work is performed. */
bool vulkan_prime_image_create(VkDevice device, const vulkan_prime_image_ops_t *ops,
      const VkPhysicalDeviceMemoryProperties *memory, const vulkan_prime_format_t *plan,
      const vulkan_prime_plane_t *planes, vulkan_prime_image_t *out);
/* After all GPU and descriptor references retire, before releasing VA tickets. */
void vulkan_prime_image_destroy(VkDevice device, const vulkan_prime_image_ops_t *ops,
      vulkan_prime_image_t *image);
RETRO_END_DECLS
#endif
