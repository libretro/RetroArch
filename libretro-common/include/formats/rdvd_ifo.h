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

enum rdvd_pgc_command_kind { RDVD_PGC_PRE = 0, RDVD_PGC_POST, RDVD_PGC_CELL };
typedef struct rdvd_cell
{
   uint32_t first_sector;
   uint32_t last_sector;
   uint32_t last_vobu_start;
   uint32_t first_ilvu_end;
   unsigned vob_id;
   unsigned cell_id;
   unsigned still_time;
   unsigned command_number;
   unsigned block_mode;
   unsigned block_type;
   bool seamless;
   bool interleaved;
   bool stc_discontinuity;
} rdvd_cell_t;

/* Program/cell numbers are one-based. Ranges include angle-block cells;
 * selecting an angle is navigation policy, not a property of this reader.
 * Sector values are relative to the applicable VOB domain. */
bool rdvd_pgc_program_cells(const rdvd_pgc_t *pgc, unsigned number,
      unsigned *first, unsigned *last);
bool rdvd_pgc_cell_read(const rdvd_pgc_t *pgc, unsigned number, rdvd_cell_t *out);

/* Index is zero-based; commands borrow eight bytes and are not executed.
 * Palette words retain their DVD wire {reserved,Y,Cr,Cb} value, not RGB.
 * All accessors revalidate the source span and clear output on failure. */
bool rdvd_pgc_command_read(const rdvd_pgc_t *pgc, enum rdvd_pgc_command_kind kind,
      unsigned index, const uint8_t **out);
bool rdvd_pgc_palette_read(const rdvd_pgc_t *pgc, unsigned index, uint32_t *out);
RETRO_END_DECLS
#endif
