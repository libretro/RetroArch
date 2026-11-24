/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBRETRO_COMMON_RPYROWAVE_H
#define LIBRETRO_COMMON_RPYROWAVE_H

#include <stddef.h>
#include <stdint.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rpyrowave_result
{
   RPYROWAVE_OK = 0,
   RPYROWAVE_TRUNCATED = -1,
   RPYROWAVE_INVALID = -2,
   RPYROWAVE_UNSUPPORTED = -3,
   RPYROWAVE_BUFFER_TOO_SMALL = -4
};

typedef struct rpyrowave_sequence
{
   uint32_t total_blocks;
   unsigned width;
   unsigned height;
   unsigned sequence;
   unsigned chroma_444;
   unsigned primaries_bt2020;
   unsigned transfer_pq;
   unsigned transform_bt2020;
   unsigned limited_range;
   unsigned chroma_left;
} rpyrowave_sequence_t;

typedef struct rpyrowave_block
{
   size_t bytes;
   uint32_t index;
   unsigned sequence;
   unsigned quant_code;
   unsigned ballot;
} rpyrowave_block_t;

typedef struct rpyrowave_location
{
   unsigned component;
   unsigned level;
   unsigned band;
   unsigned x;
   unsigned y;
   unsigned width;
   unsigned height;
} rpyrowave_location_t;

typedef struct rpyrowave_file_header
{
   rpyrowave_sequence_t sequence;
   uint32_t frame_rate_num;
   uint32_t frame_rate_den;
   unsigned reference_bit_depth;
} rpyrowave_file_header_t;

typedef struct rpyrowave_decoder rpyrowave_decoder_t;
typedef struct rpyrowave_encoder rpyrowave_encoder_t;

typedef struct rpyrowave_image
{
   const float *planes[3];
   size_t stride;
   rpyrowave_sequence_t sequence;
} rpyrowave_image_t;

/* Input is borrowed for the call. Outputs are unchanged on failure. */
int rpyrowave_read_sequence(const uint8_t *data, size_t size,
      rpyrowave_sequence_t *sequence);
int rpyrowave_read_block(const uint8_t *data, size_t size,
      rpyrowave_block_t *block);
int rpyrowave_read_file_header(const uint8_t *data, size_t size,
      rpyrowave_file_header_t *header);

/* Returns the number of possible blocks, or zero for invalid dimensions.
 * location may be NULL when only the block count is needed. */
uint32_t rpyrowave_block_layout(const rpyrowave_sequence_t *sequence,
      uint32_t index, rpyrowave_location_t *location);

/* Writes 1024 dequantized coefficients in row-major order. The caller
 * owns the output; it must not overlap the input. No allocation occurs. */
int rpyrowave_decode_block(const uint8_t *data, size_t size,
      float *coefficients, rpyrowave_block_t *block);

/* Quantized, signed row-major input, magnitude at most 262143. Group
 * dequantization scale is 1/4. Empty blocks produce zero bytes. On
 * BUFFER_TOO_SMALL, written gives the required capacity; no bytes change.
 * Input/output must not overlap. */
int rpyrowave_encode_block(const int32_t *coefficients,
      unsigned quant_code, uint32_t index, unsigned sequence,
      uint8_t *data, size_t capacity, size_t *written);

/* CPU/offline path. Allocate once per sequence, with a caller-selected
 * pixel limit. Decoding allocates nothing. Output is borrowed until the
 * next decode or free; planes are normalized Y/Cb/Cr, not RGB. */
rpyrowave_decoder_t *rpyrowave_decoder_new(unsigned width, unsigned height,
      unsigned chroma_444, size_t maximum_pixels);
void rpyrowave_decoder_free(rpyrowave_decoder_t *decoder);
int rpyrowave_decode_frame(rpyrowave_decoder_t *decoder,
      const uint8_t *data, size_t size, unsigned allow_partial,
      rpyrowave_image_t *image);

/* CPU/offline encoder. Input planes are normalized Y/Cb/Cr with a common
 * stride, borrowed for the call. All workspace is allocated at creation.
 * A global quantizer is chosen to obey maximum_bytes, including the
 * 8-byte frame header. written reports capacity on BUFFER_TOO_SMALL.
 * Output must not overlap input. Sequence/colour fields come from image. */
rpyrowave_encoder_t *rpyrowave_encoder_new(unsigned width, unsigned height,
      unsigned chroma_444, size_t maximum_pixels);
void rpyrowave_encoder_free(rpyrowave_encoder_t *encoder);
int rpyrowave_encode_frame(rpyrowave_encoder_t *encoder,
      const rpyrowave_image_t *image, size_t maximum_bytes,
      uint8_t *data, size_t capacity, size_t *written);

RETRO_END_DECLS

#endif
