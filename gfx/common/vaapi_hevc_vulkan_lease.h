/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_VULKAN_LEASE_H
#define RARCH_VAAPI_HEVC_VULKAN_LEASE_H
#include "vaapi_frame.h"
#include "vaapi_hevc_vulkan_submit.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_vulkan_lease
{
   vaapi_hevc_vulkan_submission_t *entry;
   vaapi_hevc_export_t returned;
   void *owner;
   uint64_t index, generation;
   bool fp16, broken;
} vaapi_hevc_vulkan_lease_t;

/* Cold renderer owner, zeroed lease and an idle prepared import entry.
 * All entry resources outlive this bridge and any outstanding frame. */
bool vaapi_hevc_vulkan_lease_bind(vaapi_hevc_vulkan_lease_t *lease,
      vaapi_hevc_vulkan_submission_t *entry, bool fp16);
/* One zero-timeout query; 1 idle/returned, 0 pending, -1 retained error.
 * Completion publishes the export to its decoder owner, preserving imports.
 * Decoder collect must run even while playback is paused or closing. */
int vaapi_hevc_vulkan_lease_poll(vaapi_hevc_vulkan_lease_t *lease);
/* Warm renderer owner. 1 consumes the opaque lease, 0 busy/-1 refusal leave
 * it pending, -2 retains a claimed lease after an uncertain failure. No native
 * FD/VA call, resource creation/destruction or wait. Source metadata comes
 * from the claimed carrier; destination format/dimensions must match. */
int vaapi_hevc_vulkan_lease_present(vaapi_hevc_vulkan_lease_t *lease,
      const video_hw_frame_t *frame, VkImage destination, VkImageView view,
      VkFormat format, unsigned width, unsigned height, VkImageLayout layout);
/* ONLY after actual destruction of entry's exact device. Invalidates imported
 * handles and publishes the retained source without calling Vulkan or VA.
 * False retains ownership for a later valid return; caller must not free it. */
bool vaapi_hevc_vulkan_lease_after_device_destroy(
      vaapi_hevc_vulkan_lease_t *lease, VkDevice device);
RETRO_END_DECLS
#endif
