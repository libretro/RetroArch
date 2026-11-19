/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBRETRO_PYROWAVE_CORE_H
#define LIBRETRO_PYROWAVE_CORE_H
#include <boolean.h>
#include <libretro.h>
#include <retro_common_api.h>
#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
#endif
RETRO_BEGIN_DECLS
struct video_frame_info;
void libretro_pyrowave_poll(void);
void libretro_pyrowave_stop(void);
bool libretro_pyrowave_pending(void);
/* Existing audio worker before its pause gate; exiting only after its last
 * callback, or cold on the main thread with that worker parked/quiescent. */
void libretro_pyrowave_audio_control(bool exiting);
bool libretro_pyrowave_frame_info(struct video_frame_info *info, bool advance);
unsigned libretro_pyrowave_gl_frame(void *context, unsigned *dims, bool advance,
      const struct video_frame_info *timing);
/* Internal D3D12 renderer bridge. Attachment allocates descriptors cold;
 * detach requires every decoder/display loan and frame snapshot retired. */
bool video_d3d12_pyrowave_attach(void *renderer, void *codec);
bool video_d3d12_pyrowave_source_attach(void *renderer, void *codec, void *context);
void video_d3d12_pyrowave_detach(void *renderer, void *codec);
/* Resume only after the native codec was freed or abandoned. */
void video_d3d12_pyrowave_resume(void *renderer);
/* Cold render owner: 0 pending, 1 retired/list replaced, -1 failure. */
int video_d3d12_pyrowave_quiesce(void *renderer, void *codec);
void libretro_pyrowave_d3d12_failed(void *context);
void *libretro_pyrowave_d3d12_frame(void *context, void *commands, void *fence,
      uint64_t value, unsigned *dims, bool advance, const struct video_frame_info *timing);
#ifdef HAVE_VULKAN
struct record_pyrowave_vulkan_image;
/* Cold render owner: 0 pending, 1 queue retired and primaries reset, -1 failure. */
int video_vulkan_pyrowave_quiesce(void *renderer, VkFence marker, bool *submitted);
/* After native abandonment and private fence destruction. */
void video_vulkan_pyrowave_resume(void *renderer);
/* Submit the returned private completion with THIS primary, then acknowledge
 * its result. Discard is allowed only after resetting a never-submitted primary. */
bool libretro_pyrowave_vulkan_frame(void *context, VkCommandBuffer commands,
      struct record_pyrowave_vulkan_image *image, VkFence *completion, unsigned *dims, bool advance,
      const struct video_frame_info *timing);
void libretro_pyrowave_vulkan_submitted(void *context, VkFence completion,
      VkResult result, bool discarded);
#endif
RETRO_END_DECLS
#endif
