/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_MEDIACODEC_STREAM_CONFIG_H
#define RECORD_MEDIACODEC_STREAM_CONFIG_H
#include "record_mediacodec_encoder.h"
#include <formats/rflv_mux.h>
RETRO_BEGIN_DECLS

enum record_mediacodec_stream_codec
{
   RECORD_MC_STREAM_AVC, RECORD_MC_STREAM_HEVC, RECORD_MC_STREAM_AV1
};
typedef struct record_mediacodec_stream_config
{
   video_color_metadata_t color;
   size_t max_packet_size;
   enum record_mediacodec_stream_codec codec;
   unsigned bit_depth;
} record_mediacodec_stream_config_t;
/* Cold I/O worker only, after all enabled encoders report FORMAT. Copies setup
 * data into the shared mux; retains neither encoder nor native format/CSD.
 * AVC/HEVC use push_annexb; AV1 uses ordinary push with sized OBUs. AAC is raw.
 * Both tracks use MediaCodec packet pts_us (timescale 1000000); no rebasing.
 * Caller owns the fresh mux. Packet/output lifetimes are a separate contract. */
rflv_mux_t *record_mediacodec_stream_mux_new(record_mediacodec_encoder_t *video,
      record_mediacodec_encoder_t *audio, const record_mediacodec_stream_config_t *config);
RETRO_END_DECLS
#endif
