/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MP4_AUDIO_EDIT_H
#define RECORD_MP4_AUDIO_EDIT_H
#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
/* Cold file-finalization worker only. One complete native mux moov, one AAC
 * track, no prior audio edit/roll group. Preserve media bytes and chunk offsets.
 * skip/frames use mdhd audio ticks; roll counts prerequisite AAC packets.
 * Zero source frames replace the audio track with padding, preserving offsets.
 * Delay must be established by the caller, never inferred here.
 * NULL output queries the validated size. Input/output must not overlap.
 * No allocation or I/O. Failure sets written to zero; discard partial output. */
bool record_mp4_audio_edit(const void *moov, size_t bytes,
      uint64_t skip, uint64_t frames, unsigned roll,
      void *output, size_t capacity, size_t *written);
RETRO_END_DECLS
#endif
