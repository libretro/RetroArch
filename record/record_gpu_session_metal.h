/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_SESSION_METAL_H
#define RECORD_GPU_SESSION_METAL_H
#include "record_avfoundation_session.h"
#include "record_gpu_capture_metal.h"
#include "record_mux_avfoundation.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_session_metal_config
{
   unsigned width,height,slots,bitrate,key_interval,timescale,bit_depth,mode;
   bool hevc;
} record_gpu_session_metal_config_t;
enum record_gpu_session_metal_startup
{
   RECORD_METAL_STARTUP_NONE, RECORD_METAL_STARTUP_CONFIG, RECORD_METAL_STARTUP_ENCODER,
   RECORD_METAL_STARTUP_POOL, RECORD_METAL_STARTUP_CAPTURE, RECORD_METAL_STARTUP_SESSION,
   RECORD_METAL_STARTUP_UNSUPPORTED
};
/* macOS cold I/O worker only. Selects an explicit advertised hardware encoder
 * ID, preferring the supplied Metal GPU over an encoder with no advertised
 * GPU binding. Rejects software and encoders bound to a different GPU; checks
 * the actual session ID and hardware flag. Constructs
 * the aligned encoder, fixed pixel pool, capture and owned A/V session.
 * output.video_format is NULL; first encoded sample binds the actual format.
 * On success producer outputs are borrowed only until video_finish; session
 * retirement owns every created component. NULL leaves outputs NULL and
 * destroys all partial cold resources. Device is borrowed during this call.
 * Native encoder invalidation may wait here on startup rollback. */
#if defined(__APPLE__) && defined(__clang__)
__attribute__((availability(macos,introduced=10.14)))
#endif
record_avfoundation_session_t *record_gpu_session_metal_new(void *device,
      const record_gpu_session_metal_config_t *config,
      const record_mux_avfoundation_config_t *output,size_t audio_capacity_bytes,
      const char *path,record_gpu_capture_metal_t **capture,record_gpu_queue_t **queue,
      enum record_gpu_session_metal_startup *stage,int32_t *native_status);
RETRO_END_DECLS
#endif
