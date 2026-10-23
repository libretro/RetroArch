/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_POOL_H
#define RECORD_ANDROID_CAPTURE_POOL_H
#include "record_android_capture_images.h"
#include "record_android_capture_pipe.h"
#include "record_android_capture_renderer.h"
RETRO_BEGIN_DECLS
typedef struct record_android_capture_pool record_android_capture_pool_t;
typedef struct record_android_capture_pool_config
{
   record_android_capture_image_ops_t images;
   record_android_capture_submit_ops_t submit;
   record_gpu_mediacodec_config_t gpu;
   VkPhysicalDevice physical;
   VkDevice device;
   VkQueue queue;
   const VkPhysicalDeviceMemoryProperties *memory;
   unsigned family;
   bool fail_on_drop;
} record_android_capture_pool_config_t;
/* Cold renderer setup. Owns fixed images, entries, executor and handoff.
 * Borrows I/O, device and native contexts through collection. Buffers in gpu
 * are replaced by the newly allocated pool. False may return a partial owner:
 * it is cancelled/closed, but still needs collection before borrowed teardown.
 */
bool record_android_capture_pool_new(record_dispatch_mediacodec_t *io,
      const record_android_capture_pool_config_t *config,
      record_android_capture_pool_t **out);
/* Cold, once per pool. Binding storage must outlive queued video frames.
 * Duration is in microseconds. All callbacks run on the render owner; service
 * and successful submission callbacks use the renderer's existing queue lock.
 * Source bookkeeping is the pool, never a swapchain-image object. */
bool record_android_capture_pool_bind_renderer(
      record_android_capture_pool_t *pool, unsigned duration,
      record_android_capture_renderer_t *binding);
int record_android_capture_pool_push(record_android_capture_pool_t *pool,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration);
/* Renderer-command path; same record/commit/never-execute abort contract as
 * the handoff. Commit/abort remain available during cancellation until device
 * preparation; an accepted frame must not be silently abandoned. */
int record_android_capture_pool_record(record_android_capture_pool_t *pool,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration, VkCommandBuffer command);
int record_android_capture_pool_commit(record_android_capture_pool_t *pool);
bool record_android_capture_pool_abort_recorded(
      record_android_capture_pool_t *pool);
int record_android_capture_pool_step(record_android_capture_pool_t *pool);
/* Executor status; DONE does not authorize pool/device destruction. */
enum record_dispatch_mediacodec_state record_android_capture_pool_status(
      const record_android_capture_pool_t *pool, int *error, int *system_error);
bool record_android_capture_pool_sources_retired(
      const record_android_capture_pool_t *pool);
/* Render-owner cached closure; does not change the immutable source end. */
bool record_android_capture_pool_closed(
      const record_android_capture_pool_t *pool);
/* Same producer closure and cancellation contract as the handoff. */
bool record_android_capture_pool_finish(
      record_android_capture_pool_t *pool, int64_t end_us);
void record_android_capture_pool_cancel(record_android_capture_pool_t *pool);
/* Closed producer, renderer quiescent. Starts irreversible device retirement;
 * commit/abort stop even if a child still needs completion proof. Vulkan
 * children retire before the device; buffers/GLES stay owned until collection.
 * queue_quiesced proves deferred/native work can no longer execute. */
bool record_android_capture_pool_prepare_device_destroy(
      record_android_capture_pool_t *pool, bool queue_quiesced);
bool record_android_capture_pool_after_device_destroy(
      record_android_capture_pool_t *pool);
/* Render-owner cached proof only. prepared means Vulkan children retired;
 * destroyed means the matching actual device destruction was observed. */
bool record_android_capture_pool_device_state(
      const record_android_capture_pool_t *pool, bool *destroyed);
/* Cold renderer, callers quiescent. Refuses live resources; does not free I/O.
 * Device stays alive unless the two-phase device retirement completed. */
bool record_android_capture_pool_free(record_android_capture_pool_t *pool);
RETRO_END_DECLS
#endif
