/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Private builtin-player GPU sink. Installation is allowed only while
 * unloaded, on the core owner thread. No consumer is installed by default. */
#ifndef MEDIA_HW_H
#define MEDIA_HW_H
#include <boolean.h>
#include <retro_common_api.h>
#include <formats/video_hw_frame.h>
RETRO_BEGIN_DECLS
/* NULL queries consumer readiness before and during hardware playback.
 * true transfers the lease; false leaves it with the core for retry.
 * On false, issue no GPU reads or completion signal.
 * The consumer never blocks and must read/signal or abandon accepted
 * leases. It queues presentation for the following NULL video refresh.
 * Copy the descriptor before returning true; never retain its pointer.
 * Its user data must outlive playback and accepted leases. */
typedef bool (*media_hw_refresh_t)(void *user, const video_hw_frame_t *frame);
#ifdef RARCH_INTERNAL
void libretro_media_video_hw_ready(void);
void libretro_media_suspend_video_hw(void);
bool libretro_media_set_video_hw_refresh(media_hw_refresh_t cb, void *user);
#else
void media_video_hw_ready(void);
void media_suspend_video_hw(void);
bool media_set_video_hw_refresh(media_hw_refresh_t cb, void *user);
#endif
RETRO_END_DECLS
#endif
