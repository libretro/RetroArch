/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_SOURCE_H
#define RECORD_PYROWAVE_SOURCE_H
#include "record_pyrowave_session.h"
typedef void (*record_pyrowave_source_proc_t)(void);

typedef struct record_pyrowave_source_config
{
   rpyrowave_stream_config_t stream;
   void *device, *physical, *instance, *queue;
   record_pyrowave_source_proc_t instance_proc;
   record_pyrowave_source_proc_t (*gl_proc)(const char*);
   const char *directory;
   unsigned family, format, full_width, full_height, x, y, slots, texture_mode;
   bool scale_source;
} record_pyrowave_source_config_t;

typedef struct record_pyrowave_source
{
   void *user;
   record_pyrowave_video_t video;
   bool (*poll)(void*);
   bool (*idle)(void*);
   uint64_t (*retired)(void*);
   bool (*free)(void*);
   /* Cold failure retirement: GPU proof first, then quiesce I/O before free. */
   bool (*quiesce)(void*, void*);
   bool (*abandon)(void*);
} record_pyrowave_source_t;

RETRO_BEGIN_DECLS
typedef struct record_pyrowave_shader
{
   const void *data;
   size_t bytes;
   void *owned;
} record_pyrowave_shader_t;
/* Empty directory borrows immutable executable data. An explicit override loads
 * at most 1 MiB. Release after the native compiler/module has consumed it. */
bool record_pyrowave_source_shader(const char *directory, const char *name,
      record_pyrowave_shader_t *shader);
void record_pyrowave_source_shader_release(record_pyrowave_shader_t *shader);
bool record_pyrowave_source_shader_available(const char *name);
/* Owning desktop GL context, including poll/free and framebuffer capture. */
bool record_pyrowave_source_gl_new(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config);
int record_pyrowave_source_gl_push(record_pyrowave_source_t *source,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket);
bool record_pyrowave_source_d3d12_new(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config);
bool record_pyrowave_source_d3d11_new(record_pyrowave_source_t*, const record_pyrowave_source_config_t*);
int record_pyrowave_source_d3d11_push(record_pyrowave_source_t*, void*, uint64_t, uint64_t, uint64_t*);
int record_pyrowave_source_d3d12_push(record_pyrowave_source_t *source,
      void *commands, void *fence, uint64_t value, void *texture,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket);
#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
bool record_pyrowave_source_vulkan_new(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config);
int record_pyrowave_source_vulkan_push(record_pyrowave_source_t *source,
      VkCommandBuffer commands, VkImage image, uint64_t timestamp,
      uint64_t duration, uint64_t *ticket, VkFence *completion);
/* The primary uses push()'s completion when nonzero. Before reporting an allocation
 * failure, reset its never-submitted command buffer so it cannot execute.
 * Device loss retains borrows for explicit cold abandonment. */
bool record_pyrowave_source_vulkan_submitted(record_pyrowave_source_t *source,
      VkFence fence, VkResult result);
/* Only after resetting the never-submitted command buffer. */
bool record_pyrowave_source_vulkan_discard(record_pyrowave_source_t *source,
      VkFence fence);
#endif
RETRO_END_DECLS
#endif
