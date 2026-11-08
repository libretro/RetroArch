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
int rwma_decode_packet(rwma_t *decoder, const uint8_t *data, size_t bytes,
                       size_t *consumed, rwma_frame *out);
RETRO_END_DECLS
#endif
