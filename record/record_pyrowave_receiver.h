/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_RECEIVER_H
#define RECORD_PYROWAVE_RECEIVER_H
#include <boolean.h>
#include <libretro.h>
#include <retro_common_api.h>
#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
#endif
RETRO_BEGIN_DECLS
struct video_frame_info;
bool video_d3d11_pyrowave_source_attach(void *renderer);
void video_d3d11_pyrowave_source_detach(void *renderer);
void *record_pyrowave_receiver_d3d11_frame(void *context, unsigned *dims, bool advance,
      const struct video_frame_info *timing);
/* Main-thread frontend service. start() copies a connection-file path and
 * queues cold setup; the selected core remains loaded throughout playback. */
bool record_pyrowave_receiver_start(const char *path);
/* Menu listener: keep the loaded core running until explicit consent. */
bool record_pyrowave_receiver_listen(const char *directory);
unsigned record_pyrowave_receiver_port(void);
bool record_pyrowave_receiver_incoming(void);
bool record_pyrowave_receiver_consent(bool accept);
bool record_pyrowave_receiver_active(void);
bool record_pyrowave_receiver_reset(void);
bool record_pyrowave_receiver_frame(void);
/* Existing frontend audio worker; never installs a libretro callback. */
bool record_pyrowave_receiver_audio_enabled(void);
bool record_pyrowave_receiver_audio_run(void);
void record_pyrowave_receiver_poll(void);
void record_pyrowave_receiver_stop(void);
bool record_pyrowave_receiver_pending(void);
/* Existing audio worker before its pause gate; exiting only after its last
 * callback, or cold on the main thread with that worker parked/quiescent. */
void record_pyrowave_receiver_audio_control(bool exiting);
bool record_pyrowave_receiver_frame_info(struct video_frame_info *info, bool advance);
unsigned record_pyrowave_receiver_gl_frame(void *context, unsigned *dims, bool advance,
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
void record_pyrowave_receiver_d3d12_failed(void *context);
void *record_pyrowave_receiver_d3d12_frame(void *context, void *commands, void *fence,
      uint64_t value, unsigned *dims, bool advance, const struct video_frame_info *timing);
#ifdef HAVE_VULKAN
struct record_pyrowave_vulkan_image;
/* Cold render owner: 0 pending, 1 queue retired and primaries reset, -1 failure. */
int video_vulkan_pyrowave_quiesce(void *renderer, VkFence marker, bool *submitted);
/* After native abandonment and private fence destruction. */
void video_vulkan_pyrowave_resume(void *renderer);
/* Submit the returned private completion with THIS primary, then acknowledge
 * its result. Discard is allowed only after resetting a never-submitted primary. */
bool record_pyrowave_receiver_vulkan_frame(void *context, VkCommandBuffer commands,
      struct record_pyrowave_vulkan_image *image, VkFence *completion, unsigned *dims, bool advance,
      const struct video_frame_info *timing);
void record_pyrowave_receiver_vulkan_submitted(void *context, VkFence completion,
      VkResult result, bool discarded);
#endif
RETRO_END_DECLS
#endif
