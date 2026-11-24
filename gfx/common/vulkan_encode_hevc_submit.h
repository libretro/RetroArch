/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_HEVC_SUBMIT_H
#define RARCH_VULKAN_ENCODE_HEVC_SUBMIT_H
#include "vulkan_encode_hevc_picture.h"
#include "vulkan_encode_hevc_session.h"

RETRO_BEGIN_DECLS

enum vulkan_encode_hevc_result
{
   VULKAN_ENCODE_HEVC_ERROR = -1,
   VULKAN_ENCODE_HEVC_OK,
   VULKAN_ENCODE_HEVC_BUSY
};
typedef struct vulkan_encode_hevc_submit_ops
{
   PFN_vkResetCommandBuffer reset_command;
   PFN_vkBeginCommandBuffer begin;
   PFN_vkEndCommandBuffer end;
   PFN_vkCmdPipelineBarrier2 barrier;
   PFN_vkCmdResetQueryPool reset_query;
   PFN_vkCmdBeginQuery begin_query;
   PFN_vkCmdEndQuery end_query;
   PFN_vkCmdBeginVideoCodingKHR begin_coding;
   PFN_vkCmdControlVideoCodingKHR control;
   PFN_vkCmdEncodeVideoKHR encode;
   PFN_vkCmdEndVideoCodingKHR end_coding;
   PFN_vkResetFences reset_fence;
   PFN_vkQueueSubmit submit;
   PFN_vkGetFenceStatus status;
   PFN_vkGetQueryPoolResults feedback;
   PFN_vkWaitForFences wait;
} vulkan_encode_hevc_submit_ops_t;
typedef struct vulkan_encode_hevc_dpb
{
   VkImage image[2];
   VkImageView view[2];
} vulkan_encode_hevc_dpb_t;
typedef struct vulkan_encode_hevc_resources
{
   const vulkan_encode_hevc_dpb_t *dpb;
   void *mapped;
   size_t capacity;
   VkQueue queue;
   VkCommandBuffer command;
   VkFence fence;
   VkQueryPool queries;
   VkBuffer bitstream;
   VkImage input;
   VkImageView input_view;
   VkSemaphore ready, done;
   uint64_t ready_value, done_value;
   unsigned query;
} vulkan_encode_hevc_resources_t;
typedef struct vulkan_encode_hevc_submit
{
   const vulkan_encode_hevc_session_t *session;
   const vulkan_encode_hevc_submit_ops_t *ops;
   const vulkan_encode_hevc_resources_t *resources;
   const void *packet;
   size_t packet_size;
   VkImageMemoryBarrier2 images[3];
   VkBufferMemoryBarrier2 buffer;
   VkDependencyInfo dependency;
   VkCommandBufferBeginInfo command;
   VkVideoPictureResourceInfoKHR dpb[2];
   VkVideoReferenceSlotInfoKHR bound[2];
   VkVideoEncodeRateControlInfoKHR rate;
   VkVideoBeginCodingInfoKHR begin;
   VkVideoCodingControlInfoKHR control;
   VkVideoEncodeInfoKHR encode;
   VkVideoEndCodingInfoKHR end;
   VkTimelineSemaphoreSubmitInfo timeline;
   VkSubmitInfo submit;
   VkPipelineStageFlags wait_stage;
   VkResult error;
   int32_t completion_status;
   bool active, gpu_complete, packet_ready, poisoned;
} vulkan_encode_hevc_submit_t;

/* Zero-initialize. One worker owns transaction and queue. No allocation or
 * waits. Input is GENERAL, concurrent between producer/encode families (or
 * same family); ready/done are distinct enabled timeline semaphores. Output
 * is coherently mapped. The primary command buffer uses a resettable encode
 * pool. Query feedback enables only offset/bytes for this session profile.
 * All handles belong to the session device. Borrow session/dispatch/resources
 * through retirement.
 * DPB resources are independent and retained by their pool across all work.
 * Picture bundles are borrowed only while recording this call. A successful
 * submit alone advances caller DPB initialization/previous-picture state. */
int vulkan_encode_hevc_submit_start(vulkan_encode_hevc_submit_t *transaction,
      const vulkan_encode_hevc_session_t *session,
      const vulkan_encode_hevc_submit_ops_t *ops,
      const vulkan_encode_hevc_resources_t *resources,
      const vulkan_encode_hevc_picture_t *picture,
      const vulkan_encode_hevc_picture_t *previous,
      unsigned initialized_mask, bool reset);
/* One fence/query check. OK borrows compressed bytes until retire; repeated
 * polls preserve that borrow. Input is returned to GENERAL before done.
 * ERROR may retain uncertain work: keep polling until retire succeeds. A
 * completion check reporting device loss retires encoder resource use. */
int vulkan_encode_hevc_submit_poll(vulkan_encode_hevc_submit_t *transaction,
      const void **data, size_t *size);
/* Acknowledges/discards output. Refuses incomplete/uncertain encoder work.
 * Producer GPU work and packet readers remain the caller's responsibility. */
bool vulkan_encode_hevc_submit_retire(vulkan_encode_hevc_submit_t *transaction);
/* Blocks until the transaction's GPU work completes or @timeout_ns
 * passes. False, without waiting, when there is none left to wait for. */
bool vulkan_encode_hevc_submit_wait(const vulkan_encode_hevc_submit_t *transaction,
      uint64_t timeout_ns);
RETRO_END_DECLS
#endif
