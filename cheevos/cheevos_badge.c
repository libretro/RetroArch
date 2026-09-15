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

#include "cheevos.h"

#include "../gfx/gfx_display.h"
#include "../gfx/video_driver.h"
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
 * A badge used to be read, decoded and uploaded inline in
 * rcheevos_get_badge_texture(): a file read plus a PNG decode on the
 * thread that called, and under threaded video a present's worth of
 * waiting for the upload, all at the moment an achievement popped
 * during gameplay. Every caller already copes with 0 - the menu
 * re-asks after MENU_BADGE_RETRY_RELOAD_FRAMES frames, the widgets
 * every 250 ms, both written for badges still downloading - so the
 * load now happens off this thread: the first call for a badge posts
 * an image task (decode on a worker) whose completion posts the
 * upload (asynchronous under threaded video), and the call that finds
 * the handle ready takes it. Handles keep the old ownership: each one
 * handed out belongs to the caller, who unloads it.
 *
 * The table is small and round-robin; a slot evicted while loading
 * has its sequence bumped so the late delivery is discarded, and a
 * ready handle evicted unused is unloaded. Main thread only, as the
 * callers already are when they can accept a handle. */
#define BADGE_SLOTS 16

enum badge_slot_state
{
   BADGE_SLOT_EMPTY = 0,
   BADGE_SLOT_LOADING,
   BADGE_SLOT_READY,
   BADGE_SLOT_FAILED
};

typedef struct
{
   char key[32];          /* badge file name: "NNNNN[_lock].png" */
   uintptr_t handle;
   uint32_t seq;
   uint8_t state;
} badge_slot_t;

typedef struct
{
   unsigned slot;
   uint32_t seq;
} badge_load_tag_t;

static badge_slot_t badge_slots[BADGE_SLOTS];
static unsigned     badge_slot_next;

static void badge_image_release(void *img)
{
   struct texture_image *ti = (struct texture_image*)img;
   if (ti)
   {
      image_texture_free(ti);
      free(ti);
   }
}

/* Main thread: the upload finished (handle) or failed (0). */
static void badge_upload_done(void *user, uintptr_t handle)
{
   badge_load_tag_t *tag = (badge_load_tag_t*)user;
   badge_slot_t *slot;
   if (!tag)
      return;
   slot = &badge_slots[tag->slot];
   if (slot->seq == tag->seq && slot->state == BADGE_SLOT_LOADING)
   {
      slot->handle = handle;
      slot->state  = handle ? BADGE_SLOT_READY : BADGE_SLOT_FAILED;
   }
   else if (handle)
      video_driver_texture_unload(&handle); /* evicted while in flight */
   free(tag);
}

/* Main thread: the decode finished; hand the image to the uploader. */
static void badge_decode_done(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct texture_image *img = (struct texture_image*)task_data;
   badge_load_tag_t     *tag = (badge_load_tag_t*)user_data;
   (void)task; (void)error;

   if (!tag)
   {
      badge_image_release(img);
      return;
   }
   if (!img || img->width < 1 || img->height < 1 || !img->pixels)
   {
      badge_upload_done(tag, 0);   /* frees tag */
      badge_image_release(img);
      return;
   }
   if (!video_driver_texture_load_async(img, gfx_display_texture_filter(),
            badge_upload_done, tag, badge_image_release))
   {
      badge_image_release(img);
      badge_upload_done(tag, 0);
   }
}

static badge_slot_t *badge_slot_find(const char *key)
{
   unsigned i;
   for (i = 0; i < BADGE_SLOTS; i++)
      if (badge_slots[i].state != BADGE_SLOT_EMPTY
            && string_is_equal(badge_slots[i].key, key))
         return &badge_slots[i];
   return NULL;
}

static void badge_slot_clear(badge_slot_t *slot)
{
   if (slot->state == BADGE_SLOT_READY && slot->handle)
      video_driver_texture_unload(&slot->handle);
   slot->handle = 0;
   slot->state  = BADGE_SLOT_EMPTY;
   slot->key[0] = '\0';
   slot->seq++;             /* orphans any load still in flight */
}

/* Drop every cached or in-flight badge: video context gone, or the
 * game unloaded. Ready handles are unloaded, loading ones orphaned. */
void rcheevos_badge_cache_reset(void)
{
   unsigned i;
   for (i = 0; i < BADGE_SLOTS; i++)
      badge_slot_clear(&badge_slots[i]);
}

uintptr_t rcheevos_get_badge_texture(const char* badge, bool locked, bool download_if_missing)
{
   char badge_file[24];
   char fullpath[PATH_MAX_LENGTH];
   badge_slot_t *slot;
   badge_load_tag_t *tag;
   uintptr_t tex;

   if (!badge || !badge[0])
      return 0;

   /* The slot table and the task callbacks are main-thread only. A
    * caller on another thread gets 0 and asks again from its draw
    * path, exactly as it does for a badge still downloading. */
   if (!task_is_on_main_thread())
      return 0;

   rcheevos_get_local_badge_filename(badge_file, sizeof(badge_file), badge, locked);

   if ((slot = badge_slot_find(badge_file)))
   {
      switch (slot->state)
      {
         case BADGE_SLOT_READY:
            tex          = slot->handle;
            slot->handle = 0;
            slot->state  = BADGE_SLOT_EMPTY;
            slot->key[0] = '\0';
            return tex;    /* the caller's now */
         case BADGE_SLOT_LOADING:
            return 0;
         default:          /* FAILED: fall through to the file check */
            badge_slot_clear(slot);
            break;
      }
   }

   fill_pathname_application_special(fullpath, sizeof(fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);
   fill_pathname_join(fullpath, fullpath, badge_file, sizeof(fullpath));

   if (!path_is_valid(fullpath))
   {
      if (download_if_missing)
         rcheevos_badge_request_download(badge, locked);
      return 0;
   }

   if (!(tag = (badge_load_tag_t*)malloc(sizeof(*tag))))
      return 0;

   slot            = &badge_slots[badge_slot_next];
   badge_slot_next = (badge_slot_next + 1) % BADGE_SLOTS;
   badge_slot_clear(slot);
   strlcpy(slot->key, badge_file, sizeof(slot->key));
   slot->state = BADGE_SLOT_LOADING;
   tag->slot   = (unsigned)(slot - badge_slots);
   tag->seq    = slot->seq;

   if (!task_push_image_load(fullpath,
            (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0,
            0, 0, badge_decode_done, tag))
   {
      free(tag);
      badge_slot_clear(slot);
      return 0;
   }
   return 0;   /* ready on a later call */
}
