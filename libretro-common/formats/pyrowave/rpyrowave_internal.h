/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */
#ifndef RPYROWAVE_INTERNAL_H
#define RPYROWAVE_INTERNAL_H

#include <formats/rpyrowave.h>

/* Direct atlas write. The caller may pre-clear the region. */
int rpyrowave_decode_block_region(const uint8_t *data, size_t size,
      float *coefficients, size_t stride, unsigned width, unsigned height,
      unsigned clear_region, rpyrowave_block_t *block);

#endif
