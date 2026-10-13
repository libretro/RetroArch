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
enum rdvd_ifo_table_kind
{
   RDVD_IFO_TITLES = 0, RDVD_IFO_PGCIT, RDVD_IFO_MENU,
   RDVD_IFO_MENU_CELLS, RDVD_IFO_TITLE_CELLS,
   RDVD_IFO_MENU_VOBUS, RDVD_IFO_TITLE_VOBUS
};
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
 * PGCIT and TITLE_CELLS/TITLE_VOBUS are only valid for VTS. Failure/absent tables clear the output. */
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
/* Chooses a valid management prefix of the requested kind, preferring IFO
 * over BUP. Does not check every nested table or perform filesystem I/O.
 * Both sources are caller-owned; the selected view borrows one input. */
bool rdvd_ifo_select(const uint8_t *primary, size_t primary_size,
      const uint8_t *backup, size_t backup_size, enum rdvd_ifo_kind kind,
      rdvd_ifo_t *out, bool *used_backup);
/* Finds the first exact language-code/extension match. Language code is
 * the big-endian two-byte DVD value. No implicit fallback is applied.
 * Validates the matched nested PGCIT; failure sets index to zero. */
bool rdvd_ifo_menu_language_find(const rdvd_ifo_t *ifo, unsigned code,
      unsigned extension, unsigned *index);
enum rdvd_file_kind { RDVD_FILE_IFO = 0, RDVD_FILE_BUP, RDVD_FILE_VOB };

/* Produces a basename for caller-managed VIDEO_TS filesystem routing.
 * Title set zero is VMG; VTS numbers are 1..99. Only title VOBs use segments
 * 1..9; menus and IFO/BUP use zero. Requires 13 bytes including terminator.
 * Failure clears a nonempty output buffer. No path or filesystem I/O occurs. */
bool rdvd_file_name(unsigned title_set, unsigned segment,
      enum rdvd_file_kind kind, char *out, size_t capacity);
typedef struct rdvd_vob_map
{
   const uint64_t *sizes;
   uint64_t total_bytes;
   unsigned segments;
} rdvd_vob_map_t;

/* Title VOB files are supplied in segment order (1..9). Sizes must be
 * nonzero multiples of 2048. Borrows immutable sizes, not file contents. */
bool rdvd_vob_map_read(const uint64_t *sizes, unsigned count,
      rdvd_vob_map_t *out);
typedef struct rdvd_vob_span
{
   uint64_t file_offset, bytes;
   unsigned segment;
} rdvd_vob_span_t;

/* Returns the first nonempty file-local span of a logical byte request,
 * stopping at the segment boundary/EOF. Segment numbers are one-based.
 * Revalidates geometry; callers advance by bytes to continue the request.
 * No payload allocation, copy or I/O occurs. Failure clears out. */
bool rdvd_vob_span_read(const rdvd_vob_map_t *map, uint64_t offset,
      uint64_t bytes, rdvd_vob_span_t *out);
typedef struct rdvd_vob_range { uint64_t offset, bytes; } rdvd_vob_range_t;

/* Converts a title-domain cell's inclusive sector bounds to a logical byte
 * range, checking the entire cell against the supplied title VOB geometry.
 * Caller must pair the PGC with its own VTS title domain. Angle/interleave
 * selection is separate; this range is not a playback sequence. */
bool rdvd_vob_cell_range(const rdvd_vob_map_t *map, const rdvd_pgc_t *pgc,
      unsigned cell_number, rdvd_vob_range_t *out);
typedef struct rdvd_cell_table
{
   const uint8_t *data;
   size_t size;
   unsigned entries, vobs;
} rdvd_cell_table_t;
typedef struct rdvd_cell_address
{
   uint32_t first_sector, last_sector;
   unsigned vob_id, cell_id;
} rdvd_cell_address_t;

/* C_ADT records retain domain-relative inclusive sector bounds. Validates
 * all record IDs/ranges; declared VOB count is metadata, not an index. */
bool rdvd_cell_table_read(const uint8_t *data, size_t size,
      rdvd_cell_table_t *out);
/* Zero-based index. Revalidates the borrowed table; failure clears out. */
bool rdvd_cell_address_read(const rdvd_cell_table_t *table, unsigned index,
      rdvd_cell_address_t *out);
typedef struct rdvd_vobu_map
{
   const uint8_t *data;
   size_t size;
   unsigned entries;
} rdvd_vobu_map_t;

/* Reads a borrowed VOBU_ADMAP with strictly increasing sector addresses. */
bool rdvd_vobu_map_read(const uint8_t *data, size_t size, rdvd_vobu_map_t *out);
/* Finds the last VOBU start at/before target. Revalidates the map, then
 * searches its ordered entries. Caller checks the result's cell/domain.
 * Failure clears sector/index; success may return sector zero. */
bool rdvd_vobu_find(const rdvd_vobu_map_t *map, uint32_t target,
      uint32_t *sector, unsigned *index);
/* Finds a unique C_ADT VOB/cell pair. Missing or ambiguous IDs fail;
 * results retain their own menu/title domain's inclusive sector bounds. */
bool rdvd_cell_address_find(const rdvd_cell_table_t *table, unsigned vob_id,
      unsigned cell_id, rdvd_cell_address_t *out);
/* Cell-bounded floor lookup. Target must lie inside inclusive first/last;
 * a preceding VOBU in another cell is rejected. Failure clears outputs. */
bool rdvd_vobu_find_in_cell(const rdvd_vobu_map_t *map, uint32_t target,
      uint32_t first, uint32_t last, uint32_t *sector, unsigned *index);
typedef struct rdvd_title_target
{
   rdvd_vob_span_t span;
   rdvd_vob_range_t remaining;
   rdvd_cell_t cell;
   uint32_t nav_sector;
} rdvd_title_target_t;

/* Resolves a chapter/cell/sector through PTT, PGC, title C_ADT and VOBU
 * tables into a first file-local span. Numbers are one-based. Source sizes
 * must belong to this VTS. Only ordinary noninterleaved cells are accepted;
 * angle policy, time seeking, I/O and playback scheduling are separate. */
bool rdvd_ifo_title_target(const rdvd_ifo_t *ifo, const rdvd_vob_map_t *source,
      unsigned title, unsigned chapter, unsigned cell_number, uint32_t target,
      rdvd_title_target_t *out);
RETRO_END_DECLS
#endif
