/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019-2026 - The RetroArch team
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

#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <formats/image.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>

#include "cheevos.h"

#include "../gfx/gfx_display.h"
#include "../gfx/video_driver.h"
#include "../gfx/gfx_surface.h"
#include "../tasks/tasks_internal.h"
#include "../file_path_special.h"

void rcheevos_get_local_badge_filename(char badge_file[], size_t badge_file_size, const char* badge, bool locked)
{
   size_t _len = strlcpy(badge_file, badge, badge_file_size);
   if (locked)
      _len += strlcpy_lit(badge_file + _len, "_lock", badge_file_size - _len);
   strlcpy(badge_file + _len, FILE_PATH_PNG_EXTENSION, badge_file_size - _len);
}

/* --- Badge textures ---------------------------------------------------
 * Nothing here reads a file, decodes or uploads on the thread that
 * asks. rcheevos_get_badge_texture() only looks a badge up in a small
 * table: a handle that is ready is handed over, anything else answers
 * 0 and the caller asks again on a later frame, which every caller
 * already does for a badge that is still downloading. A badge that is
 * not in the table is written into it as a request, and that is all an
 * asking thread ever does - so any thread may ask. Under the threaded
 * video wrapper the achievement widgets are iterated and drawn on the
 * video thread and ask from there; the menu asks from the main thread.
 *
 * The main thread does the work (rcheevos_badge_cache_service(), at
 * once when the asker is the main thread, else from the runloop): the
 * file check, the download request for a missing file, and the image
 * task (decode on a worker) whose completion posts the upload
 * (asynchronous under threaded video). Nothing waits on anything.
 *
 * Handles keep the old ownership: each one handed out belongs to the
 * caller, who unloads it. The server default badge, which many menu
 * entries show at once while their own is on its way, can also be
 * borrowed (rcheevos_get_default_badge_texture()): that one has a slot
 * of its own, is loaded once, and its handle stays the cache's.
 *
 * The table is small and round-robin. A slot evicted or reset while
 * its load is in flight has its sequence bumped, so the late delivery
 * is unloaded instead of landing in whatever reuses the slot. A failed
 * load stays failed until the slot is reused or the file is downloaded
 * again, so a bad file is not decoded once a frame.
 *
 * The lock word guards the table only. No file, task, driver or
 * allocator call is made holding it. */
#define RCHEEVOS_BADGE_SLOTS        16
#define RCHEEVOS_BADGE_DEFAULT_SLOT RCHEEVOS_BADGE_SLOTS
#define RCHEEVOS_BADGE_DEFAULT_NAME "00000"
#define RCHEEVOS_BADGE_KEY_LEN      16

enum rcheevos_badge_slot_state
{
   RCHEEVOS_BADGE_SLOT_EMPTY = 0,  /* slot not in use */
   RCHEEVOS_BADGE_SLOT_REQUESTED,  /* asked for; the main thread has not looked yet */
   RCHEEVOS_BADGE_SLOT_FETCHING,   /* badge being downloaded */
   RCHEEVOS_BADGE_SLOT_LOADING,    /* badge being decoded and uploaded */
   RCHEEVOS_BADGE_SLOT_READY,      /* handle waiting to be taken */
   RCHEEVOS_BADGE_SLOT_FAILED      /* missing, or download or load failed */
};

typedef struct
{
   char key[RCHEEVOS_BADGE_KEY_LEN]; /* badge name: "NNNNN" */
   uintptr_t handle;                 /* texture handle */
   uint32_t seq;                     /* matched against the load tag, so a load
                                      * outlived by its slot is not delivered */
   uint8_t state;                    /* enum rcheevos_badge_slot_state */
   uint8_t locked;                   /* non-zero for the _lock version of the badge */
   uint8_t download;                 /* REQUESTED: fetch the file if it is missing */
} rcheevos_badge_slot_t;

typedef struct
{
   unsigned slot;
   uint32_t seq;
} rcheevos_badge_load_tag_t;

static rcheevos_badge_slot_t rcheevos_badge_slots[RCHEEVOS_BADGE_SLOTS + 1];
static unsigned              rcheevos_badge_slots_next;

/* Same gate as the font file list (gfx/font_driver.c): a target with
 * no lock-free compare-and-swap must not spin, and has no second
 * thread asking for badges either. */
#if defined(HAVE_THREADS) && defined(retro_atomic_cas_int) \
 && defined(RETRO_ATOMIC_LOCK_FREE)
static retro_atomic_int_t rcheevos_badge_lock_word;
static retro_atomic_int_t rcheevos_badge_requests;
#define RCHEEVOS_BADGE_LOCK() \
   do { while (!retro_atomic_cas_int(&rcheevos_badge_lock_word, 0, 1)) { } } while (0)
#define RCHEEVOS_BADGE_UNLOCK() \
   retro_atomic_store_release_int(&rcheevos_badge_lock_word, 0)
#define RCHEEVOS_BADGE_REQUESTS_SET(v) \
   retro_atomic_store_release_int(&rcheevos_badge_requests, (v))
#define RCHEEVOS_BADGE_REQUESTS_GET() \
   retro_atomic_load_acquire_int(&rcheevos_badge_requests)
#else
static int rcheevos_badge_requests;
#define RCHEEVOS_BADGE_LOCK()            do { } while (0)
#define RCHEEVOS_BADGE_UNLOCK()          do { } while (0)
#define RCHEEVOS_BADGE_REQUESTS_SET(v)   (rcheevos_badge_requests = (v))
#define RCHEEVOS_BADGE_REQUESTS_GET()    (rcheevos_badge_requests)
#endif

static void rcheevos_badge_image_release(void* img)
{
   struct texture_image *ti = (struct texture_image*)img;
   if (ti)
   {
      image_texture_free(ti);
      free(ti);
   }
}

/* Caller holds the lock. Returns the handle the slot was holding, for
 * the caller to unload once the lock is dropped. */
static uintptr_t rcheevos_badge_slot_clear(rcheevos_badge_slot_t* slot)
{
   uintptr_t handle = (slot->state == RCHEEVOS_BADGE_SLOT_READY)
      ? slot->handle : 0;

   slot->handle   = 0;
   slot->state    = RCHEEVOS_BADGE_SLOT_EMPTY;
   slot->download = 0;
   slot->key[0]   = '\0';
   slot->seq++; /* orphans any load still in flight */
   return handle;
}

/* A load is over: @handle, or 0 if it failed. If the slot no longer
 * waits for this load the handle is nobody's, and is unloaded. */
static void rcheevos_badge_load_done(void *user, uintptr_t handle)
{
   rcheevos_badge_load_tag_t *tag = (rcheevos_badge_load_tag_t*)user;
   rcheevos_badge_slot_t *slot;
   if (!tag)
      return;

   slot = &rcheevos_badge_slots[tag->slot];

   RCHEEVOS_BADGE_LOCK();
   if (slot->seq == tag->seq && slot->state == RCHEEVOS_BADGE_SLOT_LOADING)
   {
      slot->handle = handle;
      slot->state  = handle ? RCHEEVOS_BADGE_SLOT_READY : RCHEEVOS_BADGE_SLOT_FAILED;
      handle       = 0;
   }
   RCHEEVOS_BADGE_UNLOCK();

   if (handle)
      video_driver_texture_unload(&handle);

   free(tag);
}

/* Main thread: the decode finished; hand the image to the uploader. */
static void rcheevos_badge_decode_done(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct texture_image      *img = (struct texture_image*)task_data;
   rcheevos_badge_load_tag_t *tag = (rcheevos_badge_load_tag_t*)user_data;
   (void)task; (void)error;

   if (!tag)
   {
      rcheevos_badge_image_release(img);
      return;
   }

   if (!img || img->width < 1 || img->height < 1 || !img->pixels)
   {
      rcheevos_badge_load_done(tag, 0);   /* frees tag */
      rcheevos_badge_image_release(img);
      return;
   }

   if (!video_driver_texture_load_async(img,
         gfx_display_texture_filter_latched(),
            rcheevos_badge_load_done, tag, rcheevos_badge_image_release))
   {
      rcheevos_badge_image_release(img);
      rcheevos_badge_load_done(tag, 0);
   }
}

/* Caller holds the lock. */
static rcheevos_badge_slot_t *rcheevos_badge_slot_find(const char *key, bool locked)
{
   rcheevos_badge_slot_t* slot       = rcheevos_badge_slots;
   const rcheevos_badge_slot_t* stop = slot + RCHEEVOS_BADGE_SLOTS;

   for (; slot < stop; ++slot)
   {
      if (     slot->state != RCHEEVOS_BADGE_SLOT_EMPTY
            && slot->locked == (uint8_t)locked
            && string_is_equal(slot->key, key))
         return slot;
   }

   return NULL;
}

/* Caller holds the lock. The first empty slot from the cursor on; with
 * none empty a failed one, else the one at the cursor, whose handle (if
 * it held one) comes back in @evicted. Returned EMPTY, with its key. */
static rcheevos_badge_slot_t* rcheevos_badge_slot_alloc(const char* key,
      bool locked, uintptr_t *evicted)
{
   unsigned i;
   rcheevos_badge_slot_t* slot = NULL;

   for (i = 0; i < RCHEEVOS_BADGE_SLOTS; i++)
   {
      unsigned idx = (rcheevos_badge_slots_next + i) % RCHEEVOS_BADGE_SLOTS;
      if (rcheevos_badge_slots[idx].state == RCHEEVOS_BADGE_SLOT_EMPTY)
      {
         slot = &rcheevos_badge_slots[idx];
         break;
      }
   }

   /* Full: a failed load is the cheapest thing to forget */
   for (i = 0; !slot && i < RCHEEVOS_BADGE_SLOTS; i++)
   {
      unsigned idx = (rcheevos_badge_slots_next + i) % RCHEEVOS_BADGE_SLOTS;
      if (rcheevos_badge_slots[idx].state == RCHEEVOS_BADGE_SLOT_FAILED)
         slot = &rcheevos_badge_slots[idx];
   }

   if (!slot)
      slot = &rcheevos_badge_slots[rcheevos_badge_slots_next];

   if (slot->state != RCHEEVOS_BADGE_SLOT_EMPTY)
      *evicted = rcheevos_badge_slot_clear(slot);

   rcheevos_badge_slots_next =
      ((unsigned)(slot - rcheevos_badge_slots) + 1) % RCHEEVOS_BADGE_SLOTS;

   strlcpy(slot->key, key, sizeof(slot->key));
   slot->locked = locked ? 1 : 0;
   return slot;
}

/* Drop every cached or in-flight badge: the menu list is being
 * rebuilt, or the game unloaded. Ready handles are unloaded, loads in
 * flight orphaned. */
void rcheevos_badge_cache_reset(void)
{
   unsigned i;
   unsigned count = 0;
   uintptr_t handles[RCHEEVOS_BADGE_SLOTS + 1];

   RCHEEVOS_BADGE_LOCK();
   for (i = 0; i < RCHEEVOS_BADGE_SLOTS + 1; i++)
   {
      uintptr_t handle = rcheevos_badge_slot_clear(&rcheevos_badge_slots[i]);
      if (handle)
         handles[count++] = handle;
   }
   RCHEEVOS_BADGE_UNLOCK();

   for (i = 0; i < count; i++)
      video_driver_texture_unload(&handles[i]);
}

/* Move slot @idx from LOADING to @state, unless it was reused since. */
static void rcheevos_badge_slot_settle(unsigned idx, uint32_t seq, uint8_t state)
{
   rcheevos_badge_slot_t* slot = &rcheevos_badge_slots[idx];

   RCHEEVOS_BADGE_LOCK();
   if (slot->seq == seq && slot->state == RCHEEVOS_BADGE_SLOT_LOADING)
      slot->state = state;
   RCHEEVOS_BADGE_UNLOCK();
}

/* Main thread: start the load of one requested badge. The slot is
 * LOADING by now; every exit leaves it LOADING with a task on the way,
 * FETCHING with a download on the way, or FAILED. */
static void rcheevos_badge_start_load(unsigned idx, uint32_t seq,
      const char* badge, bool locked, bool download_if_missing)
{
   gfx_surface_requirements_t req;
   char badge_file[RCHEEVOS_BADGE_KEY_LEN + 16];
   char fullpath[PATH_MAX_LENGTH];
   rcheevos_badge_load_tag_t* tag;

   fill_pathname_application_special(fullpath, sizeof(fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);
   rcheevos_get_local_badge_filename(badge_file, sizeof(badge_file), badge, locked);
   fill_pathname_join(fullpath, fullpath, badge_file, sizeof(fullpath));

   if (!path_is_valid(fullpath))
   {
      if (download_if_missing)
      {
         /* FETCHING before the request: the download's completion
          * looks for a slot in that state. */
         rcheevos_badge_slot_settle(idx, seq, RCHEEVOS_BADGE_SLOT_FETCHING);
         rcheevos_badge_request_download(badge, locked);
      }
      else
         rcheevos_badge_slot_settle(idx, seq, RCHEEVOS_BADGE_SLOT_FAILED);
      return;
   }

   if (!(tag = (rcheevos_badge_load_tag_t*)malloc(sizeof(*tag))))
   {
      rcheevos_badge_slot_settle(idx, seq, RCHEEVOS_BADGE_SLOT_FAILED);
      return;
   }

   tag->slot = idx;
   tag->seq  = seq;

   gfx_surface_query_requirements(0, &req);
   if (!task_push_image_load(fullpath, req.rgba,
         0, 0, rcheevos_badge_decode_done, tag))
   {
      free(tag);
      rcheevos_badge_slot_settle(idx, seq, RCHEEVOS_BADGE_SLOT_FAILED);
   }
}

/* Main thread: start the load of every badge asked for since the last
 * call. With nothing asked for this is one atomic load. */
void rcheevos_badge_cache_service(void)
{
   if (!RCHEEVOS_BADGE_REQUESTS_GET() || !task_is_on_main_thread())
      return;

   RCHEEVOS_BADGE_REQUESTS_SET(0);

   for (;;)
   {
      unsigned i;
      uint32_t seq  = 0;
      bool locked   = false;
      bool download = false;
      char key[RCHEEVOS_BADGE_KEY_LEN];

      RCHEEVOS_BADGE_LOCK();
      for (i = 0; i < RCHEEVOS_BADGE_SLOTS + 1; i++)
      {
         rcheevos_badge_slot_t* slot = &rcheevos_badge_slots[i];
         if (slot->state != RCHEEVOS_BADGE_SLOT_REQUESTED)
            continue;
         slot->state = RCHEEVOS_BADGE_SLOT_LOADING;
         seq         = slot->seq;
         locked      = slot->locked != 0;
         download    = slot->download != 0;
         strlcpy(key, slot->key, sizeof(key));
         break;
      }
      RCHEEVOS_BADGE_UNLOCK();

      if (i == RCHEEVOS_BADGE_SLOTS + 1)
         break;

      rcheevos_badge_start_load(i, seq, key, locked, download);
   }
}

/* @shared: the default badge's own slot, whose handle is lent rather
 * than given. */
static uintptr_t rcheevos_badge_ask(const char* badge, bool locked,
      bool download_if_missing, bool shared, bool *pending)
{
   rcheevos_badge_slot_t *slot;
   uintptr_t tex     = 0;
   uintptr_t evicted = 0;
   bool requested    = false;

   if (pending)
      *pending = false;

   if (!badge || !badge[0] || strlen(badge) >= RCHEEVOS_BADGE_KEY_LEN)
      return 0;

   RCHEEVOS_BADGE_LOCK();
   if (shared)
   {
      /* Never evicted */
      slot = &rcheevos_badge_slots[RCHEEVOS_BADGE_DEFAULT_SLOT];
      if (slot->state == RCHEEVOS_BADGE_SLOT_EMPTY)
      {
         strlcpy(slot->key, badge, sizeof(slot->key));
         slot->locked = locked ? 1 : 0;
      }
   }
   else if (!(slot = rcheevos_badge_slot_find(badge, locked)))
      slot = rcheevos_badge_slot_alloc(badge, locked, &evicted);

   switch (slot->state)
   {
      case RCHEEVOS_BADGE_SLOT_READY:
         tex = slot->handle;
         if (!shared)
         {
            /* the caller's from here on; the slot is free again */
            slot->handle = 0;
            slot->state  = RCHEEVOS_BADGE_SLOT_EMPTY;
            slot->key[0] = '\0';
         }
         break;

      case RCHEEVOS_BADGE_SLOT_EMPTY:
         slot->state    = RCHEEVOS_BADGE_SLOT_REQUESTED;
         slot->download = download_if_missing ? 1 : 0;
         requested      = true;
         break;

      case RCHEEVOS_BADGE_SLOT_REQUESTED:
         if (download_if_missing)
            slot->download = 1;
         break;

      case RCHEEVOS_BADGE_SLOT_FAILED:
         /* Failed for an asker that did not want a download; this
          * one does, once */
         if (download_if_missing && !slot->download)
         {
            slot->state    = RCHEEVOS_BADGE_SLOT_REQUESTED;
            slot->download = 1;
            requested      = true;
         }
         break;

      default:
         /* On its way */
         break;
   }
   /* A local load is a few frames; a download or a failure is not
    * something to wait for */
   if (pending)
      *pending = (   slot->state == RCHEEVOS_BADGE_SLOT_REQUESTED
                  || slot->state == RCHEEVOS_BADGE_SLOT_LOADING);
   RCHEEVOS_BADGE_UNLOCK();

   if (evicted)
      video_driver_texture_unload(&evicted);

   if (requested)
   {
      RCHEEVOS_BADGE_REQUESTS_SET(1);
      /* Off the main thread the runloop picks the request up */
      rcheevos_badge_cache_service();

      /* On it, the file check has just been made: say what it found */
      if (pending)
      {
         RCHEEVOS_BADGE_LOCK();
         slot = shared
            ? &rcheevos_badge_slots[RCHEEVOS_BADGE_DEFAULT_SLOT]
            : rcheevos_badge_slot_find(badge, locked);
         *pending = slot
            && (   slot->state == RCHEEVOS_BADGE_SLOT_REQUESTED
                || slot->state == RCHEEVOS_BADGE_SLOT_LOADING);
         RCHEEVOS_BADGE_UNLOCK();
      }
   }

   return tex;
}

uintptr_t rcheevos_get_badge_texture(const char* badge, bool locked, bool download_if_missing)
{
   return rcheevos_badge_ask(badge, locked, download_if_missing, false, NULL);
}

uintptr_t rcheevos_get_badge_texture_ex(const char* badge, bool locked,
      bool download_if_missing, bool *pending)
{
   return rcheevos_badge_ask(badge, locked, download_if_missing, false, pending);
}

uintptr_t rcheevos_get_default_badge_texture(void)
{
   return rcheevos_badge_ask(RCHEEVOS_BADGE_DEFAULT_NAME, false, false, true, NULL);
}

/* Main thread: @badge_name ("NNNNN" or "NNNNN_lock") has just been
 * written to disk. If the cache was waiting for it, load it. */
void rcheevos_update_badge_references(const char* badge_name)
{
   rcheevos_badge_slot_t* slot;
   char unlocked_badge_name[RCHEEVOS_BADGE_KEY_LEN];
   bool locked                 = false;
   bool requested              = false;
   const size_t badge_name_len = badge_name ? strlen(badge_name) : 0;

   if (!badge_name_len)
      return;

   if (     badge_name_len > 5
         && badge_name_len < sizeof(unlocked_badge_name) + 5
         && string_is_equal(&badge_name[badge_name_len - 5], "_lock"))
   {
      memcpy(unlocked_badge_name, badge_name, badge_name_len - 5);
      unlocked_badge_name[badge_name_len - 5] = '\0';
      badge_name = unlocked_badge_name;
      locked     = true;
   }

   RCHEEVOS_BADGE_LOCK();
   slot = rcheevos_badge_slot_find(badge_name, locked);
   if (slot && (   slot->state == RCHEEVOS_BADGE_SLOT_FETCHING
                || slot->state == RCHEEVOS_BADGE_SLOT_FAILED))
   {
      slot->state    = RCHEEVOS_BADGE_SLOT_REQUESTED;
      slot->download = 0;
      requested      = true;
   }
   RCHEEVOS_BADGE_UNLOCK();

   if (requested)
   {
      RCHEEVOS_BADGE_REQUESTS_SET(1);
      rcheevos_badge_cache_service();
   }
}

bool rcheevos_is_badge_available(const char* badge, bool locked)
{
   char badge_file[24];
   char fullpath[PATH_MAX_LENGTH];

   rcheevos_get_local_badge_filename(badge_file, sizeof(badge_file), badge, locked);

   fill_pathname_application_special(fullpath, sizeof(fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);
   fill_pathname_join(fullpath, fullpath, badge_file, sizeof(fullpath));

   return path_is_valid(fullpath);
}
