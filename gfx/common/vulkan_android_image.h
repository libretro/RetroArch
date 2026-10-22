/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_IMAGE_H
#define VULKAN_ANDROID_IMAGE_H
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_android.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct vulkan_android_image_ops
{
   PFN_vkGetAndroidHardwareBufferPropertiesANDROID properties;
   PFN_vkCreateImage create_image;
   PFN_vkDestroyImage destroy_image;
   PFN_vkAllocateMemory allocate;
   PFN_vkFreeMemory free_memory;
   PFN_vkBindImageMemory bind;
   PFN_vkCreateSamplerYcbcrConversion create_conversion;
   PFN_vkDestroySamplerYcbcrConversion destroy_conversion;
   PFN_vkCreateSampler create_sampler;
   PFN_vkDestroySampler destroy_sampler;
   PFN_vkCreateImageView create_view;
   PFN_vkDestroyImageView destroy_view;
} vulkan_android_image_ops_t;
typedef struct vulkan_android_image
{
   VkImage image;
   VkDeviceMemory memory;
   VkImageView view;
   VkSampler sampler;
   VkSamplerYcbcrConversion conversion;
   uint64_t external_format;
} vulkan_android_image_t;
/* Cold resource-cache population only, on the renderer owner. buffer dimensions
 * must be checked against AHardwareBuffer_describe, one layer, GPU sampling,
 * unprotected usage. Caller enables external-memory AHB and YCbCr features.
 * Explicit coded model/range must already be resolved against native metadata.
 * Failure leaves out empty and unwinds every partial resource. No CPU mapping.
 * This creates resources only: caller must separately acquire FOREIGN ownership
 * after the ready fence, bind sampler immutably, and release ownership plus a
 * GPU completion fence before returning the AImage ticket. No sampling here. */
bool vulkan_android_image_create(VkDevice device,
      const vulkan_android_image_ops_t *ops, struct AHardwareBuffer *buffer,
      unsigned width, unsigned height,
      const VkPhysicalDeviceMemoryProperties *memory,
      VkSamplerYcbcrModelConversion model, VkSamplerYcbcrRange range,
      vulkan_android_image_t *out);
/* Owner only, after all GPU use/descriptor references retire. */
void vulkan_android_image_destroy(VkDevice device,
      const vulkan_android_image_ops_t *ops, vulkan_android_image_t *image);
/* Record outside render passes, on the graphics queue owner. The external
 * producer/consumer contract must use GENERAL layout. acquire transitions
 * FOREIGN/GENERAL -> graphics/SHADER_READ_ONLY; release reverses it after the
 * final read in the selected fragment or compute stage. Neither operation
 * submits or waits. Wait the imported ready semaphore at ALL_COMMANDS
 * (ownership transfers have no defined stage), and signal/export completion
 * after release. Never discard via UNDEFINED. */
bool vulkan_android_image_barrier(VkCommandBuffer command,
      PFN_vkCmdPipelineBarrier barrier, VkImage image, unsigned graphics_family,
      VkPipelineStageFlags sample_stage, bool acquire);
RETRO_END_DECLS
#endif
