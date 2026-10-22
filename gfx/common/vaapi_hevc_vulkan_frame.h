/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_VULKAN_FRAME_H
#define RARCH_VAAPI_HEVC_VULKAN_FRAME_H
#include "vaapi_hevc_vulkan.h"
#include "vulkan_prime_image.h"
RETRO_BEGIN_DECLS
typedef struct vaapi_hevc_vulkan_frame
{
   vaapi_hevc_export_t source;
   vulkan_prime_image_ops_t ops;
   vulkan_prime_image_t layers[2];
   VkDevice device;
   VkFence fence;
   bool active, pending, broken;
} vaapi_hevc_vulkan_frame_t;
/* Cold cache population, zeroed out. Both imports must succeed before moving
 * source's export/FDs/ticket into this owner. Failure consumes nothing. VA has
 * completed the source picture; matching physical-device identity is required.
 * VA pool/session/runtime outlive this frame and its eventual return handoff. */
bool vaapi_hevc_vulkan_frame_create(vaapi_hevc_vulkan_frame_t *out,
      vaapi_hevc_export_t *source, VkPhysicalDevice gpu, VkDevice device,
      bool prime_device, const VkPhysicalDeviceMemoryProperties *memory,
      const vulkan_prime_image_ops_t *ops, PFN_vkGetPhysicalDeviceFormatProperties2 get_format,
      PFN_vkGetPhysicalDeviceImageFormatProperties2 get_image);
/* Record outside a render pass. FOREIGN/GENERAL <-> graphics/SHADER_READ_ONLY.
 * Caller records acquire before sampling, release after its last sample. This
 * neither submits nor waits; never discard imported content via UNDEFINED. */
bool vaapi_hevc_vulkan_frame_barrier(const vaapi_hevc_vulkan_frame_t *frame,
      VkCommandBuffer command, PFN_vkCmdPipelineBarrier barrier,
      unsigned graphics_family, VkPipelineStageFlags stage, bool acquire);
/* One in-flight use on the renderer owner. Poll must return 1 before accepting
 * another use. Track immediately after accepted or possibly accepted GPU use.
 * Borrow the existing submission fence; it must stay valid/unreset until poll
 * succeeds. Uncertain submission failure must retain the frame through device
 * destruction if fence completion cannot be proven. */
bool vaapi_hevc_vulkan_frame_track(vaapi_hevc_vulkan_frame_t *frame, VkFence fence);
/* One zero-timeout status query. 1 complete, 0 pending, -1 sticky error. */
int vaapi_hevc_vulkan_frame_poll(vaapi_hevc_vulkan_frame_t *frame, PFN_vkGetFenceStatus status);
/* Renderer owner, after final GPU and descriptor references retire. Destroy
 * Vulkan resources, then move the still-held export into empty returned.
 * Deliver returned through the existing lease-return mechanism; ONLY the VA
 * owner calls vaapi_hevc_export_close to release its pool ticket. */
bool vaapi_hevc_vulkan_frame_retire(vaapi_hevc_vulkan_frame_t *frame,
      vaapi_hevc_export_t *returned);
/* ONLY after vkDestroyDevice has returned for the exact device. No Vulkan
 * call; invalidate handles and move the export for the same VA-owner return. */
bool vaapi_hevc_vulkan_frame_after_device_destroy(vaapi_hevc_vulkan_frame_t *frame,
      VkDevice device, vaapi_hevc_export_t *returned);
RETRO_END_DECLS
#endif
