/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */
#ifndef LIBRETRO_COMMON_RPYROWAVE_GPU_H
#define LIBRETRO_COMMON_RPYROWAVE_GPU_H

#include <stdint.h>
#include <stddef.h>
#include <formats/rpyrowave.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rpyrowave_color_texture
{
   RPYROWAVE_COLOR_CODE = 0,
   RPYROWAVE_COLOR_SCRGB,
   RPYROWAVE_COLOR_SDR709
};

typedef struct rpyrowave_color_parameters
{
   uint32_t image_width;
   uint32_t image_height;
   uint32_t width;
   uint32_t height;
   uint32_t chroma_444;
   uint32_t chroma_left;
   uint32_t inverse;
   uint32_t flip_y;
   uint32_t texture_x;
   uint32_t texture_y;
   uint32_t texture_mode;
   uint32_t colorimetry;
   float red;
   float blue;
   float chroma_center;
   float luma_offset;
   float luma_scale;
   float chroma_scale;
   float padding[2];
} rpyrowave_color_parameters_t;

/* Default COLOR_CODE views carry nonlinear values in the signalled
 * primaries/transfer, without automatic sRGB conversion. PQ stays PQ.
 * Set texture_mode to COLOR_SCRGB for linear
 * BT.709 scRGB (1 = 80 nits): conversion to/from the signalled primaries
 * and sRGB/PQ transfer occurs in the color kernel. SDR white maps to 1.
 * Decode preserves negative and above-one scRGB values.
 * COLOR_SDR709 is decode-only: convert to linear BT.709, divide by the
 * greater of 1 and the largest RGB component, then clamp and encode sRGB.
 * SDR reference white is 80 nits. This is an explicit SDR presentation
 * policy; use COLOR_SCRGB to retain HDR luminance and out-of-gamut values.
 * colorimetry is initialized from the sequence; do not override it independently.
 * Texture bounds include the crop at x/y; flip_y reverses that crop.
 * Encode dispatch: ceil(image_width/8),ceil(image_height/8),3.
 * Decode dispatch: ceil(width/8),ceil(height/8),1. RGB alpha is 1.
 * Chroma uses a centred box or left-sited tent horizontally and a
 * centred box vertically; decode uses bilinear reconstruction.
 * Atlas values include the codec's -0.5 shift, with replicated padding.
 * No resource allocation, recording or waits occur here. */
int rpyrowave_gpu_color_parameters(const rpyrowave_sequence_t *sequence,
      unsigned bit_depth, unsigned inverse, unsigned flip_y,
      unsigned texture_width, unsigned texture_height,
      unsigned x, unsigned y, rpyrowave_color_parameters_t *parameters);

/* Optional nearest-neighbour full-texture encode scaling. The sequence and
 * wire dimensions stay fixed while a frontend swaps/resizes presentation.
 * Does not change the decoder or the standard cropped encode operation. */
int rpyrowave_gpu_color_parameters_ex(const rpyrowave_sequence_t *sequence,
      unsigned bit_depth, unsigned inverse, unsigned flip_y,
      unsigned texture_width, unsigned texture_height,
      unsigned x, unsigned y, unsigned scale_source, rpyrowave_color_parameters_t *parameters);

typedef struct rpyrowave_dwt_parameters
{
   uint32_t image_width;
   uint32_t image_height;
   uint32_t level;
   uint32_t axis;
   uint32_t phase;
   uint32_t inverse;
   uint32_t chroma_444;
   uint32_t reserved;
} rpyrowave_dwt_parameters_t;

/* Parameters are borrowed only during record. Each dispatch reads source
 * and writes the other of two atlas buffers; record must add the storage
 * dependency before the next dispatch. No GPU waits occur here. Failure
 * leaves emitted work with the owner, which must retire it before reusing
 * either buffer. Buffer 0 holds a successful result. */
typedef int (*rpyrowave_dwt_record_t)(void *userdata,
      const rpyrowave_dwt_parameters_t *parameters, unsigned source,
      unsigned groups_x, unsigned groups_y, unsigned groups_z);

int rpyrowave_dwt_record(unsigned aligned_width, unsigned aligned_height,
      unsigned chroma_444, unsigned inverse,
      rpyrowave_dwt_record_t record, void *userdata);

typedef struct rpyrowave_gpu_block
{
   uint32_t origin;
   uint32_t width;
   uint32_t height;
   uint32_t index;
} rpyrowave_gpu_block_t;

typedef struct rpyrowave_gpu_layout
{
   uint32_t image_width;
   uint32_t image_height;
   uint32_t block_count;
   uint32_t wire_offset;
   uint32_t workspace_bytes;
   uint32_t maximum_bytes;
   uint32_t width;
   uint32_t height;
   uint32_t chroma_444;
} rpyrowave_gpu_layout_t;

enum rpyrowave_packet_operation
{
   RPYROWAVE_PACKET_INITIALIZE = 0,
   RPYROWAVE_PACKET_COST = 1,
   RPYROWAVE_PACKET_SEARCH = 2,
   RPYROWAVE_PACKET_PREFIX = 3,
   RPYROWAVE_PACKET_PACK = 4,
   RPYROWAVE_PACKET_UNPACK = 5
};

typedef struct rpyrowave_packet_parameters
{
   uint32_t image_width;
   uint32_t image_height;
   uint32_t block_count;
   uint32_t operation;
   uint32_t maximum_bytes;
   uint32_t sequence;
   uint32_t width;
   uint32_t height;
   uint32_t usability;
   uint32_t fixed_quant_code;
   uint32_t fixed_quantizer;
   uint32_t reserved;
} rpyrowave_packet_parameters_t;

typedef int (*rpyrowave_packet_record_t)(void *userdata,
      const rpyrowave_packet_parameters_t *parameters, unsigned groups_x, unsigned groups_y);

/* Setup only. Obtain layouts here and keep them and block descriptors
 * immutable for the sequence. Dispatches use at most 65535 groups/axis. */
int rpyrowave_gpu_layout(const rpyrowave_sequence_t *sequence, uint32_t maximum_bytes,
      rpyrowave_gpu_layout_t *layout);
int rpyrowave_gpu_blocks(const rpyrowave_gpu_layout_t *layout,
      rpyrowave_gpu_block_t *blocks, size_t count);

/* Records into caller-owned resources, without allocation or GPU waits.
 * Parameters are borrowed only during record. Retire any emitted work
 * before reusing buffers, including on failure. Add a storage dependency
 * between operations. maximum_bytes is bounded by the layout capacity.
 * fixed_quant_code is -1
 * for exact GPU byte-budget search, or 0..255 for a fixed quantizer.
 * Workspace control words report quantizer, frame bytes, active blocks
 * and error at offsets 0,4,8,12. The bitstream starts at wire_offset. */
int rpyrowave_gpu_encode_record(const rpyrowave_gpu_layout_t *layout,
      const rpyrowave_sequence_t *sequence, uint32_t maximum_bytes, int fixed_quant_code,
      rpyrowave_packet_record_t record, void *userdata);

/* The receiver writes compressed bytes directly at wire_offset in this
 * CPU-visible upload span. Indexing reads headers only, fills the packet
 * table in place, and leaves the compressed bytes unchanged. Internal
 * packet bounds/signs are checked on GPU. Keep this span owned until the
 * upload completes. No allocation or pixel readback occurs. */
int rpyrowave_gpu_index_frame(const rpyrowave_gpu_layout_t *layout,
      void *workspace, size_t workspace_size, size_t frame_bytes,
      unsigned allow_partial, rpyrowave_packet_parameters_t *parameters);
int rpyrowave_gpu_decode_record(const rpyrowave_gpu_layout_t *layout,
      const rpyrowave_packet_parameters_t *parameters,
      rpyrowave_packet_record_t record, void *userdata);

RETRO_END_DECLS
#endif
