/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_VULKAN_H
#define RECORD_VULKAN_H
#ifdef HAVE_RECORD_VULKAN
#include "../record_driver.h"
#include "../record_gpu_renderer_vulkan.h"
RETRO_BEGIN_DECLS
/* Renderer-owned immutable device snapshot, borrowed until driver teardown. */
typedef struct record_vulkan_device
{
   VkInstance instance;
   VkPhysicalDevice physical;
   VkDevice device;
   VkQueue queue;
   PFN_vkGetInstanceProcAddr get_proc;
   uint32_t api_version;
   unsigned family, producer_family;
   bool storage_images, hevc;
} record_vulkan_device_t;
extern const record_driver_t record_vulkan;
struct video_frame_info;
bool record_vulkan_frame_info(struct video_frame_info *info, bool capture);
bool record_vulkan_can_stream(void);
void record_vulkan_poll(void);
void record_vulkan_stop(void);
bool record_vulkan_pending(void);
bool record_vulkan_defer_start(void);
bool record_vulkan_cancel_start(void);
/* Render producer. Source and primary command belong to this frame. */
bool record_vulkan_gpu_capture(void *data, VkCommandBuffer command,
      const vulkan_encode_capture_source_t *source, uint64_t timestamp,
      unsigned dims, unsigned output_mode);
const VkSubmitInfo *record_vulkan_gpu_submission(void *data, const VkSubmitInfo *submit);
void record_vulkan_gpu_submitted(void *data, VkResult result);
/* Only after permanently discarding the unsubmitted primary contents. */
void record_vulkan_gpu_discard(void *data);
/* Stop before a resize can skip the next captured picture. */
void record_vulkan_gpu_reconfigure(void *data);
RETRO_END_DECLS
#endif
#endif
