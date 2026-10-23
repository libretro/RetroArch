/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MUX_MEDIACODEC_H
#define RECORD_MUX_MEDIACODEC_H
#include "record_mediacodec_encoder.h"
RETRO_BEGIN_DECLS
typedef struct record_mux_mediacodec record_mux_mediacodec_t;
/* One recording I/O worker. fd is a new, empty, seekable output opened by
 * the caller, which retains it until free. audio enables one AAC track. */
record_mux_mediacodec_t *record_mux_mediacodec_new(int fd, bool audio);
/* Native AMediaFormat borrowed only for this call; bind before start.
 * Accept H.264/HEVC/AV1 and AAC formats subject to native mux admission. */
bool record_mux_mediacodec_track(
      record_mux_mediacodec_t *mux, bool audio, void *format);
/* -1 sticky error, 0 waiting for all formats/track ends, 1 success.
 * Successful start and finish are idempotent. Native I/O is worker-only. */
int record_mux_mediacodec_start(record_mux_mediacodec_t *mux);
/* Packet borrowed only during this call. Preserve encoder output order and
 * the source epoch; no implicit timestamp rebasing. Codec config is skipped.
 * Packet EOS does not supply duration: end() takes the source end timestamp. */
bool record_mux_mediacodec_write(record_mux_mediacodec_t *mux, bool audio,
      const record_mediacodec_packet_t *packet);
bool record_mux_mediacodec_end(
      record_mux_mediacodec_t *mux, bool audio, int64_t end_us);
int record_mux_mediacodec_finish(record_mux_mediacodec_t *mux);
/* Worker-only cancellation also closes the native mux; fd remains caller-owned.
 */
void record_mux_mediacodec_free(record_mux_mediacodec_t *mux);
RETRO_END_DECLS
#endif
