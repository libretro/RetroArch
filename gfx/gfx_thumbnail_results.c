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
/* Tables kept beside the held ones, the last playlists opened */
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
   unsigned hold;
   bool dirty; /* notes its cache file lacks */
} gfx_thumb_results_slot_t;

static const char *const gfx_thumb_result_dirs[GFX_THUMBNAIL_RESULT_TYPES] =
{
   "Named_Snaps",
   "Named_Titles",
   "Named_Boxarts",
   "Named_Logos"
};

static gfx_thumb_results_slot_t *gfx_thumb_results_slots;
static size_t gfx_thumb_results_slot_count;
static unsigned gfx_thumb_results_clock;
static unsigned gfx_thumb_results_generation;
static char **gfx_thumb_results_resolved;
static size_t gfx_thumb_results_resolved_count;
/* Pushed out of the store before they were resolved */
static char **gfx_thumb_results_evicted;
static size_t gfx_thumb_results_evicted_count;
/* Pushed-out playlists whose notes wait to be written */
static gfx_thumbnail_results_save_t *gfx_thumb_results_parked;
static size_t gfx_thumb_results_parked_count;

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
      for (i = 0; i < gfx_thumb_results_slot_count; i++)
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
   slot->hold          = 0;
   slot->dirty         = false;
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

/* The parked entry for @path, else the count */
static size_t gfx_thumb_results_parked_at(const char *path)
{
   size_t i;
   for (i = 0; i < gfx_thumb_results_parked_count; i++)
      if (string_is_equal(gfx_thumb_results_parked[i].playlist_path, path))
         break;
   return i;
}

static void gfx_thumb_results_unpark_at(size_t i)
{
   gfx_thumb_results_parked[i] =
      gfx_thumb_results_parked[--gfx_thumb_results_parked_count];
}

static void gfx_thumb_results_parked_free(size_t i)
{
   free(gfx_thumb_results_parked[i].playlist_path);
   gfx_thumbnail_results_free(gfx_thumb_results_parked[i].table);
   gfx_thumbnail_results_free(gfx_thumb_results_parked[i].notes);
   gfx_thumb_results_unpark_at(i);
}

/* A pushed-out slot's unwritten notes wait for a save, with its table. */
static void gfx_thumb_results_park(gfx_thumb_results_slot_t *slot)
{
   gfx_thumbnail_results_save_t *list;
   char *path;
   if (!slot->dirty || !(path = strdup(slot->playlist_path)))
      return;
   if (!(list = (gfx_thumbnail_results_save_t*)realloc(
         gfx_thumb_results_parked,
         (gfx_thumb_results_parked_count + 1) * sizeof(*list))))
   {
      free(path);
      return;
   }
   list[gfx_thumb_results_parked_count].playlist_path = path;
   list[gfx_thumb_results_parked_count].table         = slot->table;
   list[gfx_thumb_results_parked_count].notes         = slot->notes;
   gfx_thumb_results_parked_count++;
   gfx_thumb_results_parked = list;
   slot->table              = NULL;
   slot->notes              = NULL;
}

/* Reopened before its notes were written */
static void gfx_thumb_results_unpark(gfx_thumb_results_slot_t *slot)
{
   size_t i = gfx_thumb_results_parked_at(slot->playlist_path);
   if (i == gfx_thumb_results_parked_count)
      return;
   slot->table = gfx_thumb_results_parked[i].table;
   slot->notes = gfx_thumb_results_parked[i].notes;
   slot->dirty = true;
   free(gfx_thumb_results_parked[i].playlist_path);
   gfx_thumb_results_unpark_at(i);
}

static void gfx_thumb_results_push_out(gfx_thumb_results_slot_t *slot)
{
   gfx_thumb_results_park(slot);
   gfx_thumb_results_evict(slot);
   gfx_thumb_results_slot_clear(slot);
}

/* The oldest slot nothing holds, once @kept or more are unheld */
static gfx_thumb_results_slot_t *gfx_thumb_results_oldest(size_t kept)
{
   size_t i;
   size_t unheld                    = 0;
   gfx_thumb_results_slot_t *oldest = NULL;
   for (i = 0; i < gfx_thumb_results_slot_count; i++)
   {
      gfx_thumb_results_slot_t *slot = &gfx_thumb_results_slots[i];
      if (!slot->playlist_path || slot->hold)
         continue;
      unheld++;
      if (!oldest || slot->stamp < oldest->stamp)
         oldest = slot;
   }
   return (unheld >= kept) ? oldest : NULL;
}

/* An empty slot, else NULL */
static gfx_thumb_results_slot_t *gfx_thumb_results_free_slot(void)
{
   size_t i;
   gfx_thumb_results_slot_t *slots;
   for (i = 0; i < gfx_thumb_results_slot_count; i++)
      if (!gfx_thumb_results_slots[i].playlist_path)
         return &gfx_thumb_results_slots[i];
   if (!(slots = (gfx_thumb_results_slot_t*)realloc(gfx_thumb_results_slots,
         (gfx_thumb_results_slot_count + 1) * sizeof(*slots))))
      return NULL;
   gfx_thumb_results_slots = slots;
   memset(&slots[gfx_thumb_results_slot_count], 0, sizeof(*slots));
   return &slots[gfx_thumb_results_slot_count++];
}

/* The record for @key in a notes table, else its count */
static size_t gfx_thumb_results_note_rec(const gfx_thumbnail_results_t *n,
      const char * const *key)
{
   size_t rec;
   for (rec = 0; rec < n->count; rec++)
      if (!gfx_thumb_results_cmp_key(n->recs[rec].key, key))
         break;
   return rec;
}

/* @from's names where @into has none for that entry and type; the
 * newer notes are @into's. Takes @from. */
static gfx_thumbnail_results_t *gfx_thumb_results_merge(
      gfx_thumbnail_results_t *into, gfx_thumbnail_results_t *from)
{
   size_t i, rec;
   unsigned type;
   if (!into || !from)
      return into ? into : from;
   for (i = 0; i < from->count; i++)
   {
      const char *const *key = from->recs[i].key;
      rec = gfx_thumb_results_note_rec(into, key);
      for (type = 0; type < GFX_THUMBNAIL_RESULT_TYPES; type++)
      {
         if (     !from->recs[i].names[type]
               || (rec < into->count && into->recs[rec].names[type]))
            continue;
         if (     rec == into->count
               && (rec = gfx_thumbnail_results_add(into, key[0], key[1],
                  "", key[3])) == (size_t)-1)
            break;
         gfx_thumbnail_results_set(into, rec, type,
               from->recs[i].names[type]);
      }
   }
   gfx_thumbnail_results_free(from);
   return into;
}

unsigned gfx_thumbnail_results_open(const char *playlist_path)
{
   unsigned need = 0;
   gfx_thumb_results_slot_t *slot;

   if (!playlist_path || !*playlist_path)
      return 0;
   if (!(slot = gfx_thumb_results_slot(playlist_path)))
   {
      char *copy = strdup(playlist_path);
      if (!copy)
         return 0;
      if (     !(slot = gfx_thumb_results_oldest(GFX_THUMB_RESULTS_KEPT))
            && !(slot = gfx_thumb_results_free_slot()))
      {
         free(copy);
         return 0;
      }
      gfx_thumb_results_push_out(slot);
      gfx_thumb_results_unevict(playlist_path);
      slot->playlist_path = copy;
      gfx_thumb_results_unpark(slot);
   }
   slot->stamp = ++gfx_thumb_results_clock;
   if (!slot->table)
      need |= GFX_THUMBNAIL_RESULTS_LOAD;
   if (!gfx_thumb_results_was_resolved(playlist_path))
      need |= GFX_THUMBNAIL_RESULTS_RESOLVE;
   return need;
}

bool gfx_thumbnail_results_install(const char *playlist_path,
      gfx_thumbnail_results_t *t, gfx_thumbnail_results_t *notes,
      bool resolved)
{
   size_t i;
   gfx_thumb_results_slot_t *slot;
   if (!(slot = gfx_thumb_results_slot(playlist_path)))
   {
      /* Pushed out while resolving: a reopen only loads */
      if (gfx_thumb_results_unevict(playlist_path) && resolved)
         gfx_thumb_results_mark_resolved(playlist_path);
      if ((i = gfx_thumb_results_parked_at(playlist_path))
            < gfx_thumb_results_parked_count)
      {
         gfx_thumbnail_results_save_t *p = &gfx_thumb_results_parked[i];
         gfx_thumbnail_results_free(p->table);
         p->table = t;
         p->notes = gfx_thumb_results_merge(p->notes, notes);
         return false;
      }
      gfx_thumbnail_results_free(t);
      gfx_thumbnail_results_free(notes);
      return false;
   }
   if (resolved)
      gfx_thumb_results_mark_resolved(playlist_path);
   gfx_thumbnail_results_free(slot->table);
   slot->table = t;
   slot->notes = gfx_thumb_results_merge(slot->notes, notes);
   return true;
}

bool gfx_thumbnail_results_take(gfx_thumbnail_results_save_t *save,
      gfx_thumbnail_results_busy_t busy, void *userdata)
{
   size_t i = 0;
   while (i < gfx_thumb_results_parked_count)
   {
      if (busy && busy(gfx_thumb_results_parked[i].playlist_path, userdata))
         i++;
      else if (gfx_thumb_results_parked[i].table)
      {
         *save = gfx_thumb_results_parked[i];
         gfx_thumb_results_unpark_at(i);
         return true;
      }
      else /* its load failed */
         gfx_thumb_results_parked_free(i);
   }
   return false;
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
   if ((rec = gfx_thumb_results_note_rec(n, key)) < n->count)
   {
      if (string_is_equal(n->recs[rec].names[type], name))
         return true;
   }
   else if ((rec = gfx_thumbnail_results_add(n, content_path, label,
         "", system)) == (size_t)-1)
      return false;
   if (!gfx_thumbnail_results_set(n, rec, type, name))
      return false;
   slot->dirty = true;
   return true;
}

const char *gfx_thumbnail_results_lookup(const char *content_path,
      const char *label, const char *system, unsigned type,
      bool allow_non_png)
{
   size_t i;
   const char *name;
   const char *own;
   for (i = 0; i < gfx_thumb_results_slot_count; i++)
   {
      if (!(name = gfx_thumbnail_results_find(
            gfx_thumb_results_slots[i].notes,
            content_path, label, system, type)))
         continue;
      own = gfx_thumbnail_results_find(gfx_thumb_results_slots[i].table,
            content_path, label, system, type);
      if (!own || !gfx_thumbnail_name_allowed(own, allow_non_png))
         return name;
   }
   for (i = 0; i < gfx_thumb_results_slot_count; i++)
      if (     (name = gfx_thumbnail_results_find(
               gfx_thumb_results_slots[i].table,
               content_path, label, system, type))
            && gfx_thumbnail_name_allowed(name, allow_non_png))
         return name;
   return NULL;
}

/* Its table, notes and marks; a late resolve of it is dropped. */
static void gfx_thumb_results_drop(const char *playlist_path)
{
   size_t i;
   gfx_thumb_results_slot_t *slot;

   if ((slot = gfx_thumb_results_slot(playlist_path)))
      gfx_thumb_results_slot_clear(slot);
   if ((i = gfx_thumb_results_parked_at(playlist_path))
         < gfx_thumb_results_parked_count)
      gfx_thumb_results_parked_free(i);
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
}

void gfx_thumbnail_results_forget(const char *playlist_path)
{
   char path[PATH_MAX_LENGTH];

   if (!playlist_path || !*playlist_path)
      return;
   gfx_thumb_results_drop(playlist_path);
   strlcpy(path, playlist_path, sizeof(path));
   if (strlcat(path, FILE_PATH_THUMBS_EXTENSION, sizeof(path)) < sizeof(path))
      filestream_delete(path);
}

void gfx_thumbnail_results_rename(const char *old_path,
      const char *new_path)
{
   char from[PATH_MAX_LENGTH];
   char *to;
   size_t i;

   if (!old_path || !*old_path || !new_path || !*new_path)
      return;
   gfx_thumb_results_drop(new_path);
   if ((i = gfx_thumb_results_parked_at(old_path))
         < gfx_thumb_results_parked_count)
   {
      if ((to = strdup(new_path)))
      {
         free(gfx_thumb_results_parked[i].playlist_path);
         gfx_thumb_results_parked[i].playlist_path = to;
      }
      else
         gfx_thumb_results_parked_free(i);
   }
   gfx_thumb_results_drop(old_path);
   if (     strlcpy(from, old_path, sizeof(from)) >= sizeof(from)
         || strlcat(from, FILE_PATH_THUMBS_EXTENSION, sizeof(from))
            >= sizeof(from)
         || !(to = (char*)malloc(PATH_MAX_LENGTH)))
      return;
   /* A missing cache fails the rename, which is fine */
   if (     strlcpy(to, new_path, PATH_MAX_LENGTH) < PATH_MAX_LENGTH
         && strlcat(to, FILE_PATH_THUMBS_EXTENSION, PATH_MAX_LENGTH)
            < PATH_MAX_LENGTH)
      filestream_rename(from, to);
   free(to);
}

bool gfx_thumbnail_results_hold(const char *playlist_path, unsigned hold)
{
   gfx_thumb_results_slot_t *slot = gfx_thumb_results_slot(playlist_path);
   if (!slot)
      return false;
   slot->hold |= hold;
   return true;
}

void gfx_thumbnail_results_release(unsigned hold)
{
   size_t i;
   gfx_thumb_results_slot_t *slot;
   for (i = 0; i < gfx_thumb_results_slot_count; i++)
   {
      slot = &gfx_thumb_results_slots[i];
      /* Shown until now: the newest of the others */
      if (slot->hold && !(slot->hold &= ~hold))
         slot->stamp = ++gfx_thumb_results_clock;
   }
   while ((slot = gfx_thumb_results_oldest(GFX_THUMB_RESULTS_KEPT + 1)))
      gfx_thumb_results_push_out(slot);
}

unsigned gfx_thumbnail_results_generation(void)
{
   return gfx_thumb_results_generation;
}

void gfx_thumbnail_results_deinit(void)
{
   size_t i;
   for (i = 0; i < gfx_thumb_results_slot_count; i++)
      gfx_thumb_results_slot_clear(&gfx_thumb_results_slots[i]);
   free(gfx_thumb_results_slots);
   for (i = 0; i < gfx_thumb_results_resolved_count; i++)
      free(gfx_thumb_results_resolved[i]);
   free(gfx_thumb_results_resolved);
   for (i = 0; i < gfx_thumb_results_evicted_count; i++)
      free(gfx_thumb_results_evicted[i]);
   free(gfx_thumb_results_evicted);
   while (gfx_thumb_results_parked_count)
      gfx_thumb_results_parked_free(0);
   free(gfx_thumb_results_parked);
   gfx_thumb_results_slots          = NULL;
   gfx_thumb_results_slot_count     = 0;
   gfx_thumb_results_parked         = NULL;
   gfx_thumb_results_resolved       = NULL;
   gfx_thumb_results_resolved_count = 0;
   gfx_thumb_results_evicted        = NULL;
   gfx_thumb_results_evicted_count  = 0;
   gfx_thumb_results_clock          = 0;
   gfx_thumb_results_generation++;
}
