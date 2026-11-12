/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_STREAM_MEDIACODEC_H
#define RECORD_STREAM_MEDIACODEC_H
#include "record_mediacodec_worker.h"
#include "record_mediacodec_stream_config.h"
#include <net/net_rtmp_client.h>
RETRO_BEGIN_DECLS
typedef struct record_stream_mediacodec record_stream_mediacodec_t;
/* Cold I/O owner. Copies config; borrows encoders and client until free.
 * The controller must establish stream priming/source epochs separately;
 * file calibration prefixes and MP4 edits must not be used for this output.
 * No producer stop/free until worker free has retired this output. */
record_stream_mediacodec_t *record_stream_mediacodec_new(
      record_mediacodec_encoder_t *video, record_mediacodec_encoder_t *audio,
      net_rtmp_client_t *client, const record_mediacodec_stream_config_t *config);
const record_mediacodec_output_ops_t *record_stream_mediacodec_ops(void);
/* One bounded network/mux action, before worker_step. -1 error, 0 BUSY,
 * 1 progress, 2 completed. Caller schedules returned socket interests and
 * owns setup/drain deadlines. No waits, internal clocks or packet allocation. */
int record_stream_mediacodec_step(record_stream_mediacodec_t *stream,
      uint32_t receive_time, net_rtmp_client_view_t *wait);
/* After worker_free succeeds. Retires cancellation before freeing mux bytes;
 * false requires another retry. Does not destroy borrowed client/encoders. */
bool record_stream_mediacodec_free(record_stream_mediacodec_t *stream);
RETRO_END_DECLS
#endif
