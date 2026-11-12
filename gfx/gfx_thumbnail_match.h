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

#ifndef __GFX_THUMBNAIL_MATCH_H
#define __GFX_THUMBNAIL_MATCH_H

#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct gfx_thumbnail_match_index gfx_thumbnail_match_index_t;

enum gfx_thumbnail_match_status
{
   GFX_THUMBNAIL_MATCH_NONE = 0, /* no candidate: try the next query string */
   GFX_THUMBNAIL_MATCH_FOUND,    /* *out names the file */
   GFX_THUMBNAIL_MATCH_REFUSED   /* hits were different games: stop this query string */
};

/* Elements one sort step moves at most. */
#define GFX_THUMBNAIL_MATCH_SORT_STEP 4096

/* A stable bottom-up merge sort run in bounded steps: with a total
 * order it gives qsort's order. */
typedef struct gfx_thumbnail_match_sort
{
   int (*cmp)(const void *a, const void *b);
   char *base;
   char *src;
   char *dst;
   size_t count;
   size_t size;
   size_t width; /* run length the pass merges */
   size_t i;     /* left run head */
   size_t mid;   /* left run end */
   size_t j;     /* right run head */
   size_t hi;    /* right run end */
   size_t k;     /* next output */
   size_t moved; /* elements moved since _begin */
} gfx_thumbnail_match_sort_t;

/* base and scratch: count elements of size bytes; scratch is borrowed
 * until a step returns false. */
void gfx_thumbnail_match_sort_begin(gfx_thumbnail_match_sort_t *s,
      void *base, void *scratch, size_t count, size_t size,
      int (*cmp)(const void *a, const void *b));
/* Moves at most GFX_THUMBNAIL_MATCH_SORT_STEP elements. false, having
 * done nothing, once base is sorted. */
bool gfx_thumbnail_match_sort_step(gfx_thumbnail_match_sort_t *s);

gfx_thumbnail_match_index_t *gfx_thumbnail_match_index_new(void);
/* name: file name as on disk, with extension ("Tetris (USA).png").
 * false on OOM, a NULL argument, or once finalize has begun. */
bool gfx_thumbnail_match_index_add(gfx_thumbnail_match_index_t *idx, const char *name);
/* Sorts in bounded steps; true while steps remain. Call after the last
 * add; finds see nothing until it returns false, nor after an OOM. */
bool gfx_thumbnail_match_index_finalize_step(gfx_thumbnail_match_index_t *idx);
/* Runs every step; not for the main thread. */
void gfx_thumbnail_match_index_finalize(gfx_thumbnail_match_index_t *idx);
/* The names added; 0 once a finalize ran out of memory. */
size_t gfx_thumbnail_match_index_count(const gfx_thumbnail_match_index_t *idx);
void gfx_thumbnail_match_index_free(gfx_thumbnail_match_index_t *idx);

/* query: label, db name or file stem, no extension. On FOUND, *out points into idx
 * and stays valid until gfx_thumbnail_match_index_free. */
enum gfx_thumbnail_match_status gfx_thumbnail_match_find(
      const gfx_thumbnail_match_index_t *idx, const char *query, const char **out);

/* Runs queries in order (NULL entries skipped): FOUND stops, NONE and REFUSED move
 * to the next query string. Returns the last status. */
enum gfx_thumbnail_match_status gfx_thumbnail_match_find_first(
      const gfx_thumbnail_match_index_t *idx, const char * const *queries,
      size_t num_queries, const char **out);

/* The T2 fold of in into out (len bytes, always terminated). Returns the length. */
size_t gfx_thumbnail_match_fold(const char *in, char *out, size_t len);

RETRO_END_DECLS

#endif
