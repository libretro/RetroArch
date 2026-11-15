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
#include <queues/mpsc_stack.h>

#include "cheevos.h"

#include "../gfx/gfx_display.h"
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

/* Like mpsc_stack, targets without pointer atomics are single-context. */
#ifdef RETRO_ATOMIC_HAS_PTR
typedef retro_atomic_ptr_t badge_ptr_t;
#define badge_ptr_load retro_atomic_load_acquire_ptr
#define badge_ptr_store retro_atomic_store_release_ptr
#define badge_ptr_exchange retro_atomic_exchange_ptr
#define badge_ptr_cas retro_atomic_cas_ptr
#else
typedef void *badge_ptr_t;
#define badge_ptr_load(p) (*(p))
#define badge_ptr_store(p, v) (*(p) = (v))
static void *badge_ptr_exchange(badge_ptr_t *p, void *value)
{
   void *old = *p;
   *p = value;
   return old;
}
static bool badge_ptr_cas(badge_ptr_t *p, void *expected, void *value)
{
   if (*p != expected)
      return false;
   *p = value;
   return true;
}
#endif

static bool badge_claim_request(retro_atomic_int_t *asked)
{
#ifdef RETRO_ATOMIC_HAS_CAS
   return retro_atomic_cas_int(asked, 0, 1) != 0;
#else
   if (*asked)
      return false;
   *asked = 1;
   return true;
#endif
}

/* Keys are immutable while published. Only the main thread changes the
 * index; upload completions publish surfaces and producers queue requests. */
#define RCHEEVOS_BADGE_SLOTS 16
#define RCHEEVOS_BADGE_DEFAULT_SLOT RCHEEVOS_BADGE_SLOTS
#define RCHEEVOS_BADGE_DEFAULT_NAME "00000"
#define RCHEEVOS_BADGE_KEY_LEN 16

enum
{
   BADGE_REQUESTED,
   BADGE_FETCHING,
   BADGE_LOADING,
   BADGE_READY,
   BADGE_FAILED
};
enum
{
   BADGE_ASK,
   BADGE_DOWNLOADED,
   BADGE_DONE
};
typedef struct rcheevos_badge_entry
{
   badge_ptr_t next;
   badge_ptr_t surface;
   retro_atomic_int_t state, retired, download, asked;
   struct rcheevos_badge_entry *retire_next;
   unsigned inflight;
   int generation;
   bool locked, shared;
   char key[RCHEEVOS_BADGE_KEY_LEN];
} rcheevos_badge_entry_t;

typedef struct rcheevos_badge_message
{
   mpsc_stack_node_t node;
   unsigned type;
   int generation;
   bool locked, download, shared;
   char key[RCHEEVOS_BADGE_KEY_LEN];
} rcheevos_badge_message_t;

typedef struct
{
   rcheevos_badge_message_t message;
   rcheevos_badge_entry_t *entry;
   gfx_surface_t *surface;
} rcheevos_badge_load_tag_t;

static mpsc_stack_t badge_messages;
static badge_ptr_t badge_head;
static retro_atomic_int_t badge_readers;
static retro_atomic_int_t badge_generation;
static rcheevos_badge_entry_t *badge_slots[RCHEEVOS_BADGE_SLOTS + 1];
static rcheevos_badge_entry_t *badge_retired;
static unsigned badge_next;
static int badge_owned_generation;
static bool badge_servicing, badge_work;

static void badge_read_begin(void)
{
   /* Pairs with unlink/collect: reclamation cannot miss an active reader. */
   retro_atomic_fetch_add_seq_cst_int(&badge_readers, 1);
}
static void badge_read_end(void)
{
   retro_atomic_fetch_add_seq_cst_int(&badge_readers, -1);
}
static rcheevos_badge_entry_t *badge_find(const char *key, bool locked,
                                          bool shared)
{
   rcheevos_badge_entry_t *e =
       (rcheevos_badge_entry_t *)badge_ptr_load(&badge_head);
   for (; e; e = (rcheevos_badge_entry_t *)badge_ptr_load(&e->next))
      if (e->generation == retro_atomic_load_acquire_int(&badge_generation) &&
          e->locked == locked && e->shared == shared &&
          string_is_equal(e->key, key))
         return e;
   return NULL;
}
static void badge_retire(rcheevos_badge_entry_t *entry)
{
   badge_ptr_t *link = &badge_head;
   rcheevos_badge_entry_t *e;
   retro_atomic_store_release_int(&entry->retired, 1);
   while ((e = (rcheevos_badge_entry_t *)badge_ptr_load(link)))
   {
      if (e == entry)
      {
         badge_ptr_store(link, badge_ptr_load(&e->next));
         break;
      }
      link = &e->next;
   }
   entry->retire_next = badge_retired;
   badge_retired = entry;
}
static void badge_collect(void)
{
   rcheevos_badge_entry_t **link = &badge_retired;
   retro_atomic_thread_fence_seq_cst();
   if (retro_atomic_load_seq_cst_int(&badge_readers))
      return;
   while (*link)
   {
      rcheevos_badge_entry_t *e = *link;
      gfx_surface_t *s = (gfx_surface_t *)badge_ptr_exchange(&e->surface, NULL);
      if (s)
         gfx_surface_free(s);
      if (e->inflight)
         link = &e->retire_next;
      else
      {
         *link = e->retire_next;
         free(e);
      }
   }
}
static bool badge_post(unsigned type, const char *key, bool locked,
                       bool download, bool shared)
{
   rcheevos_badge_message_t *m =
       (rcheevos_badge_message_t *)calloc(1, sizeof(*m));
   if (!m)
      return false;
   m->generation = retro_atomic_load_acquire_int(&badge_generation);
   m->type = type;
   m->locked = locked;
   m->download = download;
   m->shared = shared;
   if (key)
      strlcpy(m->key, key, sizeof(m->key));
   mpsc_stack_push(&badge_messages, &m->node);
   return true;
}
static void badge_reset_owned(void)
{
   unsigned i;
   int generation = retro_atomic_load_acquire_int(&badge_generation);
   for (i = 0; i < RCHEEVOS_BADGE_SLOTS + 1; i++)
      if (badge_slots[i] && badge_slots[i]->generation != generation)
      {
         badge_retire(badge_slots[i]);
         badge_slots[i] = NULL;
      }
   badge_owned_generation = generation;
   badge_collect();
}
void rcheevos_badge_cache_reset(void)
{
   retro_atomic_fetch_add_seq_cst_int(&badge_generation, 1);
   if (task_is_on_main_thread())
      badge_reset_owned();
}
static void rcheevos_badge_load_done(rcheevos_badge_load_tag_t *tag,
                                     bool landed)
{
   rcheevos_badge_entry_t *e = tag->entry;
   if (landed && !retro_atomic_load_acquire_int(&e->retired))
   {
      badge_ptr_store(&e->surface, tag->surface);
      tag->surface = NULL;
      retro_atomic_store_release_int(&e->state, BADGE_READY);
   }
   else
   {
      if (tag->surface)
         gfx_surface_free(tag->surface);
      tag->surface = NULL;
      retro_atomic_store_release_int(&e->state, BADGE_FAILED);
   }
   tag->message.type = BADGE_DONE;
   mpsc_stack_push(&badge_messages, &tag->message.node);
   rcheevos_badge_cache_service();
}
static void rcheevos_badge_landed(void *user, gfx_surface_t *s, unsigned slot)
{
   (void)slot;
   rcheevos_badge_load_done((rcheevos_badge_load_tag_t *)user, s->handle != 0);
}
static void rcheevos_badge_decode_done(retro_task_t *task, void *task_data,
                                       void *user_data, const char *error)
{
   struct texture_image *img = (struct texture_image *)task_data;
   rcheevos_badge_load_tag_t *tag = (rcheevos_badge_load_tag_t *)user_data;
   gfx_surface_t *s;
   (void)task;
   (void)error;
   if (!tag)
   {
      if (img)
      {
         image_texture_free(img);
         free(img);
      }
      return;
   }
   s = tag->surface;
   s->release = rcheevos_badge_landed;
   s->user = tag;
   if (!gfx_surface_submit_image(s, img))
      rcheevos_badge_load_done(tag, false);
   else if (!s->inflight)
      rcheevos_badge_load_done(tag, s->handle != 0);
}
static void badge_start_load(rcheevos_badge_entry_t *e)
{
   gfx_surface_requirements_t req;
   char badge_file[RCHEEVOS_BADGE_KEY_LEN + 16];
   char fullpath[PATH_MAX_LENGTH];
   rcheevos_badge_load_tag_t *tag;
   e->inflight++;
   retro_atomic_store_release_int(&e->state, BADGE_LOADING);
   fill_pathname_application_special(
       fullpath, sizeof(fullpath),
       APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);
   rcheevos_get_local_badge_filename(badge_file, sizeof(badge_file), e->key,
                                     e->locked);
   fill_pathname_join(fullpath, fullpath, badge_file, sizeof(fullpath));
   if (!path_is_valid(fullpath))
   {
      bool download = retro_atomic_load_acquire_int(&e->download) != 0;
      retro_atomic_store_release_int(&e->state,
                                     download ? BADGE_FETCHING : BADGE_FAILED);
      if (download)
         rcheevos_badge_request_download(e->key, e->locked);
      e->inflight--;
      return;
   }
   tag = (rcheevos_badge_load_tag_t *)calloc(1, sizeof(*tag));
   if (!tag)
   {
      retro_atomic_store_release_int(&e->state, BADGE_FAILED);
      e->inflight--;
      return;
   }
   tag->entry = e;
   tag->surface = gfx_surface_new_still(gfx_display_texture_filter_latched());
   gfx_surface_query_requirements(0, &req);
   if (!tag->surface || !task_push_image_load(fullpath, req.rgba, 0, 0,
                                              rcheevos_badge_decode_done, tag))
      rcheevos_badge_load_done(tag, false);
}
static void badge_process(rcheevos_badge_message_t *m)
{
   rcheevos_badge_entry_t *e;
   unsigned index;
   int state;
   if (m->type == BADGE_DONE)
   {
      rcheevos_badge_load_tag_t *tag = (rcheevos_badge_load_tag_t *)m;
      tag->entry->inflight--;
      free(tag);
      return;
   }
   if (m->generation != retro_atomic_load_acquire_int(&badge_generation))
   {
      free(m);
      return;
   }
   if (m->type == BADGE_DOWNLOADED)
   {
      for (index = 0; index < RCHEEVOS_BADGE_SLOTS + 1; index++)
      {
         e = badge_slots[index];
         if (!e || e->generation != m->generation || e->locked != m->locked ||
             !string_is_equal(e->key, m->key))
            continue;
         state = retro_atomic_load_acquire_int(&e->state);
         if (state == BADGE_FETCHING || state == BADGE_FAILED)
         {
            retro_atomic_store_release_int(&e->download, 0);
            retro_atomic_store_release_int(&e->state, BADGE_REQUESTED);
            badge_work = true;
         }
      }
      free(m);
      return;
   }
   e = badge_find(m->key, m->locked, m->shared);
   if (!e && m->type == BADGE_ASK)
   {
      e = (rcheevos_badge_entry_t *)calloc(1, sizeof(*e));
      if (!e)
      {
         free(m);
         return;
      }
      e->generation = m->generation;
      strlcpy(e->key, m->key, sizeof(e->key));
      e->locked = m->locked;
      e->shared = m->shared;
#ifdef RETRO_ATOMIC_HAS_PTR
      retro_atomic_ptr_init(&e->surface, NULL);
      retro_atomic_ptr_init(&e->next, NULL);
#endif
      retro_atomic_int_init(&e->state, BADGE_REQUESTED);
      retro_atomic_int_init(&e->retired, 0);
      retro_atomic_int_init(&e->download, m->download);
      retro_atomic_int_init(&e->asked, 0);
      index = m->shared ? RCHEEVOS_BADGE_DEFAULT_SLOT : badge_next;
      if (!m->shared)
         badge_next = (badge_next + 1) % RCHEEVOS_BADGE_SLOTS;
      if (badge_slots[index])
         badge_retire(badge_slots[index]);
      badge_slots[index] = e;
      badge_ptr_store(&e->next, badge_ptr_load(&badge_head));
      badge_ptr_store(&badge_head, e);
   }
   if (e)
   {
      state = retro_atomic_load_acquire_int(&e->state);
      if (m->type == BADGE_ASK)
      {
         if (state == BADGE_READY && !badge_ptr_load(&e->surface))
            retro_atomic_store_release_int(&e->state, BADGE_REQUESTED);
         if (m->download && !retro_atomic_load_acquire_int(&e->download))
         {
            retro_atomic_store_release_int(&e->download, 1);
            if (state == BADGE_FAILED)
               retro_atomic_store_release_int(&e->state, BADGE_REQUESTED);
         }
      }
      retro_atomic_store_release_int(&e->asked, 0);
      badge_work = true;
   }
   free(m);
}
void rcheevos_badge_cache_service(void)
{
   unsigned pass, i;
   if (!task_is_on_main_thread() || badge_servicing)
      return;
   if (badge_owned_generation !=
       retro_atomic_load_acquire_int(&badge_generation))
      badge_reset_owned();
   if (!badge_work && !badge_retired && mpsc_stack_empty(&badge_messages))
      return;
   badge_servicing = true;
   for (pass = 0; pass < 2; pass++)
   {
      mpsc_stack_node_t *fifo =
          mpsc_stack_reverse(mpsc_stack_drain(&badge_messages));
      while (fifo)
      {
         mpsc_stack_node_t *n = fifo;
         fifo = n->next;
         badge_process((rcheevos_badge_message_t *)n);
      }
      if (!pass)
      {
         badge_work = false;
         for (i = 0; i < RCHEEVOS_BADGE_SLOTS + 1; i++)
            if (badge_slots[i] &&
                retro_atomic_load_acquire_int(&badge_slots[i]->state) ==
                    BADGE_REQUESTED)
               badge_start_load(badge_slots[i]);
      }
   }
   badge_collect();
   badge_servicing = false;
}
static gfx_surface_t *rcheevos_badge_ask(const char *key, bool locked,
                                         bool download, bool shared,
                                         bool *pending)
{
   unsigned attempt;
   if (pending)
      *pending = false;
   if (!key || !*key || strlen(key) >= RCHEEVOS_BADGE_KEY_LEN)
      return NULL;
   rcheevos_badge_cache_service();
   for (attempt = 0; attempt < 2; attempt++)
   {
      rcheevos_badge_entry_t *e;
      gfx_surface_t *surface = NULL;
      bool request = false, waiting = true;
      badge_read_begin();
      e = badge_find(key, locked, shared);
      if (e && !retro_atomic_load_acquire_int(&e->retired))
      {
         int state = retro_atomic_load_acquire_int(&e->state);
         if (state == BADGE_READY)
         {
            surface = (gfx_surface_t *)badge_ptr_load(&e->surface);
            if (surface && !shared &&
                !badge_ptr_cas(&e->surface, surface, NULL))
               surface = NULL;
            if (surface && (retro_atomic_load_acquire_int(&e->retired) ||
                            e->generation != retro_atomic_load_acquire_int(
                                                 &badge_generation)))
            {
               if (!shared)
                  gfx_surface_free(surface);
               surface = NULL;
            }
            request = !surface;
         }
         else if (state == BADGE_FAILED)
         {
            waiting = download && !retro_atomic_load_acquire_int(&e->download);
            request = waiting;
         }
         else
         {
            waiting = state == BADGE_REQUESTED || state == BADGE_LOADING;
            if (download && !retro_atomic_load_acquire_int(&e->download))
               request = true;
         }
         if (request && !badge_claim_request(&e->asked))
            request = false;
         if (request && !badge_post(BADGE_ASK, key, locked, download, shared))
            retro_atomic_store_release_int(&e->asked, 0);
      }
      else
         request = true;
      if (!e && request)
         badge_post(BADGE_ASK, key, locked, download, shared);
      badge_read_end();
      if (surface)
         return surface;
      if (pending)
         *pending = waiting;
      if (!task_is_on_main_thread() || !request)
         return NULL;
      rcheevos_badge_cache_service();
   }
   return NULL;
}
gfx_surface_t *rcheevos_get_badge_texture(const char *badge, bool locked,
                                          bool download)
{
   return rcheevos_badge_ask(badge, locked, download, false, NULL);
}
gfx_surface_t *rcheevos_get_badge_texture_ex(const char *badge, bool locked,
                                             bool download, bool *pending)
{
   return rcheevos_badge_ask(badge, locked, download, false, pending);
}
gfx_surface_t *rcheevos_get_default_badge_texture(void)
{
   return rcheevos_badge_ask(RCHEEVOS_BADGE_DEFAULT_NAME, false, false, true,
                             NULL);
}
void rcheevos_update_badge_references(const char *badge)
{
   char key[RCHEEVOS_BADGE_KEY_LEN];
   size_t len = badge ? strlen(badge) : 0;
   bool locked = false;
   if (!len)
      return;
   if (len > 5 && string_is_equal(badge + len - 5, "_lock"))
   {
      len -= 5;
      locked = true;
   }
   if (len >= sizeof(key))
      return;
   memcpy(key, badge, len);
   key[len] = 0;
   badge_post(BADGE_DOWNLOADED, key, locked, false, false);
   rcheevos_badge_cache_service();
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
