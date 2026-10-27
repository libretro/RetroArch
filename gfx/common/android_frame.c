/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "android_frame.h"
#include <retro_atomic.h>
#include <formats/image.h>
#include <stdlib.h>
#include <string.h>
typedef struct android_frame_slot
{
   retro_atomic_int_t
         state; /* 0 free, 1 pending, 2 claimed, 3 return publication */
   uint64_t generation;
   android_frame_view_t view;
} android_frame_slot_t;
struct android_frame_pool
{
   void *notify_user;
   void (*retain)(void *);
   void (*notify_unref)(void *);
   unsigned count;
   bool closed;
   android_frame_slot_t slot[32];
};
android_frame_pool_t *android_frame_pool_new(unsigned count)
{
   android_frame_pool_t *p;
   unsigned i;
   if (!count || count > 32)
      return NULL;
#ifndef RETRO_ATOMIC_LOCK_FREE
   return NULL;
#endif
   p = (android_frame_pool_t *)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->count = count;
   for (i = 0; i < count; i++)
      retro_atomic_int_init(&p->slot[i].state, 0);
   return p;
}
bool android_frame_pool_set_retire_notifier(android_frame_pool_t *p,
      void *user, void (*retain)(void *), void (*notify_unref)(void *))
{
   if (!p || p->closed || p->retain || !user || !retain || !notify_unref)
      return false;
   p->notify_user = user;
   p->retain = retain;
   p->notify_unref = notify_unref;
   return true;
}
void android_frame_pool_close(android_frame_pool_t *p)
{
   if (p)
      p->closed = true;
}
bool android_frame_pool_free(android_frame_pool_t *p)
{
   unsigned i;
   if (!p)
      return true;
   if (!p->closed)
      return false;
   for (i = 0; i < p->count; i++)
      if (retro_atomic_load_acquire_int(&p->slot[i].state))
         return false;
   free(p);
   return true;
}
bool android_frame_finish(
      void *owner, uint64_t index, uint64_t generation, int fd)
{
   android_frame_pool_t *p = (android_frame_pool_t *)owner;
   android_frame_slot_t *s;
   void *notify_user;
   void (*retain)(void *);
   void (*notify_unref)(void *);
   if (!p || index >= p->count || fd < -1)
      return false;
   s = &p->slot[(unsigned)index];
   if (!retro_atomic_cas_int(&s->state, 2, 3))
      return false;
   if (s->generation != generation
         || !android_image_queue_return(
               s->view.image.ticket, s->view.image.generation, fd))
   {
      retro_atomic_store_release_int(&s->state, 2);
      return false;
   }
   notify_user = p->notify_user;
   retain = p->retain;
   notify_unref = p->notify_unref;
   if (retain)
      retain(notify_user);
   memset(&s->view, 0, sizeof(s->view));
   retro_atomic_store_release_int(&s->state, 0);
   if (notify_unref)
      notify_unref(notify_user);
   return true;
}
static void android_frame_abandon(
      void *owner, uint64_t index, uint64_t generation)
{
   android_frame_view_t view;
   android_frame_pool_t *p = (android_frame_pool_t *)owner;
   if (!p || index >= p->count)
      return;
   if (android_frame_claim(owner, index, generation,
             (uint64_t)(uintptr_t)&p->slot[(unsigned)index], &view))
      android_frame_finish(owner, index, generation, -1);
}
bool android_frame_callback(void (*callback)(void *, uint64_t, uint64_t))
{
   return callback == android_frame_abandon;
}
bool android_frame_unclaim(void *owner, uint64_t index, uint64_t generation)
{
   android_frame_pool_t *p = (android_frame_pool_t *)owner;
   android_frame_slot_t *s;
   if (!p || index >= p->count)
      return false;
   s = &p->slot[(unsigned)index];
   if (!retro_atomic_cas_int(&s->state, 2, 3))
      return false;
   if (s->generation != generation)
   {
      retro_atomic_store_release_int(&s->state, 2);
      return false;
   }
   retro_atomic_store_release_int(&s->state, 1);
   return true;
}
bool android_frame_claim(void *owner, uint64_t index, uint64_t generation,
      uint64_t image, android_frame_view_t *view)
{
   android_frame_pool_t *p = (android_frame_pool_t *)owner;
   android_frame_slot_t *s;
   if (!p || !view || index >= p->count)
      return false;
   s = &p->slot[(unsigned)index];
   if (image != (uint64_t)(uintptr_t)s
         || !retro_atomic_cas_int(&s->state, 1, 2))
      return false;
   if (s->generation != generation)
   {
      retro_atomic_store_release_int(&s->state, 1);
      return false;
   }
   *view = s->view;
   return true;
}
int android_frame_export(android_frame_pool_t *p,
      const android_frame_view_t *view, video_hw_frame_t *out)
{
   unsigned i;
   android_frame_slot_t *s;
   video_hw_frame_t frame;
   if (!p || p->closed || !view || !out || !view->image.ticket
         || !view->image.buffer || !view->format.serial
         || (view->pixfmt != IMAGE_PIXFMT_NV12
               && view->pixfmt != IMAGE_PIXFMT_P010)
         || view->image.left >= view->image.right
         || view->image.top >= view->image.bottom
         || view->image.right > view->image.buffer_width
         || view->image.bottom > view->image.buffer_height)
      return -1;
   for (i = 0; i < p->count; i++)
      if (!retro_atomic_load_acquire_int(&p->slot[i].state))
         break;
   if (i == p->count)
      return 1;
   s = &p->slot[i];
   if (s->generation == ~(uint64_t)0)
      return -1;
   s->generation++;
   s->view = *view;
   memset(&frame, 0, sizeof(frame));
   frame.image = (uint64_t)(uintptr_t)s;
   frame.owner = p;
   frame.done = i;
   frame.done_value = s->generation;
   frame.abandon = android_frame_abandon;
   frame.width = view->image.right - view->image.left;
   frame.height = view->image.bottom - view->image.top;
   frame.crop_x = view->image.left;
   frame.crop_y = view->image.top;
   frame.pixfmt = view->pixfmt;
   frame.color = view->color;
   retro_atomic_store_release_int(&s->state, 1);
   *out = frame;
   return 0;
}
