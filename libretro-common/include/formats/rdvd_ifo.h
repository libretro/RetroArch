/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_IFO_H
#define LIBRETRO_RDVD_IFO_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS

typedef struct rdvd_pgc
{
   const uint8_t *data;
   size_t size;
   unsigned programs;
   unsigned cells;
   unsigned offsets[4];
   unsigned command_counts[3];
} rdvd_pgc_t;

/* Reads a bounded serialized IFO program chain. The caller supplies the
 * record span; no filesystem I/O, allocation or command execution occurs.
 * Tables may not overlap the fixed header or each other. Program starts
 * must be increasing valid cell numbers. All views borrow immutable input
 * which must remain alive through use. Failure clears the output. */
bool rdvd_pgc_read(const uint8_t *data, size_t size, rdvd_pgc_t *out);
RETRO_END_DECLS
#endif
