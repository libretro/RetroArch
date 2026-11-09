/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_OUTPUT_VULKAN_H
#define RECORD_GPU_OUTPUT_VULKAN_H
#include "record_gpu_worker_vulkan.h"
#include "record_mux_mp4.h"
RETRO_BEGIN_DECLS

enum record_gpu_output_vulkan_result
{
   RECORD_VULKAN_OUTPUT_ERROR = -1,
   RECORD_VULKAN_OUTPUT_BUSY,
   RECORD_VULKAN_OUTPUT_PROGRESS,
   RECORD_VULKAN_OUTPUT_END
};
enum record_gpu_output_vulkan_error
{
   RECORD_VULKAN_OUTPUT_ERROR_NONE,
   RECORD_VULKAN_OUTPUT_ERROR_VIDEO,
   RECORD_VULKAN_OUTPUT_ERROR_AUDIO,
   RECORD_VULKAN_OUTPUT_ERROR_MUX,
   RECORD_VULKAN_OUTPUT_ERROR_FILE,
   RECORD_VULKAN_OUTPUT_ERROR_ABORTED
};
typedef struct record_gpu_output_vulkan record_gpu_output_vulkan_t;
typedef struct record_gpu_output_vulkan_config
{
   record_mp4_config_t video;
   unsigned audio_rate, audio_kbps;
   uint32_t audio_layout;
   size_t audio_fifo_frames;
} record_gpu_output_vulkan_config_t;

/* Construct/service/destroy on an I/O worker, never main/render/audio threads.
 * Borrows the GPU worker; caller owns capture, encoder and their drain/free.
 * PCM starts at zero; align video timestamps in the mux timescale. Audio is optional (rate=0), otherwise native
 * AC-3 at 32/44.1/48 kHz in its frontend speaker-mask order. No resampling.
 * video.ac3_header must be NULL. Creates a new UTF-8 named file. No scheduler
 * is installed here; task handlers may execute on the main thread. */
record_gpu_output_vulkan_t *
record_gpu_output_vulkan_new(record_gpu_worker_vulkan_t *worker,
                             const record_gpu_output_vulkan_config_t *config,
                             const char *path);
/* One PCM producer, signed interleaved 16-bit samples. Copies the whole batch
 * or nothing; BUSY never drops audio. No allocation, waits or logging. */
int record_gpu_output_vulkan_audio(record_gpu_output_vulkan_t *output,
                                   const int16_t *samples, size_t frames);
/* Audio producer closes after its final push; idempotent. A short final AC-3
 * block is zero-padded to 1536 samples; at most 1535 padded samples are stored.
 * Close the video producer through its existing capture/queue owner too. */
bool record_gpu_output_vulkan_audio_finish(record_gpu_output_vulkan_t *output);
/* Bounded step: file completion/submission, one GPU worker step, or one audio
 * block. Disk backpressure holds mux output, then the GPU input pool fills.
 * END requires video EOS, audio close/drain, mux acknowledgement and successful
 * file flush/close. Writes/flush/close may block this dedicated I/O worker. ERROR stops feeding
 * the borrowed GPU worker: its owner must stop/drain/free it separately.
 * After ERROR/abort, keep calling step until any file borrow is retired (BUSY
 * means in flight), close/quiesce the PCM producer, then free. */
int record_gpu_output_vulkan_step(record_gpu_output_vulkan_t *output);
/* Worker-only, after BUSY: true requires a scheduled retry;
 * false awaits producer input/close.
 * Includes pending file cancellation; no service or ownership changes. */
bool record_gpu_output_vulkan_retry_needed(const record_gpu_output_vulkan_t *p);
/* Worker-only cancellation request, no wait; may need a later retry. */
bool record_gpu_output_vulkan_abort(record_gpu_output_vulkan_t *output);
enum record_gpu_output_vulkan_error
record_gpu_output_vulkan_error(const record_gpu_output_vulkan_t *output);
/* Preserved errno, I/O-worker only. */
int record_gpu_output_vulkan_file_error(const record_gpu_output_vulkan_t *output);
/* Refuses active output, an open PCM producer, or an outstanding file borrow.
 * Caller must ensure neither producer nor worker is inside an API call. */
bool record_gpu_output_vulkan_free(record_gpu_output_vulkan_t *output);
RETRO_END_DECLS
#endif
