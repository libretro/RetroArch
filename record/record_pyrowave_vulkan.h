/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_VULKAN_H
#define RECORD_PYROWAVE_VULKAN_H
#include <vulkan/vulkan.h>
#include <formats/rpyrowave_gpu.h>
#include "record_gpu_queue.h"
#include "record_pyrowave.h"
#include "record_pyrowave_session.h"

RETRO_BEGIN_DECLS
typedef struct record_pyrowave_vulkan record_pyrowave_vulkan_t;
void record_pyrowave_vulkan_session_video(record_pyrowave_vulkan_t *codec, record_pyrowave_video_t *video);

typedef struct record_pyrowave_vulkan_config
{
   PFN_vkGetDeviceProcAddr get_proc;
   VkDevice device;
   VkQueue queue;
   const VkPhysicalDeviceMemoryProperties *memory;
   const VkPhysicalDeviceLimits *limits;
   const uint32_t *shaders[3];
   size_t shader_bytes[3];
   rpyrowave_sequence_t sequence;
   VkFormatFeatureFlags rgba32f_features;
   VkQueueFlags queue_flags;
   uint32_t queue_family;
   uint32_t maximum_bytes;
   unsigned slots, bit_depth, texture_mode;
   unsigned texture_width, texture_height, x, y;
   bool flip_y, decode;
   bool scale_source;
} record_pyrowave_vulkan_config_t;

typedef struct record_pyrowave_vulkan_image
{
   record_gpu_frame_t frame;
   VkImage image;
   VkImageView view;
   VkImageLayout layout;
   uint32_t queue_family;
   unsigned texture_mode;
} record_pyrowave_vulkan_image_t;

/* Cold setup, borrowing device/queue until free. Properties and SPIR-V are
 * borrowed only during construction. Requires a compute-capable queue and
 * RGBA32F sampled/storage images. No command submission, thread or wait is
 * hidden here. Cached mappings are preferred; otherwise a cold CPU pool
 * stages compressed data only. Noncoherent memory is flushed/invalidated.
 * Hot render methods run on one queue owner. take/packet/release belong to
 * the encoder's I/O consumer; receive_acquire/submit/cancel/finish belong to
 * the decoder's I/O producer. Both sides quiesce before free/abandon. */
record_pyrowave_vulkan_t *record_pyrowave_vulkan_new(const record_pyrowave_vulkan_config_t *config);

/* commands is recording outside a render pass, on the configured queue/family.
 * completion is an unsignaled, otherwise unused fence. The caller submits
 * exactly once, in call order, with that fence, and keeps it until poll
 * consumes it. Command buffers may not be resubmitted. Source views are
 * non-sRGB float-loadable 2D views with the configured extent/crop. The caller
 * establishes source layout, visibility and ownership before these commands,
 * and retains the source until source_retired >= ticket. BUSY consumes none. */
int record_pyrowave_vulkan_push(record_pyrowave_vulkan_t *codec, VkCommandBuffer commands,
      VkFence completion, VkImageView source, VkImageLayout layout,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket);
uint64_t record_pyrowave_vulkan_source_retired(const record_pyrowave_vulkan_t *codec);
int record_pyrowave_vulkan_take(record_pyrowave_vulkan_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_vulkan_packet(record_pyrowave_vulkan_t *codec, unsigned slot,
      record_pyrowave_packet_t *packet);
bool record_pyrowave_vulkan_release(record_pyrowave_vulkan_t *codec, unsigned slot);

/* Direct compressed receive span. Header indexing is in place; an invalid
 * submit retains its reservation. No Vulkan calls on this I/O side. */
int record_pyrowave_vulkan_receive_acquire(record_pyrowave_vulkan_t *codec, void **data, size_t *capacity);
int record_pyrowave_vulkan_receive_submit(record_pyrowave_vulkan_t *codec,
      size_t bytes, uint64_t timestamp, uint64_t duration);
bool record_pyrowave_vulkan_receive_cancel(record_pyrowave_vulkan_t *codec);
int record_pyrowave_vulkan_receive_drop(record_pyrowave_vulkan_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_vulkan_receive_peek(record_pyrowave_vulkan_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_vulkan_decode(record_pyrowave_vulkan_t *codec,
      VkCommandBuffer commands, VkFence completion);
/* poll publishes completed RGBA32F images in texture_mode. Views stay owned
 * through final display completion. Same-queue display may sample GENERAL.
 * Other queues require caller-managed semaphore/ownership transfers, returning
 * the image to GENERAL and queue_family before the display fence signals. */
int record_pyrowave_vulkan_image(record_pyrowave_vulkan_t *codec, record_pyrowave_vulkan_image_t *image);
bool record_pyrowave_vulkan_image_drop(record_pyrowave_vulkan_t *codec, unsigned slot);
/* Return after recording FINAL display use, in GENERAL on queue_family.
 * A fence may already be signaled or protect another slot from the same
 * submission. Retain it WITHOUT RESET until fence_borrowed() is false after
 * poll/discard, and until the caller's own display pins have retired. */
bool record_pyrowave_vulkan_present(record_pyrowave_vulkan_t *codec, unsigned slot, VkFence completion);
/* Only after the caller reset/freed the associated NEVER-submitted command
 * buffer, ensuring it cannot execute. Retires that fence's source/display
 * lends without Vulkan work. Never discard a submitted buffer this way. */
bool record_pyrowave_vulkan_discard(record_pyrowave_vulkan_t *codec, VkFence completion);
bool record_pyrowave_vulkan_poll(record_pyrowave_vulkan_t *codec);
/* Render owner. These acknowledge native fence loans, not input/output queue
 * ownership. Never reset a fence on an independent status query alone. */
bool record_pyrowave_vulkan_fence_borrowed(const record_pyrowave_vulkan_t *codec, VkFence fence);
bool record_pyrowave_vulkan_idle(const record_pyrowave_vulkan_t *codec);
/* END closes input only; GPU, packet and image borrows must still retire. */
bool record_pyrowave_vulkan_finish(record_pyrowave_vulkan_t *codec);
bool record_pyrowave_vulkan_free(record_pyrowave_vulkan_t *codec);
/* Cold recovery only after device loss or externally established quiescence,
 * with callers stopped and all associated command buffers reset/freed. Native
 * objects are destroyed here BEFORE the caller destroys the VkDevice. */
bool record_pyrowave_vulkan_abandon(record_pyrowave_vulkan_t *codec);
RETRO_END_DECLS
#endif
