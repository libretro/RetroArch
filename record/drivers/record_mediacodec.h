/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MEDIACODEC_H
#define RECORD_MEDIACODEC_H
#include "../record_driver.h"
#if defined(HAVE_RECORD_MEDIACODEC)                                            \
      && (!defined(ANDROID) || !defined(HAVE_VULKAN) || !defined(HAVE_THREADS) \
            || !defined(HAVE_CONFIGFILE))
#error Native MediaCodec recording requires Android, Vulkan, threads and config files
#endif
RETRO_BEGIN_DECLS
struct video_frame_info;
extern const record_driver_t record_mediacodec;
void record_mediacodec_frame_info(struct video_frame_info *info, bool capture);
void record_mediacodec_poll(void);
void record_mediacodec_stop(void);
bool record_mediacodec_pending(void);
/* Main owner after stop. Acknowledged producer quiescence allows the existing
 * Vulkan teardown drain to prove child retirement, while GLES/I/O stay owned.
 * This does not claim native completion or device death. */
bool record_mediacodec_reset_pending(void);
bool record_mediacodec_defer_start(void);
bool record_mediacodec_cancel_start(void);
RETRO_END_DECLS
#endif
