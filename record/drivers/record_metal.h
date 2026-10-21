/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_METAL_H
#define RECORD_METAL_H
#include "../record_driver.h"
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_OSX && defined(HAVE_METAL) && defined(HAVE_AVF) \
      && defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE)
#define HAVE_RECORD_METAL
#endif
#endif
RETRO_BEGIN_DECLS
struct video_frame_info;
extern const record_driver_t record_metal;
void record_metal_frame_info(struct video_frame_info *info,bool capture);
void record_metal_poll(void);
void record_metal_stop(void);
bool record_metal_pending(void);
bool record_metal_defer_start(void);
bool record_metal_cancel_start(void);
/* Render producer only; commands must be committed by the renderer after
 * capture. Uses final drawable encoding and viewport, with no CPU readback. */
void record_metal_gpu_capture(void *data,void *commands,void *source,
      unsigned pos,unsigned dims,uint64_t timestamp,unsigned output_mode);
RETRO_END_DECLS
#endif
