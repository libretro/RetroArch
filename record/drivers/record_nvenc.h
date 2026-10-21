/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_NVENC_H
#define RECORD_NVENC_H
#include "../record_driver.h"
#if defined(_WIN32) && defined(HAVE_D3D12) && defined(HAVE_RAC3) \
   && defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE) \
   && !defined(__WINRT__) && !defined(_XBOX)
#define HAVE_RECORD_NVENC
#endif
RETRO_BEGIN_DECLS
struct video_frame_info;
extern const record_driver_t record_nvenc;
/* Main controller; frame snapshots keep the recorder alive through drawing. */
void record_nvenc_frame_info(struct video_frame_info *info, bool capture);
void record_nvenc_poll(void);
void record_nvenc_stop(void);
bool record_nvenc_pending(void);
bool record_nvenc_defer_start(void);
bool record_nvenc_cancel_start(void);
/* Render producer only; no allocation, waits, CPU pixels or logging. */
bool record_nvenc_gpu_reap(void *data);
void record_nvenc_gpu_capture(void *data, void *source, unsigned state,
      unsigned pos, unsigned dims, uint64_t timestamp, unsigned output_mode);
RETRO_END_DECLS
#endif
