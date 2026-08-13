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

#include <stdlib.h>
#include <string.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../msg_hash.h"
#include "../verbosity.h"
#include "font_driver.h"
#include "video_thread_wrapper.h"
#include <retro_atomic.h>

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
 * from it, and freed when the last of them is gone.  A renderer that
 * only reads what it is given (borrows_font_data) gets the shared
 * buffer; one that disposes of the bytes on its own schedule gets a
 * private copy and is unaffected, which is why coretext - whose
 * buffer is released by CoreGraphics rather than at font teardown -
 * needs no change.
 * ------------------------------------------------------------------ */

typedef struct font_file_ref
{
   struct font_file_ref *next;
   char                 *path;
   uint8_t              *data;
   size_t                len;
   unsigned              refs;
} font_file_ref_t;

/* handle -> buffer, so the teardown path can find what a renderer was
 * handed without the renderer or font_data_t having to carry it. */
typedef struct font_file_use
{
   struct font_file_use *next;
   const void           *handle;
   font_file_ref_t      *ref;      /* NULL when no file was involved */
   void                (*real_free)(void *data);
} font_file_use_t;

static font_file_ref_t *font_file_refs;
static font_file_use_t *font_file_uses;

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
/* retro_atomic.h's fallback backend has no compare-and-swap: it is
 * selected precisely on the targets whose toolchains offer no
 * lock-free integer atomics, which are the single-core consoles, and
 * there the list is only ever touched from one thread.  Gate on the
 * primitive rather than on a platform list, so a target that gains
 * atomics gains the locking with them. */
#if defined(HAVE_THREADS) && defined(retro_atomic_cas_int)
static retro_atomic_int_t font_file_lock_word;
#define FONT_FILE_LOCK() \
   do { while (!retro_atomic_cas_int(&font_file_lock_word, 0, 1)) { } } while (0)
#define FONT_FILE_UNLOCK() \
   retro_atomic_store_release_int(&font_file_lock_word, 0)
#else
#define FONT_FILE_LOCK()   do { } while (0)
#define FONT_FILE_UNLOCK() do { } while (0)
#endif

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
   void            *data = NULL;
   int64_t          len  = 0;

   FONT_FILE_LOCK();
   entry = font_file_ref_lookup(path);
   FONT_FILE_UNLOCK();

   if (entry)
      return entry;

   if (!filestream_read_file(path, &data, &len) || len <= 0)
   {
      if (data)
         free(data);
      return NULL;
   }

   if (!(entry = (font_file_ref_t*)calloc(1, sizeof(*entry))))
   {
      free(data);
      return NULL;
   }
   {
      /* strlcpy rather than strdup: the latter is not declared under
       * a strict C89 compile, and this file is built that way. */
      size_t path_len = strlen(path) + 1;
      if (!(entry->path = (char*)malloc(path_len)))
      {
         free(data);
         free(entry);
         return NULL;
      }
      strlcpy(entry->path, path, path_len);
   }

   entry->data    = (uint8_t*)data;
   entry->len     = (size_t)len;
   entry->refs    = 1;

   FONT_FILE_LOCK();
   if ((raced = font_file_ref_lookup(path)))
   {
      FONT_FILE_UNLOCK();
      free(entry->data);
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

   free(entry->data);
   free(entry->path);
   free(entry);
}

/* Caller holds the lock.  On failure the reference is dropped rather
 * than leaked: a font whose use cannot be recorded would keep the
 * buffer alive for the rest of the run. */
static void font_file_use_add(const void *handle, font_file_ref_t *entry,
      void (*real_free)(void *data))
{
   font_file_use_t *use;

   if (!handle || !(use = (font_file_use_t*)calloc(1, sizeof(*use))))
   {
      font_file_ref_release(entry);
      return;
   }

   use->handle    = handle;
   use->ref       = entry;
   use->real_free = real_free;
   use->next      = font_file_uses;
   font_file_uses = use;
}

/* The free() every caller of font_renderer_create_default() ends up
 * invoking.
 *
 * The reference cannot be dropped by whoever tears the font down: some
 * thirty video drivers hold the driver pointer and the handle and call
 * 'font_driver->free(font_data)' themselves, and asking every one of
 * them to call a new destructor instead would be thirty edits in files
 * that mostly cannot be built outside their own platform.  So the
 * release rides on the free itself.  The driver handed back by
 * font_renderer_create_default() is a copy of the backend's with this
 * in place of its free, which means the reference is dropped wherever
 * the font is destroyed, by callers that need to know nothing about
 * any of it.
 *
 * The backend's own teardown runs first and outside the lock: freetype
 * disposes of its face there, and the face holds a pointer into the
 * bytes this is about to release. */
static void font_renderer_shared_free_for(void *data,
      const font_renderer_driver_t *backend)
{
   font_file_use_t **link;
   font_file_use_t  *use  = NULL;

   if (!data)
      return;

   FONT_FILE_LOCK();
   for (link = &font_file_uses; *link; link = &(*link)->next)
      if ((*link)->handle == data)
      {
         use   = *link;
         *link = use->next;
         break;
      }
   FONT_FILE_UNLOCK();

   /* No record means the bookkeeping allocation failed at creation.
    * The font itself is still real and still owns an atlas and
    * whatever the backend hung off it, so tear it down regardless -
    * returning here would turn a failed calloc of a few bytes into a
    * leaked font, which is the worse of the two by a wide margin.
    * There is no reference to drop in that case, by construction. */
   if (!use)
   {
      if (backend && backend->free)
         backend->free(data);
      return;
   }

   if (use->real_free)
      use->real_free(data);

   FONT_FILE_LOCK();
   font_file_ref_release(use->ref);
   FONT_FILE_UNLOCK();

   free(use);
}

/* One wrapper per backend, built on first use.  Static, so the pointer
 * handed to callers stays valid for the life of the process exactly as
 * the backend's own struct did. */
#define FONT_SHARED_DRV_MAX 4
static font_renderer_driver_t  font_shared_drv[FONT_SHARED_DRV_MAX];
static const font_renderer_driver_t *font_shared_src[FONT_SHARED_DRV_MAX];
static unsigned                font_shared_drv_count;

/* One trampoline per slot, so the wrapper knows which backend it
 * stands for without the record it may not have. */
static void font_renderer_shared_free_0(void *d)
{ font_renderer_shared_free_for(d, font_shared_src[0]); }
static void font_renderer_shared_free_1(void *d)
{ font_renderer_shared_free_for(d, font_shared_src[1]); }
static void font_renderer_shared_free_2(void *d)
{ font_renderer_shared_free_for(d, font_shared_src[2]); }
static void font_renderer_shared_free_3(void *d)
{ font_renderer_shared_free_for(d, font_shared_src[3]); }

static void (* const font_shared_free_fn[FONT_SHARED_DRV_MAX])(void *) =
{
   font_renderer_shared_free_0, font_renderer_shared_free_1,
   font_renderer_shared_free_2, font_renderer_shared_free_3
};

static const font_renderer_driver_t *font_renderer_shared_driver(
      const font_renderer_driver_t *backend)
{
   unsigned i;

   for (i = 0; i < font_shared_drv_count; i++)
      if (font_shared_src[i] == backend)
         return &font_shared_drv[i];

   if (font_shared_drv_count >= FONT_SHARED_DRV_MAX)
      return backend;

   i                    = font_shared_drv_count++;
   font_shared_src[i]   = backend;
   font_shared_drv[i]   = *backend;
   font_shared_drv[i].free = font_shared_free_fn[i];
   return &font_shared_drv[i];
}

/* Monotonic counter incremented whenever any font instance is
 * freed. Consumers that cache per-font derived data (e.g. the
 * smooth ticker glyph width cache in gfx_animation.c) key their
 * entries on this value: font_data_t pointers can be recycled by
 * the allocator across free/create cycles, so pointer equality
 * alone cannot prove a cached entry still describes a live font */
static uint32_t font_driver_generation = 0;

/* Every live font, so they can be rebuilt when the file behind them
 * should change. Singly linked through font_data_t::next. */
static font_data_t *font_live = NULL;

static void font_driver_release_renderer_state(
      const font_renderer_t *renderer, void *renderer_data,
      bool is_threaded);

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

unsigned font_driver_reload_fonts(void)
{
   font_data_t *font;
   unsigned     n = 0;

   for (font = font_live; font; font = font->next)
   {
      const font_renderer_t *renderer = font->renderer;
      void                  *fresh    = NULL;

      /* Only a font created from an explicit path can be re-resolved;
       * one the renderer chose for itself has nothing to re-read. */
      if (!font->path || !renderer || !renderer->init)
         continue;

      /* The backend's own init does the resolving and the reading, so
       * this is the same call that created the font in the first
       * place - just against whatever the path now points at. */
      {
         char resolved[PATH_MAX_LENGTH];
         const char *want = font_driver_resolve_path(font,
               resolved, sizeof(resolved));

         /* Most language changes do not change the file. Only Arabic
          * and Persian, Chinese, Korean and Thai have a face of their
          * own; everything else shares the menu font, so English to
          * French, or either to Japanese, resolves to what is already
          * loaded. Rebuilding then would throw away the atlas and the
          * GPU texture behind every font, read the same TTF back, and
          * bump the generation so every derived metric was recomputed
          * too - all to arrive where it started. */
         if (font->path && string_is_equal(want, font->path))
            continue;

         if (!(fresh = renderer->init(font->video_data, want,
                     font->size, font->is_threaded)))
            continue;   /* keep the old font rather than lose text */

         /* Remember what it is actually built from now. */
         if (want != font->path)
         {
            free(font->path);
            font->path = strdup(want);
         }
      }

      /* Swap in place: the font_data_t address does not change, so
       * every holder of the pointer stays valid. Only the renderer
       * state behind it is replaced. */
      font_driver_release_renderer_state(renderer, font->renderer_data,
            font->is_threaded);
      font->renderer_data = fresh;
      font_driver_cache_metrics(font);
      n++;
   }

   if (n)
      /* Derived data cached outside this file - ticker widths, menu
       * line heights - is now stale. */
      font_driver_generation++;

   return n;
}

uint32_t font_driver_get_generation(void)
{
   return font_driver_generation;
}

int font_renderer_create_default(
      const font_renderer_driver_t **drv,
      void **handle, const char *font_path, unsigned font_size,
      enum font_atlas_format fmt)
{
   static const font_renderer_driver_t *font_backends[] = {
#ifdef HAVE_FREETYPE
      &freetype_font_renderer,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
      &coretext_font_renderer,
#endif
      &stb_font_renderer,
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

      /* Ask the renderer where to look. It gets the requested path so
       * it can resolve against it - freetype hands it to fontconfig,
       * which answers with a system font when a fallback was asked
       * for - and returns NULL to accept the request as it stands.
       * Doing the lookup and the read here is what keeps file I/O out
       * of the renderers entirely. */
      {
         const char * const *cand = font_backends[i]->get_default_fonts
            ? font_backends[i]->get_default_fonts(font_path, &face)
            : NULL;

         for (; cand && *cand; cand++)
         {
            /* An empty entry means the renderer has an internal or
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
       * renderer that borrows gets the shared buffer and the
       * reference is held for as long as its handle lives; one that
       * takes ownership gets a copy and the reference is dropped
       * here, so its teardown is exactly as it was. */
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

      /* A renderer that takes ownership does so the moment init() is
       * called, not when it succeeds: stb stores the buffer, then
       * rejects a malformed font and frees it on the way out. Freeing
       * here as well was a double free on any unreadable or truncated
       * font file. A borrowing renderer frees nothing, so the
       * reference is dropped below on either outcome. */
      *handle = font_backends[i]->init(data, (size_t)len, face,
            font_size, fmt);
      if (*handle)
      {
         /* Recorded even when no file was involved - a built-in or
          * system font still has to reach the backend's own free
          * through the wrapper below. */
         FONT_FILE_LOCK();
         font_file_use_add(*handle, entry,
               font_backends[i]->free);
         FONT_FILE_UNLOCK();
         *drv = font_renderer_shared_driver(font_backends[i]);
         return 1;
      }

      if (entry)
      {
         FONT_FILE_LOCK();
         font_file_ref_release(entry);
         FONT_FILE_UNLOCK();
         entry = NULL;
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
         ? font_data : (void*)video_state_get_ptr()->osd_font);
   const font_renderer_t *renderer  = (font && msg && msg_len)
   ? font->renderer : NULL;

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
      renderer->render_msg(data,
            font->renderer_data, new_msg, new_msg_len, params);
   }
}

void font_driver_bind_block(void *font_data, void *block)
{
   font_data_t *font               = (font_data_t*)font_data;
   const font_renderer_t *renderer = font ? font->renderer : NULL;
   if (renderer && renderer->bind_block)
      renderer->bind_block(font->renderer_data, block);
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
      unsigned video_width,
      unsigned video_height,
      font_data_impl_t *font_data)
{
   const font_renderer_t *renderer = font_data->font ? font_data->font->renderer : NULL;

   /* A rebuilt font has different metrics; pick them up before
    * anything is drawn with the old ones. */
   font_driver_sync_impl(font_data);

   if (font_data->raster_block.carr.coords.vertices == 0)
      return;
   if (renderer && renderer->flush)
      renderer->flush(video_width, video_height, font_data->font->renderer_data);
   font_data->raster_block.carr.coords.vertices = 0;
}

int font_driver_get_message_width(void *font_data,
      const char *msg, size_t len, float scale)
{
   font_data_t *font               = (font_data_t*)(font_data
         ? font_data : (void*)video_state_get_ptr()->osd_font);
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
 *  - Vulkan: vkQueueWaitIdle under queue_lock only drains
 *    submitted work, not command buffers being recorded.
 *
 * Dispatch renderer->free to the video thread via
 * video_thread_texture_handle so it runs serialised with
 * the video thread's frame rendering.  This is the same
 * pattern used by texture load/unload.
 *
 * video_thread_texture_handle is self-safe: if the wrapper
 * is not active (VIDEO_FLAG_THREAD_WRAPPER_ACTIVE not set),
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
   if (is_threaded)
   {
      font_free_cmd_t cmd;
      cmd.renderer      = renderer;
      cmd.renderer_data = renderer_data;
      cmd.is_threaded   = is_threaded;
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
      font_driver_generation++;

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

font_data_t *font_driver_init_first(
      void *video_data, const char *font_path, float font_size,
      bool threading_hint, bool is_threaded,
      const font_renderer_t *backend)
{
   const void *font_driver = NULL;
   void *font_handle       = NULL;
   bool ok                 = false;
#ifdef HAVE_THREADS
   if (     threading_hint
         && is_threaded
         && !video_driver_is_hw_context())
      ok = video_thread_font_init(&font_driver, &font_handle,
            video_data, font_path, font_size, backend, font_init_first,
            is_threaded);
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
   video_driver_state_t *video_st = video_state_get_ptr();

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
   video_driver_state_t *video_st = video_state_get_ptr();

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

void font_driver_free_osd_for(void *video_data)
{
   /* Only the owner may free it. Teardown of an instance that no
    * longer owns the font - a stale or deferred free - must leave the
    * live one alone. */
   video_driver_state_t *video_st = video_state_get_ptr();

   if (video_st->osd_font && video_st->osd_font_owner == video_data)
      font_driver_free_osd();
}
