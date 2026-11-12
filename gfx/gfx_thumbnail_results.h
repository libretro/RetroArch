/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GFX_THUMBNAIL_RESULTS_H
#define __GFX_THUMBNAIL_RESULTS_H

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* In the order of the thumbnail type settings 1-4. */
enum gfx_thumbnail_result_type
{
   GFX_THUMBNAIL_RESULT_SNAPS = 0,
   GFX_THUMBNAIL_RESULT_TITLES,
   GFX_THUMBNAIL_RESULT_BOXARTS,
   GFX_THUMBNAIL_RESULT_LOGOS,
   GFX_THUMBNAIL_RESULT_TYPES
};

/* What gfx_thumbnail_results_open() says a playlist still needs. */
#define GFX_THUMBNAIL_RESULTS_LOAD    (1 << 0)
#define GFX_THUMBNAIL_RESULTS_RESOLVE (1 << 1)

/* Per entry, the file the thumbnail task matched for each type. An
 * entry is keyed by content path, label, crc32 and system folder;
 * lookups ignore the crc32, which only the cache file needs. */
typedef struct gfx_thumbnail_results gfx_thumbnail_results_t;

/* "Named_Snaps" .. "Named_Logos"; NULL past the last type. */
const char *gfx_thumbnail_result_type_dir(unsigned type);
/* The type whose folder is @dir, else GFX_THUMBNAIL_RESULT_TYPES. */
unsigned gfx_thumbnail_result_type_of(const char *dir);

/* @src with & * / : ` " < > ? \ | as '_', as the thumbnail
 * repository names files. Returns the length written. */
size_t gfx_thumbnail_scrub_name(char *s, size_t len, const char *src);
/* The thumbnails/<system> folder for a db_name: "MAME" for any
 * "MAME...", else the first of "A|B" without its extension. */
size_t gfx_thumbnail_system_name(char *s, size_t len, const char *db_name);
/* The extensions the menu shows, ".png" first; NULL-terminated. */
extern const char *const gfx_thumbnail_image_exts[];
/* Whether the menu shows a file named @name in a thumbnail folder:
 * no path separator, and an extension it shows. */
bool gfx_thumbnail_name_allowed(const char *name, bool allow_non_png);

/* A table belongs to one thread at a time. */
gfx_thumbnail_results_t *gfx_thumbnail_results_new(void);
void gfx_thumbnail_results_free(gfx_thumbnail_results_t *t);
/* Returns the new record, or (size_t)-1. Not once a sort has begun,
 * unless it ran out of memory. */
size_t gfx_thumbnail_results_add(gfx_thumbnail_results_t *t,
      const char *content_path, const char *label,
      const char *crc32, const char *system);
bool gfx_thumbnail_results_set(gfx_thumbnail_results_t *t, size_t rec,
      unsigned type, const char *name);
/* Sorts in bounded steps; true while steps remain. Until then the table
 * refuses adds, sets, gets and finds. On OOM it stays unsorted, which
 * find still searches. */
bool gfx_thumbnail_results_sort_step(gfx_thumbnail_results_t *t);
/* Runs every step; not for the main thread. */
void gfx_thumbnail_results_sort(gfx_thumbnail_results_t *t);
size_t gfx_thumbnail_results_count(const gfx_thumbnail_results_t *t);
/* @key: content path, label, crc32, system. @names: one per type,
 * NULL where nothing matched. Both borrowed from @t until it is freed;
 * all NULL when refused. */
void gfx_thumbnail_results_get(const gfx_thumbnail_results_t *t,
      size_t rec, const char **key, const char **names);
/* Borrowed from @t until it is freed. */
const char *gfx_thumbnail_results_find(const gfx_thumbnail_results_t *t,
      const char *content_path, const char *label, const char *system,
      unsigned type);

/* The store keeps the tables of the last playlists opened.
 * Main thread only. */
unsigned gfx_thumbnail_results_open(const char *playlist_path);
/* Takes @t. false, with @t freed, once the playlist is not kept;
 * @resolved still counts once newer playlists pushed it out. */
bool gfx_thumbnail_results_install(const char *playlist_path,
      gfx_thumbnail_results_t *t, bool resolved);
bool gfx_thumbnail_results_note(const char *playlist_path,
      const char *content_path, const char *label, const char *system,
      unsigned type, const char *name);
/* Borrowed until the next _open, _install, _forget or _deinit. */
const char *gfx_thumbnail_results_lookup(const char *content_path,
      const char *label, const char *system, unsigned type);
/* The playlist is gone: its table, and its cache file. */
void gfx_thumbnail_results_forget(const char *playlist_path);
void gfx_thumbnail_results_deinit(void);

RETRO_END_DECLS

#endif
