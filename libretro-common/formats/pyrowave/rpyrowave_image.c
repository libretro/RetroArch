/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <formats/rpyrowave.h>
#include "rpyrowave_internal.h"

struct rpyrowave_decoder
{
   float *planes;
   float *scratch;
   uint8_t *seen;
   size_t pixels;
   uint32_t blocks;
   unsigned width;
   unsigned height;
   unsigned aligned_width;
   unsigned aligned_height;
   unsigned chroma_444;
};

struct rpyrowave_encoder
{
   rpyrowave_decoder_t *workspace;
   int32_t quantized[1024];
};

/* ITU-T T.800 (06/2019), Table F.4 and inverse lifting equation F-7. */
static void rpyrowave_inverse_line(float *line, unsigned length)
{
   static const float lifting[4] =
   { 0.443506852043971f, 0.882911075530934f,
      -0.052980118572961f, -1.586134342059924f };
   unsigned step;
   unsigned i;
   for (i = 0; i < length; i += 2)
   {
      line[i]     *= 1.230174104914001f;
      line[i + 1] /= 1.230174104914001f;
   }
   for (step = 0; step < 4; step++)
      for (i = step & 1; i < length; i += 2)
      {
         unsigned left  = i ? i - 1 : 1;
         unsigned right = i + 1 < length ? i + 1 : length - 2;
         line[i] -= lifting[step] * (line[left] + line[right]);
      }
}

static void rpyrowave_inverse_plane(rpyrowave_decoder_t *decoder,
      float *plane, unsigned chroma)
{
   unsigned stride = decoder->aligned_width;
   int level;
   for (level = 4; level >= (chroma && !decoder->chroma_444 ? 1 : 0); level--)
   {
      unsigned width  = decoder->aligned_width >> level;
      unsigned height = decoder->aligned_height >> level;
      unsigned x;
      unsigned y;
      for (x = 0; x < width; x++)
      {
         for (y = 0; y < height / 2; y++)
         {
            decoder->scratch[y * 2]     = plane[y * stride + x];
            decoder->scratch[y * 2 + 1] = plane[(y + height / 2) * stride + x];
         }
         rpyrowave_inverse_line(decoder->scratch, height);
         for (y = 0; y < height; y++)
            plane[y * stride + x] = decoder->scratch[y];
      }
      for (y = 0; y < height; y++)
      {
         float *row = plane + y * stride;
         for (x = 0; x < width / 2; x++)
         {
            decoder->scratch[x * 2]     = row[x];
            decoder->scratch[x * 2 + 1] = row[x + width / 2];
         }
         rpyrowave_inverse_line(decoder->scratch, width);
         for (x = 0; x < width; x++)
            row[x] = decoder->scratch[x];
      }
   }
}

rpyrowave_decoder_t *rpyrowave_decoder_new(unsigned width, unsigned height,
      unsigned chroma_444, size_t maximum_pixels)
{
   rpyrowave_sequence_t sequence;
   rpyrowave_decoder_t *decoder;
   uint32_t blocks;
   unsigned aligned_width;
   unsigned aligned_height;
   size_t pixels;
   memset(&sequence, 0, sizeof(sequence));
   sequence.width      = width;
   sequence.height     = height;
   sequence.chroma_444 = chroma_444;
   blocks = rpyrowave_block_layout(&sequence, 0, NULL);
   if (!blocks)
      return NULL;
   aligned_width  = (width + 31) & ~31U;
   aligned_height = (height + 31) & ~31U;
   if (aligned_width < 128)
      aligned_width = 128;
   if (aligned_height < 128)
      aligned_height = 128;
   pixels = (size_t)aligned_width * aligned_height;
   if (pixels > maximum_pixels || pixels > (size_t)-1 / (3 * sizeof(float)))
      return NULL;
   decoder = (rpyrowave_decoder_t*)calloc(1, sizeof(*decoder));
   if (!decoder)
      return NULL;
   decoder->planes  = (float*)malloc(pixels * 3 * sizeof(float));
   decoder->scratch = (float*)malloc((aligned_width > aligned_height
            ? aligned_width : aligned_height) * sizeof(float));
   decoder->seen    = (uint8_t*)malloc((blocks + 7) / 8);
   if (!decoder->planes || !decoder->scratch || !decoder->seen)
   {
      rpyrowave_decoder_free(decoder);
      return NULL;
   }
   decoder->width          = width;
   decoder->height         = height;
   decoder->aligned_width  = aligned_width;
   decoder->aligned_height = aligned_height;
   decoder->chroma_444     = chroma_444;
   decoder->pixels         = pixels;
   decoder->blocks         = blocks;
   return decoder;
}

void rpyrowave_decoder_free(rpyrowave_decoder_t *decoder)
{
   if (!decoder)
      return;
   free(decoder->seen);
   free(decoder->scratch);
   free(decoder->planes);
   free(decoder);
}

int rpyrowave_decode_frame(rpyrowave_decoder_t *decoder,
      const uint8_t *data, size_t size, unsigned allow_partial,
      rpyrowave_image_t *image)
{
   rpyrowave_sequence_t sequence;
   size_t offset = 8;
   uint32_t received = 0;
   unsigned component;
   int result;
   if (!decoder || !image)
      return RPYROWAVE_INVALID;
   result = rpyrowave_read_sequence(data, size, &sequence);
   if (result != RPYROWAVE_OK)
      return result;
   if (sequence.width != decoder->width || sequence.height != decoder->height
         || sequence.chroma_444 != decoder->chroma_444)
      return RPYROWAVE_UNSUPPORTED;
   if (sequence.total_blocks > decoder->blocks)
      return RPYROWAVE_INVALID;
   memset(decoder->planes, 0, decoder->pixels * 3 * sizeof(float));
   memset(decoder->seen, 0, (decoder->blocks + 7) / 8);
   while (offset < size)
   {
      rpyrowave_block_t block;
      rpyrowave_location_t location;
      float *target;
      unsigned region_width;
      unsigned region_height;
      const uint8_t *packet = data + offset;
      result = rpyrowave_read_block(packet, size - offset, &block);
      if (result != RPYROWAVE_OK)
         return result;
      offset += block.bytes;
      if (block.sequence != sequence.sequence)
         return RPYROWAVE_INVALID;
      if (!block.ballot || block.index >= decoder->blocks
            || (decoder->seen[block.index / 8] & (1U << (block.index % 8))))
         continue;
      rpyrowave_block_layout(&sequence, block.index, &location);
      target = decoder->planes + decoder->pixels * location.component;
      if (location.band & 1)
         target += location.width;
      if (location.band & 2)
         target += (size_t)location.height * decoder->aligned_width;
      target += (size_t)location.y * decoder->aligned_width + location.x;
      region_width  = location.width - location.x;
      region_height = location.height - location.y;
      if (region_width > 32)
         region_width = 32;
      if (region_height > 32)
         region_height = 32;
      result = rpyrowave_decode_block_region(packet, block.bytes, target,
            decoder->aligned_width, region_width, region_height, 0, &block);
      if (result != RPYROWAVE_OK)
         return result;
      decoder->seen[block.index / 8] |= (uint8_t)(1U << (block.index % 8));
      received++;
   }
   if (received > sequence.total_blocks)
      return RPYROWAVE_INVALID;
   if (!allow_partial && received != sequence.total_blocks)
      return RPYROWAVE_TRUNCATED;
   for (component = 0; component < 3; component++)
   {
      unsigned x;
      unsigned y;
      unsigned shift = component && !sequence.chroma_444 ? 1 : 0;
      float *plane   = decoder->planes + component * decoder->pixels;
      rpyrowave_inverse_plane(decoder, plane, component != 0);
      for (y = 0; y < decoder->height >> shift; y++)
         for (x = 0; x < decoder->width >> shift; x++)
         {
            float value = plane[y * decoder->aligned_width + x] + 0.5f;
            plane[y * decoder->aligned_width + x] =
               value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
         }
      image->planes[component] = plane;
   }
   image->stride   = decoder->aligned_width;
   image->sequence = sequence;
   return RPYROWAVE_OK;
}

static void rpyrowave_forward_line(float *line, unsigned length)
{
   static const float lifting[4] =
   { -1.586134342059924f, -0.052980118572961f,
      0.882911075530934f, 0.443506852043971f };
   unsigned step;
   unsigned i;
   for (step = 0; step < 4; step++)
      for (i = 1 - (step & 1); i < length; i += 2)
      {
         unsigned left  = i ? i - 1 : 1;
         unsigned right = i + 1 < length ? i + 1 : length - 2;
         line[i] += lifting[step] * (line[left] + line[right]);
      }
   for (i = 0; i < length; i += 2)
   {
      line[i]     /= 1.230174104914001f;
      line[i + 1] *= 1.230174104914001f;
   }
}

static void rpyrowave_forward_plane(rpyrowave_decoder_t *workspace,
      float *plane, unsigned chroma)
{
   unsigned level;
   unsigned stride = workspace->aligned_width;
   for (level = chroma && !workspace->chroma_444 ? 1 : 0; level < 5; level++)
   {
      unsigned width  = workspace->aligned_width >> level;
      unsigned height = workspace->aligned_height >> level;
      unsigned x;
      unsigned y;
      for (y = 0; y < height; y++)
      {
         float *row = plane + y * stride;
         for (x = 0; x < width; x++)
            workspace->scratch[x] = row[x];
         rpyrowave_forward_line(workspace->scratch, width);
         for (x = 0; x < width / 2; x++)
         {
            row[x]             = workspace->scratch[x * 2];
            row[x + width / 2] = workspace->scratch[x * 2 + 1];
         }
      }
      for (x = 0; x < width; x++)
      {
         for (y = 0; y < height; y++)
            workspace->scratch[y] = plane[y * stride + x];
         rpyrowave_forward_line(workspace->scratch, height);
         for (y = 0; y < height / 2; y++)
         {
            plane[y * stride + x] = workspace->scratch[y * 2];
            plane[(y + height / 2) * stride + x] = workspace->scratch[y * 2 + 1];
         }
      }
   }
}

rpyrowave_encoder_t *rpyrowave_encoder_new(unsigned width, unsigned height,
      unsigned chroma_444, size_t maximum_pixels)
{
   rpyrowave_encoder_t *encoder = (rpyrowave_encoder_t*)malloc(sizeof(*encoder));
   if (!encoder)
      return NULL;
   encoder->workspace = rpyrowave_decoder_new(width, height, chroma_444,
         maximum_pixels);
   if (!encoder->workspace)
   {
      free(encoder);
      return NULL;
   }
   return encoder;
}

void rpyrowave_encoder_free(rpyrowave_encoder_t *encoder)
{
   if (!encoder)
      return;
   rpyrowave_decoder_free(encoder->workspace);
   free(encoder);
}

static size_t rpyrowave_encode_candidate(rpyrowave_encoder_t *encoder,
      const rpyrowave_sequence_t *sequence, unsigned quant_code,
      uint8_t *data, size_t capacity, uint32_t *active)
{
   rpyrowave_decoder_t *workspace = encoder->workspace;
   size_t offset = 8;
   uint32_t index;
   double scale = ldexp((double)(8 + (quant_code & 7)),
         -1 - (int)(quant_code >> 3));
   *active = 0;
   for (index = 0; index < workspace->blocks; index++)
   {
      rpyrowave_location_t location;
      const float *source;
      unsigned x;
      unsigned y;
      size_t bytes = 0;
      int result;
      rpyrowave_block_layout(sequence, index, &location);
      source = workspace->planes + location.component * workspace->pixels;
      if (location.band & 1)
         source += location.width;
      if (location.band & 2)
         source += (size_t)location.height * workspace->aligned_width;
      for (y = 0; y < 32; y++)
         for (x = 0; x < 32; x++)
         {
            int32_t quantized = 0;
            if (location.x + x < location.width && location.y + y < location.height)
            {
               float value = source[(location.y + y) * workspace->aligned_width
                  + location.x + x];
               double magnitude = floor(fabs((double)value) / scale);
               if (magnitude > 262143.0)
                  return (size_t)-1;
               quantized = (int32_t)magnitude;
               if (value < 0.0f)
                  quantized = -quantized;
            }
            encoder->quantized[y * 32 + x] = quantized;
         }
      result = rpyrowave_encode_block(encoder->quantized, quant_code,
            index, sequence->sequence, data ? data + offset : NULL,
            data ? capacity - offset : 0, &bytes);
      if (result != RPYROWAVE_OK
            && (data || result != RPYROWAVE_BUFFER_TOO_SMALL))
         return (size_t)-1;
      if (bytes > (size_t)-1 - offset)
         return (size_t)-1;
      offset += bytes;
      if (bytes)
         (*active)++;
   }
   return offset;
}

int rpyrowave_encode_frame(rpyrowave_encoder_t *encoder,
      const rpyrowave_image_t *image, size_t maximum_bytes,
      uint8_t *data, size_t capacity, size_t *written)
{
   rpyrowave_decoder_t *workspace;
   const rpyrowave_sequence_t *sequence;
   unsigned component;
   unsigned lo = 0;
   /* Finer steps lose mantissa bits in the reference's FP16 dequantizer. */
   unsigned hi = 191;
   unsigned selected = 0;
   unsigned quant_code;
   uint32_t active;
   uint32_t first;
   uint32_t second;
   size_t required;
   if (!encoder || !image || !written || maximum_bytes < 8)
      return RPYROWAVE_INVALID;
   workspace = encoder->workspace;
   sequence  = &image->sequence;
   if (sequence->width != workspace->width || sequence->height != workspace->height
         || sequence->chroma_444 != workspace->chroma_444)
      return RPYROWAVE_UNSUPPORTED;
   if (image->stride < workspace->width
         || image->stride > (size_t)-1 / (workspace->height * sizeof(float))
         || sequence->sequence > 7
         || sequence->primaries_bt2020 > 1 || sequence->transfer_pq > 1
         || sequence->transform_bt2020 > 1 || sequence->limited_range > 1
         || sequence->chroma_left > 1)
      return RPYROWAVE_INVALID;
   for (component = 0; component < 3; component++)
   {
      unsigned x;
      unsigned y;
      unsigned shift  = component && !workspace->chroma_444 ? 1 : 0;
      unsigned width  = workspace->width >> shift;
      unsigned height = workspace->height >> shift;
      float *plane    = workspace->planes + component * workspace->pixels;
      if (!image->planes[component])
         return RPYROWAVE_INVALID;
      for (y = 0; y < workspace->aligned_height >> shift; y++)
      {
         const float *row = image->planes[component]
            + (y < height ? y : height - 1) * image->stride;
         for (x = 0; x < workspace->aligned_width >> shift; x++)
         {
            float value = row[x < width ? x : width - 1];
            if (!(value >= 0.0f && value <= 1.0f))
               return RPYROWAVE_INVALID;
            plane[y * workspace->aligned_width + x] = value - 0.5f;
         }
      }
      rpyrowave_forward_plane(workspace, plane, component != 0);
   }
   /* Rank orders mantissas backwards so finer ranks have smaller steps. */
   while (lo <= hi)
   {
      unsigned rank = lo + (hi - lo) / 2;
      quant_code = (rank & ~7U) | (7 - (rank & 7));
      required = rpyrowave_encode_candidate(encoder, sequence, quant_code,
            NULL, 0, &active);
      if (required <= maximum_bytes && required != (size_t)-1)
      {
         selected = rank;
         lo       = rank + 1;
      }
      else
      {
         if (!rank)
            break;
         hi = rank - 1;
      }
   }
   quant_code = (selected & ~7U) | (7 - (selected & 7));
   required = rpyrowave_encode_candidate(encoder, sequence, quant_code,
         NULL, 0, &active);
   if (required > maximum_bytes || required == (size_t)-1)
      required = 8;
   *written = required;
   if (capacity < required)
      return RPYROWAVE_BUFFER_TOO_SMALL;
   if (!data)
      return RPYROWAVE_INVALID;
   active = 0;
   if (required > 8 && rpyrowave_encode_candidate(encoder, sequence, quant_code,
            data, capacity, &active) != required)
      return RPYROWAVE_INVALID;
   first  = sequence->width - 1;
   first |= (sequence->height - 1) << 14;
   first |= sequence->sequence << 28;
   first |= 0x80000000U;
   second = active | (sequence->chroma_444 << 26)
      | (sequence->primaries_bt2020 << 27) | (sequence->transfer_pq << 28)
      | (sequence->transform_bt2020 << 29) | (sequence->limited_range << 30)
      | (sequence->chroma_left << 31);
   for (component = 0; component < 4; component++)
   {
      data[component]     = (uint8_t)(first >> (component * 8));
      data[component + 4] = (uint8_t)(second >> (component * 8));
   }
   return RPYROWAVE_OK;
}
