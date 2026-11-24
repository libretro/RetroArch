/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_DRIVER_H
#define RECORD_PYROWAVE_DRIVER_H
#include "../record_driver.h"
#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_OPENGL_CORE) && defined(HAVE_CONFIGFILE) && !defined(HAVE_OPENGLES)
#define HAVE_RECORD_PYROWAVE_GL
#endif
#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_D3D12) && defined(HAVE_CONFIGFILE) \
   && defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
#define HAVE_RECORD_PYROWAVE_D3D12
#endif
#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_D3D11) && defined(HAVE_CONFIGFILE) \
   && defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
#define HAVE_RECORD_PYROWAVE_D3D11
#endif
#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_VULKAN) && defined(HAVE_CONFIGFILE) && !defined(ANDROID)
#define HAVE_RECORD_PYROWAVE_VULKAN
#include <vulkan/vulkan.h>
#endif
#if defined(HAVE_RECORD_PYROWAVE_D3D11) || defined(HAVE_RECORD_PYROWAVE_D3D12) || defined(HAVE_RECORD_PYROWAVE_VULKAN) || defined(HAVE_RECORD_PYROWAVE_GL)
#define HAVE_RECORD_PYROWAVE_SOURCE
#endif
RETRO_BEGIN_DECLS
struct video_frame_info;
void record_pyrowave_d3d11_frame(void *context, void *view, unsigned format,
      unsigned full_dims, unsigned pos, unsigned dims, unsigned output_mode,
      uint64_t timestamp, uint64_t duration, bool capture);
extern const record_driver_t record_pyrowave;
bool record_pyrowave_frame_info(struct video_frame_info *info, bool capture);
void record_pyrowave_poll(void);
void record_pyrowave_stop(void);
/* Render owner: fail a borrowed sender context; keeps native loans for cold retirement. */
void record_pyrowave_failed(void *context);
bool record_pyrowave_pending(void);
bool record_pyrowave_frontend_active(void);
bool record_pyrowave_frontend_pause(void);
bool record_pyrowave_reset_pending(void);
bool record_pyrowave_defer_start(void);
bool record_pyrowave_cancel_start(void);
#ifdef HAVE_RECORD_PYROWAVE_GL
void record_pyrowave_gl_frame(void *context, unsigned format,
      unsigned full_dims, unsigned pos, unsigned dims, unsigned output_mode,
      uint64_t timestamp, uint64_t duration, bool capture);
#endif
/* Render owner, before executing the frame list. SDK handles are borrowed. */
void record_pyrowave_d3d12_frame(void *context, void *commands, void *fence,
      uint64_t value, void *source, unsigned format, unsigned full_dims,
      unsigned pos, unsigned dims, unsigned output_mode, uint64_t timestamp,
      uint64_t duration, bool capture);
#ifdef HAVE_RECORD_PYROWAVE_VULKAN
VkFence record_pyrowave_vulkan_frame(void *context, VkCommandBuffer commands,
      VkImage image, unsigned format, unsigned full_dims, unsigned pos,
      unsigned dims, unsigned output_mode, uint64_t timestamp,
      uint64_t duration, bool capture);
void record_pyrowave_vulkan_submitted(void *context, VkFence fence, VkResult result);
/* Render owner, after resetting a primary that will never be submitted. */
void record_pyrowave_vulkan_unsubmitted(void *context, VkFence fence);
#endif
RETRO_END_DECLS
#endif
