/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef RH265_SPS_CROP_H
#define RH265_SPS_CROP_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
#define RH265_SPS_CROP_MAX_NAL_SIZE 65536
/* Worker/cold metadata conversion, never a render callback. Replaces only
 * the conformance window of an escaped HEVC 4:2:0 SPS NAL (including its two
 * header bytes, without a start code). Active image is the top-left rectangle
 * of the specified coded size. Dimensions must be even, 2..16384. The coded
 * dimensions must match the SPS. Does not recode or inspect picture NALs.
 * Parses/bounds the SPS prefix; this is not a full SPS/body validator.
 * Success allocates an escaped NAL for free() by the caller. Failure sets
 * *output=NULL and *output_size=0. All allocations and loops are bounded. */
bool rh265_sps_crop(const uint8_t *nal,size_t size,unsigned coded_width,
      unsigned coded_height,unsigned visible_width,unsigned visible_height,
      uint8_t **output,size_t *output_size);
RETRO_END_DECLS
#endif
