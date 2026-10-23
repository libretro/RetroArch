/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_RENDERER_H
#define RECORD_ANDROID_CAPTURE_RENDERER_H
#include "record_android_capture_submit.h"
/* Android Vulkan recording_gpu binding, immutable while frames reference it.
 * Context and source leases survive resize and remain alive through collection.
 * Callbacks never block, allocate, read back pixels or acquire the queue lock.
 */
enum record_android_capture_renderer_flags
{
   RECORD_ANDROID_CAPTURE_MENU = 1
};
typedef struct record_android_capture_renderer
{
   void *context;
   void (*release)(void *context, bool device_destroyed);
   /* Frame command outside render passes. Restore COLOR_ATTACHMENT_OPTIMAL.
    * Exactly 1 takes the source lease; no queue submission occurs here.
    * Reject unsupported format/extent/output mode without recording a copy. */
   int (*record)(void *context, const record_android_capture_source_t *source,
         VkCommandBuffer command, uint64_t pts, unsigned dims, unsigned mode);
   /* SUCCESS follows accepted main work under its existing queue lock.
    * Other results can precede submission/lock acquisition: retain the lease,
    * cancel output, and never submit completion or infer never-execute proof.
    */
   void (*submitted)(void *context, VkResult result);
   /* Optional bounded retirement/completion service under the queue lock. */
   void (*service)(void *context);
   unsigned flags;
} record_android_capture_renderer_t;
#endif
