/* XMA raw frames and WMA v1/v2/Pro audio decoding. LGPL-2.1-or-later. */
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
/* Allocation only at creation. No file I/O, threads, resampling or pacing.
 * Borrowed PCM survives until the next decode, reset or free on this context.
 * One context has one owner; independent contexts can decode concurrently. */
int rwma_create(rwma_t **out, const rwma_config *config);
void rwma_free(rwma_t *decoder);
void rwma_reset(rwma_t *decoder);
/* Complete raw frame, including its length prefix, with an arbitrary bit
 * offset. No padding, alignment or encoded-frame copy is required. Trims
 * are reported separately, never applied silently. An error invalidates
 * history: reset before decoding again. A view is cleared on failure. */
int rwma_decode_xma(rwma_t *decoder, const uint8_t *data, size_t bytes,
                    size_t bit_offset, size_t bit_count, rwma_frame *frame);
/* WMA packet window: advance by consumed bytes and call again. If consumed
 * is zero, lend the same input again to obtain the next frame. Input need
 * not survive a positive consumption; decoded output has its own lifetime.
 * A zero-byte input drains overlap once at EOF. Containers/demuxing remain
 * caller-owned. WMA v1/v2 return priming delay in trim_start as metadata;
 * WMA Pro applies stream trims and suppresses its initial priming frame. */
int rwma_decode_packet(rwma_t *decoder, const uint8_t *data, size_t bytes,
                       size_t *consumed, rwma_frame *frame);
RETRO_END_DECLS
#endif
