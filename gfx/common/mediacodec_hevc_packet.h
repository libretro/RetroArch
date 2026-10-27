/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_HEVC_PACKET_H
#define MEDIACODEC_HEVC_PACKET_H
#include <formats/rh265.h>
#include <stddef.h>
#include <stdint.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
/* Android HEVC csd-0 is VPS/SPS/PPS with Annex-B start codes. These functions
 * consume validated, escaped NALs borrowed from the existing rh265 parser;
 * they do not replace syntax validation or establish hardware eligibility.
 * 1 copied; 0 valid but storage needed; -1 invalid/overlapping input.
 * needed receives exact bytes for 0/1, zero for -1. NULL/0 measures only.
 * Payload output stays untouched on failure/BUSY. needed is independent
 * caller storage, never aliased with input metadata or payload output.
 * No allocation; a caller may provision/reuse bounded staging off frame paths.
 */
int mediacodec_hevc_csd(const rh265_nal_t sets[3], uint8_t *output,
      size_t capacity, size_t *needed);
int mediacodec_hevc_packet(const rh265_access_unit_t *au,
      const rh265_nal_t *slices, unsigned count, uint8_t *output,
      size_t capacity, size_t *needed);
RETRO_END_DECLS
#endif
