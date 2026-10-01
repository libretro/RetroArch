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
   struct font_file_ref *next;
   char                 *path;
   uint8_t              *data;
   /* The open file, when data is mapped from it rather than read */
   RFILE                *mapped;
   size_t                len;
   unsigned              refs;
} font_file_ref_t;

static font_file_ref_t *font_file_refs;

/* A plain int rather than an slock_t, because an slock_t has to be
 * created and there is nowhere race-free to create it: rthreads has no
 * one-time-init primitive, and the first font is not reliably built on
 * a known thread - with threaded video, init is dispatched to the video
 * thread.  A statically initialised word needs no such moment.
 *
 * Spinning is only acceptable because every critical section here is a
 * walk of a list that holds one entry per distinct font file - two, in
 * the case this exists for.  The file read deliberately happens outside
 * the lock; spinning while another thread pulls megabytes off a memory
 * card would be far worse than the contention it avoids. */
/* Gate on the primitives rather than on a platform list, so a target
 * that gains atomics gains the locking with them.  Both gates are
 * needed: the fallback backend has no compare-and-swap at all, and the
 * PS2 backend has one that is atomic but not lock-free, taken with the
 * interrupts masked.  Spinning on it would hang the EE outright -- one
 * core, and a kernel that reschedules only out of an interrupt, so the
 * thread holding the word never runs again.  Those targets keep the
 * unlocked path, where the list is only ever touched from one
 * thread. */
#if defined(HAVE_THREADS) && defined(retro_atomic_cas_int) \
 && defined(RETRO_ATOMIC_LOCK_FREE)
static retro_atomic_int_t font_file_lock_word;
#define FONT_FILE_LOCK() \
   do { while (!retro_atomic_cas_int(&font_file_lock_word, 0, 1)) { } } while (0)
#define FONT_FILE_UNLOCK() \
   retro_atomic_store_release_int(&font_file_lock_word, 0)
#else
#define FONT_FILE_LOCK()   do { } while (0)
#define FONT_FILE_UNLOCK() do { } while (0)
#endif

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

/* Caller holds the lock. */
static font_file_ref_t *font_file_ref_lookup(const char *path)
{
   font_file_ref_t *entry;

   for (entry = font_file_refs; entry; entry = entry->next)
      if (string_is_equal(entry->path, path))
      {
         entry->refs++;
         return entry;
      }
   return NULL;
}

/* Takes the lock itself, and does the file read outside it.  A second
 * thread asking for the same path while the read is in flight will
 * read it too and lose the race to insert; that costs one redundant
 * read in a case that does not arise in this codebase, and is the
 * price of not holding a lock across storage I/O. */
static font_file_ref_t *font_file_ref_acquire(const char *path)
{
   font_file_ref_t *entry;
   font_file_ref_t *raced;
   RFILE           *mapped = NULL;
   void            *data   = NULL;
   int64_t          len    = 0;

   FONT_FILE_LOCK();
   entry = font_file_ref_lookup(path);
   FONT_FILE_UNLOCK();

   if (entry)
      return entry;

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
      return NULL;
   }

   if (!(entry = (font_file_ref_t*)calloc(1, sizeof(*entry))))
   {
      font_file_data_free(mapped, data);
      return NULL;
   }
   {
      /* strlcpy rather than strdup: the latter is not declared under
       * a strict C89 compile, and this file is built that way. */
      size_t path_len = strlen(path) + 1;
      if (!(entry->path = (char*)malloc(path_len)))
      {
         font_file_data_free(mapped, data);
         free(entry);
         return NULL;
      }
      strlcpy(entry->path, path, path_len);
   }

   entry->data    = (uint8_t*)data;
   entry->mapped  = mapped;
   entry->len     = (size_t)len;
   entry->refs    = 1;

   FONT_FILE_LOCK();
   if ((raced = font_file_ref_lookup(path)))
   {
      FONT_FILE_UNLOCK();
      font_file_data_free(entry->mapped, entry->data);
      free(entry->path);
      free(entry);
      return raced;
   }
   entry->next    = font_file_refs;
   font_file_refs = entry;
   FONT_FILE_UNLOCK();
   return entry;
}

/* Caller holds the lock. */
static void font_file_ref_release(font_file_ref_t *entry)
{
   font_file_ref_t **link;

   if (!entry || --entry->refs)
      return;

   for (link = &font_file_refs; *link; link = &(*link)->next)
      if (*link == entry)
      {
         *link = entry->next;
         break;
      }

   font_file_data_free(entry->mapped, entry->data);
   free(entry->path);
   free(entry);
}

/* Caller holds the lock.  On failure the reference is dropped rather
 * than leaked: a font whose use cannot be recorded would keep the
 * buffer alive for the rest of the run. */
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
/* Padding between cells, so linear filtering does not bleed one
 * glyph into the next */
#define FONT_CACHE_PADDING   1
#define FONT_CACHE_HASH_SIZE 0x100
#define FONT_CACHE_HASH(c)   (((c) ^ ((c) >> 8)) & (FONT_CACHE_HASH_SIZE - 1))
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
 * and the widgets give it when they set up their language fonts.
 * Guarded by the file lock. */
static char font_fallback_pkg_dir[PATH_MAX_LENGTH];

/* One read per fallback file per process. The bytes are published
 * before the state flips to READY, and are kept for the life of the
 * process once read. */
static retro_atomic_int_t font_fallback_state[FONT_FALLBACK_COUNT];
#ifdef FONT_FALLBACK_LOADS
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
   font_cache_slot_t *map[FONT_CACHE_HASH_SIZE];
   font_cache_slot_t slots[FONT_CACHE_SLOTS];
   /* The cells each growth added, so no cell ever moves */
   font_cache_slot_t *grown[FONT_CACHE_GROWTHS];
   unsigned grown_len[FONT_CACHE_GROWTHS];
   unsigned growths;
   unsigned cols;
   unsigned rows;
   /* A frame wanted more cells than there were: grow when the next one
    * asks for the atlas */
   unsigned grow_frame;
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
   FONT_FILE_LOCK();
   if (*font_fallback_pkg_dir)
      fill_pathname_join_special(load->path, font_fallback_pkg_dir,
            font_fallback_files[id], sizeof(load->path));
   else
      *load->path = '\0';
   FONT_FILE_UNLOCK();

   if (     !*load->path
         || !(thread = sthread_create(font_fallback_load_thread, load)))
   {
      free(load);
      /* Nowhere to read it from yet: let a later miss try again once
       * the pkg directory is known */
      retro_atomic_store_release_int(&font_fallback_state[id],
            *font_fallback_pkg_dir ? FONT_FALLBACK_FAILED
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
 * reusing it would draw the new glyph in the earlier one's place. 0
 * until the first frame, which leaves plain LRU in place for a cache
 * used outside the video loop. */
static retro_atomic_int_t font_frame_epoch;

void font_driver_frame_begin(void)
{
   retro_atomic_fetch_add_int(&font_frame_epoch, 1);
}

/* The least recently used cell not looked up in this frame, taken out
 * of the hash; NULL when every cell is in use this frame */
static font_cache_slot_t *font_cache_take_slot(font_cache_t *c)
{
   unsigned i;
   unsigned frame      = (unsigned)retro_atomic_load_acquire_int(
         &font_frame_epoch);
   unsigned oldest_age = 0;
   font_cache_slot_t **link;

   font_cache_slot_t *victim = NULL;
   unsigned b;

   for (b = 0; b <= c->growths; b++)
   {
      font_cache_slot_t *blk = b ? c->grown[b - 1] : c->slots;
      unsigned           len = b ? c->grown_len[b - 1] : FONT_CACHE_SLOTS;
      for (i = 0; i < len; i++)
      {
         /* Unsigned subtraction handles the counter wrapping */
         unsigned age = c->usage_counter - blk[i].last_used;
         if (frame && blk[i].last_frame == frame)
            continue;
         if (!victim || age > oldest_age)
         {
            oldest_age = age;
            victim     = &blk[i];
         }
      }
   }

   if (!victim)
   {
      /* Room is made at the start of a later frame, when nothing drawn
       * from the atlas at its present size is still waiting */
      if (c->atlas.max_width && c->growths < FONT_CACHE_GROWTHS)
      {
         c->grow_wanted = true;
         c->grow_frame  = frame;
      }
      return NULL;
   }

   if (victim->charcode != FONT_CACHE_NO_CODE)
   {
      for (link = &c->map[FONT_CACHE_HASH(victim->charcode)];
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

static void font_cache_dirty_cell(struct font_atlas *atlas,
      unsigned x, unsigned y, unsigned w, unsigned h)
{
   if (!atlas->dirty)
   {
      atlas->dirty_x0 = x;
      atlas->dirty_y0 = y;
      atlas->dirty_x1 = x + w;
      atlas->dirty_y1 = y + h;
      atlas->dirty    = true;
   }
   else
   {
      if (x < atlas->dirty_x0)
         atlas->dirty_x0 = x;
      if (y < atlas->dirty_y0)
         atlas->dirty_y0 = y;
      if (x + w > atlas->dirty_x1)
         atlas->dirty_x1 = x + w;
      if (y + h > atlas->dirty_y1)
         atlas->dirty_y1 = y + h;
   }
}

static const struct font_glyph *font_cache_miss(font_cache_t *c,
      uint32_t code)
{
   unsigned gi                   = c->rast->glyph_index(c->face, code);
   const font_rasterizer_t *rast = c->rast;
   void *face                    = c->face;
   font_cache_slot_t *slot;
   unsigned cell_w, cell_h;
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
   cell_w = VIDEO_SCALE_W(c->cell_dims);
   cell_h = VIDEO_SCALE_H(c->cell_dims);
   esz    = (c->atlas.format == FONT_ATLAS_FORMAT_A16)
      ? sizeof(uint16_t) : sizeof(uint8_t);
   dst    = c->atlas.buffer
      + ((size_t)slot->glyph.atlas_offset_x
      +  (size_t)slot->glyph.atlas_offset_y * c->atlas.width) * esz;

   if (!rast->render_glyph(face, code, gi, dst, c->atlas.width,
            cell_w, cell_h, c->atlas.format, &slot->glyph))
   {
      /* Nothing to draw: the cell goes back as the oldest there is */
      slot->last_used  = c->usage_counter - 0x80000000u;
      slot->last_frame--;
      return NULL;
   }

   slot->charcode               = code;
   slot->next                   = c->map[FONT_CACHE_HASH(code)];
   c->map[FONT_CACHE_HASH(code)] = slot;
   slot->last_used              = c->usage_counter++;
   font_cache_dirty_cell(&c->atlas, slot->glyph.atlas_offset_x,
         slot->glyph.atlas_offset_y, cell_w, cell_h);
   return &slot->glyph;
}

static const struct font_glyph *font_cache_get_glyph(void *data,
      uint32_t code)
{
   font_cache_t      *c = (font_cache_t*)data;
   font_cache_slot_t *slot;

   if (!c)
      return NULL;

   for (slot = c->map[FONT_CACHE_HASH(code)]; slot; slot = slot->next)
   {
      if (slot->charcode == code)
      {
         slot->last_used  = c->usage_counter++;
         slot->last_frame = (unsigned)retro_atomic_load_acquire_int(
               &font_frame_epoch);
         return &slot->glyph;
      }
   }

   return font_cache_miss(c, code);
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
         || width  > c->atlas.max_width
         || height > c->atlas.max_height)
      return false;
   if (!(buffer = (uint8_t*)calloc(height, (size_t)width * esz)))
      return false;
   if (!(blk = (font_cache_slot_t*)calloc(added, sizeof(*blk))))
   {
      free(buffer);
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
         blk[i].glyph.atlas_offset_x = x * (cell_w + FONT_CACHE_PADDING);
         blk[i].glyph.atlas_offset_y = y * (cell_h + FONT_CACHE_PADDING);
         /* Unused: older than any cell in use */
         blk[i].last_used            = c->usage_counter - 0x80000000u;
         i++;
      }
   }

   free(c->atlas.buffer);
   c->atlas.buffer             = buffer;
   c->atlas.width              = width;
   c->atlas.height             = height;
   c->grown[c->growths]        = blk;
   c->grown_len[c->growths++]  = added;
   c->cols                     = cols;
   c->rows                     = rows;
   /* All of it, for a consumer making its texture anew */
   c->atlas.dirty              = true;
   c->atlas.dirty_x0           = 0;
   c->atlas.dirty_y0           = 0;
   c->atlas.dirty_x1           = width;
   c->atlas.dirty_y1           = height;
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
   if (c->grow_wanted && (unsigned)retro_atomic_load_acquire_int(
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
      FONT_FILE_LOCK();
      font_file_ref_release(c->ref);
      FONT_FILE_UNLOCK();
   }
   for (i = 0; i < c->growths; i++)
      free(c->grown[i]);
   free(c->atlas.buffer);
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
         c->slots[i].glyph.atlas_offset_x = x * (cell_w + FONT_CACHE_PADDING);
         c->slots[i].glyph.atlas_offset_y = y * (cell_h + FONT_CACHE_PADDING);
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
      FONT_FILE_LOCK();
      font_file_ref_release(ref);
      FONT_FILE_UNLOCK();
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

/* Every live font, so they can be rebuilt when the file behind them
 * should change. Singly linked through font_data_t::next. */
static font_data_t *font_live = NULL;

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
 * submitted and waited on. The queue is global because only one menu
 * driver is live at a time and the widget and screensaver fonts share
 * the same frame clock; sizing is per handle in flight, not per
 * caller.
 *
 * Unlocked, like font_live and unlike the font file cache above: every
 * caller is on the main thread. The retires come from menu and widget
 * render paths, and the ageing from video_driver_frame(), which the
 * runloop calls on the main thread and which the threaded wrapper
 * forwards from rather than runs on the video thread. That invariant
 * is not new here — video_driver_frame() already keeps its frame
 * timing in function statics, which would race far more visibly. */
#define FONT_FREE_DEFERRED_MAX 16
#define FONT_FREE_DEFERRED_FRAMES 2

static struct
{
   font_data_t *font;
   unsigned     frames;
} font_free_deferred[FONT_FREE_DEFERRED_MAX];

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
      FONT_FILE_LOCK();
      strlcpy(font_fallback_pkg_dir, pkg_dir,
            sizeof(font_fallback_pkg_dir));
      FONT_FILE_UNLOCK();
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

unsigned font_driver_reload_fonts(void)
{
   font_data_t *font;
   unsigned     n = 0;

   for (font = font_live; font; font = font->next)
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
               FONT_FILE_LOCK();
               font_file_ref_release(entry);
               FONT_FILE_UNLOCK();
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
         FONT_FILE_LOCK();
         font_file_ref_release(entry);
         FONT_FILE_UNLOCK();
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
 * Neutral:
 * 0020 - 002F: 001xxxxx (c & 0xE0) == 0x20
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
#define IS_DIR_NEUTRAL(p)  ((*(p)&0xE0) == 0x20)
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

static INLINE unsigned font_get_arabic_replacement(
      const char* src, const char* start, const char* end)
{
   /* 0x0620 to 0x064F */
   static const unsigned arabic_shape_map[0x100][0x4] = {
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

         if (prev2 >= start)
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

         prev_connected = !!arabic_shape_map[prev2_id][2];

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
      prev_connected = !!arabic_shape_map[prev_id][2];
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

      next_connected = !!arabic_shape_map[next_id][1];
   }

   if ((ret =
            arabic_shape_map[id][prev_connected | (next_connected <<
               1)]))
      return ret;
   return arabic_shape_map[id][prev_connected];
}
/* clang-format on */

/* True if any byte has bit 7 set.
 *
 * Everything the reshaper reacts to needs one: IS_MBCONT is 0x80-0xBF,
 * IS_HEBREW 0xD6-0xD7, IS_ARABIC 0xD8-0xDB. IS_DIR_NEUTRAL does match
 * ASCII 0x20-0x3F, but it is only consulted after an IS_RTL hit has
 * set reverse or entered a skip loop, which plain ASCII cannot reach.
 * So a message with no high bytes leaves the reshaper byte-identical
 * to the way it went in. */
static INLINE bool font_msg_has_high_byte(const char *msg, size_t msg_len)
{
   const unsigned char *p = (const unsigned char*)msg;
   const unsigned char *e = p + msg_len;
   const size_t      mask = (size_t)~(size_t)0 / 0xFF * 0x80;

   while (p < e && ((uintptr_t)p & (sizeof(size_t) - 1)))
      if (*p++ & 0x80)
         return true;

   while (p + sizeof(size_t) <= e)
   {
      size_t w;
      memcpy(&w, p, sizeof(w));
      if (w & mask)
         return true;
      p += sizeof(size_t);
   }

   while (p < e)
      if (*p++ & 0x80)
         return true;
   return false;
}

static char* font_driver_reshape_msg(const char* msg, size_t msg_len,
      unsigned char *s, size_t len, size_t *out_len)
{
   const unsigned char *src;
   bool                 reverse    = false;
   /* worst case transformations are 2 bytes to 4 bytes -- aliaspider */
   size_t               _len       = (msg_len * 2) + 1;
   unsigned char       *dst        = s;
   /* Highest dst that can still take the longest sequence emitted
    * below (4 bytes) plus the terminator. The 2x estimate above only
    * holds while the walk moves forward; the reverse pass can step
    * back over bytes it has already emitted, so output length is not
    * actually bounded by the input length and the buffer has to be
    * bounded directly. */
   unsigned char       *dst_max    = s + len - 5;

   /* Nothing to reshape: hand back the input and skip both the walk
    * and the copy into s. This is every English HUD string, including
    * the statistics block s is sized for. */
   if (!font_msg_has_high_byte(msg, msg_len))
   {
      *out_len = msg_len;
      return (char*)msg;
   }

   if (len < _len)
   {
      /* Input too long for the buffer: truncate to fit.
       * With a 512-byte caller buffer the limit is 255 source bytes,
       * which exceeds any realistic on-screen message.  This path
       * is effectively dead code for normal HUD/OSD rendering.
       *
       * Place the truncated, null-terminated copy in the upper half
       * of the buffer (offset len/2).  The output grows forward
       * from s[0] at most 2x the source consumption rate, so
       * dst can never overtake src: after consuming k source bytes,
       * dst <= 2k while src = len/2 + k, and 2k < len/2 + k
       * holds for all k < len/2, which is guaranteed since
       * msg_len < len/2. */
      unsigned char *copy_dst;
      msg_len = (len / 2) - 1;
      /* Back up to a UTF-8 character boundary */
      while (msg_len > 0 && IS_MBCONT((const unsigned char*)&msg[msg_len]))
         msg_len--;
      copy_dst = s + (len / 2);
      memcpy(copy_dst, msg, msg_len);
      copy_dst[msg_len] = '\0';
      msg = (const char*)copy_dst;
   }

   src = (const unsigned char*)msg;

   while ((*src || reverse) && dst < dst_max)
   {
      if (reverse)
      {
         src--;
         while (src > (const unsigned char*)msg && IS_MBCONT(src))
            src--;

         if (src >= (const unsigned char*)msg && (IS_RTL(src) || IS_DIR_NEUTRAL(src) || is_misc_ws(src)))
         {
            if (IS_ARABIC(src))
            {
               unsigned replacement = font_get_arabic_replacement(
                     (const char*)src, msg, (const char*)msg + msg_len);

               if (replacement)
               {
                  if (replacement < 0x80)
                     *dst++ = replacement;
                  else if (replacement < 0x800)
                  {
                     *dst++ = 0xC0 | (replacement >> 6);
                     *dst++ = 0x80 | (replacement       & 0x3F);
                  }
                  else if (replacement < 0x10000)
                  {
                     /* merged glyphs */
                     if ((replacement >= 0xFEF5) && (replacement <= 0xFEFC))
                        src -= 2;

                     *dst++ = 0xE0 | ( replacement >> 12);
                     *dst++ = 0x80 | ((replacement >>  6) & 0x3F);
                     *dst++ = 0x80 | ( replacement        & 0x3F);
                  }
                  else
                  {
                     *dst++ = 0xF0 |  (replacement >> 18);
                     *dst++ = 0x80 | ((replacement >> 12) & 0x3F);
                     *dst++ = 0x80 | ((replacement >>  6) & 0x3F);
                     *dst++ = 0x80 | ( replacement        & 0x3F);
                  }

                  continue;
               }
            }

            *dst++ = *src++;
            while (IS_MBCONT(src) && dst < dst_max)
               *dst++ = *src++;
            src--;

            while (IS_MBCONT(src))
               src--;
         }
         else
         {
            reverse = false;
            src++;
            while (  IS_MBCONT(src)
                  || IS_RTL(src)
                  || IS_DIR_NEUTRAL(src)
                  || is_misc_ws(src))
               src++;
         }
      }
      else
      {
         if (IS_RTL(src))
         {
            reverse = true;
            while (  IS_MBCONT(src)
                  || IS_RTL(src)
                  || IS_DIR_NEUTRAL(src)
                  || is_misc_ws(src))
               src++;
         }
         else
            *dst++ = *src++;
      }
   }

   *dst = '\0';
   *out_len = (size_t)(dst - s);
   return (char*)s;
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
      unsigned char tmp_buffer[1536];
      size_t        new_msg_len     = 0;
      char         *new_msg         = font_driver_reshape_msg(msg, msg_len,
            tmp_buffer, sizeof(tmp_buffer), &new_msg_len);
#else
      char         *new_msg         = (char*)msg;
      size_t        new_msg_len     = msg_len;
#endif
      /* Without a block the backend draws this string on its own;
       * with one, the draw is counted when the block is flushed */
      if (p_disp && !(font->block_bound && renderer->bind_block))
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
   const struct font_glyph *glyph_q;
   void *font_data = renderer_data;
   int width       = 0;

   if (!renderer || !renderer_data || !msg)
      return 0;

   get_glyph = renderer->get_glyph;
   glyph_q   = get_glyph(font_data, '?');
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
      font_data_t **link      = &font_live;

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
   for (live = font_live; live; live = live->next)
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

void font_driver_free_deferred(font_data_t *font)
{
   unsigned i;

   if (!font)
      return;

   for (i = 0; i < FONT_FREE_DEFERRED_MAX; i++)
   {
      if (!font_free_deferred[i].font)
      {
         font_free_deferred[i].font   = font;
         font_free_deferred[i].frames = FONT_FREE_DEFERRED_FRAMES;
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

void font_driver_free_pending(bool flush)
{
   unsigned i;

   for (i = 0; i < FONT_FREE_DEFERRED_MAX; i++)
   {
      if (!font_free_deferred[i].font)
         continue;

      if (flush || --font_free_deferred[i].frames == 0)
      {
         font_data_t *font          = font_free_deferred[i].font;
         /* Cleared before the free: font_driver_free() can marshal to
          * the video thread, and this slot must not look occupied to
          * anything that runs in the meantime. */
         font_free_deferred[i].font = NULL;
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

         font_driver_cache_metrics(font);

         /* Track it so font_driver_reload_fonts() can find it. */
         font->next          = font_live;
         font_live           = font;

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
