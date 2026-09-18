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
#define RCHEEVOS_BADGE_SLOTS 16

enum rcheevos_badge_slot_state
{
   RCHEEVOS_BADGE_SLOT_EMPTY = 0,  /* slot not in use */
   RCHEEVOS_BADGE_SLOT_FETCHING,   /* badge being downloaded */
   RCHEEVOS_BADGE_SLOT_LOADING,    /* badge being loaded into memory */
   RCHEEVOS_BADGE_SLOT_READY,      /* badge is ready */
   RCHEEVOS_BADGE_SLOT_FAILED      /* download or load into memory failed */
};

#define RCHEEVOS_BADGE_KEY_LEN 16

typedef struct
{
   char key[RCHEEVOS_BADGE_KEY_LEN]; /* badge name: "NNNNN" */
   uintptr_t handle;                 /* texture handle */
   uint32_t seq;                     /* secondary key to match against load_tag in case in-flight request gets overwritten */
   uint8_t state;                    /* state of badge request */
   uint8_t locked;                   /* non-zero if associated to _lock version of badge */
} rcheevos_badge_slot_t;

typedef struct
{
   unsigned slot;
   uint32_t seq;
} rcheevos_badge_load_tag_t;

static rcheevos_badge_slot_t rcheevos_badge_slots[RCHEEVOS_BADGE_SLOTS + 1];
static unsigned              rcheevos_badge_slots_next = RCHEEVOS_BADGE_SLOTS;

static void rcheevos_badge_image_release(void* img)
{
   struct texture_image *ti = (struct texture_image*)img;
   if (ti)
   {
      image_texture_free(ti);
      free(ti);
   }
}

/* Main thread: the load finished (handle) or failed (0). */
static void rcheevos_badge_load_done(void *user, uintptr_t handle)
{
   rcheevos_badge_load_tag_t *tag = (rcheevos_badge_load_tag_t*)user;
   rcheevos_badge_slot_t *slot;
   if (!tag)
      return;

   slot = &rcheevos_badge_slots[tag->slot];
   if (slot->seq == tag->seq && slot->state == RCHEEVOS_BADGE_SLOT_LOADING)
   {
      slot->handle = handle;
      slot->state  = handle ? RCHEEVOS_BADGE_SLOT_READY : RCHEEVOS_BADGE_SLOT_FAILED;
   }
   else if (handle)
   {
      /* evicted while in flight */
      video_driver_texture_unload(&handle);
   }

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

static void rcheevos_badge_slot_clear(rcheevos_badge_slot_t* slot)
{
   if (slot->state == RCHEEVOS_BADGE_SLOT_READY && slot->handle)
      video_driver_texture_unload(&slot->handle);

   slot->handle = 0;
   slot->state = RCHEEVOS_BADGE_SLOT_EMPTY;
   slot->key[0] = '\0';
   slot->seq++; /* orphans any load still in flight */
}

static rcheevos_badge_slot_t* rcheevos_badge_slot_alloc(const char* key, bool locked)
{
   rcheevos_badge_slot_t* slot;

   if (rcheevos_badge_slots_next >= RCHEEVOS_BADGE_SLOTS)
   {
      memset(&rcheevos_badge_slots, 0, sizeof(rcheevos_badge_slots));
      rcheevos_badge_slots_next = 0;
   }

   slot = &rcheevos_badge_slots[rcheevos_badge_slots_next];
   if (slot->state != RCHEEVOS_BADGE_SLOT_EMPTY)
   {
      unsigned rcheevos_badge_slots_override = rcheevos_badge_slots_next;
      do {
         rcheevos_badge_slots_next = (rcheevos_badge_slots_next + 1) % RCHEEVOS_BADGE_SLOTS;
         slot = &rcheevos_badge_slots[rcheevos_badge_slots_next];

         if (rcheevos_badge_slots_next == rcheevos_badge_slots_override) {
            /* all slots full, just claim the one marked as next */
            rcheevos_badge_slot_clear(slot);
            break;
         }
      } while (slot->state != RCHEEVOS_BADGE_SLOT_EMPTY);
   }
   rcheevos_badge_slots_next = (rcheevos_badge_slots_next + 1) % RCHEEVOS_BADGE_SLOTS;

   strlcpy(slot->key, key, sizeof(slot->key));
   slot->locked = locked;
   slot->state = RCHEEVOS_BADGE_SLOT_LOADING;
   return slot;
}

static rcheevos_badge_slot_t *rcheevos_badge_slot_find(const char *key, bool locked)
{
   if (rcheevos_badge_slots_next < RCHEEVOS_BADGE_SLOTS)
   {
      const size_t key_len = strlen(key) + 1;
      rcheevos_badge_slot_t* slot = rcheevos_badge_slots;
      const rcheevos_badge_slot_t* stop = slot + RCHEEVOS_BADGE_SLOTS;

      for (; slot < stop; ++slot)
      {
         if (slot->state != RCHEEVOS_BADGE_SLOT_EMPTY && slot->locked == locked
               && memcmp(slot->key, key, key_len) == 0)
            return slot;
      }
   }

   return NULL;
}

/* Drop every cached or in-flight badge: video context gone, or the
 * game unloaded. Ready handles are unloaded, loading ones orphaned. */
void rcheevos_badge_cache_reset(void)
{
   if (rcheevos_badge_slots_next < RCHEEVOS_BADGE_SLOTS)
   {
      rcheevos_badge_slot_t* slot = rcheevos_badge_slots;
      const rcheevos_badge_slot_t* stop = slot + RCHEEVOS_BADGE_SLOTS + 1;
      for (; slot < stop; ++slot)
      {
         if (slot->state == RCHEEVOS_BADGE_SLOT_READY && slot->handle)
            video_driver_texture_unload(&slot->handle);
      }
   }

   memset(rcheevos_badge_slots, 0, sizeof(rcheevos_badge_slots));
}

static int rcheevos_load_badge_texture(rcheevos_badge_slot_t* slot, const char* badge, bool locked, bool download_if_missing)
{
   gfx_surface_requirements_t req;
   char badge_file[RCHEEVOS_BADGE_KEY_LEN];
   char fullpath[PATH_MAX_LENGTH];
   rcheevos_badge_load_tag_t* tag;

   /* Check to see if the badge is available on disk */
   fill_pathname_application_special(fullpath, sizeof(fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);
   rcheevos_get_local_badge_filename(badge_file, sizeof(badge_file), badge, locked);
   fill_pathname_join(fullpath, fullpath, badge_file, sizeof(fullpath));

   if (!path_is_valid(fullpath))
   {
      /* Not available on disk */
      if (download_if_missing)
      {
         /* Fetch it */
         if (!slot)
            slot = rcheevos_badge_slot_alloc(badge, locked);
         slot->state = RCHEEVOS_BADGE_SLOT_FETCHING;

         rcheevos_badge_request_download(badge, locked);
      }

      return 0;
   }

   if (!(tag = (rcheevos_badge_load_tag_t*)malloc(sizeof(*tag))))
      return 0;

   if (!slot)
      slot = rcheevos_badge_slot_alloc(badge, locked);
   slot->state = RCHEEVOS_BADGE_SLOT_LOADING;

   tag->slot = (unsigned)(slot - rcheevos_badge_slots);
   tag->seq = slot->seq;

   gfx_surface_query_requirements(0, &req);
   if (!task_push_image_load(fullpath, req.rgba,
         0, 0, rcheevos_badge_decode_done, tag))
   {
      free(tag);
      rcheevos_badge_slot_clear(slot);
      return 0;
   }

   return 0;   /* ready on a later call */
}

uintptr_t rcheevos_get_badge_texture(const char* badge, bool locked, bool download_if_missing)
{
   rcheevos_badge_slot_t *slot;
   uintptr_t tex;

   if (!badge || !badge[0])
      return 0;

#ifdef HAVE_THREADS
   /* The OpenGL driver crashes if gfx_display_reset_textures_list is not called on the video thread.
    * If threaded video is enabled, it'll automatically dispatch the request to the video thread.
    * If threaded video is not enabled, just return null. The video thread should assume the image
    * wasn't downloaded and check again in a few frames.
    */
   if (!video_driver_is_threaded() && !task_is_on_main_thread())
      return 0;
#endif

   /* Check to see if we're already processing this badge */
   if ((slot = rcheevos_badge_slot_find(badge, locked)))
   {
      switch (slot->state)
      {
         case RCHEEVOS_BADGE_SLOT_READY:
            tex          = slot->handle;
            slot->handle = 0;
            slot->state  = RCHEEVOS_BADGE_SLOT_EMPTY;
            slot->key[0] = '\0';
            return tex;    /* the caller is responsible for it now */

         default:
            /* Not ready yet, or failed to load. If not ready yet, state will eventually change */
            return 0;
      }
   }

   if (string_is_equal(badge, "00000"))
   {
      slot = &rcheevos_badge_slots[RCHEEVOS_BADGE_SLOTS];
      if (slot->state == RCHEEVOS_BADGE_SLOT_READY)
         return slot->handle;

      if (slot->state != RCHEEVOS_BADGE_SLOT_EMPTY) /* Not ready yet or failed to load */
         return 0;

      locked = false; /* Default badge is never locked */
   }

   return rcheevos_load_badge_texture(slot, badge, locked, download_if_missing);
}

void rcheevos_update_badge_references(const char* badge_name)
{
   rcheevos_badge_slot_t* slot;

   bool locked = false;
   char unlocked_badge_name[8];
   const size_t badge_name_len = strlen(badge_name);
   if (badge_name_len > 6 && badge_name_len < sizeof(unlocked_badge_name) + 5 &&
      strcmp(&badge_name[badge_name_len - 5], "_lock") == 0)
   {
      memcpy(unlocked_badge_name, badge_name, badge_name_len - 5);
      unlocked_badge_name[badge_name_len - 5] = '\0';
      badge_name = unlocked_badge_name;
      locked = true;
   }

   slot = rcheevos_badge_slot_find(badge_name, locked);
   if (slot != NULL)
   {
      rcheevos_load_badge_texture(slot, badge_name, locked, false);

      /* If state was not updated, change to failed. */
      if (slot->state == RCHEEVOS_BADGE_SLOT_FETCHING)
         slot->state = RCHEEVOS_BADGE_SLOT_FAILED;
   }
}

static void rcheevos_client_download_user_badge()
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   const rc_client_user_t* user = rc_client_get_user_info(rcheevos_locals->client);
   if (user)
   {
      char badge_name[32];
      snprintf(badge_name, sizeof(badge_name), "u%u", rc_djb2(user->username));

      rcheevos_client_download_badge_from_url(user->avatar_url, badge_name);
   }
}

static void rcheevos_client_download_subset_badge(const char* badge_name)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals->client);
   if (game && strcmp(game->badge_name, &badge_name[1]) == 0)
   {
      rcheevos_client_download_badge_from_url(game->badge_url, badge_name);
   }
   else
   {
      rc_client_subset_list_t* subset_list = rc_client_create_subset_list(rcheevos_locals->client);
      uint32_t i;
      for (i = 0; i < subset_list->num_subsets; ++i)
      {
         if (strcmp(subset_list->subsets[i]->badge_name, &badge_name[1]) == 0)
         {
            rcheevos_client_download_badge_from_url(subset_list->subsets[i]->badge_url, badge_name);
            break;
         }
      }
      rc_client_destroy_subset_list(subset_list);
   }
}

static void rcheevos_client_download_achievement_badge(const char* badge_name, bool locked)
{
   /* have to find the achievement associated to badge_name, then fetch either badge_url
    * or badge_locked_url based on the locked parameter */
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rc_client_achievement_list_t* list = rc_client_create_achievement_list(rcheevos_locals->client,
      RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
      RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
   if (list)
   {
      const char* url = NULL;
      uint32_t i, j;
      for (i = 0; i < list->num_buckets && !url; i++)
      {
         for (j = 0; j < list->buckets[i].num_achievements; j++)
         {
            const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
            if (achievement && strcmp(achievement->badge_name, badge_name) == 0)
            {
               url = locked ? achievement->badge_locked_url : achievement->badge_url;
               break;
            }
         }
      }

      if (url)
      {
         char locked_badge_name[32];
         if (locked)
         {
            snprintf(locked_badge_name, sizeof(locked_badge_name), "%s_lock", badge_name);
            badge_name = locked_badge_name;
         }

         rcheevos_client_download_badge_from_url(url, badge_name);
      }

      rc_client_destroy_achievement_list(list);
   }
}

/* A badge file is missing locally: fetch it. Which URL depends on the
 * kind of badge, which the name's first letter says. */
void rcheevos_badge_request_download(const char* badge, bool locked)
{
   if (!badge || !badge[0])
      return;
   if (badge[0] == 'i')
      rcheevos_client_download_subset_badge(badge);
   else if (badge[0] == 'u')
      rcheevos_client_download_user_badge();
   else
      rcheevos_client_download_achievement_badge(badge, locked);
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
