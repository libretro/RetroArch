/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* First: whether the VFS maps files depends on HAVE_MMAP, which
 * vfs/vfs.h - pulled in by streams/file_stream.h - reads */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <math.h>

#include <vfs/vfs.h>

#include "../configuration.h"
#include "../msg_hash.h"
#include "../verbosity.h"
#include "font_driver.h"
#include "gfx_display.h"
#include "video_thread_wrapper.h"
#include <retro_atomic.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include <compat/strl.h>

/* ------------------------------------------------------------------
 * Shared font file bytes.
 *
 * Ozone builds six fonts and gfx_widgets three, and between them they
 * name two files: nine whole-file reads of two faces, every context
 * reset, which is twice per content load/close.  On a Vita that was
 * 21.91 MiB off the memory card and the same again in resident
 * buffers each time round.
 *
 * The bytes of one path are read once and handed to every font built
 * from it, and freed when the last of them is gone.  A rasterizer that
 * only reads what it is given (borrows_font_data) gets the shared
 * buffer; one that disposes of the bytes on its own schedule gets a
 * private copy - coretext, whose buffer is released by CoreGraphics
 * rather than at font teardown.
 * ------------------------------------------------------------------ */

typedef struct font_file_ref
{
   /* The list's: fixed once the entry is published */
   struct font_file_ref *next;
   /* Fixed once published; NULL for a private copy, which is never
    * in the list */
   char                 *path;
   /* Published by the store that makes refs positive */
   uint8_t              *data;
   /* The open file, when data is mapped from it rather than read */
   RFILE                *mapped;
   size_t                len;
   /* How many fonts hold the bytes; 0 with none read, or one of the
    * two states below while one thread reads them in or lets them
    * go. A thread that finds either takes a private copy rather than
    * waiting. */
   retro_atomic_int_t    refs;
} font_file_ref_t;

#define FONT_REF_LOADING  (-1)
#define FONT_REF_DROPPING (-2)

/* Entries are only ever added, at the head, and never taken out: there
 * is one per distinct font file, a handful, and an entry whose fonts
 * are gone holds no bytes. */
#ifdef RETRO_ATOMIC_HAS_PTR
typedef retro_atomic_ptr_t font_list_head_t;
#define FONT_HEAD_LOAD(h)       retro_atomic_load_acquire_ptr(h)
#define FONT_HEAD_CAS(h, o, n)  retro_atomic_cas_ptr((h), (o), (n))
#else
/* The fallback backend serves single-context targets */
typedef void *font_list_head_t;
#define FONT_HEAD_LOAD(h)       (*(h))
#define FONT_HEAD_CAS(h, o, n) \
   (*(h) == (void*)(o) ? (*(h) = (void*)(n), true) : false)
#endif

static font_list_head_t font_file_refs;
#define FONT_REFS_HEAD() \
   ((font_file_ref_t*)FONT_HEAD_LOAD(&font_file_refs))
#define FONT_REFS_PUSH(head, entry) \
   FONT_HEAD_CAS(&font_file_refs, (head), (entry))

/* Lets go of a face's bytes: closing the file unmaps them, or they were
 * read into memory of their own. */
static void font_file_data_free(RFILE *mapped, void *data)
{
#if defined(VFS_HAVE_FILE_MAPPING) && defined(HAVE_MMAP)
   if (mapped)
   {
      filestream_close(mapped);
      return;
   }
#else
   (void)mapped;
#endif
   free(data);
}

/* The bytes of @path, mapped or read into @ref */
static bool font_file_ref_read(font_file_ref_t *ref, const char *path)
{
   RFILE   *mapped = NULL;
   void    *data   = NULL;
   int64_t  len    = 0;

#if defined(VFS_HAVE_FILE_MAPPING) && defined(HAVE_MMAP)
   /* Mapped where there is mmap(): the pages of a face are only read in
    * as its glyphs are drawn, and stay the file's, so a fallback font
    * of megabytes costs what is used of it rather than all of it. Every
    * rasterizer only reads the bytes; one that keeps them is handed a
    * copy. The file has to be replaced rather than rewritten while it
    * is mapped - archive extraction renames each file into place - as
    * a mapping does not survive the file being truncated under it.
    * Win32 is left reading: a mapped file there cannot be replaced at
    * all, which would stop the assets updating. */
   {
      RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
      if (f)
      {
         int64_t        map_len = 0;
         int64_t        size    = filestream_get_size(f);
         const uint8_t *map     = filestream_get_mapped_ptr(f, &map_len);
         if (map && size > 0 && map_len == size)
         {
            mapped = f;
            data   = (void*)map;
            len    = size;
         }
         else
            filestream_close(f);
      }
   }
   if (!mapped)
#endif
   if (!filestream_read_file(path, &data, &len) || len <= 0)
   {
      if (data)
         free(data);
      return false;
   }

   ref->data   = (uint8_t*)data;
   ref->mapped = mapped;
   ref->len    = (size_t)len;
   return true;
}

/* The list's entry for @path, added if there is none */
static font_file_ref_t *font_file_ref_entry(const char *path)
{
   font_file_ref_t *added = NULL;

   for (;;)
   {
      font_file_ref_t *e;
      font_file_ref_t *head = FONT_REFS_HEAD();

      for (e = head; e; e = e->next)
         if (string_is_equal(e->path, path))
         {
            if (added)
            {
               free(added->path);
               free(added);
            }
            return e;
         }

      if (!added)
      {
         /* strlcpy rather than strdup: the latter is not declared
          * under a strict C89 compile, and this file is built that
          * way. */
         size_t path_len = strlen(path) + 1;
         if (!(added = (font_file_ref_t*)calloc(1, sizeof(*added))))
            return NULL;
         if (!(added->path = (char*)malloc(path_len)))
         {
            free(added);
            return NULL;
         }
         strlcpy(added->path, path, path_len);
         retro_atomic_int_init(&added->refs, 0);
      }
      added->next = head;
      if (FONT_REFS_PUSH(head, added))
         return added;
   }
}

/* A reference to the bytes of @path: the list's, shared, or a private
 * copy when another thread is reading them in or letting them go - a
 * redundant read in a case that hardly arises, and never a wait. */
static font_file_ref_t *font_file_ref_acquire(const char *path)
{
   font_file_ref_t *ref;
   font_file_ref_t *entry = font_file_ref_entry(path);

   while (entry)
   {
      int r = retro_atomic_load_acquire_int(&entry->refs);
      if (r > 0)
      {
         if (retro_atomic_cas_int(&entry->refs, r, r + 1))
            return entry;
      }
      else if (r == 0)
      {
         if (retro_atomic_cas_int(&entry->refs, 0, FONT_REF_LOADING))
         {
            if (!font_file_ref_read(entry, path))
            {
               retro_atomic_store_release_int(&entry->refs, 0);
               return NULL;
            }
            retro_atomic_store_release_int(&entry->refs, 1);
            return entry;
         }
      }
      else
         break;
   }

   if (!(ref = (font_file_ref_t*)calloc(1, sizeof(*ref))))
      return NULL;
   if (!font_file_ref_read(ref, path))
   {
      free(ref);
      return NULL;
   }
   retro_atomic_int_init(&ref->refs, 1);
   return ref;
}

static void font_file_ref_release(font_file_ref_t *ref)
{
   if (!ref)
      return;

   if (!ref->path)
   {
      font_file_data_free(ref->mapped, ref->data);
      free(ref);
      return;
   }

   for (;;)
   {
      int r = retro_atomic_load_acquire_int(&ref->refs);
      if (r > 1)
      {
         if (retro_atomic_cas_int(&ref->refs, r, r - 1))
            return;
      }
      /* The last: the bytes go, and the entry stays for the next */
      else if (retro_atomic_cas_int(&ref->refs, 1, FONT_REF_DROPPING))
      {
         font_file_data_free(ref->mapped, ref->data);
         ref->data   = NULL;
         ref->mapped = NULL;
         ref->len    = 0;
         retro_atomic_store_release_int(&ref->refs, 0);
         return;
      }
   }
}

/* ------------------------------------------------------------------
 * Glyph cache.
 *
 * Every font a video driver draws with is one of these: a 16 x 16 grid
 * of cells in one atlas, a hash from codepoint to cell, least recently
 * used eviction and a dirty rectangle for the driver to upload. The
 * rasterizer behind it only draws a glyph into the cell it is given.
 *
 * A codepoint the font has no glyph for is drawn from a fallback face
 * picked by its Unicode block - the CJK, Korean, Thai and general
 * fallback fonts the assets ship in pkg/ - and cached like any other.
 * A fallback file is read the first time a codepoint needs it, on a
 * thread of its own, and shared by every font from then on; until the
 * bytes are there the codepoint draws blank and is not cached, so it is
 * looked up again on the next draw. A hit costs what it always has.
 * ------------------------------------------------------------------ */

#define FONT_CACHE_ROWS      16
#define FONT_CACHE_COLS      16
#define FONT_CACHE_SLOTS     (FONT_CACHE_ROWS * FONT_CACHE_COLS)
/* Each growth doubles the columns and rows: 1024, then 4096 cells */
#define FONT_CACHE_GROWTHS   2
/* The oldest cells a search for one to take remembers */
#define FONT_CACHE_CANDIDATES 32
/* Padding between cells, so linear filtering does not bleed one
 * glyph into the next */
#define FONT_CACHE_PADDING   1
#define FONT_CACHE_HASH_SIZE 0x100
/* A bucket of @c's map: as many buckets as cells, so a chain is a cell
 * long on average however far the cache has grown */
#define FONT_CACHE_HASH(c, code) \
   ((c)->map[((code) ^ ((code) >> 8)) & (c)->map_mask])
/* No codepoint: an evicted cell whose redraw failed */
#define FONT_CACHE_NO_CODE   0xFFFFFFFFu

enum font_fallback_id
{
   FONT_FALLBACK_CJK = 0,
   FONT_FALLBACK_KOREAN,
   FONT_FALLBACK_THAI,
   FONT_FALLBACK_ETHIOPIC,
   FONT_FALLBACK_GENERAL,
   FONT_FALLBACK_COUNT
};

enum font_fallback_state
{
   FONT_FALLBACK_IDLE = 0,
   FONT_FALLBACK_LOADING,
   FONT_FALLBACK_READY,
   FONT_FALLBACK_FAILED
};

/* The fallback files load on a worker, which needs threads and a
 * lock-free compare-and-swap; without them no fallback font loads. */
#if defined(HAVE_THREADS) && defined(retro_atomic_cas_int) \
 && defined(RETRO_ATOMIC_LOCK_FREE)
#define FONT_FALLBACK_LOADS 1
#endif

#ifdef FONT_FALLBACK_LOADS
static const char * const font_fallback_files[FONT_FALLBACK_COUNT] = {
   "chinese-fallback-font.ttf",
   "korean-fallback-font.ttf",
   "thai-fallback-font.ttf",
   "ethiopic-fallback-font.ttf",
   "fallback-font.ttf"
};
#endif

/* Where the fallback files are: the assets' pkg/ directory, as the menu
 * and the widgets give it when they set up their language fonts, from
 * whichever thread builds them. Each value is a node of its own,
 * published at the head and kept, so a reader on any thread reads the
 * head's string; a run sets one or two. */
typedef struct font_pkg_dir
{
   struct font_pkg_dir *next;
   char                 path[1];
} font_pkg_dir_t;

static font_list_head_t font_fallback_pkg_dirs;

#ifdef FONT_FALLBACK_LOADS
static const char *font_fallback_pkg_dir(void)
{
   font_pkg_dir_t *head = (font_pkg_dir_t*)
      FONT_HEAD_LOAD(&font_fallback_pkg_dirs);
   return head ? head->path : "";
}
#endif

static void font_fallback_pkg_dir_set(const char *dir)
{
   size_t          len  = strlen(dir) + 1;
   font_pkg_dir_t *node = NULL;

   for (;;)
   {
      font_pkg_dir_t *head = (font_pkg_dir_t*)
         FONT_HEAD_LOAD(&font_fallback_pkg_dirs);
      if (head && string_is_equal(head->path, dir))
      {
         free(node);
         return;
      }
      if (!node && !(node = (font_pkg_dir_t*)malloc(
                  sizeof(*node) + len)))
         return;
      strlcpy(node->path, dir, len);
      node->next = head;
      if (FONT_HEAD_CAS(&font_fallback_pkg_dirs, head, node))
         return;
   }
}

/* One read per fallback file per process. The bytes are published
 * before the state flips to READY, and are kept for the life of the
 * process once read. */
#ifdef FONT_FALLBACK_LOADS
static retro_atomic_int_t font_fallback_state[FONT_FALLBACK_COUNT];
static font_file_ref_t   *font_fallback_ref[FONT_FALLBACK_COUNT];
#endif

typedef struct font_cache_slot
{
   struct font_cache_slot *next;
   struct font_glyph glyph;
   uint32_t charcode;
   unsigned last_used;
   /* font_frame_epoch when it was last looked up */
   unsigned last_frame;
} font_cache_slot_t;

typedef struct font_cache
{
   /* The hash of the cells in use: map_small until a growth makes a
    * larger one */
   font_cache_slot_t **map;
   unsigned map_mask;
   font_cache_slot_t *map_small[FONT_CACHE_HASH_SIZE];
   font_cache_slot_t slots[FONT_CACHE_SLOTS];
   /* The cells each growth added, so no cell ever moves */
   font_cache_slot_t *grown[FONT_CACHE_GROWTHS];
   /* The oldest cells the last search found, oldest first, with when
    * each was last used then: a later miss in the frame the search ran
    * in takes the first still untouched instead of searching again.
    * Every other cell was younger, or looked up in that frame. */
   font_cache_slot_t *cand[FONT_CACHE_CANDIDATES];
   unsigned cand_used[FONT_CACHE_CANDIDATES];
   unsigned grown_len[FONT_CACHE_GROWTHS];
   unsigned growths;
   unsigned cols;
   unsigned rows;
   /* A frame wanted more cells than there were: grow when the next one
    * asks for the atlas */
   unsigned grow_frame;
   unsigned cand_count;
   unsigned cand_next;
   unsigned cand_frame;
   /* The frame a search found every cell busy in, when full_set:
    * no cell is free again before the next one */
   unsigned full_frame;
   bool full_set;
   bool grow_wanted;
   struct font_atlas atlas;
   const font_rasterizer_t *rast;
   void *face;
   /* The bytes behind the face, when they are shared */
   font_file_ref_t *ref;
   void *fallback_face[FONT_FALLBACK_COUNT];
   const font_rasterizer_t *fallback_rast[FONT_FALLBACK_COUNT];
   /* Drawn for a codepoint whose fallback file is still being read */
   struct font_glyph pending_glyph;
   struct font_line_metrics metrics;
   float font_size;
   unsigned cell_dims;
   unsigned usage_counter;
   /* Per fallback: opened, or found to be of no use to this font */
   uint8_t fallback_done[FONT_FALLBACK_COUNT];
} font_cache_t;

/* The fallback a codepoint the font lacks is drawn from, or -1 */
static int font_fallback_for(uint32_t code)
{
   if (code < 0x80)
      return -1;
   /* Hangul jamo, compatibility jamo, extended jamo and syllables */
   if (     (code >= 0x1100 && code <= 0x11FF)
         || (code >= 0x3130 && code <= 0x318F)
         || (code >= 0xA960 && code <= 0xA97F)
         || (code >= 0xAC00 && code <= 0xD7FF))
      return FONT_FALLBACK_KOREAN;
   if (code >= 0x0E00 && code <= 0x0E7F)
      return FONT_FALLBACK_THAI;
   /* Ethiopic and its supplement and extensions */
   if (     (code >= 0x1200 && code <= 0x139F)
         || (code >= 0x2D80 && code <= 0x2DDF)
         || (code >= 0xAB00 && code <= 0xAB2F)
         || (code >= 0x1E7E0 && code <= 0x1E7FF))
      return FONT_FALLBACK_ETHIOPIC;
   /* CJK radicals, punctuation, kana, bopomofo, the unified ideographs
    * and their extensions, compatibility forms and the full-width
    * forms */
   if (     (code >= 0x2E80 && code <= 0x9FFF)
         || (code >= 0xF900 && code <= 0xFAFF)
         || (code >= 0xFE30 && code <= 0xFE4F)
         || (code >= 0xFF00 && code <= 0xFFEF)
         || (code >= 0x20000 && code <= 0x3FFFF))
      return FONT_FALLBACK_CJK;
   return FONT_FALLBACK_GENERAL;
}

/* Where a codepoint goes when the fallback font_fallback_for() picked
 * has no glyph for it, or -1. The ranges are those where, with the
 * fallback fonts the assets ship, the other face draws what the picked
 * one lacks - Hangul jamo and circled numbers in the CJK face, circled
 * Hangul in the Korean one, and so on - so a codepoint no face has
 * does not have every fallback font read in search of it. */
static int font_fallback_next_for(uint32_t code)
{
   /* Hangul jamo and compatibility jamo; enclosed alphanumerics (①);
    * vertical and small form variants */
   if (     (code >= 0x1100 && code <= 0x11FF)
         || (code >= 0x3130 && code <= 0x318F)
         || (code >= 0x2460 && code <= 0x24FF)
         || (code >= 0xFE10 && code <= 0xFE1F)
         || (code >= 0xFE50 && code <= 0xFE6F))
      return FONT_FALLBACK_CJK;
   /* Enclosed CJK letters and months: circled Hangul */
   if (code >= 0x3200 && code <= 0x32FF)
      return FONT_FALLBACK_KOREAN;
   /* Yijing hexagram symbols */
   if (code >= 0x4DC0 && code <= 0x4DFF)
      return FONT_FALLBACK_GENERAL;
   return -1;
}

#ifdef FONT_FALLBACK_LOADS
typedef struct font_fallback_load
{
   int  id;
   char path[PATH_MAX_LENGTH];
} font_fallback_load_t;

static void font_fallback_load_thread(void *userdata)
{
   font_fallback_load_t *load = (font_fallback_load_t*)userdata;
   font_file_ref_t      *ref  = font_file_ref_acquire(load->path);
   font_fallback_ref[load->id] = ref;
   retro_atomic_store_release_int(&font_fallback_state[load->id],
         ref ? FONT_FALLBACK_READY : FONT_FALLBACK_FAILED);
   free(load);
}

/* Starts the read of fallback @id unless it has been started before */
static void font_fallback_request(int id)
{
   sthread_t            *thread;
   font_fallback_load_t *load;

   if (!retro_atomic_cas_int(&font_fallback_state[id],
            FONT_FALLBACK_IDLE, FONT_FALLBACK_LOADING))
      return;

   if (!(load = (font_fallback_load_t*)malloc(sizeof(*load))))
   {
      retro_atomic_store_release_int(&font_fallback_state[id],
            FONT_FALLBACK_FAILED);
      return;
   }
   load->id = id;
   if (*font_fallback_pkg_dir())
      fill_pathname_join_special(load->path, font_fallback_pkg_dir(),
            font_fallback_files[id], sizeof(load->path));
   else
      *load->path = '\0';

   if (     !*load->path
         || !(thread = sthread_create(font_fallback_load_thread, load)))
   {
      free(load);
      /* Nowhere to read it from yet: let a later miss try again once
       * the pkg directory is known */
      retro_atomic_store_release_int(&font_fallback_state[id],
            *font_fallback_pkg_dir() ? FONT_FALLBACK_FAILED
                                   : FONT_FALLBACK_IDLE);
      return;
   }
   sthread_detach(thread);
}
#endif

/* The fallback face this font draws codepoints of fallback @id from:
 * NULL with *pending set while its file is still being read, NULL
 * without when there is none to be had. */
static void *font_cache_fallback_face(font_cache_t *c, int id,
      const font_rasterizer_t **rast, bool *pending)
{
#ifdef FONT_FALLBACK_LOADS
   font_file_ref_t *ref;
   uint8_t         *data;

   *pending = false;
   if (c->fallback_done[id])
   {
      *rast = c->fallback_rast[id];
      return c->fallback_face[id];
   }

   switch (retro_atomic_load_acquire_int(&font_fallback_state[id]))
   {
      case FONT_FALLBACK_READY:
         break;
      case FONT_FALLBACK_FAILED:
         c->fallback_done[id] = 1;
         return NULL;
      default:
         font_fallback_request(id);
         *pending = (retro_atomic_load_acquire_int(&font_fallback_state[id])
               == FONT_FALLBACK_LOADING);
         return NULL;
   }

   c->fallback_done[id] = 1;
   ref                  = font_fallback_ref[id];
   /* The font already is this file: nothing more to find in it */
   if (!ref || ref == c->ref)
      return NULL;

   /* A rasterizer that takes the bytes would need a private copy of a
    * file of megabytes for every font; stb reads them in place. */
   *rast = c->rast->borrows_font_data ? c->rast : &stb_font_rasterizer;
   data  = ref->data;
   if (!(c->fallback_face[id] = (*rast)->init(data, ref->len, 0,
               c->font_size)))
      return NULL;
   c->fallback_rast[id] = *rast;
   return c->fallback_face[id];
#else
   (void)c;
   (void)id;
   (void)rast;
   *pending = false;
   return NULL;
#endif
}

/* Advanced once a video frame by font_driver_frame_begin(). A cell
 * looked up in the current frame may already have glyphs queued from
 * it, so it is not handed to another codepoint until the frame is over:
 * reusing it would draw the new glyph in the earlier one's place. The
 * same holds for the atlas's inflight frames before it, which the GPU
 * may still be drawing. 0 until the first frame, which leaves plain LRU
 * in place for a cache used outside the video loop.  It is only ever compared, never used
 * to publish anything, so it is read relaxed: a plain load on every
 * backend, where the glyph lookup reads it once a glyph. */
static retro_atomic_int_t font_frame_epoch;

void font_driver_frame_begin(void)
{
   retro_atomic_fetch_add_int(&font_frame_epoch, 1);
}

/* Whether @s was looked up in @frame or in one of the atlas's inflight
 * frames before it; 0 for a cell never looked up in a frame */
#define FONT_CACHE_BUSY(c, s, frame) \
   (   (frame) && (s)->last_frame \
    && (unsigned)((frame) - (s)->last_frame) <= (c)->atlas.inflight)

/* Searches every cell for the least recently used one not busy in
 * @frame, keeping the oldest few in c->cand for the misses after */
static font_cache_slot_t *font_cache_search(font_cache_t *c, unsigned frame)
{
   unsigned i, b, k;
   unsigned n = 0;

   for (b = 0; b <= c->growths; b++)
   {
      font_cache_slot_t *blk = b ? c->grown[b - 1] : c->slots;
      unsigned           len = b ? c->grown_len[b - 1] : FONT_CACHE_SLOTS;
      for (i = 0; i < len; i++)
      {
         /* Unsigned subtraction handles the counter wrapping */
         unsigned age = c->usage_counter - blk[i].last_used;
         if (FONT_CACHE_BUSY(c, &blk[i], frame))
            continue;
         if (     n == FONT_CACHE_CANDIDATES
               && age <= c->usage_counter - c->cand_used[n - 1])
            continue;
         /* After any of the same age, so the first found goes first */
         for (k = (n < FONT_CACHE_CANDIDATES) ? n++ : n - 1;
               k > 0 && age > c->usage_counter - c->cand_used[k - 1]; k--)
         {
            c->cand[k]      = c->cand[k - 1];
            c->cand_used[k] = c->cand_used[k - 1];
         }
         c->cand[k]      = &blk[i];
         c->cand_used[k] = blk[i].last_used;
      }
   }

   c->cand_count = n;
   c->cand_next  = n ? 1 : 0;
   c->cand_frame = frame;
   return n ? c->cand[0] : NULL;
}

/* The least recently used cell not busy in this frame, taken out of
 * the hash; NULL when every cell is */
static font_cache_slot_t *font_cache_take_slot(font_cache_t *c)
{
   unsigned frame            = (unsigned)retro_atomic_load_relaxed_int(
         &font_frame_epoch);
   font_cache_slot_t *victim = NULL;
   font_cache_slot_t **link;

   if (c->full_set && c->full_frame == frame)
      return NULL;

   if (c->cand_frame == frame)
   {
      while (c->cand_next < c->cand_count)
      {
         font_cache_slot_t *s = c->cand[c->cand_next];
         unsigned        used = c->cand_used[c->cand_next++];
         if (s->last_used == used && !FONT_CACHE_BUSY(c, s, frame))
         {
            victim = s;
            break;
         }
      }
   }
   if (!victim)
      victim = font_cache_search(c, frame);

   if (!victim)
   {
      if (frame)
      {
         c->full_set   = true;
         c->full_frame = frame;
      }
      /* Room is made at the start of a later frame, when nothing drawn
       * from the atlas at its present size is still waiting */
      if (c->atlas.max_dims && c->growths < FONT_CACHE_GROWTHS)
      {
         c->grow_wanted = true;
         c->grow_frame  = frame;
      }
      return NULL;
   }

   if (victim->charcode != FONT_CACHE_NO_CODE)
   {
      for (link = &FONT_CACHE_HASH(c, victim->charcode);
            *link; link = &(*link)->next)
      {
         if (*link == victim)
         {
            *link = victim->next;
            break;
         }
      }
   }
   victim->charcode   = FONT_CACHE_NO_CODE;
   victim->next       = NULL;
   victim->last_frame = frame;
   return victim;
}

/* @size bytes for the atlas's samples, zeroed, from its storage */
static uint8_t *font_atlas_alloc(struct font_atlas *atlas, size_t size)
{
   uint8_t *buf;
   if (!atlas->storage)
      return (uint8_t*)calloc(1, size);
   if ((buf = (uint8_t*)atlas->storage(atlas->storage_user, size)))
      memset(buf, 0, size);
   return buf;
}

/* Lets go of a buffer the atlas's samples were in */
static void font_atlas_release(struct font_atlas *atlas, uint8_t *buffer)
{
   if (atlas->storage_free)
      atlas->storage_free(atlas->storage_user, buffer);
   else
      free(buffer);
}

bool font_atlas_place(struct font_atlas *atlas,
      void *(*storage)(void *user, size_t size),
      void (*storage_free)(void *user, void *buffer), void *user)
{
   size_t   size = (size_t)atlas->width * atlas->height
                 * ((atlas->format == FONT_ATLAS_FORMAT_A16) ? 2 : 1);
   uint8_t *buf;
   if (!storage || !(buf = (uint8_t*)storage(user, size)))
      return false;
   memcpy(buf, atlas->buffer, size);
   font_atlas_release(atlas, atlas->buffer);
   atlas->buffer       = buf;
   atlas->storage      = storage;
   atlas->storage_free = storage_free;
   atlas->storage_user = user;
   return true;
}

void font_dirty_rects_add(unsigned (*r)[2], unsigned *n,
      unsigned pos, unsigned end)
{
   unsigned i, j, bi = 0, bj = 1;
   unsigned cand[FONT_ATLAS_DIRTY_MAX + 1][2];
   size_t   best_add = (size_t)-1;

   if (*n < FONT_ATLAS_DIRTY_MAX)
   {
      r[*n][0]   = pos;
      r[(*n)++][1] = end;
      return;
   }

   /* Full: of the rectangles and the new one, the two whose union adds
    * least are joined */
   memcpy(cand, r, sizeof(cand[0]) * FONT_ATLAS_DIRTY_MAX);
   cand[FONT_ATLAS_DIRTY_MAX][0] = pos;
   cand[FONT_ATLAS_DIRTY_MAX][1] = end;
   for (i = 0; i < FONT_ATLAS_DIRTY_MAX; i++)
      for (j = i + 1; j <= FONT_ATLAS_DIRTY_MAX; j++)
      {
         size_t both = VIDEO_SCALE_AREA(
                  VIDEO_SCALE_MAX(cand[i][1], cand[j][1])
                - VIDEO_SCALE_MIN(cand[i][0], cand[j][0]));
         size_t each = VIDEO_SCALE_AREA(cand[i][1] - cand[i][0])
                     + VIDEO_SCALE_AREA(cand[j][1] - cand[j][0]);
         size_t add  = both > each ? both - each : 0;
         if (add < best_add)
         {
            bi       = i;
            bj       = j;
            best_add = add;
         }
      }
   cand[bi][0] = VIDEO_SCALE_MIN(cand[bi][0], cand[bj][0]);
   cand[bi][1] = VIDEO_SCALE_MAX(cand[bi][1], cand[bj][1]);
   cand[bj][0] = cand[FONT_ATLAS_DIRTY_MAX][0];
   cand[bj][1] = cand[FONT_ATLAS_DIRTY_MAX][1];
   memcpy(r, cand, sizeof(cand[0]) * FONT_ATLAS_DIRTY_MAX);
}

/* @pos and @dims add packed: the atlas never outgrows
 * VIDEO_SCALE_DIM_MAX, so nothing carries between halves */
static void font_cache_dirty_cell(struct font_atlas *atlas,
      unsigned pos, unsigned dims)
{
   unsigned end = pos + dims;

   if (!atlas->dirty)
   {
      atlas->dirty_xy0        = pos;
      atlas->dirty_xy1        = end;
      atlas->dirty_rect[0][0] = pos;
      atlas->dirty_rect[0][1] = end;
      atlas->dirty_rects      = 1;
      atlas->dirty            = true;
      return;
   }
   atlas->dirty_xy0 = VIDEO_SCALE_MIN(atlas->dirty_xy0, pos);
   atlas->dirty_xy1 = VIDEO_SCALE_MAX(atlas->dirty_xy1, end);

   /* Only the corners describe a region someone else made */
   if (atlas->dirty_rects)
      font_dirty_rects_add(atlas->dirty_rect, &atlas->dirty_rects, pos, end);
}

/* Out of line, so a hit needs none of the registers a miss saves */
VIDEO_NOINLINE static const struct font_glyph *font_cache_miss(
      font_cache_t *c, uint32_t code)
{
   unsigned gi                   = c->rast->glyph_index(c->face, code);
   const font_rasterizer_t *rast = c->rast;
   void *face                    = c->face;
   font_cache_slot_t *slot;
   size_t   esz;
   uint8_t *dst;

   if (!gi)
   {
      int ids[2];
      int i;
      ids[0] = font_fallback_for(code);
      ids[1] = (ids[0] >= 0) ? font_fallback_next_for(code) : -1;
      for (i = 0; i < 2 && ids[i] >= 0; i++)
      {
         bool pending                     = false;
         const font_rasterizer_t *fb_rast = NULL;
         void *fb_face = font_cache_fallback_face(c, ids[i], &fb_rast,
               &pending);
         unsigned fb_gi;

         if (pending)
            return &c->pending_glyph;
         if (fb_face && (fb_gi = fb_rast->glyph_index(fb_face, code)))
         {
            rast = fb_rast;
            face = fb_face;
            gi   = fb_gi;
            break;
         }
      }
   }

   /* Every cell already holds a glyph drawn this frame: this one goes
    * undrawn - the caller draws its stand-in - rather than overwrite
    * one, and is drawn once a frame frees a cell */
   if (!(slot = font_cache_take_slot(c)))
      return NULL;
   esz    = (c->atlas.format == FONT_ATLAS_FORMAT_A16)
      ? sizeof(uint16_t) : sizeof(uint8_t);
   dst    = c->atlas.buffer
      + ((size_t)VIDEO_SCALE_W(slot->glyph.atlas_pos)
      +  (size_t)VIDEO_SCALE_H(slot->glyph.atlas_pos) * c->atlas.width)
      * esz;

   if (!rast->render_glyph(face, code, gi, dst, c->atlas.width,
            c->cell_dims, c->atlas.format, &slot->glyph))
   {
      /* Nothing to draw: the cell goes back as the oldest there is,
       * free again at once - no frame has drawn from it since it was
       * taken */
      slot->last_used  = c->usage_counter - 0x80000000u;
      slot->last_frame = 0;
      c->cand_count    = 0;
      return NULL;
   }

   slot->charcode               = code;
   slot->next                   = FONT_CACHE_HASH(c, code);
   FONT_CACHE_HASH(c, code)     = slot;
   slot->last_used              = c->usage_counter++;
   font_cache_dirty_cell(&c->atlas, slot->glyph.atlas_pos, c->cell_dims);
   return &slot->glyph;
}

static const struct font_glyph *font_cache_get_glyph(void *data,
      uint32_t code)
{
   font_cache_t      *c = (font_cache_t*)data;
   font_cache_slot_t *slot;

   if (!c)
      return NULL;

   for (slot = FONT_CACHE_HASH(c, code); slot; slot = slot->next)
   {
      if (slot->charcode == code)
      {
         slot->last_used  = c->usage_counter++;
         slot->last_frame = (unsigned)retro_atomic_load_relaxed_int(
               &font_frame_epoch);
         return &slot->glyph;
      }
   }

   return font_cache_miss(c, code);
}

/* Makes the map as many buckets as there are cells and puts every cell
 * in use in it; without the memory, the map stays as it was */
static void font_cache_rehash(font_cache_t *c)
{
   unsigned b, i;
   unsigned cells          = c->cols * c->rows;
   font_cache_slot_t **map = (font_cache_slot_t**)calloc(cells,
         sizeof(*map));

   if (!map)
      return;
   if (c->map != c->map_small)
      free(c->map);
   c->map      = map;
   c->map_mask = cells - 1;

   for (b = 0; b <= c->growths; b++)
   {
      font_cache_slot_t *blk = b ? c->grown[b - 1] : c->slots;
      unsigned           len = b ? c->grown_len[b - 1] : FONT_CACHE_SLOTS;
      for (i = 0; i < len; i++)
      {
         if (blk[i].charcode == FONT_CACHE_NO_CODE)
            continue;
         blk[i].next = FONT_CACHE_HASH(c, blk[i].charcode);
         FONT_CACHE_HASH(c, blk[i].charcode) = &blk[i];
      }
   }
}

/* Doubles the columns and the rows, leaving every cell where it was so
 * no glyph's offsets change; the new cells start empty. False, with
 * nothing changed, when the atlas would outgrow the consumer's texture
 * or memory runs out. */
static bool font_cache_grow(font_cache_t *c)
{
   unsigned x, y, i;
   unsigned cell_w     = VIDEO_SCALE_W(c->cell_dims);
   unsigned cell_h     = VIDEO_SCALE_H(c->cell_dims);
   unsigned cols       = c->cols * 2;
   unsigned rows       = c->rows * 2;
   unsigned width      = (cell_w + FONT_CACHE_PADDING) * cols;
   unsigned height     = (cell_h + FONT_CACHE_PADDING) * rows;
   size_t   esz        = (c->atlas.format == FONT_ATLAS_FORMAT_A16)
      ? sizeof(uint16_t) : sizeof(uint8_t);
   unsigned added      = cols * rows - c->cols * c->rows;
   uint8_t *buffer;
   font_cache_slot_t *blk;

   if (     c->growths >= FONT_CACHE_GROWTHS
         || width  > VIDEO_SCALE_W(c->atlas.max_dims)
         || height > VIDEO_SCALE_H(c->atlas.max_dims))
      return false;
   if (!(blk = (font_cache_slot_t*)calloc(added, sizeof(*blk))))
      return false;
   if (!(buffer = font_atlas_alloc(&c->atlas,
               (size_t)height * width * esz)))
   {
      free(blk);
      return false;
   }

   for (y = 0; y < c->atlas.height; y++)
      memcpy(buffer + (size_t)y * width * esz,
            c->atlas.buffer + (size_t)y * c->atlas.width * esz,
            (size_t)c->atlas.width * esz);

   for (i = 0, y = 0; y < rows; y++)
   {
      for (x = 0; x < cols; x++)
      {
         if (x < c->cols && y < c->rows)
            continue;
         blk[i].charcode             = FONT_CACHE_NO_CODE;
         blk[i].glyph.atlas_pos      = VIDEO_SCALE_PACK(
               x * (cell_w + FONT_CACHE_PADDING),
               y * (cell_h + FONT_CACHE_PADDING));
         /* Unused: older than any cell in use */
         blk[i].last_used            = c->usage_counter - 0x80000000u;
         i++;
      }
   }

   font_atlas_release(&c->atlas, c->atlas.buffer);
   c->atlas.buffer             = buffer;
   c->atlas.width              = width;
   c->atlas.height             = height;
   c->grown[c->growths]        = blk;
   c->grown_len[c->growths++]  = added;
   c->cols                     = cols;
   c->rows                     = rows;
   c->cand_count               = 0;
   font_cache_rehash(c);
   /* All of it, for a consumer making its texture anew */
   c->atlas.dirty              = true;
   c->atlas.dirty_xy0          = 0;
   c->atlas.dirty_xy1          = VIDEO_SCALE_PACK(width, height);
   c->atlas.dirty_rects        = 0;
   return true;
}

/* The atlas, grown first when a past frame ran out of cells: the
 * consumer asks for it before drawing anything in a frame, so nothing
 * drawn from the atlas at its old size is still waiting when its size
 * changes. */
static struct font_atlas *font_cache_get_atlas(void *data)
{
   font_cache_t *c = (font_cache_t*)data;
   if (!c)
      return NULL;
   if (c->grow_wanted && (unsigned)retro_atomic_load_relaxed_int(
            &font_frame_epoch) != c->grow_frame)
   {
      c->grow_wanted = false;
      font_cache_grow(c);
   }
   return &c->atlas;
}

static void font_cache_get_line_metrics(void *data,
      struct font_line_metrics **metrics)
{
   font_cache_t *c = (font_cache_t*)data;
   if (c)
      *metrics = &c->metrics;
}

static void font_cache_free(void *data)
{
   unsigned i;
   font_cache_t *c = (font_cache_t*)data;

   if (!c)
      return;

   for (i = 0; i < FONT_FALLBACK_COUNT; i++)
      if (c->fallback_face[i])
         c->fallback_rast[i]->free(c->fallback_face[i]);
   if (c->face)
      c->rast->free(c->face);
   if (c->ref)
   {
      font_file_ref_release(c->ref);
   }
   for (i = 0; i < c->growths; i++)
      free(c->grown[i]);
   if (c->map != c->map_small)
      free(c->map);
   font_atlas_release(&c->atlas, c->atlas.buffer);
   free(c);
}

/* What the video drivers' fonts draw through */
static const font_renderer_driver_t font_cache_driver = {
   NULL,
   font_cache_get_atlas,
   font_cache_get_glyph,
   font_cache_free,
   NULL,
   "font_cache",
   font_cache_get_line_metrics,
   true
};

/* Builds the cache around an opened face; takes @face and @ref */
static font_cache_t *font_cache_new(const font_rasterizer_t *rast,
      void *face, font_file_ref_t *ref, float font_size,
      enum font_atlas_format fmt)
{
   unsigned x, y, i;
   unsigned cell_w, cell_h;
   font_cache_t *c = (font_cache_t*)calloc(1, sizeof(*c));

   if (!c)
      goto error;

   c->rast      = rast;
   c->map       = c->map_small;
   c->map_mask  = FONT_CACHE_HASH_SIZE - 1;
   c->face      = face;
   c->ref       = ref;
   c->font_size = font_size;
   c->cell_dims = rast->cell_dims(face);
   cell_w       = VIDEO_SCALE_W(c->cell_dims);
   cell_h       = VIDEO_SCALE_H(c->cell_dims);

   /* The rasterizers bound their cells; the atlas stays within common
    * texture limits and its size cannot overflow */
   if (!cell_w || !cell_h || cell_w > 255 || cell_h > 255)
      goto error;

   c->cols         = FONT_CACHE_COLS;
   c->rows         = FONT_CACHE_ROWS;
   c->atlas.width  = (cell_w + FONT_CACHE_PADDING) * FONT_CACHE_COLS;
   c->atlas.height = (cell_h + FONT_CACHE_PADDING) * FONT_CACHE_ROWS;
   c->atlas.format = fmt;
   if (!(c->atlas.buffer = (uint8_t*)calloc(c->atlas.height,
               (size_t)c->atlas.width
               * ((fmt == FONT_ATLAS_FORMAT_A16) ? 2 : 1))))
      goto error;

   for (i = 0, y = 0; y < FONT_CACHE_ROWS; y++)
   {
      for (x = 0; x < FONT_CACHE_COLS; x++, i++)
      {
         c->slots[i].charcode             = FONT_CACHE_NO_CODE;
         c->slots[i].glyph.atlas_pos      = VIDEO_SCALE_PACK(
               x * (cell_w + FONT_CACHE_PADDING),
               y * (cell_h + FONT_CACHE_PADDING));
         /* Unused: older than any cell in use */
         c->slots[i].last_used            = c->usage_counter - 0x80000000u;
      }
   }

   rast->get_line_metrics(face, &c->metrics);
   c->pending_glyph.advance_x = (int)cell_w;

   /* Printable ASCII up front; control characters are never drawn */
   for (i = 32; i < 127; i++)
      font_cache_get_glyph(c, i);

   return c;

error:
   if (c)
   {
      free(c->atlas.buffer);
      free(c);
   }
   rast->free(face);
   if (ref)
   {
      font_file_ref_release(ref);
   }
   return NULL;
}

/* Monotonic counter incremented whenever any font instance is
 * freed. Consumers that cache per-font derived data (e.g. the
 * smooth ticker glyph width cache in gfx_animation.c) key their
 * entries on this value: font_data_t pointers can be recycled by
 * the allocator across free/create cycles, so pointer equality
 * alone cannot prove a cached entry still describes a live font.
 * Atomic because the main thread bumps it while the threaded video
 * worker reads it, drawing widgets during content. */
static retro_atomic_int_t font_driver_generation
   = RETRO_ATOMIC_INT_INITIALIZER(0);

/* A font belongs to the thread that made it: the main thread, or the
 * threaded video wrapper's thread, which makes the OSD font and the
 * widgets' fonts. Each thread keeps its own fonts - the list of the
 * live ones, the retired ones waiting out their frames, the language
 * they were last made for - so no list is shared and none is locked.
 * A font is freed on its own thread: one retired elsewhere is handed
 * to it (font_driver_free_deferred()). */
enum
{
   FONT_OWNER_MAIN = 0,
   FONT_OWNER_VIDEO,
   FONT_OWNER_COUNT
};

static unsigned font_driver_owner_here(void)
{
#ifdef HAVE_THREADS
   if (video_thread_is_video_thread())
      return FONT_OWNER_VIDEO;
#endif
   return FONT_OWNER_MAIN;
}

/* Every live font of each thread, so it can be rebuilt when the file
 * behind it should change. Singly linked through font_data_t::next. */
static font_data_t *font_live[FONT_OWNER_COUNT];

/* Bumped by font_driver_reload_fonts(); each thread rebuilds its fonts
 * that follow the language when it sees a new value. */
static retro_atomic_int_t font_language_epoch
   = RETRO_ATOMIC_INT_INITIALIZER(0);
static int font_language_seen[FONT_OWNER_COUNT];

/* Fonts whose replacement has already been built, waiting out the
 * frames in which the GPU may still be reading their atlas.
 *
 * A caller that rebuilds a font from the menu render path runs before
 * the video driver's frame function in the same runloop iteration, so
 * a handle released there can still be referenced by a command list
 * that has not been submitted, let alone completed. On a driver with
 * deferred submission that is a crash: D3D12 hit it for years on an
 * XMB font scale change. Retiring instead of freeing lets the caller
 * swap in the new font immediately and hands the release to
 * font_driver_free_pending(), which runs after frames are submitted.
 *
 * Two frames is the interval the old XMB open-coded countdown used and
 * is what the D3D12 report was tested against: by the time two frames
 * have gone out, the one that could have referenced the atlas has been
 * submitted and waited on. Sizing is per handle in flight, not per
 * caller.
 *
 * One queue per thread, aged on that thread's frame clock: the main
 * thread's by video_driver_frame(), the video thread's by the threaded
 * wrapper after each frame it presents. */
#define FONT_FREE_DEFERRED_MAX 16
#define FONT_FREE_DEFERRED_FRAMES 2

static struct
{
   font_data_t *font;
   unsigned     frames;
} font_free_deferred[FONT_OWNER_COUNT][FONT_FREE_DEFERRED_MAX];

/* Fonts retired by a thread other than their owner, linked through
 * font_data_t::retired_next; the owner takes them at its next
 * font_driver_free_pending(). */
static font_list_head_t font_retired[FONT_OWNER_COUNT];

static void font_driver_release_renderer_state(
      const font_renderer_t *renderer, void *renderer_data,
      bool is_threaded);

static bool font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size,
      const font_renderer_t *backend, bool is_threaded);

/* Read the renderer's line metrics into the font, or approximate them
 * from the width of 'a' when it has none. Done at creation and again
 * after a rebuild, since a different face has different metrics. */
static void font_driver_cache_metrics(font_data_t *font)
{
   struct font_line_metrics *m = NULL;

   if (     font->renderer->get_line_metrics
         && font->renderer->get_line_metrics(font->renderer_data, &m)
         && m)
      font->metrics = *m;
   else
   {
      /* font_size = width('a') / 0.6, height = font_size * 1.7,
       * ascender = font_size * 1.58 * 0.75, descender the rest. */
      float sz = 0.0f;
      if (font->renderer->get_message_width)
         sz = (float)font->renderer->get_message_width(
               font->renderer_data, "a", 1, 1.0f) / 0.6f;
      font->metrics.height    = sz * 1.7f;
      font->metrics.ascender  = sz * 1.58f * 0.75f;
      font->metrics.descender = sz * 1.58f * 0.25f;
   }
}

const char *font_driver_language_font_file(void)
{
   switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
   {
      case RETRO_LANGUAGE_ARABIC:
      case RETRO_LANGUAGE_PERSIAN:
         return "fallback-font.ttf";
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         return "chinese-fallback-font.ttf";
      case RETRO_LANGUAGE_KOREAN:
         return "korean-fallback-font.ttf";
      case RETRO_LANGUAGE_THAI:
         return "thai-fallback-font.ttf";
      default:
         break;
   }

   return NULL;
}

void font_driver_set_language_font(font_data_t *font,
      const char *pkg_dir, const char *default_path)
{
   if (!font)
      return;

   /* The fallback fonts live beside the language fonts */
   if (pkg_dir && *pkg_dir)
   {
      font_fallback_pkg_dir_set(pkg_dir);
   }

   free(font->lang_pkg_dir);
   free(font->lang_default_path);
   font->lang_pkg_dir      = (pkg_dir && *pkg_dir)
      ? strdup(pkg_dir) : NULL;
   font->lang_default_path = (default_path && *default_path)
      ? strdup(default_path) : NULL;
}

/* The path this font should be using now. For a language-following
 * font that is worked out again from the current language; for any
 * other it is the path it already has. */
static const char *font_driver_resolve_path(font_data_t *font,
      char *s, size_t len)
{
   const char *lang_font;

   if (!font->lang_pkg_dir || !font->lang_default_path)
      return font->path;

   if (!(lang_font = font_driver_language_font_file()))
      return font->lang_default_path;

   fill_pathname_join_special(s, font->lang_pkg_dir, lang_font, len);
   return s;
}

/* Rebuild one font in place against a path and a size. The
 * font_data_t address does not change, so every holder of the pointer
 * stays valid; only the renderer state behind it is replaced.
 *
 * Goes through font_init_first(), so the rebuild takes the thread
 * route its creation took and a font belonging to the video thread
 * has its texture work done there.
 *
 * The new state is built before the old is released: on failure the
 * caller keeps a working font rather than losing its text. */
static bool font_driver_rebuild(font_data_t *font,
      const char *path, float size)
{
   const void *drv    = NULL;
   void       *handle = NULL;
   bool        ok     = false;

   if (!font || !font->renderer)
      return false;

   /* font->renderer is the backend font_init_first() was handed, so
    * this is the call that made the font, against a different file.
    * A rebuild goes through the video thread whenever the wrapper is
    * up, whatever thread made the font: the OSD font is made on the
    * thread that owns the context and carries no threading hint, but
    * a font-size change rebuilds it from the settings path on the
    * main thread, and a GL backend making its context current there
    * while the video thread holds it is an X BadAccess. On the video
    * thread itself the call runs directly. */
#ifdef HAVE_THREADS
   if (video_driver_thread_wrapper_active())
      ok = video_thread_font_init(&drv, &handle, font->video_data,
            path, size, font->renderer, font_init_first,
            font->is_threaded);
   else
#endif
   ok = font_init_first(&drv, &handle, font->video_data, path, size,
         font->renderer, font->is_threaded);

   if (!ok)
      return false;

   font_driver_release_renderer_state(font->renderer,
         font->renderer_data, font->is_threaded);

   font->renderer      = (const font_renderer_t*)drv;
   font->renderer_data = handle;
   font->size          = size;

   /* Remember what it is built from now. The guard matters: path can
    * be font->path itself, where font_driver_resolve_path() hands
    * back the font's own string. */
   if (path != font->path)
   {
      free(font->path);
      font->path = (path && *path) ? strdup(path) : NULL;
   }

   font_driver_cache_metrics(font);
   return true;
}

unsigned font_driver_follow_language(void)
{
   font_data_t *font;
   unsigned     n     = 0;
   unsigned     owner = font_driver_owner_here();
   int          epoch = retro_atomic_load_acquire_int(&font_language_epoch);

   if (font_language_seen[owner] == epoch)
      return 0;
   font_language_seen[owner] = epoch;

   for (font = font_live[owner]; font; font = font->next)
   {
      char resolved[PATH_MAX_LENGTH];
      const char *want;

      /* Only a font created from an explicit path can be re-resolved;
       * one the renderer chose for itself has nothing to re-read. */
      if (!font->path || !font->renderer || !font->renderer->init)
         continue;

      want = font_driver_resolve_path(font, resolved, sizeof(resolved));

      /* Most language changes do not change the file. Only Arabic
       * and Persian, Chinese, Korean and Thai have a face of their
       * own; everything else shares the menu font, so English to
       * French, or either to Japanese, resolves to what is already
       * loaded. Rebuilding then would throw away the atlas and the
       * GPU texture behind every font, read the same TTF back, and
       * bump the generation so every derived metric was recomputed
       * too - all to arrive where it started. */
      if (string_is_equal(want, font->path))
         continue;

      if (font_driver_rebuild(font, want, font->size))
         n++;   /* on failure, keep the old font rather than lose text */
   }

   if (n)
      /* Derived data cached outside this file - ticker widths, menu
       * line heights - is now stale. */
      retro_atomic_fetch_add_int(&font_driver_generation, 1);

   return n;
}

unsigned font_driver_reload_fonts(void)
{
   retro_atomic_fetch_add_int(&font_language_epoch, 1);
   return font_driver_follow_language();
}

uint32_t font_driver_get_generation(void)
{
   return (uint32_t)retro_atomic_load_acquire_int(&font_driver_generation);
}

/* The video singleton's stable address, bound on the main thread at
 * video init before the threaded wrapper spawns. Under the wrapper
 * the OSD fonts are created, measured and freed on the video thread,
 * so every reach into ra-video state in this file goes through the
 * capture rather than the getter; no thread entry calls into a
 * singleton getter through here. */
static video_driver_state_t *font_driver_video_st;

void font_driver_bind_video_state(void *video_st)
{
   font_driver_video_st = (video_driver_state_t*)video_st;
}

int font_renderer_create_default(
      const font_renderer_driver_t **drv,
      void **handle, const char *font_path, unsigned font_size,
      enum font_atlas_format fmt)
{
   static const font_rasterizer_t *font_backends[] = {
#ifdef HAVE_FREETYPE
      &freetype_font_rasterizer,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
      &coretext_font_rasterizer,
#endif
      &stb_font_rasterizer,
      NULL
   };
   unsigned i;

   for (i = 0; font_backends[i]; i++)
   {
      const char      *path  = font_path;
      uint8_t         *data  = NULL;
      int64_t          len   = 0;
      unsigned         face  = 0;
      font_file_ref_t *entry = NULL;
      void            *f;

      /* Ask the rasterizer where to look. It gets the requested path so
       * it can resolve against it - freetype hands it to fontconfig,
       * which answers with a system font when a fallback was asked
       * for - and returns NULL to accept the request as it stands.
       * Doing the lookup and the read here is what keeps file I/O out
       * of the rasterizers entirely. */
      {
         const char * const *cand = font_backends[i]->get_default_fonts
            ? font_backends[i]->get_default_fonts(font_path, &face)
            : NULL;

         for (; cand && *cand; cand++)
         {
            /* An empty entry means the rasterizer has an internal or
             * system source and wants no file. */
            if (!**cand || path_is_valid(*cand))
            {
               path = *cand;
               break;
            }
         }

         /* Nothing asked for and nothing offered: this backend has
          * nothing to work with. */
         if (!path)
            continue;
      }

      /* One read per path, however many fonts are built from it.  A
       * rasterizer that borrows gets the shared buffer and the cache
       * holds the reference for as long as the font lives; one that
       * takes ownership gets a copy and the reference is dropped
       * here. */
      if (path && *path)
      {
         if ((entry = font_file_ref_acquire(path)))
         {
            if (font_backends[i]->borrows_font_data)
            {
               data = entry->data;
               len  = (int64_t)entry->len;
            }
            else
            {
               if ((data = (uint8_t*)malloc(entry->len)))
               {
                  memcpy(data, entry->data, entry->len);
                  len = (int64_t)entry->len;
               }
               font_file_ref_release(entry);
               entry = NULL;
            }
         }
      }

      /* A rasterizer that takes ownership does so the moment init() is
       * called, not when it succeeds, so nothing is freed here on its
       * behalf. A borrowing one frees nothing, so the reference is
       * dropped below when it fails. */
      if ((f = font_backends[i]->init(data, (size_t)len, face,
                  (float)font_size)))
      {
         font_cache_t *c = font_cache_new(font_backends[i], f, entry,
               (float)font_size, fmt);
         if (!c)
            break;
         *handle = c;
         *drv    = &font_cache_driver;
         return 1;
      }

      if (entry)
      {
         font_file_ref_release(entry);
      }
   }

   *drv    = NULL;
   *handle = NULL;

   return 0;
}

static bool font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size,
      const font_renderer_t *backend, bool is_threaded)
{
   void *data;

   if (font_path && !font_path[0])
      font_path = NULL;

   if (!backend || !backend->init)
      return false;

   if (!(data = backend->init(video_data, font_path, font_size,
               is_threaded)))
      return false;

   *font_driver = backend;
   *font_handle = data;
   return true;
}

#ifdef HAVE_LANGEXTRA
/* ASCII:       0xxxxxxx  (c & 0x80) == 0x00
 * other start: 11xxxxxx  (c & 0xC0) == 0xC0
 * other cont:  10xxxxxx  (c & 0xC0) == 0x80
 * misc. white space:
 * 2000 - 200D: 11100010 10000000 1000xxxx (c[2] < 0x8E) (3 bytes)
 * Hebrew:
 * 0591 - 05F4: 1101011x (c & 0xFE) == 0xD6 (2 bytes)
 * Arabic:
 * 0600 - 06FF: 110110xx (c & 0xFC) == 0xD8 (2 bytes)
 */

/* clang-format off */
#define IS_ASCII(p)        ((*(p)&0x80) == 0x00)
#define IS_MBSTART(p)      ((*(p)&0xC0) == 0xC0)
#define IS_MBCONT(p)       ((*(p)&0xC0) == 0x80)
#define IS_HEBREW(p)       ((*(p)&0xFE) == 0xD6)
#define IS_ARABIC(p)       ((*(p)&0xFC) == 0xD8)
#define IS_RTL(p)          (IS_HEBREW(p) || IS_ARABIC(p))
#define GET_ID_ARABIC(p)   (((unsigned char)(p)[0] << 6) | ((unsigned char)(p)[1] & 0x3F))


/* Checks for miscellaneous whitespace characters in the range U+2000 to U+200D */
static INLINE unsigned is_misc_ws(const unsigned char* src)
{
   unsigned res = 0;
   if (*(src) == 0xE2) /* first byte */
   {
      src++;
      if (*(src) == 0x80) /* second byte */
      {
         src++;
         res = (*(src) < 0x8E); /* third byte */
      }
   }
   return res;
}

/* Presentation forms of the Arabic block by low byte: isolated, final,
 * initial, medial; 0 where a letter has none */
const uint16_t font_driver_arabic_shapes[0x100][0x4] = {
   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0600 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },

   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0610 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },

   { 0 },                               /* 0x0620 */
   { 0xFE80 },
   { 0xFE81, 0xFE82 },
   { 0xFE83, 0xFE84 },
   { 0xFE85, 0xFE86 },
   { 0xFE87, 0xFE88 },
   { 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C },
   { 0xFE8D, 0xFE8E },

   { 0xFE8F, 0xFE90, 0xFE91, 0xFE92 },
   { 0xFE93, 0xFE94 },
   { 0xFE95, 0xFE96, 0xFE97, 0xFE98 },
   { 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C },
   { 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0 },
   { 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4 },
   { 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8 },
   { 0xFEA9, 0xFEAA },

   { 0xFEAB, 0xFEAC },                  /* 0x0630 */
   { 0xFEAD, 0xFEAE },
   { 0xFEAF, 0xFEB0 },
   { 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4 },
   { 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8 },
   { 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC },
   { 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0 },
   { 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4 },

   { 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8 },
   { 0xFEC9, 0xFECA, 0xFECB, 0xFECC },
   { 0xFECD, 0xFECE, 0xFECF, 0xFED0 },
   { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },

   { 0 },                               /* 0x0640 */
   { 0xFED1, 0xFED2, 0xFED3, 0xFED4 },
   { 0xFED5, 0xFED6, 0xFED7, 0xFED8 },
   { 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC },
   { 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0 },
   { 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4 },
   { 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8 },
   { 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC },

   { 0xFEED, 0xFEEE },
   { 0xFEEF, 0xFEF0, 0xFBE8, 0xFBE9 },
   { 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4 },
   { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },

   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0650 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0660 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0670 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },

   { 0 }, { 0 },
   { 0xFB56, 0xFB57, 0xFB58, 0xFB59 },
   { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0680 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0690 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06A0 */
   { 0 }, { 0 }, { 0 }, { 0 },

   { 0 },
   { 0xFB8E, 0xFB8F, 0xFB90, 0xFB91 },
   { 0 }, { 0 },

   { 0 }, { 0 }, { 0 },
   { 0xFB92, 0xFB93, 0xFB94, 0xFB95 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06B0 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06C0 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },

   { 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF },
   { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06D0 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06E0 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },


   { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06F0 */
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
   { 0 }, { 0 }, { 0 }, { 0 },
};

static INLINE unsigned font_get_arabic_replacement(
      const char* src, const char* start, const char* end)
{
   unsigned ret = 0;
   bool          prev_connected = false;
   bool          next_connected = false;
   unsigned char id             = GET_ID_ARABIC(src);
   const char*   prev           = src - 2;
   const char*   next           = src + 2;

   /* prev/next straddle src by one Arabic character (2 bytes). Bounds
    * must be tested before IS_ARABIC dereferences them: prev can point
    * before start when src is at the first character, and the forward
    * scan must not read past the terminator. */
   if ((prev >= start) && IS_ARABIC(prev))
   {
      unsigned char prev_id = GET_ID_ARABIC(prev);

      /* nonspacing diacritics 0x4b -- 0x5f */
      while (prev_id > 0x4A && prev_id < 0x60)
      {
         prev -= 2;
         if ((prev >= start) && IS_ARABIC(prev))
            prev_id = GET_ID_ARABIC(prev);
         else
            break;
      }

      if (prev_id == 0x44) /* Arabic Letter Lam */
      {
         unsigned char prev2_id = 0;
         const char*   prev2    = prev - 2;

         if ((prev2 >= start) && IS_ARABIC(prev2))
            prev2_id            = GET_ID_ARABIC(prev2);

         /* nonspacing diacritics 0x4b -- 0x5f */
         while (prev2_id > 0x4A && prev2_id < 0x60)
         {
            prev2 -= 2;
            if ((prev2 >= start) && IS_ARABIC(prev2))
               prev2_id = GET_ID_ARABIC(prev2);
            else
               break;
         }

         prev_connected = !!font_driver_arabic_shapes[prev2_id][2];

         switch (id)
         {
            case 0x22: /* Arabic Letter Alef with Madda Above */
               return 0xFEF5 + prev_connected;
            case 0x23: /* Arabic Letter Alef with Hamza Above */
               return 0xFEF7 + prev_connected;
            case 0x25: /* Arabic Letter Alef with Hamza Below */
               return 0xFEF9 + prev_connected;
            case 0x27: /* Arabic Letter Alef */
               return 0xFEFB + prev_connected;
         }
      }
      prev_connected = !!font_driver_arabic_shapes[prev_id][2];
   }

   if ((next + 1 < end) && IS_ARABIC(next))
   {
      unsigned char next_id = GET_ID_ARABIC(next);

      /* nonspacing diacritics 0x4b -- 0x5f */
      while (next_id > 0x4A && next_id < 0x60)
      {
         next += 2;
         if ((next + 1 >= end) || !IS_ARABIC(next))
            break;
         next_id = GET_ID_ARABIC(next);
      }

      next_connected = !!font_driver_arabic_shapes[next_id][1];
   }

   if ((ret =
            font_driver_arabic_shapes[id][prev_connected | (next_connected <<
               1)]))
      return ret;
   return font_driver_arabic_shapes[id][prev_connected];
}
/* clang-format on */

/* Start of the first right-to-left character in msg, or msg + msg_len.
 * Words with no byte at or above 0x80 are skipped whole: every
 * right-to-left character is written with such bytes. */
static const unsigned char *font_msg_find_rtl(const char *msg,
      size_t msg_len)
{
   const unsigned char *p = (const unsigned char*)msg;
   const unsigned char *e = p + msg_len;
   const size_t      mask = (size_t)~(size_t)0 / 0xFF * 0x80;

   while (p < e && ((uintptr_t)p & (sizeof(size_t) - 1)))
   {
      if (IS_RTL(p))
         return p;
      p++;
   }

   while (p + sizeof(size_t) <= e)
   {
      size_t w;
      memcpy(&w, p, sizeof(w));
      if (w & mask)
      {
         const unsigned char *q = p + sizeof(size_t);
         for (; p < q; p++)
            if (IS_RTL(p))
               return p;
      }
      else
         p += sizeof(size_t);
   }

   for (; p < e; p++)
      if (IS_RTL(p))
         return p;
   return e;
}

/* Bidi classes, a subset of the Unicode ones: the strong classes, the
 * numbers, the weak separators, then the neutrals */
enum font_bidi_class
{
   FONT_BIDI_L = 0,
   FONT_BIDI_R,
   FONT_BIDI_AL,
   FONT_BIDI_EN,
   FONT_BIDI_AN,
   FONT_BIDI_ES,
   FONT_BIDI_CS,
   FONT_BIDI_ET,
   FONT_BIDI_NSM,
   FONT_BIDI_WS,
   FONT_BIDI_ON
};

/* Characters a line can hold and be laid out. font_driver_render_msg's
 * buffer cuts its input to 1023 bytes, so every line it draws fits; a
 * longer line is drawn as typed. */
#define FONT_BIDI_MAX_CHARS 1024
/* Open brackets a line can nest, as in the Unicode algorithm, and pairs
 * it can hold; brackets past them are not paired */
#define FONT_BIDI_MAX_PAIRS 63
/* Set in a lam's level once the lam-alef glyph before it stands for it,
 * and in a resolved bracket's class while pairing */
#define FONT_BIDI_TAKEN     0x80
/* A level or class without that flag */
#define FONT_BIDI_LEVEL(v)  ((v) & 0x7F)

/* Length of the character at p: its lead byte and every continuation
 * byte after it, never reaching end. Stepping back over continuation
 * bytes finds the same boundaries. */
static unsigned font_bidi_char_len(const unsigned char *p,
      const unsigned char *end)
{
   const unsigned char *q = p + 1;
   while (q < end && IS_MBCONT(q))
      q++;
   return (unsigned)(q - p);
}

/* Start of the character before p, never before start */
static const unsigned char *font_bidi_prev(const unsigned char *p,
      const unsigned char *start)
{
   p--;
   while (p > start && IS_MBCONT(p))
      p--;
   return p;
}

/* Class of the n-byte character at p, from the Unicode tables for
 * ASCII, Latin-1, the Hebrew and Arabic blocks and general punctuation.
 * Everything else is left-to-right. */
static unsigned char font_bidi_class(const unsigned char *p, unsigned n)
{
   unsigned c = *p;
   unsigned cp;

   if (c < 0x80)
   {
      if (c >= '0' && c <= '9')
         return FONT_BIDI_EN;
      if ((c | 0x20) >= 'a' && (c | 0x20) <= 'z')
         return FONT_BIDI_L;
      switch (c)
      {
         case ' ':
         case '\t':
            return FONT_BIDI_WS;
         case '+':
         case '-':
            return FONT_BIDI_ES;
         case ',':
         case '.':
         case '/':
         case ':':
            return FONT_BIDI_CS;
         case '#':
         case '$':
         case '%':
            return FONT_BIDI_ET;
      }
      return FONT_BIDI_ON;
   }

   if (n == 2 && c < 0xE0)
   {
      cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
      if (cp < 0x100)
      {
         if (cp == 0xA0)
            return FONT_BIDI_CS;
         if ((cp >= 0xA2 && cp <= 0xA5) || cp == 0xB0 || cp == 0xB1)
            return FONT_BIDI_ET;
         if (cp == 0xB2 || cp == 0xB3 || cp == 0xB9)
            return FONT_BIDI_EN;
         if (cp < 0xC0 && cp != 0xAA && cp != 0xB5 && cp != 0xBA)
            return FONT_BIDI_ON;
         if (cp == 0xD7 || cp == 0xF7)
            return FONT_BIDI_ON;
         return FONT_BIDI_L;
      }
      if (cp >= 0x300 && cp <= 0x36F)
         return FONT_BIDI_NSM;
      if (cp >= 0x590 && cp <= 0x5FF)
      {
         if (     (cp >= 0x591 && cp <= 0x5BD)
               || cp == 0x5BF || cp == 0x5C1 || cp == 0x5C2
               || cp == 0x5C4 || cp == 0x5C5 || cp == 0x5C7)
            return FONT_BIDI_NSM;
         return FONT_BIDI_R;
      }
      if (cp >= 0x600 && cp <= 0x6FF)
      {
         if (cp <= 0x605 || (cp >= 0x660 && cp <= 0x669)
               || cp == 0x66B || cp == 0x66C || cp == 0x6DD)
            return FONT_BIDI_AN;
         if (cp >= 0x6F0 && cp <= 0x6F9)
            return FONT_BIDI_EN;
         if (cp == 0x609 || cp == 0x60A || cp == 0x66A)
            return FONT_BIDI_ET;
         if (cp == 0x60C)
            return FONT_BIDI_CS;
         if (     cp == 0x606 || cp == 0x607 || cp == 0x60E || cp == 0x60F
               || cp == 0x6DE || cp == 0x6E9)
            return FONT_BIDI_ON;
         if (     (cp >= 0x610 && cp <= 0x61A)
               || (cp >= 0x64B && cp <= 0x65F)
               ||  cp == 0x670
               || (cp >= 0x6D6 && cp <= 0x6DC)
               || (cp >= 0x6DF && cp <= 0x6E4)
               ||  cp == 0x6E7 || cp == 0x6E8
               || (cp >= 0x6EA && cp <= 0x6ED))
            return FONT_BIDI_NSM;
         return FONT_BIDI_AL;
      }
   }
   else if (n == 3 && c == 0xE2)
   {
      cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      if (cp >= 0x2000 && cp <= 0x200D)
         return FONT_BIDI_WS;
      if (cp == 0x200E)
         return FONT_BIDI_L;
      if (cp == 0x200F)
         return FONT_BIDI_R;
      if (cp == 0x202F || cp == 0x2044)
         return FONT_BIDI_CS;
      if (cp >= 0x2030 && cp <= 0x2034)
         return FONT_BIDI_ET;
      if (cp == 0x2028 || cp == 0x205F)
         return FONT_BIDI_WS;
      if (     (cp >= 0x2010 && cp <= 0x2027)
            || (cp >= 0x2035 && cp <= 0x205E))
         return FONT_BIDI_ON;
      if (cp >= 0x20A0 && cp <= 0x20CF)
         return FONT_BIDI_ET;
   }
   return FONT_BIDI_L;
}

/* Gives paired brackets a direction from what they hold and what comes
 * before them. lv[] holds the classes of the n characters of the line
 * from line to eol, of direction dir. */
static void font_bidi_pair_brackets(const unsigned char *line,
      const unsigned char *eol, unsigned char *lv, size_t n,
      unsigned char dir)
{
   static const char    brackets[] = "()[]{}";
   uint16_t             opened[FONT_BIDI_MAX_PAIRS];
   uint16_t             pair[FONT_BIDI_MAX_PAIRS][2];
   unsigned char        opened_ch[FONT_BIDI_MAX_PAIRS];
   const unsigned char *p     = line;
   size_t               i, j;
   size_t               depth = 0;
   size_t               pairs = 0;

   /* A closing bracket pairs with the nearest open one of its kind and
    * drops any opened after it */
   for (i = 0; i < n; i++, p += font_bidi_char_len(p, eol))
   {
      const char *m;
      size_t      k;
      if (     lv[i] != FONT_BIDI_ON
            || !(m = (const char*)memchr(brackets, *p,
                  sizeof(brackets) - 1)))
         continue;
      if (!((m - brackets) & 1))
      {
         if (depth == FONT_BIDI_MAX_PAIRS)
            break;
         opened[depth]      = (uint16_t)i;
         opened_ch[depth++] = *p;
         continue;
      }
      for (j = depth; j > 0; j--)
         if (opened_ch[j - 1] == (unsigned char)m[-1])
            break;
      if (!j)
         continue;
      if (pairs == FONT_BIDI_MAX_PAIRS)
         break;
      /* Kept in the order of their opening brackets */
      for (k = pairs++; k > 0 && pair[k - 1][0] > opened[j - 1]; k--)
      {
         pair[k][0] = pair[k - 1][0];
         pair[k][1] = pair[k - 1][1];
      }
      pair[k][0] = opened[j - 1];
      pair[k][1] = (uint16_t)i;
      depth      = j - 1;
   }
   if (!pairs)
      return;

   /* A pair takes the line's direction when it holds that direction,
    * numbers counting as right-to-left. One that holds only the other
    * direction takes the direction before it. Brackets given one are
    * flagged for the marks after them. */
   for (j = 0; j < pairs; j++)
   {
      unsigned char to = FONT_BIDI_ON;
      for (i = pair[j][0] + 1; i < pair[j][1] && to != dir; i++)
         if (FONT_BIDI_LEVEL(lv[i]) <= FONT_BIDI_AN)
            to = (FONT_BIDI_LEVEL(lv[i]) == FONT_BIDI_L)
               ? FONT_BIDI_L : FONT_BIDI_R;
      if (to == FONT_BIDI_ON)
         continue;
      if (to != dir)
      {
         to = dir;
         for (i = pair[j][0]; i > 0; i--)
            if (FONT_BIDI_LEVEL(lv[i - 1]) <= FONT_BIDI_AN)
            {
               to = (FONT_BIDI_LEVEL(lv[i - 1]) == FONT_BIDI_L)
                  ? FONT_BIDI_L : FONT_BIDI_R;
               break;
            }
      }
      lv[pair[j][0]] = to | FONT_BIDI_TAKEN;
      lv[pair[j][1]] = to | FONT_BIDI_TAKEN;
   }

   /* Marks on a bracket take its direction */
   for (i = pair[0][0], p = line; i > 0; i--)
      p += font_bidi_char_len(p, eol);
   for (i = pair[0][0]; i < n; i++)
   {
      unsigned k = font_bidi_char_len(p, eol);
      if (lv[i] & FONT_BIDI_TAKEN)
      {
         unsigned char to = FONT_BIDI_LEVEL(lv[i]);
         lv[i]            = to;
         for (p += k; i + 1 < n; p += k)
         {
            k = font_bidi_char_len(p, eol);
            if (font_bidi_class(p, k) != FONT_BIDI_NSM)
               break;
            lv[++i] = to;
         }
         continue;
      }
      p += k;
   }
}

/* Resolves the levels of the line from line to eol with a single-line
 * subset of the Unicode bidi algorithm: lv[] gets the level of each
 * character in typed order and *base the line's. Returns the number of
 * characters, 0 for a line with nothing right-to-left, or (size_t)-1
 * for one over cap characters. */
static size_t font_bidi_layout_line(const unsigned char *line,
      const unsigned char *eol, unsigned char *lv, size_t cap,
      unsigned char *base)
{
   const unsigned char *p;
   size_t               i, j;
   size_t               n       = 0;
   unsigned char        dir     = FONT_BIDI_ON;
   unsigned char        strong;
   bool                 has_rtl = false;

   /* The first strong character sets the line's direction */
   for (p = line; p < eol; n++)
   {
      unsigned k;
      if (n == cap)
         return (size_t)-1;
      k     = font_bidi_char_len(p, eol);
      lv[n] = font_bidi_class(p, k);
      if (     lv[n] == FONT_BIDI_R || lv[n] == FONT_BIDI_AL
            || lv[n] == FONT_BIDI_AN)
         has_rtl = true;
      if (dir == FONT_BIDI_ON && lv[n] <= FONT_BIDI_AL)
         dir = (lv[n] == FONT_BIDI_L) ? FONT_BIDI_L : FONT_BIDI_R;
      p += k;
   }
   if (!has_rtl)
      return 0;
   if (dir == FONT_BIDI_ON)
      dir = FONT_BIDI_L;

   /* A mark takes the class of what it sits on. A European number after
    * Arabic is an Arabic number, and Arabic is right-to-left. */
   strong = dir;
   for (i = 0; i < n; i++)
   {
      if (lv[i] == FONT_BIDI_NSM)
         lv[i] = i ? lv[i - 1] : dir;
      if (lv[i] <= FONT_BIDI_AL)
         strong = lv[i];
      if (lv[i] == FONT_BIDI_EN && strong == FONT_BIDI_AL)
         lv[i] = FONT_BIDI_AN;
   }
   for (i = 0; i < n; i++)
      if (lv[i] == FONT_BIDI_AL)
         lv[i] = FONT_BIDI_R;

   /* A separator between two numbers of a kind is part of them: either
    * separator between European numbers, a common one between Arabic */
   for (i = 1; i + 1 < n; i++)
      if (     lv[i - 1] == lv[i + 1]
            && (  (lv[i] == FONT_BIDI_ES && lv[i - 1] == FONT_BIDI_EN)
               || (lv[i] == FONT_BIDI_CS && (lv[i - 1] == FONT_BIDI_EN
                     || lv[i - 1] == FONT_BIDI_AN))))
         lv[i] = lv[i - 1];

   /* So are terminators next to a European number; other separators
    * are neutral */
   for (i = 0; i < n; i = j)
   {
      j = i + 1;
      if (lv[i] == FONT_BIDI_ET)
      {
         unsigned char to = FONT_BIDI_ON;
         while (j < n && lv[j] == FONT_BIDI_ET)
            j++;
         if (     (i > 0 && lv[i - 1] == FONT_BIDI_EN)
               || (j < n && lv[j] == FONT_BIDI_EN))
            to = FONT_BIDI_EN;
         memset(lv + i, to, j - i);
      }
      else if (lv[i] == FONT_BIDI_ES || lv[i] == FONT_BIDI_CS)
         lv[i] = FONT_BIDI_ON;
   }

   /* A European number after a left-to-right character is
    * left-to-right */
   strong = dir;
   for (i = 0; i < n; i++)
      if (lv[i] <= FONT_BIDI_R)
         strong = lv[i];
      else if (lv[i] == FONT_BIDI_EN && strong == FONT_BIDI_L)
         lv[i] = FONT_BIDI_L;

   font_bidi_pair_brackets(line, eol, lv, n, dir);

   /* A neutral run takes the direction on both sides of it, numbers
    * counting as right-to-left, or else the line's. Either end of the
    * line counts as the line's direction, so trailing whitespace takes
    * the line's level. */
   for (i = 0; i < n; i = j)
   {
      unsigned char before = dir;
      unsigned char after  = dir;
      j = i + 1;
      if (lv[i] < FONT_BIDI_WS)
         continue;
      while (j < n && lv[j] >= FONT_BIDI_WS)
         j++;
      if (i > 0)
         before = (lv[i - 1] == FONT_BIDI_L) ? FONT_BIDI_L : FONT_BIDI_R;
      if (j < n)
         after  = (lv[j]     == FONT_BIDI_L) ? FONT_BIDI_L : FONT_BIDI_R;
      memset(lv + i, (before == after) ? before : dir, j - i);
   }

   /* Right-to-left is level 1, a number 2, and left-to-right 0 in a
    * left-to-right line or 2 in a right-to-left one */
   for (i = 0; i < n; i++)
   {
      if (lv[i] == FONT_BIDI_R)
         lv[i] = 1;
      else if (lv[i] != FONT_BIDI_L || dir == FONT_BIDI_R)
         lv[i] = 2;
      else
         lv[i] = 0;
   }

   *base = (dir == FONT_BIDI_R);
   return n;
}

typedef struct
{
   const unsigned char *line;
   const unsigned char *eol;
   unsigned char       *lv;
   unsigned char       *dst;
   unsigned char       *dst_max;
   bool                 full;
} font_bidi_emit_t;

/* Copies p to q to the output, cut at a character boundary when it
 * does not fit */
static void font_bidi_copy(font_bidi_emit_t *e, const unsigned char *p,
      const unsigned char *q)
{
   size_t k = (size_t)(q - p);
   if (k > (size_t)(e->dst_max - e->dst))
   {
      k       = (size_t)(e->dst_max - e->dst);
      e->full = true;
      while (k > 0 && IS_MBCONT(p + k))
         k--;
   }
   memcpy(e->dst, p, k);
   e->dst += k;
}

/* Writes character i, from p to q, of a right-to-left run whose first
 * character is i0 at start: Arabic shaped from its neighbours in typed
 * order, brackets mirrored */
static void font_bidi_put_rtl(font_bidi_emit_t *e, size_t i,
      const unsigned char *p, const unsigned char *q,
      size_t i0, const unsigned char *start)
{
   static const char pairs[] = "()[]{}<>";
   size_t            k       = (size_t)(q - p);
   unsigned          r       = 0;

   if (k == 2 && IS_ARABIC(p))
      r = font_get_arabic_replacement((const char*)p,
            (const char*)e->line, (const char*)e->eol);

   if ((size_t)(e->dst_max - e->dst) < (r ? 3 : k))
   {
      e->full = true;
      return;
   }

   if (r)
   {
      /* A lam-alef glyph stands for the lam too: the lam before the
       * alef, behind any diacritics, is not drawn again */
      if (r >= 0xFEF5 && r <= 0xFEFC)
      {
         const unsigned char *y = p;
         while (i > i0)
         {
            const unsigned char *x = font_bidi_prev(y, start);
            unsigned char        id;
            i--;
            if (y - x != 2 || !IS_ARABIC(x))
               break;
            if ((id = (unsigned char)GET_ID_ARABIC(x)) == 0x44)
            {
               e->lv[i] |= FONT_BIDI_TAKEN;
               break;
            }
            /* nonspacing diacritics 0x4b -- 0x5f */
            if (id < 0x4B || id > 0x5F)
               break;
            y = x;
         }
      }
      /* Every replacement is a presentation form: three bytes */
      *e->dst++ = 0xE0 | ( r >> 12);
      *e->dst++ = 0x80 | ((r >>  6) & 0x3F);
      *e->dst++ = 0x80 | ( r        & 0x3F);
   }
   else
   {
      const char *m;
      memcpy(e->dst, p, k);
      if (k == 1 && (m = (const char*)memchr(pairs, *p, sizeof(pairs) - 1)))
         *e->dst = (unsigned char)pairs[(m - pairs) ^ 1];
      e->dst += k;
   }
}

/* Writes characters i0 to i1, from b0 to b1, all at level or above, in
 * visual order. At an even level its own characters keep their order
 * and go out in one copy per run; at an odd level they go out last to
 * first. Runs above the level are laid out inside, in their place. */
static void font_bidi_emit(font_bidi_emit_t *e, size_t i0, size_t i1,
      const unsigned char *b0, const unsigned char *b1, unsigned level)
{
   const unsigned char *p;
   const unsigned char *q;
   size_t               i, j;

   if (!(level & 1))
   {
      for (i = i0, p = b0; i < i1 && !e->full; i = j, p = q)
      {
         bool up = FONT_BIDI_LEVEL(e->lv[i]) > level;
         for (j = i, q = p; j < i1
               && (FONT_BIDI_LEVEL(e->lv[j]) > level) == up; j++)
            q += font_bidi_char_len(q, b1);
         if (up)
            font_bidi_emit(e, i, j, p, q, level + 1);
         else
            font_bidi_copy(e, p, q);
      }
      return;
   }

   for (i = i1, p = b1; i > i0 && !e->full; i = j, p = q)
   {
      bool up = FONT_BIDI_LEVEL(e->lv[i - 1]) > level;
      for (j = i, q = p; j > i0
            && (FONT_BIDI_LEVEL(e->lv[j - 1]) > level) == up; j--)
         q = font_bidi_prev(q, b0);
      if (up)
         font_bidi_emit(e, j, i, q, p, level + 1);
      else
      {
         /* Last to first, but a base goes out before the marks on it,
          * so the renderer draws each mark over its own base */
         const unsigned char *c = p;
         size_t               k = i;
         while (k > j && !e->full)
         {
            const unsigned char *x = font_bidi_prev(c, q);
            const unsigned char *y;
            size_t               m = k - 1;
            size_t               t;
            while (m > j && font_bidi_class(x, font_bidi_char_len(x, p))
                  == FONT_BIDI_NSM)
            {
               x = font_bidi_prev(x, q);
               m--;
            }
            for (t = m, y = x; t < k && !e->full; t++)
            {
               const unsigned char *z = y + font_bidi_char_len(y, p);
               if (!(e->lv[t] & FONT_BIDI_TAKEN))
                  font_bidi_put_rtl(e, t, y, z, j, q);
               y = z;
            }
            k = m;
            c = x;
         }
      }
   }
}

/* font_driver_reshape_msg() for a msg whose first right-to-left
 * character is at eol */
static INLINE char* font_driver_reshape_rtl(const char* msg, size_t msg_len,
      const unsigned char *eol, unsigned char *s, size_t len,
      size_t *out_len)
{
   unsigned char        lv[FONT_BIDI_MAX_CHARS];
   font_bidi_emit_t     e;
   const unsigned char *line = (const unsigned char*)msg;
   const unsigned char *end  = line + msg_len;

   /* A message after U+202D LEFT-TO-RIGHT OVERRIDE is already in visual
    * order: drawn as given, without the mark */
   if (     msg_len >= 3 && line[0] == 0xE2 && line[1] == 0x80
         && line[2] == 0xAD)
   {
      *out_len = msg_len - 3;
      return (char*)msg + 3;
   }

   /* Output is at most 1.5 times its input, two bytes of Arabic making
    * a three-byte presentation form, so input that could outgrow the
    * buffer is cut to fit at a character boundary */
   if (msg_len + msg_len / 2 > len - 1)
   {
      msg_len = (len - 1) / 3 * 2 + ((len - 1) % 3 != 0);
      while (msg_len > 0 && IS_MBCONT(line + msg_len))
         msg_len--;
      end = line + msg_len;
      if (eol > end)
         eol = end;
   }

   e.lv      = lv;
   e.dst     = s;
   e.dst_max = s + len - 1;
   e.full    = false;

   /* The lines before the first right-to-left one go out as typed */
   while (eol > line && eol[-1] != '\n')
      eol--;
   if (eol > line)
      font_bidi_copy(&e, line, eol);
   line = eol;

   for (;;)
   {
      const unsigned char *p;
      size_t               n    = 0;
      unsigned char        base = 0;

      if (!(eol = (const unsigned char*)memchr(line, '\n',
                  (size_t)(end - line))))
         eol = end;

      for (p = line; p < eol && !IS_RTL(p); p++);
      if (p < eol && (n = font_bidi_layout_line(line, eol, lv,
                  FONT_BIDI_MAX_CHARS, &base)) == (size_t)-1)
         n = 0;

      if (!n)
         font_bidi_copy(&e, line, eol);
      else
      {
         e.line = line;
         e.eol  = eol;
         font_bidi_emit(&e, 0, n, line, eol, base);
      }

      if (e.full || eol == end || e.dst == e.dst_max)
         break;
      *e.dst++ = '\n';
      line     = eol + 1;
   }

   *e.dst   = '\0';
   *out_len = (size_t)(e.dst - s);
   return (char*)s;
}

static char* font_driver_reshape_msg(const char* msg, size_t msg_len,
      unsigned char *s, size_t len, size_t *out_len)
{
   const unsigned char *eol = font_msg_find_rtl(msg, msg_len);

   /* Nothing right-to-left: hand back the input and skip both the walk
    * and the copy into s. This is every English HUD string, including
    * the statistics overlay. */
   if (eol == (const unsigned char*)msg + msg_len || !len)
   {
      *out_len = msg_len;
      return (char*)msg;
   }
   return font_driver_reshape_rtl(msg, msg_len, eol, s, len, out_len);
}

size_t font_driver_layout_line(const char *msg, size_t msg_len,
      char *s, size_t len, bool *rtl)
{
   const unsigned char *p   = (const unsigned char*)msg;
   const unsigned char *end = p + msg_len;
   const unsigned char *eol = p;
   size_t               out_len;

   /* Not font_msg_find_rtl(): this runs once per label, and a second
    * caller would keep that scan from being inlined into the drawing
    * path */
   while (eol < end && !IS_RTL(eol))
      eol++;
   *rtl = false;
   if (     eol == end
         || len < FONT_LAYOUT_LINE_SIZE
         || msg_len + msg_len / 2 + 1 > len
         || memchr(msg, '\n', msg_len)
         || (msg_len >= 3 && p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0xAD))
      return 0;

   /* The first strong character sets the line's direction */
   while (p < end)
   {
      unsigned      k = font_bidi_char_len(p, end);
      unsigned char c = font_bidi_class(p, k);
      if (c <= FONT_BIDI_AL)
      {
         *rtl = (c != FONT_BIDI_L);
         break;
      }
      p += k;
   }

   /* The buffer is big enough that nothing is cut */
   font_driver_reshape_rtl(msg, msg_len, eol, (unsigned char*)s, len,
         &out_len);
   return out_len;
}

bool font_driver_is_mark(const char *s, size_t len)
{
   return font_bidi_class((const unsigned char*)s, (unsigned)len)
      == FONT_BIDI_NSM;
}

/* Bytes the reshaper writes for the k-byte character at p of the line
 * from line to eol: three for an Arabic letter it shapes, none for a lam
 * the lam-alef glyph after it stands for, k for anything else */
static size_t font_bidi_out_len(const unsigned char *p, unsigned k,
      const unsigned char *line, const unsigned char *eol)
{
   if (k != 2 || !IS_ARABIC(p))
      return k;
   if ((unsigned char)GET_ID_ARABIC(p) == 0x44)
   {
      const unsigned char *q = p + 2;
      while (     q + 1 < eol && IS_ARABIC(q)
               && (unsigned char)GET_ID_ARABIC(q) > 0x4A
               && (unsigned char)GET_ID_ARABIC(q) < 0x60)
         q += 2;
      if (q + 1 < eol && IS_ARABIC(q))
      {
         unsigned r = font_get_arabic_replacement((const char*)q,
               (const char*)line, (const char*)eol);
         if (r >= 0xFEF5 && r <= 0xFEFC)
            return 0;
      }
   }
   return font_get_arabic_replacement((const char*)p, (const char*)line,
         (const char*)eol) ? 3 : 2;
}

#define FONT_BIDI_IS_MARK(p, k) \
   (font_bidi_class((p), (k)) == FONT_BIDI_NSM)

/* font_driver_get_caret_x() for a line holding right-to-left text: the
 * line as drawn measured up to the caret. Typed characters i < j are
 * drawn j left of i exactly when the lowest level from i to j is odd,
 * so the drawn bytes left of the caret are counted walking out from
 * the character it goes against, either way; a mark goes with the
 * character it sits on, after it in what is drawn. */
VIDEO_NOINLINE static int font_driver_caret_rtl(void *font_data,
      const unsigned char *line, size_t len, size_t at, float scale)
{
   unsigned char        vis[FONT_LAYOUT_LINE_SIZE];
   unsigned char        lv[FONT_BIDI_MAX_CHARS];
   const unsigned char *eol  = line + len;
   const unsigned char *c    = line;
   const unsigned char *p;
   const char          *v;
   unsigned char        base;
   unsigned char        m;
   size_t               n, t, j, vlen;
   size_t               off  = 0;
   size_t               pend = 0;
   unsigned             k;
   bool                 right;
   bool                 left = false;

   if (     !(n = font_bidi_layout_line(line, eol, lv,
                  FONT_BIDI_MAX_CHARS, &base))
         || n == (size_t)-1)
      return font_driver_get_message_width(font_data, (const char*)line,
            at, scale);
   v = font_driver_reshape_msg((const char*)line, len, vis, sizeof(vis),
         &vlen);

   /* The character the caret goes before in reading order, past any
    * marks; at the end, after the last */
   for (t = 0; t < n && (size_t)(c - line) < at; t++)
      c += font_bidi_char_len(c, eol);
   while (t < n && FONT_BIDI_IS_MARK(c, k = font_bidi_char_len(c, eol)))
   {
      c += k;
      t++;
   }
   if (t < n)
      right = (lv[t] & 1);
   else
   {
      c = font_bidi_prev(eol, line);
      for (t = n - 1; t > 0
            && FONT_BIDI_IS_MARK(c, font_bidi_char_len(c, eol)); t--)
         c = font_bidi_prev(c, line);
      right = !(lv[t] & 1);
   }

   /* Its own glyph and marks, when the caret goes at their right */
   k = font_bidi_char_len(c, eol);
   if (right)
      off += font_bidi_out_len(c, k, line, eol);
   for (j = t + 1, p = c + k; j < n
         && FONT_BIDI_IS_MARK(p, k = font_bidi_char_len(p, eol)); j++)
   {
      if (right)
         off += k;
      p += k;
   }

   /* After it in typed order: left of it under an odd lowest level */
   m = lv[t];
   for (; j < n; j++)
   {
      k = font_bidi_char_len(p, eol);
      if (lv[j] < m)
         m = lv[j];
      if (!FONT_BIDI_IS_MARK(p, k))
         left = (m & 1);
      if (left)
         off += font_bidi_out_len(p, k, line, eol);
      p += k;
   }

   /* Before it: left of it under an even one, a mark with the character
    * before it */
   m = lv[t];
   for (j = t, p = c; j > 0; j--)
   {
      p = font_bidi_prev(p, line);
      k = font_bidi_char_len(p, eol);
      if (lv[j - 1] < m)
         m = lv[j - 1];
      if (j > 1 && FONT_BIDI_IS_MARK(p, k))
      {
         pend += k;
         continue;
      }
      if (!(m & 1))
         off += font_bidi_out_len(p, k, line, eol) + pend;
      pend = 0;
   }

   if (off > vlen)
      off = vlen;
   return font_driver_get_message_width(font_data, v, off, scale);
}

int font_driver_get_caret_x(void *font_data, const char *line,
      size_t len, size_t at, float scale)
{
   if (at > len)
      at = len;
   if (font_msg_find_rtl(line, len) != (const unsigned char*)line + len)
      return font_driver_caret_rtl(font_data, (const unsigned char*)line,
            len, at, scale);
   return font_driver_get_message_width(font_data, line, at, scale);
}

#endif

#ifndef HAVE_LANGEXTRA
int font_driver_get_caret_x(void *font_data, const char *line,
      size_t len, size_t at, float scale)
{
   return font_driver_get_message_width(font_data, line,
         at > len ? len : at, scale);
}
#endif

void font_driver_render_msg(void *data, const char *msg, size_t msg_len,
      const struct font_params *params, void *font_data)
{
   font_data_t                *font = (font_data_t*)(font_data
         ? font_data : (font_driver_video_st
            ? (void*)font_driver_video_st->osd_font : NULL));
   const font_renderer_t *renderer  = (font && msg && msg_len)
   ? font->renderer : NULL;

   gfx_display_t *p_disp            = disp_get_ptr();

   /* Quads asked for before this text have to land under it */
   gfx_display_flush_batch(p_disp);

   if (renderer && renderer->render_msg)
   {
#ifdef HAVE_LANGEXTRA
      /* It needs to be this big because of the Statistics text
       * unfortunately */
      unsigned char tmp_buffer[FONT_LAYOUT_LINE_SIZE];
      size_t        new_msg_len     = 0;
      char         *new_msg         = font_driver_reshape_msg(msg, msg_len,
            tmp_buffer, sizeof(tmp_buffer), &new_msg_len);
#else
      char         *new_msg         = (char*)msg;
      size_t        new_msg_len     = msg_len;
#endif
      /* Without a block the backend draws this string on its own;
       * with one, the draw is counted when the block is flushed */
      if (p_disp && !(font->block_bound && renderer->flush))
         p_disp->stats.v[GFX_DISPLAY_STAT_FONT_DRAWS]++;
      renderer->render_msg(data,
            font->renderer_data, new_msg, new_msg_len, params);
   }
}

void font_driver_bind_block(void *font_data, void *block)
{
   font_data_t *font               = (font_data_t*)font_data;
   const font_renderer_t *renderer = font ? font->renderer : NULL;
   if (renderer && renderer->bind_block)
   {
      renderer->bind_block(font->renderer_data, block);
      font->block_bound = (block != NULL);
   }
}

/* Flushing is slow - only do it if font has actually been used */
void font_driver_sync_impl(font_data_impl_t *font_data)
{
   int glyph_width;
   uint32_t gen = font_driver_get_generation();

   if (!font_data || !font_data->font)
      return;
   if (font_data->metrics_generation == gen)
      return;

   font_data->metrics_generation = gen;

   if ((glyph_width = font_driver_get_message_width(
               font_data->font, "a", 1, 1.0f)) > 0)
      font_data->glyph_width     = (unsigned)glyph_width;

   font_data->wideglyph_width    = 100;

   if (font_data->wideglyph_str && glyph_width > 0)
   {
      int wide = font_driver_get_message_width(font_data->font,
            font_data->wideglyph_str,
            strlen(font_data->wideglyph_str), 1.0f);
      if (wide > 0)
         font_data->wideglyph_width = wide * 100 / glyph_width;
   }

   font_data->line_height        =
      (int)roundf(font_data->font->metrics.height);
   font_data->line_ascender      =
      (int)roundf(font_data->font->metrics.ascender);
   font_data->line_centre_offset =
      (int)roundf((font_data->font->metrics.ascender
            - font_data->font->metrics.descender) * 0.5f);
}

void font_flush(
      unsigned video_dims,
      font_data_impl_t *font_data)
{
   const font_renderer_t *renderer = font_data->font ? font_data->font->renderer : NULL;

   /* A rebuilt font has different metrics; pick them up before
    * anything is drawn with the old ones. */
   font_driver_sync_impl(font_data);

   if (font_data->raster_block.carr.coords.vertices == 0)
      return;
   {
      gfx_display_t *p_disp = disp_get_ptr();
      /* Quads asked for before this text have to land under it */
      gfx_display_flush_batch(p_disp);
      if (p_disp && renderer && renderer->flush)
         p_disp->stats.v[GFX_DISPLAY_STAT_FONT_DRAWS]++;
   }
   if (renderer && renderer->flush)
      renderer->flush(video_dims, font_data->font->renderer_data);
   font_data->raster_block.carr.coords.vertices = 0;
}

void font_driver_resolve_params(const struct font_params *params,
      font_params_resolved_t *out)
{
   if (params)
   {
      out->color_hp    = params->color_hp;
      out->x           = params->x;
      out->y           = params->y;
      out->scale       = params->scale;
      out->x_scale     = params->x_scale > 0.0f ? params->x_scale : 1.0f;
      out->drop_mod    = params->drop_mod;
      out->drop_alpha  = params->drop_alpha;
      out->drop_x      = params->drop_x;
      out->drop_y      = params->drop_y;
      out->text_align  = params->text_align;
      out->full_screen = params->full_screen;
      out->rgba[0]     = FONT_COLOR_GET_RED(params->color);
      out->rgba[1]     = FONT_COLOR_GET_GREEN(params->color);
      out->rgba[2]     = FONT_COLOR_GET_BLUE(params->color);
      out->rgba[3]     = FONT_COLOR_GET_ALPHA(params->color);
      out->color[0]    = out->rgba[0] / 255.0f;
      out->color[1]    = out->rgba[1] / 255.0f;
      out->color[2]    = out->rgba[2] / 255.0f;
      out->color[3]    = out->rgba[3] / 255.0f;
   }
   else
   {
      settings_t *settings = config_get_ptr();
      out->color_hp    = NULL;
      out->x           = settings->floats.video_msg_pos_x;
      out->y           = settings->floats.video_msg_pos_y;
      out->scale       = 1.0f;
      out->x_scale     = 1.0f;
      out->drop_mod    = 0.3f;
      out->drop_alpha  = 1.0f;
      out->drop_x      = -2;
      out->drop_y      = -2;
      out->text_align  = TEXT_ALIGN_LEFT;
      out->full_screen = true;
      out->color[0]    = settings->floats.video_msg_color_r;
      out->color[1]    = settings->floats.video_msg_color_g;
      out->color[2]    = settings->floats.video_msg_color_b;
      out->color[3]    = 1.0f;
      out->rgba[0]     = (unsigned)(out->color[0] * 255);
      out->rgba[1]     = (unsigned)(out->color[1] * 255);
      out->rgba[2]     = (unsigned)(out->color[2] * 255);
      out->rgba[3]     = 255;
   }
}

int font_renderer_get_message_width(
      const font_renderer_driver_t *renderer, void *renderer_data,
      const char *msg, size_t msg_len, float scale)
{
   const struct font_glyph *(*get_glyph)(void*, uint32_t);
   void *font_data = renderer_data;
   int width       = 0;

   if (!renderer || !renderer_data || !msg)
      return 0;

   get_glyph = renderer->get_glyph;
#define FONT_MEASURE_SUM width
#include "font_measure.h"
   return (int)(width * scale);
}

int font_driver_get_message_width(void *font_data,
      const char *msg, size_t len, float scale)
{
   font_data_t *font               = (font_data_t*)(font_data
         ? font_data : (font_driver_video_st
            ? (void*)font_driver_video_st->osd_font : NULL));
   const font_renderer_t *renderer = font ? font->renderer : NULL;
   if (renderer && renderer->get_message_width)
      return renderer->get_message_width(font->renderer_data, msg, len, scale);
   return -1;
}

#ifdef HAVE_THREADS
typedef struct
{
   const font_renderer_t *renderer;
   void                  *renderer_data;
   bool                   is_threaded;
} font_free_cmd_t;

static uintptr_t font_driver_free_wrap(void *data)
{
   font_free_cmd_t *cmd = (font_free_cmd_t*)data;
   if (cmd->renderer && cmd->renderer->free)
      cmd->renderer->free(cmd->renderer_data, cmd->is_threaded);
   return 0;
}
#endif

/* Free the renderer-owned state (glyph atlas / GPU textures / etc.)
 * behind a (renderer, handle) pair.  Shared between the normal
 * font_driver_free teardown path and the OOM cleanup path in
 * font_driver_init_first; keeping the thread-dispatch logic in one
 * place avoids the two call sites drifting out of sync.
 *
 * When threaded video is active, font resources (GPU textures, GL
 * names, D3D COM objects) belong to the video thread's rendering
 * context.  Freeing them on the main thread races with the video
 * thread's draw calls:
 *
 *  - GL: context is single-threaded; gl2_raster_font_free
 *    calls make_current to steal the context, but the video
 *    thread may be mid-frame.
 *  - D3D11: ImmediateContext is not thread-safe; Release on
 *    the main thread while the video thread draws is UB.
 *  - D3D12: fenceValue++ from the main thread races with
 *    the video thread's own fence signalling.
 *  - Vulkan: the atlas textures are parked for the frame thread
 *    to retire, and the glyph uploads share its staging pool.
 *
 * Dispatch renderer->free to the video thread via
 * video_thread_texture_handle so it runs serialised with
 * the video thread's frame rendering.  This is the same
 * pattern used by texture load/unload.
 *
 * video_thread_texture_handle is self-safe: if the wrapper
 * is not active (the wrapper is not active),
 * it falls back to calling func(data) on the current thread.
 * If called from the video thread itself, it calls func
 * directly (no deadlock). */
static void font_driver_release_renderer_state(
      const font_renderer_t *renderer, void *renderer_data,
      bool is_threaded)
{
   if (!renderer || !renderer->free)
      return;

#ifdef HAVE_THREADS
   /* Same reasoning as the init side: the free has to reach whichever
    * thread owns the context, and the font's recorded is_threaded can
    * be stale by the time it is released. */
   if (is_threaded || video_driver_thread_wrapper_active())
   {
      font_free_cmd_t cmd;
      cmd.renderer      = renderer;
      cmd.renderer_data = renderer_data;
      /* The renderer's is_threaded means "not on the context thread,
       * bind it yourself": the GL renderers answer it with
       * make_current(), and on release that unbinds the context from
       * the calling thread. This call runs on the video thread, whose
       * context is already current and must stay so. */
      cmd.is_threaded   = false;
      video_thread_texture_handle(&cmd, font_driver_free_wrap);
      return;
   }
#endif

   renderer->free(renderer_data, is_threaded);
}

void font_driver_free(font_data_t *font)
{
   if (font)
   {
      bool is_threaded        = false;
      font_data_t **link      = &font_live[font->owner];

      /* Invalidate any externally cached per-font derived data */
      retro_atomic_fetch_add_int(&font_driver_generation, 1);

      while (*link)
      {
         if (*link == font)
         {
            *link = font->next;
            break;
         }
         link = &(*link)->next;
      }

      free(font->path);
      free(font->lang_pkg_dir);
      free(font->lang_default_path);
      font->path              = NULL;
      font->lang_pkg_dir      = NULL;
      font->lang_default_path = NULL;

#ifdef HAVE_THREADS
      /* Ask for the real threaded state, not the video_threaded
       * setting. The two differ when a hw-render core is loaded,
       * since that forces the video driver to run non-threaded. */
      is_threaded = video_driver_is_threaded();
#endif

      font_driver_release_renderer_state(font->renderer,
            font->renderer_data, is_threaded);

      font->renderer      = NULL;
      font->renderer_data = NULL;

      free(font);
   }
}

bool font_driver_matches(const font_data_t *font,
      const char *path, float size)
{
   const font_data_t *live;

   if (!font)
      return false;

   /* Confirm the handle is still one of ours before reading through
    * it. A caller that freed a font without clearing its pointer
    * would otherwise turn this into a use-after-free, and "no" is the
    * answer it needs in that case anyway. The list holds one entry
    * per live font, so this is a walk of a handful of pointers. */
   for (live = font_live[font_driver_owner_here()]; live;
         live = live->next)
      if (live == font)
         break;
   if (!live)
      return false;

   if (font->size != size)
      return false;

   /* A font whose face the renderer chose carries no path, and can
    * only match a request that likewise names none. */
   if (!font->path)
      return (!path || !*path);
   if (!path || !*path)
      return false;

   return string_is_equal(font->path, path);
}

static void font_driver_defer(unsigned owner, font_data_t *font)
{
   unsigned i;

   for (i = 0; i < FONT_FREE_DEFERRED_MAX; i++)
   {
      if (!font_free_deferred[owner][i].font)
      {
         font_free_deferred[owner][i].font   = font;
         font_free_deferred[owner][i].frames = FONT_FREE_DEFERRED_FRAMES;
         return;
      }
   }

   /* Queue full. Only reachable if a caller retires faster than
    * frames go out, which would mean it is rebuilding more than
    * FONT_FREE_DEFERRED_MAX/2 fonts between two frames. Freeing here
    * is the behaviour this function exists to avoid, so say so rather
    * than leak the handle silently. */
   RARCH_WARN("[Font] Deferred free queue full; releasing immediately.\n");
   font_driver_free(font);
}

void font_driver_free_deferred(font_data_t *font)
{
   unsigned owner;

   if (!font)
      return;
   if ((owner = font_driver_owner_here()) != font->owner)
   {
      font_list_head_t *head = &font_retired[font->owner];
      for (;;)
      {
         void *top          = FONT_HEAD_LOAD(head);
         font->retired_next = top;
         if (FONT_HEAD_CAS(head, top, font))
            return;
      }
   }
   font_driver_defer(owner, font);
}

void font_driver_free_pending(bool flush)
{
   unsigned i;
   unsigned owner         = font_driver_owner_here();
   font_list_head_t *head = &font_retired[owner];
   font_data_t *retired;

   /* What other threads retired: into the queue, or gone on a flush */
   for (;;)
   {
      retired = (font_data_t*)FONT_HEAD_LOAD(head);
      if (!retired || FONT_HEAD_CAS(head, retired, NULL))
         break;
   }
   while (retired)
   {
      font_data_t *next = (font_data_t*)retired->retired_next;
      if (flush)
         font_driver_free(retired);
      else
         font_driver_defer(owner, retired);
      retired           = next;
   }

   for (i = 0; i < FONT_FREE_DEFERRED_MAX; i++)
   {
      if (!font_free_deferred[owner][i].font)
         continue;

      if (flush || --font_free_deferred[owner][i].frames == 0)
      {
         font_data_t *font          = font_free_deferred[owner][i].font;
         /* Cleared before the free: font_driver_free() can marshal to
          * the video thread, and this slot must not look occupied to
          * anything that runs in the meantime. */
         font_free_deferred[owner][i].font = NULL;
         font_driver_free(font);
      }
   }
}

font_data_t *font_driver_init_first(
      void *video_data, const char *font_path, float font_size,
      bool threading_hint, bool is_threaded,
      const font_renderer_t *backend)
{
   const void *font_driver = NULL;
   void *font_handle       = NULL;
   bool ok                 = false;
#ifdef HAVE_THREADS
   /* Dispatch on the wrapper's presence, not on is_threaded: font
    * resources belong to whichever thread drives the driver, and
    * between SET_HW_RENDER and the video reinit that follows,
    * is_threaded already reads false while the wrapper still owns
    * the context. video_thread_font_init() re-checks and refuses if
    * the wrapper is gone. */
   if (     threading_hint
         && video_driver_thread_wrapper_active())
      /* Runs on the video thread, where the context is current;
       * is_threaded would make a GL renderer rebind it from there. */
      ok = video_thread_font_init(&font_driver, &font_handle,
            video_data, font_path, font_size, backend, font_init_first,
            false);
   else
#endif
   ok = font_init_first(&font_driver, &font_handle,
         video_data, font_path, font_size, backend, is_threaded);

   if (ok)
   {
      font_data_t *font      = (font_data_t*)malloc(sizeof(*font));

      if (font)
      {
         font->renderer      = (const font_renderer_t*)font_driver;
         font->renderer_data = font_handle;
         font->size          = font_size;
         font->next          = NULL;
         font->video_data    = video_data;
         font->path          = (font_path && *font_path)
            ? strdup(font_path) : NULL;
         font->lang_pkg_dir      = NULL;
         font->lang_default_path = NULL;
         font->is_threaded   = is_threaded;
         font->threading_hint= threading_hint;
         /* No block is bound until the first one is */
         font->block_bound   = false;

         font_driver_cache_metrics(font);

         /* Track it so font_driver_reload_fonts() can find it. */
         font->owner         = (uint8_t)font_driver_owner_here();
         font->next          = font_live[font->owner];
         font_live[font->owner] = font;

         return font;
      }

      /* Wrapper malloc failed after font_init_first (or
       * video_thread_font_init) had already succeeded.  The raster
       * font's init path allocates the glyph atlas / GPU textures /
       * COM objects behind font_handle; returning NULL here without
       * releasing them would leak the entire raster-font state and,
       * on subsequent re-init attempts, accumulate.  Dispatch via
       * the shared helper so threaded-video builds free GPU state
       * on the video thread, matching the normal teardown path. */
      font_driver_release_renderer_state(
            (const font_renderer_t*)font_driver,
            font_handle, is_threaded);
   }

   return NULL;
}

/* Unconditional release. Callers outside this file must go through
 * font_driver_free_osd_for(), which will not touch a font belonging to
 * another driver instance. */
static void font_driver_free_osd(void)
{
   video_driver_state_t *video_st = font_driver_video_st;

   /* Unbound == video never initialised == no shared OSD font. */
   if (!video_st)
      return;

   if (video_st->osd_font)
      font_driver_free((font_data_t*)video_st->osd_font);

   video_st->osd_font       = NULL;
   video_st->osd_font_owner = NULL;
}

void font_driver_init_osd(
      void *video_data,
      const video_info_t *video_info,
      bool is_threaded,
      const font_renderer_t *backend)
{
   /* A font left over from a different instance cannot be adopted:
    * its images belong to a device that is gone, whose handles the
    * new one will recycle. Drop it rather than keep it. Guarding on
    * presence alone is what let a stale font survive a reinit. */
   video_driver_state_t *video_st = font_driver_video_st;

   /* Unbound == video never initialised == no shared OSD font. */
   if (!video_st)
      return;

   if (video_st->osd_font && video_st->osd_font_owner != video_data)
   {
      font_data_t *stale = (font_data_t*)video_st->osd_font;

      RARCH_WARN("[Font] Discarding stale OSD font from a previous video driver instance.\n");
      stale->renderer = NULL;
      stale->renderer_data = NULL;
      font_driver_free_osd();
   }

   /* threading_hint is false: both callers - video_driver_init_internal()
    * and the threaded wrapper's CMD_INIT - already run on the thread that
    * owns the graphics context, so there is nothing to marshal. The hint
    * exists for callers that do not, such as gfx_display. */
   if (!video_st->osd_font && video_info)
      video_st->osd_font = font_driver_init_first(video_data,
            *video_info->path_font ? video_info->path_font : NULL,
            video_info->font_size, false, is_threaded, backend);

   if (video_st->osd_font)
      video_st->osd_font_owner = video_data;
}

bool font_driver_reinit_osd(const char *font_path, float font_size)
{
   video_driver_state_t *video_st = font_driver_video_st;
   font_data_t          *font     = video_st
         ? (font_data_t*)video_st->osd_font : NULL;

   /* No shared OSD font: video is not up, or the driver keeps its own
    * and never registered one here. Let the caller fall back. */
   if (!font)
      return false;

   /* Normalised the way font_driver_init_osd() passes it, so both the
    * comparison below and the rebuild see the same thing it would. */
   if (font_path && !*font_path)
      font_path = NULL;

   /* Nothing to do. Shares the predicate with the menu and widget
    * layout paths rather than repeating it: the empty-path and exact
    * size rules are fiddly enough that a second copy would drift, and
    * this one additionally declines to read through a handle that is
    * no longer live. The exact size compare is deliberate — both
    * sides come from the same settings float, and excess x87
    * precision can only cost a redundant rebuild, never merge two
    * sizes. */
   if (font_driver_matches(font, font_path, font_size))
      return true;

   if (!font_driver_rebuild(font, font_path, font_size))
   {
      RARCH_WARN("[Font] Could not rebuild the OSD font; keeping the current one.\n");
      return true;
   }

   /* Derived data cached outside this file - the widgets' line
    * metrics, gfx_animation's ticker widths - is now stale. */
   retro_atomic_fetch_add_int(&font_driver_generation, 1);
   return true;
}

void font_driver_free_osd_for(void *video_data)
{
   /* Only the owner may free it. Teardown of an instance that no
    * longer owns the font - a stale or deferred free - must leave the
    * live one alone. */
   video_driver_state_t *video_st = font_driver_video_st;

   /* Unbound == video never initialised == no shared OSD font. */
   if (!video_st)
      return;

   if (video_st->osd_font && video_st->osd_font_owner == video_data)
      font_driver_free_osd();
}
