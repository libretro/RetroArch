/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_IMAGES_H
#define RECORD_ANDROID_CAPTURE_IMAGES_H
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_android.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_android_capture_images record_android_capture_images_t;
typedef struct record_android_capture_image_ops
{
   PFN_vkGetPhysicalDeviceImageFormatProperties2 query;
   PFN_vkGetAndroidHardwareBufferPropertiesANDROID properties;
   PFN_vkCreateImage create;
   PFN_vkDestroyImage destroy;
   PFN_vkAllocateMemory allocate;
   PFN_vkFreeMemory free_memory;
   PFN_vkBindImageMemory bind;
} record_android_capture_image_ops_t;
/* Cold renderer-owner allocation. Device belongs to gpu, with Vulkan 1.1 and
 * Android external-memory import enabled. 1..16 distinct GPU-sampled buffers
 * use dedicated memory and TRANSFER_DST images. Known RGB8/RGB10/RGBA16F only.
 * No CPU mapping, queue operation or frame-path allocation occurs here. */
record_android_capture_images_t *record_android_capture_images_new(
      VkPhysicalDevice gpu, VkDevice device,
      const record_android_capture_image_ops_t *ops,
      const VkPhysicalDeviceMemoryProperties *memory, unsigned count,
      unsigned width, unsigned height, VkFormat format);
/* Borrowed immutable handles. The producer must initialize each image and
 * synchronize Vulkan/foreign ownership plus worker reads before slot reuse.
 * Importing a buffer alone does not establish readiness or retire GPU use. */
VkImage record_android_capture_images_image(
      const record_android_capture_images_t *images, unsigned slot);
struct AHardwareBuffer *record_android_capture_images_buffer(
      const record_android_capture_images_t *images, unsigned slot);
/* Renderer-owner terminal preparation, after completion or DEVICE_LOST proof
 * for every submitted use and retirement of worker GPU reads. Destroys child
 * Vulkan objects before their device; retains native buffer/library ownership.
 */
void record_android_capture_images_prepare_device_destroy(
      record_android_capture_images_t *images);
/* Renderer-owner teardown, all GPU uses retired. Device must remain alive
 * unless preparation completed. Native references may outlive device teardown.
 */
void record_android_capture_images_free(
      record_android_capture_images_t *images);
RETRO_END_DECLS
#endif
