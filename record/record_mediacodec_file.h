/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MEDIACODEC_FILE_H
#define RECORD_MEDIACODEC_FILE_H
#include <boolean.h>
#include <retro_common_api.h>
#include <stdint.h>
RETRO_BEGIN_DECLS
typedef struct record_mediacodec_file record_mediacodec_file_t;
/* Cold I/O worker only: creation and final sync/close can block this worker.
 * Creates a new UTF-8 native path exclusively, never truncates or unlinks.
 * Existing paths (including symlinks) are refused. error receives errno. */
record_mediacodec_file_t *record_mediacodec_file_new(
      const char *path, int *error);
/* Caller must have created a NEW empty file exclusively, with no other writer
 * or shared descriptor. This also admits a fresh detached document-provider fd.
 * Verifies regular/read-write/non-append/empty/position-zero on Android, and
 * disables inheritance. Success sets *owned_fd=-1; failure takes nothing.
 * Descriptor checks alone cannot prove exclusive creation provenance. */
record_mediacodec_file_t *record_mediacodec_file_adopt_fresh(
      int *owned_fd, int *error);
/* Borrowed until close; destroy the muxer and stop all users first. */
int record_mediacodec_file_descriptor(const record_mediacodec_file_t *file);
/* Cold I/O owner, after destroying the mux. Rewrites only trailing moov. */
bool record_mediacodec_file_audio_edit(record_mediacodec_file_t *file,
      uint64_t skip, uint64_t frames, unsigned roll, int *error);
/* Cold I/O owner, consumes file even on error. complete requests final fsync
 * after successful mux finalization. Closes once, including on EINTR: never
 * retry a descriptor number that may already have been reused. */
bool record_mediacodec_file_close(
      record_mediacodec_file_t *file, bool complete, int *error);
RETRO_END_DECLS
#endif
