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

typedef struct rdvd_pgci_table
{
   const uint8_t *data;
   size_t size;
   unsigned entries;
} rdvd_pgci_table_t;

typedef struct rdvd_pgci_entry
{
   rdvd_pgc_t pgc;
   unsigned entry_id;
   unsigned block_mode;
   unsigned block_type;
   unsigned parental_mask;
} rdvd_pgci_entry_t;

/* Reads a PGCIT span using its inclusive last-byte field. Validates the
 * entry table and fixed-header bounds of every referenced PGC. */
bool rdvd_pgci_table_read(const uint8_t *data, size_t size, rdvd_pgci_table_t *out);

/* Index is zero-based. Bounds a selected record at the nearest later
 * record, regardless of entry ordering; duplicate offsets may share a PGC.
 * Returned metadata and commands borrow the table input. No navigation or
 * parental-policy decision is made. Failure clears the output. */
bool rdvd_pgci_entry_read(const rdvd_pgci_table_t *table, unsigned index,
      rdvd_pgci_entry_t *out);

enum rdvd_ifo_kind { RDVD_IFO_NONE = 0, RDVD_IFO_VMG, RDVD_IFO_VTS };
enum rdvd_ifo_table_kind { RDVD_IFO_TITLES = 0, RDVD_IFO_PGCIT, RDVD_IFO_MENU };
typedef struct rdvd_ifo
{
   const uint8_t *data;
   size_t size;
   size_t tables[8];
   uint32_t first_play;
   uint32_t menu_vob_sector;
   uint32_t title_vob_sector;
   unsigned table_count;
   unsigned title_sets;
   unsigned version;
   enum rdvd_ifo_kind kind;
} rdvd_ifo_t;
typedef struct rdvd_ifo_span { const uint8_t *data; size_t size; } rdvd_ifo_span_t;

/* Parses the management prefix of a complete borrowed VMGI/VTSI file.
 * Checks the declared IFO sector extent and all management table pointers.
 * Attributes, BUP fallback and filesystem/VOB I/O are not interpreted here.
 * VOB sector fields are external-domain addresses, not IFO-buffer offsets. */
bool rdvd_ifo_read(const uint8_t *data, size_t size, rdvd_ifo_t *out);

/* Selects a present table, bounded by the nearest later management table.
 * Borrowed input must remain immutable and alive through all view use.
 * TITLES selects VMG title records or VTS chapter pointers by file kind;
 * PGCIT is only valid for VTS. Failure/absent tables clear the output. */
bool rdvd_ifo_table_read(const rdvd_ifo_t *ifo, enum rdvd_ifo_table_kind kind,
      rdvd_ifo_span_t *out);
typedef struct rdvd_title
{
   unsigned angles, chapters, parental_id, title_set, local_title;
   unsigned playback_flags;
   uint32_t title_set_sector;
} rdvd_title_t;

/* VMG title numbers are one-based. Returns metadata only; sector addresses
 * belong to the disc domain. Zero chapter counts are retained. */
bool rdvd_ifo_title_read(const rdvd_ifo_t *ifo, unsigned number,
      rdvd_title_t *out);

/* Returns an optional VMG first-play PGC, borrowing the management input.
 * Does not execute commands or select a playback domain. */
bool rdvd_ifo_first_play_read(const rdvd_ifo_t *ifo, rdvd_pgc_t *out);
typedef struct rdvd_chapter
{
   unsigned pgc_number, program_number, first_cell, last_cell;
   rdvd_pgc_t pgc;
} rdvd_chapter_t;

/* Resolves one-based local title/chapter numbers through VTS PTT and PGCIT.
 * Cell ranges include angle blocks. No navigation policy or VOB I/O occurs.
 * Unsorted/shared title offsets are bounded at the nearest later start. */
bool rdvd_ifo_chapter_read(const rdvd_ifo_t *ifo, unsigned title,
      unsigned chapter, rdvd_chapter_t *out);
typedef struct rdvd_menu_table
{
   const uint8_t *data;
   size_t size;
   unsigned languages;
} rdvd_menu_table_t;

/* Reads a borrowed PGCI_UT language table. Referenced PGCIT headers must
 * fit their record spans; nested program chains are checked on access. */
bool rdvd_menu_table_read(const uint8_t *data, size_t size,
      rdvd_menu_table_t *out);
typedef struct rdvd_menu_language
{
   rdvd_pgci_table_t chains;
   unsigned language_code, extension, menu_flags;
} rdvd_menu_language_t;

/* Zero-based index; shared and unsorted language records are supported.
 * Metadata and nested PGCIT borrow immutable table input. Failure clears out. */
bool rdvd_menu_language_read(const rdvd_menu_table_t *table, unsigned index,
      rdvd_menu_language_t *out);
enum rdvd_menu_id
{
   RDVD_MENU_TITLE = 2, RDVD_MENU_ROOT, RDVD_MENU_SUBPICTURE,
   RDVD_MENU_AUDIO, RDVD_MENU_ANGLE, RDVD_MENU_CHAPTER
};

/* Selects the first entry PGC matching a menu ID in an explicit zero-based
 * language index. No language fallback or parental/UOP policy is applied.
 * VMG supports title menus; VTS supports root through chapter menus. */
bool rdvd_ifo_menu_read(const rdvd_ifo_t *ifo, unsigned language_index,
      enum rdvd_menu_id menu, rdvd_pgci_entry_t *out);
RETRO_END_DECLS
#endif
