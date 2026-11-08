/* Copyright (C) 2026 The RetroArch team. MIT license, see COPYING.MIT. */
#ifndef LIBRETRO_RWMA_H
#define LIBRETRO_RWMA_H
#include <retro_common_api.h>
#include <stddef.h>
#include <stdint.h>
RETRO_BEGIN_DECLS
#define RWMA_OK 0
#define RWMA_BAD_DATA (-1)
#define RWMA_UNSUPPORTED (-2)
#define RWMA_NO_MEMORY (-3)
#define RWMA_XMA_RAW 1
#define RWMA_PRO 2
#define RWMA_V1 3
#define RWMA_V2 4
typedef struct rwma_t rwma_t;
typedef struct rwma_config
{
   const uint8_t *extra;
   size_t extra_size;
   unsigned codec, sample_rate, channels, block_align, bit_rate;
} rwma_config;
typedef struct rwma_frame
{
   const float *planes[8];
   unsigned samples, channels, sample_rate, trim_start, trim_end;
} rwma_frame;
typedef struct rwma_xma_state
{
   uint32_t version, sample_rate, channels, pending, drained, failed;
   uint32_t previous[2];
} rwma_xma_state;
typedef struct rwma_xma_view
{
   rwma_xma_state state;
   const float *history[2];
   size_t history_samples[2];
} rwma_xma_view;
/* Reuse the raw context at 24/32/44.1/48 kHz, up to its creation channel
 * count. A changed profile resets history; the same profile preserves it.
 * No allocation; invalid profiles leave the decoder unchanged. */
int rwma_xma_configure(rwma_t *decoder, unsigned sample_rate, unsigned channels);
/* Quiescent raw-XMA contexts only. Capture borrows history until the next
 * decode/reset/restore/free. Restore validates before changing the destination,
 * allocates nothing, and copies directly into its final overlap storage.
 * A source may be the destination's exact captured view, but may not otherwise
 * overlap its storage. Serialize metadata and samples, never the pointers. */
int rwma_xma_capture(const rwma_t *decoder, rwma_xma_view *out);
int rwma_xma_restore(rwma_t *decoder, const rwma_xma_view *source);
/* One exclusive owner per context. Decode/reset allocate nothing. Output
 * borrows decoder storage until its next decode/reset/free. No device I/O,
 * resampling, mixing, sleeping or playback pacing. */
int rwma_create(rwma_t **out, const rwma_config *config);
/* WAVEFORMATEX for WMA1, WMA2 or WMA Pro. extra borrows this span. */
int rwma_parse_wave(rwma_config *config, const uint8_t *data, size_t bytes);
void rwma_free(rwma_t *decoder);
void rwma_reset(rwma_t *decoder);
int rwma_decode_xma(rwma_t *decoder, const uint8_t *data, size_t bytes,
                    size_t bit_offset, size_t bit_count, rwma_frame *out);
/* Final overlap tail, borrowed once. Reset before decoding again. */
int rwma_drain_xma(rwma_t *decoder, rwma_frame *out);
int rwma_decode_packet(rwma_t *decoder, const uint8_t *data, size_t bytes,
                       size_t *consumed, rwma_frame *out);
RETRO_END_DECLS
#endif
