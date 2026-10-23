/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_SUBMIT_H
#define RECORD_ANDROID_CAPTURE_SUBMIT_H
#include <vulkan/vulkan_core.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_android_capture_submit record_android_capture_submit_t;
typedef struct record_android_capture_submit_ops
{
   PFN_vkCreateCommandPool create_pool;
   PFN_vkDestroyCommandPool destroy_pool;
   PFN_vkAllocateCommandBuffers allocate_commands;
   PFN_vkCreateFence create_fence;
   PFN_vkDestroyFence destroy_fence;
   PFN_vkCreateSemaphore create_semaphore;
   PFN_vkDestroySemaphore destroy_semaphore;
   PFN_vkResetCommandBuffer reset_command;
   PFN_vkBeginCommandBuffer begin;
   PFN_vkEndCommandBuffer end;
   PFN_vkCmdPipelineBarrier barrier;
   PFN_vkCmdCopyImage copy;
   PFN_vkResetFences reset_fences;
   PFN_vkGetFenceStatus fence_status;
   PFN_vkGetSemaphoreFdKHR export_fd;
   /* Same deferred queue contract as vulkan_android_queue_push: NOT_READY
    * accepts a copied submission, TIMEOUT submits nothing; errors retain it. */
   VkResult (*submit)(void *context, VkQueue queue, uint32_t count,
         const VkSubmitInfo *submits, VkFence fence);
   void (*close_fd)(void *context, int fd);
   void *context;
} record_android_capture_submit_ops_t;

typedef struct record_android_capture_source
{
   VkImage image;
   VkSemaphore ready;
   void *owner;
   /* One immutable source lease, including the optional binary ready signal.
    * device_destroyed forbids Vulkan calls during final source retirement. */
   void (*release)(void *owner, bool device_destroyed);
   VkFormat format;
   VkImageLayout layout;
   unsigned width, height, x, y;
} record_android_capture_source_t;
/* Cold graphics-owner setup of one fixed capture slot. Borrows the dedicated
 * RGB image/device/queue. Creates a command buffer, fence and exportable binary
 * semaphore. Caller enables FOREIGN ownership and SYNC_FD export support. */
record_android_capture_submit_t *record_android_capture_submit_new(
      VkDevice device, VkQueue queue, unsigned family, VkImage image,
      VkFormat format, unsigned width, unsigned height,
      const record_android_capture_submit_ops_t *ops);
/* Renderer-owner, outside render passes. Source is a resolved single-layer
 * TRANSFER_SRC-capable image on this device, owned by this family. Rendering
 * precedes the copy on this queue or signals ready. 1 accepts its lease, 0
 * BUSY/refused takes nothing, -1 local error takes nothing, -2 ambiguous
 * submission takes it. No allocation, CPU wait, readback or color/transfer
 * conversion occurs. */
int record_android_capture_submit_push(record_android_capture_submit_t *entry,
      const record_android_capture_source_t *source);
/* Renderer recording command, outside render passes, same queue/family.
 * No ready semaphore: the renderer supplies all prior dependencies. Records
 * one copy and takes the lease on 1. The command must never be replayed. */
int record_android_capture_submit_record(record_android_capture_submit_t *entry,
      const record_android_capture_source_t *source, VkCommandBuffer command);
/* Exactly once, AFTER the renderer accepted that command on the same queue.
 * Submits only a completion signal/fence, no second copy. 0 retains pending
 * completion; poll retries only this signal submission, never source work.
 * Negative retains ambiguous ownership for terminal device recovery. */
int record_android_capture_submit_commit(
      record_android_capture_submit_t *entry);
/* ONLY after the renderer reset/discarded its unsubmitted command and proves
 * it can never execute. Refuses after commit; releases the original lease. */
bool record_android_capture_submit_abort_recorded(
      record_android_capture_submit_t *entry);
/* One zero-timeout fence query. 1 copy retired, 0 pending, -1 sticky error.
 * Export failures cannot leave a binary semaphore eligible for re-signaling. */
int record_android_capture_submit_poll(record_android_capture_submit_t *entry);
/* Renderer owner transfers one owned SYNC_FD, including valid -1. 1 transfers,
 * 0 unavailable/already taken, -1 error. Worker waits it on the GPU before
 * sampling. Taking a ready fd does not retire the producer's source lease. */
int record_android_capture_submit_take_ready(
      record_android_capture_submit_t *entry, int *owned_fd);
/* Renderer owner, only after worker GPU reads retire and its fd is consumed.
 * Refuses until the copy fence and source lease retire. Enables slot reuse. */
bool record_android_capture_submit_release(
      record_android_capture_submit_t *entry);
/* Terminal cancellation, caller has retired worker reads. 1 quiescent,
 * 0 copy still pending, -1 no completion proof. Never retries a submission. */
int record_android_capture_submit_discard(
      record_android_capture_submit_t *entry);
/* Terminal renderer teardown, worker GPU reads retired. Destroys child Vulkan
 * objects before device destruction, after a completion query succeeds or
 * reports DEVICE_LOST. queue_quiesced additionally permits never-submitted
 * work: all deferred work is permanently removed and native queue execution
 * is proven complete. Source bookkeeping stays held until the next call. */
bool record_android_capture_submit_prepare_device_destroy(
      record_android_capture_submit_t *entry, bool queue_quiesced);
/* ONLY after preparation and the matching vkDestroyDevice returned. Retires
 * held source bookkeeping and fd without invoking Vulkan. */
bool record_android_capture_submit_after_device_destroy(
      record_android_capture_submit_t *entry);
/* Renderer quiescent. Refuses producer or consumer ownership in flight. */
bool record_android_capture_submit_free(record_android_capture_submit_t *entry);
RETRO_END_DECLS
#endif
