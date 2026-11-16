/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_OUTPUT_CONFIG_H
#define RECORD_GPU_OUTPUT_CONFIG_H
#include "record_mux_mp4.h"
#include <formats/video_color.h>
enum record_gpu_output_result
{
   RECORD_GPU_OUTPUT_ERROR = -1, RECORD_GPU_OUTPUT_BUSY,
   RECORD_GPU_OUTPUT_PROGRESS, RECORD_GPU_OUTPUT_END
};
enum record_gpu_output_error
{
   RECORD_GPU_OUTPUT_ERROR_NONE, RECORD_GPU_OUTPUT_ERROR_VIDEO,
   RECORD_GPU_OUTPUT_ERROR_AUDIO, RECORD_GPU_OUTPUT_ERROR_MUX,
   RECORD_GPU_OUTPUT_ERROR_FILE, RECORD_GPU_OUTPUT_ERROR_ABORTED,
   RECORD_GPU_OUTPUT_ERROR_NETWORK
};
struct net_rtmp_client;
typedef struct record_gpu_output_config
{
   record_mp4_config_t video;
   /* Coded source metadata for HEVC streaming, never display defaults.
    * Session setup replaces this from its encoder configuration. */
   video_color_metadata_t stream_color;
   /* I/O-owner-only READY client; NULL selects file output. */
   struct net_rtmp_client *stream_client;
   unsigned audio_rate, audio_kbps;
   uint32_t audio_layout;
   size_t audio_fifo_frames;
   /* Shared streaming consumer: cap source PCM at accepted coded video time. */
   bool stream_audio_follow_video;
} record_gpu_output_config_t;

#endif
