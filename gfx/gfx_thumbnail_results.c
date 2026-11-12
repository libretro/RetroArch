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

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../file_path_special.h"
#include "gfx_thumbnail_match.h"
#include "gfx_thumbnail_results.h"

/* Strings live in blocks that never move, so records hold pointers. */
#define GFX_THUMB_RESULTS_BLOCK 16384
/* Playlists whose tables are kept: a menu list, history, explore and
 * a companion window can show different playlists at once. */
#define GFX_THUMB_RESULTS_KEPT  4

typedef struct gfx_thumb_results_block
{
   struct gfx_thumb_results_block *next;
   char *data;
   size_t used;
   size_t size;
} gfx_thumb_results_block_t;

typedef struct gfx_thumb_result
{
   const char *key[4];
   const char *names[GFX_THUMBNAIL_RESULT_TYPES];
} gfx_thumb_result_t;

struct gfx_thumbnail_results
{
   gfx_thumb_result_t *recs;
   gfx_thumb_results_block_t *blocks;
   gfx_thumb_result_t *scratch; /* set while sorting */
   gfx_thumbnail_match_sort_t sort;
   size_t count;
   size_t capacity;
   bool sorted;
};

typedef struct gfx_thumb_results_slot
{
   char *playlist_path;
   gfx_thumbnail_results_t *table;
   gfx_thumbnail_results_t *notes;
   unsigned stamp;
} gfx_thumb_results_slot_t;

static const char *const gfx_thumb_result_dirs[GFX_THUMBNAIL_RESULT_TYPES] =
{
   "Named_Snaps",
   "Named_Titles",
   "Named_Boxarts",
   "Named_Logos"
};

static gfx_thumb_results_slot_t gfx_thumb_results_slots[GFX_THUMB_RESULTS_KEPT];
static unsigned gfx_thumb_results_clock;
static char **gfx_thumb_results_resolved;
static size_t gfx_thumb_results_resolved_count;
/* Pushed out of the store before they were resolved */
static char **gfx_thumb_results_evicted;
static size_t gfx_thumb_results_evicted_count;

const char *gfx_thumbnail_result_type_dir(unsigned type)
{
   if (type < GFX_THUMBNAIL_RESULT_TYPES)
      return gfx_thumb_result_dirs[type];
   return NULL;
}

unsigned gfx_thumbnail_result_type_of(const char *dir)
{
   unsigned i;
   if (dir)
      for (i = 0; i < GFX_THUMBNAIL_RESULT_TYPES; i++)
         if (string_is_equal(dir, gfx_thumb_result_dirs[i]))
            return i;
   return GFX_THUMBNAIL_RESULT_TYPES;
}

size_t gfx_thumbnail_scrub_name(char *s, size_t len, const char *src)
{
   char *p;
   if (!s || !len)
      return 0;
   strlcpy(s, src ? src : "", len);
   for (p = s; (p = strpbrk(p, "&*/:`\"<>?\\|")); p++)
      *p = '_';
   return strlen(s);
}

size_t gfx_thumbnail_system_name(char *s, size_t len, const char *db_name)
{
   char tmp_buf[NAME_MAX_LENGTH];
   const char *pos;

   if (!s || !len)
      return 0;
   s[0] = '\0';
   if (!db_name || !*db_name)
      return 0;
   /* There is only one MAME thumbnail repo */
   if (strncmp(db_name, "MAME", 4) == 0)
      return strlcpy(s, "MAME", len);
   /* Core info lists several databases: the first one */
   pos = strchr(db_name, '|');
   if (pos && (size_t)(pos - db_name) + 1 < sizeof(tmp_buf))
      strlcpy(tmp_buf, db_name, (size_t)(pos - db_name) + 1);
   else
      strlcpy(tmp_buf, db_name, sizeof(tmp_buf));
   fill_pathname(s, tmp_buf, "", len);
   return strlen(s);
}

const char *const gfx_thumbnail_image_exts[] =
{
   ".png", ".jpg", ".jpeg", ".bmp", ".tga",
#ifdef HAVE_RWEBP
   ".webp",
#endif
#ifdef HAVE_RWEBM
   ".webm", ".mkv",
#endif
#ifdef HAVE_RMP4
   ".mp4",
#endif
   NULL
};

bool gfx_thumbnail_name_allowed(const char *name, bool allow_non_png)
{
   size_t i;
   const char *ext = path_get_extension(name);

   /* A damaged cache must not name a file outside its folder */
   if (!ext || !*ext || strchr(name, '/') || strchr(name, '\\'))
      return false;
   if (string_is_equal_noncase(ext, gfx_thumbnail_image_exts[0] + 1))
      return true;
   if (!allow_non_png)
      return false;
   for (i = 1; gfx_thumbnail_image_exts[i]; i++)
      if (string_is_equal_noncase(ext, gfx_thumbnail_image_exts[i] + 1))
         return true;
   return false;
}

static const char *gfx_thumb_results_intern(gfx_thumbnail_results_t *t,
      const char *s)
{
   size_t _len                  = strlen(s) + 1;
   gfx_thumb_results_block_t *b = t->blocks;

   if (!b || b->size - b->used < _len)
   {
      size_t size = (_len > GFX_THUMB_RESULTS_BLOCK)
         ? _len : GFX_THUMB_RESULTS_BLOCK;
      if (!(b = (gfx_thumb_results_block_t*)malloc(sizeof(*b))))
         return NULL;
      if (!(b->data = (char*)malloc(size)))
      {
         free(b);
         return NULL;
      }
      b->size   = size;
      b->used   = 0;
      b->next   = t->blocks;
      t->blocks = b;
   }
   memcpy(b->data + b->used, s, _len);
   b->used += _len;
   return b->data + b->used - _len;
}

static const char *gfx_thumb_results_reuse(gfx_thumbnail_results_t *t,
      const char *s, const char *prev)
{
   if (prev && string_is_equal(prev, s))
      return prev;
   return gfx_thumb_results_intern(t, s);
}

/* Content path, label and system; the crc32 (key[2]) is not compared. */
static int gfx_thumb_results_cmp_key(const char * const *a,
      const char * const *b)
{
   int r;
   if ((r = strcmp(a[0], b[0])))
      return r;
   if ((r = strcmp(a[1], b[1])))
      return r;
   return strcmp(a[3], b[3]);
}

static int gfx_thumb_results_cmp_rec(const void *a, const void *b)
{
   return gfx_thumb_results_cmp_key(
         ((const gfx_thumb_result_t*)a)->key,
         ((const gfx_thumb_result_t*)b)->key);
}

gfx_thumbnail_results_t *gfx_thumbnail_results_new(void)
{
   return (gfx_thumbnail_results_t*)calloc(1, sizeof(gfx_thumbnail_results_t));
}

void gfx_thumbnail_results_free(gfx_thumbnail_results_t *t)
{
   gfx_thumb_results_block_t *b;
   if (!t)
      return;
   while ((b = t->blocks))
   {
      t->blocks = b->next;
      free(b->data);
      free(b);
   }
   free(t->recs);
   free(t->scratch);
   free(t);
}

size_t gfx_thumbnail_results_add(gfx_thumbnail_results_t *t,
      const char *content_path, const char *label,
      const char *crc32, const char *system)
{
   unsigned i;
   gfx_thumb_result_t *rec;
   const gfx_thumb_result_t *prev;

   if (     !t || t->sorted || t->scratch
         || !content_path || !label || !system)
      return (size_t)-1;
   if (t->count == t->capacity)
   {
      size_t cap               = t->capacity ? t->capacity * 2 : 64;
      gfx_thumb_result_t *recs = (gfx_thumb_result_t*)
         realloc(t->recs, cap * sizeof(*recs));
      if (!recs)
         return (size_t)-1;
      t->recs     = recs;
      t->capacity = cap;
   }
   rec  = &t->recs[t->count];
   prev = t->count ? rec - 1 : NULL;
   if (     !(rec->key[0] = gfx_thumb_results_intern(t, content_path))
         || !(rec->key[1] = gfx_thumb_results_intern(t, label))
         || !(rec->key[2] = gfx_thumb_results_reuse(t, crc32 ? crc32 : "",
               prev ? prev->key[2] : NULL))
         || !(rec->key[3] = gfx_thumb_results_reuse(t, system,
               prev ? prev->key[3] : NULL)))
      return (size_t)-1;
   for (i = 0; i < GFX_THUMBNAIL_RESULT_TYPES; i++)
      rec->names[i] = NULL;
   return t->count++;
}

bool gfx_thumbnail_results_set(gfx_thumbnail_results_t *t, size_t rec,
      unsigned type, const char *name)
{
   if (     !t || t->scratch || rec >= t->count || !name
         || type >= GFX_THUMBNAIL_RESULT_TYPES)
      return false;
   return (t->recs[rec].names[type] = gfx_thumb_results_intern(t, name))
      != NULL;
}

bool gfx_thumbnail_results_sort_step(gfx_thumbnail_results_t *t)
{
   if (!t || t->sorted)
      return false;
   if (!t->scratch && t->count > 1)
   {
      if (!(t->scratch = (gfx_thumb_result_t*)malloc(
            t->count * sizeof(*t->scratch))))
         return false;
      gfx_thumbnail_match_sort_begin(&t->sort, t->recs, t->scratch,
            t->count, sizeof(*t->recs), gfx_thumb_results_cmp_rec);
   }
   if (t->scratch && gfx_thumbnail_match_sort_step(&t->sort))
      return true;
   free(t->scratch);
   t->scratch = NULL;
   t->sorted  = true;
   return false;
}

void gfx_thumbnail_results_sort(gfx_thumbnail_results_t *t)
{
   while (gfx_thumbnail_results_sort_step(t));
}

size_t gfx_thumbnail_results_count(const gfx_thumbnail_results_t *t)
{
   return t ? t->count : 0;
}

void gfx_thumbnail_results_get(const gfx_thumbnail_results_t *t,
      size_t rec, const char **key, const char **names)
{
   unsigned i;
   const gfx_thumb_result_t *r = (t && !t->scratch && rec < t->count)
      ? &t->recs[rec] : NULL;
   for (i = 0; i < 4; i++)
      key[i] = r ? r->key[i] : NULL;
   for (i = 0; i < GFX_THUMBNAIL_RESULT_TYPES; i++)
      names[i] = r ? r->names[i] : NULL;
}

const char *gfx_thumbnail_results_find(const gfx_thumbnail_results_t *t,
      const char *content_path, const char *label, const char *system,
      unsigned type)
{
   const char *key[4];
   size_t i;

   if (     !t || t->scratch || !content_path || !label || !system
         || type >= GFX_THUMBNAIL_RESULT_TYPES)
      return NULL;
   key[0] = content_path;
   key[1] = label;
   key[2] = "";
   key[3] = system;
   if (t->sorted)
   {
      size_t lo = 0;
      size_t hi = t->count;
      while (lo < hi)
      {
         size_t mid = lo + (hi - lo) / 2;
         if (gfx_thumb_results_cmp_key(t->recs[mid].key, key) < 0)
            lo = mid + 1;
         else
            hi = mid;
      }
      for (i = lo; i < t->count
            && !gfx_thumb_results_cmp_key(t->recs[i].key, key); i++)
         if (t->recs[i].names[type])
            return t->recs[i].names[type];
      return NULL;
   }
   for (i = 0; i < t->count; i++)
      if (     t->recs[i].names[type]
            && !gfx_thumb_results_cmp_key(t->recs[i].key, key))
         return t->recs[i].names[type];
   return NULL;
}

static gfx_thumb_results_slot_t *gfx_thumb_results_slot(const char *path)
{
   size_t i;
   if (path)
      for (i = 0; i < GFX_THUMB_RESULTS_KEPT; i++)
         if (     gfx_thumb_results_slots[i].playlist_path
               && string_is_equal(gfx_thumb_results_slots[i].playlist_path,
                  path))
            return &gfx_thumb_results_slots[i];
   return NULL;
}

static void gfx_thumb_results_slot_clear(gfx_thumb_results_slot_t *slot)
{
   free(slot->playlist_path);
   gfx_thumbnail_results_free(slot->table);
   gfx_thumbnail_results_free(slot->notes);
   slot->playlist_path = NULL;
   slot->table         = NULL;
   slot->notes         = NULL;
   slot->stamp         = 0;
}

static bool gfx_thumb_results_was_resolved(const char *path)
{
   size_t i;
   for (i = 0; i < gfx_thumb_results_resolved_count; i++)
      if (string_is_equal(gfx_thumb_results_resolved[i], path))
         return true;
   return false;
}

static void gfx_thumb_results_mark_resolved(const char *path)
{
   char **list;
   char *copy;
   if (gfx_thumb_results_was_resolved(path) || !(copy = strdup(path)))
      return;
   if (!(list = (char**)realloc(gfx_thumb_results_resolved,
         (gfx_thumb_results_resolved_count + 1) * sizeof(*list))))
   {
      free(copy);
      return;
   }
   list[gfx_thumb_results_resolved_count++] = copy;
   gfx_thumb_results_resolved                = list;
}

/* Takes the slot's path; its resolve may still be running. */
static void gfx_thumb_results_evict(gfx_thumb_results_slot_t *slot)
{
   char **list;
   if (     !slot->playlist_path
         || gfx_thumb_results_was_resolved(slot->playlist_path)
         || !(list = (char**)realloc(gfx_thumb_results_evicted,
               (gfx_thumb_results_evicted_count + 1) * sizeof(*list))))
      return;
   list[gfx_thumb_results_evicted_count++] = slot->playlist_path;
   gfx_thumb_results_evicted                = list;
   slot->playlist_path                      = NULL;
}

static bool gfx_thumb_results_unevict(const char *path)
{
   size_t i;
   for (i = 0; i < gfx_thumb_results_evicted_count; i++)
   {
      if (string_is_equal(gfx_thumb_results_evicted[i], path))
      {
         free(gfx_thumb_results_evicted[i]);
         gfx_thumb_results_evicted[i] =
            gfx_thumb_results_evicted[--gfx_thumb_results_evicted_count];
         return true;
      }
   }
   return false;
}

unsigned gfx_thumbnail_results_open(const char *playlist_path)
{
   unsigned need = 0;
   gfx_thumb_results_slot_t *slot;

   if (!playlist_path || !*playlist_path)
      return 0;
   if (!(slot = gfx_thumb_results_slot(playlist_path)))
   {
      size_t i;
      char *copy = strdup(playlist_path);
      if (!copy)
         return 0;
      slot = &gfx_thumb_results_slots[0];
      for (i = 1; i < GFX_THUMB_RESULTS_KEPT; i++)
         if (gfx_thumb_results_slots[i].stamp < slot->stamp)
            slot = &gfx_thumb_results_slots[i];
      gfx_thumb_results_evict(slot);
      gfx_thumb_results_slot_clear(slot);
      gfx_thumb_results_unevict(playlist_path);
      slot->playlist_path = copy;
   }
   slot->stamp = ++gfx_thumb_results_clock;
   if (!slot->table)
      need |= GFX_THUMBNAIL_RESULTS_LOAD;
   if (!gfx_thumb_results_was_resolved(playlist_path))
      need |= GFX_THUMBNAIL_RESULTS_RESOLVE;
   return need;
}

bool gfx_thumbnail_results_install(const char *playlist_path,
      gfx_thumbnail_results_t *t, bool resolved)
{
   gfx_thumb_results_slot_t *slot;
   if (!(slot = gfx_thumb_results_slot(playlist_path)))
   {
      /* Pushed out while resolving: a reopen only loads */
      if (gfx_thumb_results_unevict(playlist_path) && resolved)
         gfx_thumb_results_mark_resolved(playlist_path);
      gfx_thumbnail_results_free(t);
      return false;
   }
   if (resolved)
      gfx_thumb_results_mark_resolved(playlist_path);
   gfx_thumbnail_results_free(slot->table);
   slot->table = t;
   return true;
}

bool gfx_thumbnail_results_note(const char *playlist_path,
      const char *content_path, const char *label, const char *system,
      unsigned type, const char *name)
{
   const char *key[4];
   gfx_thumbnail_results_t *n;
   size_t rec;
   gfx_thumb_results_slot_t *slot = gfx_thumb_results_slot(playlist_path);

   if (     !slot || !content_path || !label || !system || !name
         || type >= GFX_THUMBNAIL_RESULT_TYPES)
      return false;
   if (!slot->notes && !(slot->notes = gfx_thumbnail_results_new()))
      return false;
   n      = slot->notes;
   key[0] = content_path;
   key[1] = label;
   key[2] = "";
   key[3] = system;
   for (rec = 0; rec < n->count; rec++)
      if (!gfx_thumb_results_cmp_key(n->recs[rec].key, key))
         break;
   if (rec < n->count)
   {
      if (string_is_equal(n->recs[rec].names[type], name))
         return true;
   }
   else if ((rec = gfx_thumbnail_results_add(n, content_path, label,
         "", system)) == (size_t)-1)
      return false;
   return gfx_thumbnail_results_set(n, rec, type, name);
}

const char *gfx_thumbnail_results_lookup(const char *content_path,
      const char *label, const char *system, unsigned type)
{
   size_t i;
   const char *name;
   for (i = 0; i < GFX_THUMB_RESULTS_KEPT; i++)
      if ((name = gfx_thumbnail_results_find(gfx_thumb_results_slots[i].notes,
            content_path, label, system, type)))
         return name;
   for (i = 0; i < GFX_THUMB_RESULTS_KEPT; i++)
      if ((name = gfx_thumbnail_results_find(gfx_thumb_results_slots[i].table,
            content_path, label, system, type)))
         return name;
   return NULL;
}

void gfx_thumbnail_results_forget(const char *playlist_path)
{
   char path[PATH_MAX_LENGTH];
   size_t i;
   gfx_thumb_results_slot_t *slot;

   if (!playlist_path || !*playlist_path)
      return;
   if ((slot = gfx_thumb_results_slot(playlist_path)))
      gfx_thumb_results_slot_clear(slot);
   gfx_thumb_results_unevict(playlist_path);
   for (i = 0; i < gfx_thumb_results_resolved_count; i++)
   {
      if (string_is_equal(gfx_thumb_results_resolved[i], playlist_path))
      {
         free(gfx_thumb_results_resolved[i]);
         gfx_thumb_results_resolved[i] =
            gfx_thumb_results_resolved[--gfx_thumb_results_resolved_count];
         break;
      }
   }
   strlcpy(path, playlist_path, sizeof(path));
   if (strlcat(path, FILE_PATH_THUMBS_EXTENSION, sizeof(path)) < sizeof(path))
      filestream_delete(path);
}

void gfx_thumbnail_results_deinit(void)
{
   size_t i;
   for (i = 0; i < GFX_THUMB_RESULTS_KEPT; i++)
      gfx_thumb_results_slot_clear(&gfx_thumb_results_slots[i]);
   for (i = 0; i < gfx_thumb_results_resolved_count; i++)
      free(gfx_thumb_results_resolved[i]);
   free(gfx_thumb_results_resolved);
   for (i = 0; i < gfx_thumb_results_evicted_count; i++)
      free(gfx_thumb_results_evicted[i]);
   free(gfx_thumb_results_evicted);
   gfx_thumb_results_resolved       = NULL;
   gfx_thumb_results_resolved_count = 0;
   gfx_thumb_results_evicted        = NULL;
   gfx_thumb_results_evicted_count  = 0;
   gfx_thumb_results_clock          = 0;
}
