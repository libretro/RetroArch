/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_REQUEST_H
#define RECORD_ANDROID_CAPTURE_REQUEST_H
#include "record_android_capture_pool.h"
RETRO_BEGIN_DECLS
typedef struct record_android_capture_request record_android_capture_request_t;
enum record_android_capture_operation
{
   RECORD_ANDROID_CAPTURE_CREATE,
   RECORD_ANDROID_CAPTURE_SERVICE,
   RECORD_ANDROID_CAPTURE_FINISH,
   RECORD_ANDROID_CAPTURE_CANCEL,
   RECORD_ANDROID_CAPTURE_PREPARE_DEVICE,
   RECORD_ANDROID_CAPTURE_COLLECT,
   RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE
};
/* Main-owner cold allocation. Copies the validated Vulkan pool configuration
 * and memory properties. Operation contexts/callbacks must survive collection;
 * context pointers must not refer to driver or swapchain allocations. Native
 * operations/device prerequisites remain the
 * caller's admission responsibility. Keep the original driver alive through
 * CREATE acknowledgment or cancellation. This is the only GPU controller for
 * its I/O job, which stays borrowed through free. EGL,
 * pool and GPU executor construction/collection occur on the render owner.
 * No frontend driver is registered by this private bridge. */
record_android_capture_request_t *record_android_capture_request_new(
      record_dispatch_mediacodec_t *io,
      const record_android_capture_pool_config_t *config, unsigned duration,
      bool allow_drop, bool menu_source);
/* Main owner outside frame publication. At most one embedded job is posted.
 * FINISH/CANCEL/PREPARE/COLLECT refuse queued frames naming this binding;
 * producers must first stop publishing it. Wrapped posting neither waits nor
 * allocates; without a wrapper it runs cold work on the current render owner.
 * end_us is immutable for an accepted operation, including dropped frames.
 * QUIESCE_DEVICE acknowledges only stopped publication and render-producer
 * quiescence. Registered native owners remain retained for the existing device
 * teardown drain; it is not child completion or actual device-death proof. */
bool record_android_capture_request_post(record_android_capture_request_t *job,
      enum record_android_capture_operation operation, int64_t end_us);
/* Main owner after post: 0 awaiting acknowledgment or progress, 1 complete,
 * -1 refused/failed. A discarded node is failure, never proof no setup ran. */
int record_android_capture_request_result(
      const record_android_capture_request_t *job);
/* Stable binding address, main owner after successful CREATE acknowledgment.
 * Retain job/binding through queued frames and native collection. */
const record_android_capture_renderer_t *record_android_capture_request_binding(
      const record_android_capture_request_t *job);
/* Main owner after acknowledgment. False may retain native EGL/device owners.
 * Refuses outstanding jobs, frames or uncollected resources. Closes the unused
 * GPU side of I/O if setup never acquired a pool. Does not close PCM or free
 * I/O.
 */
bool record_android_capture_request_free(record_android_capture_request_t *job);
RETRO_END_DECLS
#endif
