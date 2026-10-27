/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "android_image_queue.h"
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef ANDROID_IMAGE_QUEUE_TEST_NO_FENCES
#include <poll.h>
#include <fcntl.h>
#endif
#include <errno.h>
struct android_image_ticket
{
   retro_atomic_int_t
         state; /* 0 free, 1 leased, 2 returned, 3 return publication */
   uint64_t generation;
   void *image;
   int ready_fd, done_fd;
};
struct android_image_queue
{
   android_image_ops_t ops;
   void *reader;
   unsigned count;
   bool closed, failed;
   android_image_ticket_t ticket[32];
};
android_image_queue_t *android_image_queue_new(
      void *reader, unsigned slots, const android_image_ops_t *ops)
{
   android_image_queue_t *q;
   unsigned i;
   if (!reader || !slots || slots > 32 || !ops || !ops->acquire
         || !ops->describe || !ops->delete_image || !ops->delete_reader)
      return NULL;
#ifndef RETRO_ATOMIC_LOCK_FREE
   return NULL;
#endif
   q = (android_image_queue_t *)calloc(1, sizeof(*q));
   if (!q)
      return NULL;
   q->reader = reader;
   q->count = slots;
   q->ops = *ops;
   for (i = 0; i < slots; i++)
   {
      retro_atomic_int_init(&q->ticket[i].state, 0);
      q->ticket[i].ready_fd = q->ticket[i].done_fd = -1;
   }
   return q;
}
int android_image_queue_acquire(
      android_image_queue_t *q, android_image_view_t *view)
{
   unsigned i;
   android_image_ticket_t *t;
   int result;
   if (view)
   {
      memset(view, 0, sizeof(*view));
      view->ready_fd = -1;
   }
   if (!q || !view || q->failed)
      return -1;
   if (q->closed)
      return 0;
   for (i = 0; i < q->count; i++)
      if (!retro_atomic_load_acquire_int(&q->ticket[i].state))
         break;
   if (i == q->count)
      return 0;
   t = &q->ticket[i];
   if (t->generation == ~(uint64_t)0)
   {
      q->failed = true;
      return -1;
   }
   result = q->ops.acquire(q->reader, &t->image, &t->ready_fd);
   if (result <= 0)
   {
      /* A failed native acquire must take nothing, including fence ownership.
       */
      if (t->image || t->ready_fd != -1)
      {
         q->failed = true;
         if (t->image)
            retro_atomic_store_release_int(
                  &t->state, t->ready_fd >= -1 ? 2 : 1);
         else if (t->ready_fd >= 0)
         {
            close(t->ready_fd);
            t->ready_fd = -1;
         }
         return -1;
      }
      if (result < 0)
         q->failed = true;
      return result;
   }
   if (result != 1 || !t->image || t->ready_fd < -1)
   {
      q->failed = true;
      if (t->image)
         retro_atomic_store_release_int(&t->state, t->ready_fd >= -1 ? 2 : 1);
      else if (t->ready_fd >= 0)
      {
         close(t->ready_fd);
         t->ready_fd = -1;
      }
      return -1;
   }
   t->generation++;
   t->done_fd = -1;
   if (!q->ops.describe(t->image, view) || !view->buffer || !view->width
         || !view->height || view->left >= view->right
         || view->top >= view->bottom || view->right > view->width
         || view->bottom > view->height || view->timestamp_ns < 0)
   {
      q->failed = true;
      retro_atomic_store_release_int(
            &t->state, 2); /* discard after ready fence */
      memset(view, 0, sizeof(*view));
      view->ready_fd = -1;
      return -1;
   }
   view->ticket = t;
   view->generation = t->generation;
   view->ready_fd = t->ready_fd;
   retro_atomic_store_release_int(&t->state, 1);
   return 1;
}
bool android_image_queue_return(
      android_image_ticket_t *t, uint64_t generation, int done_fd)
{
   if (!t || done_fd < -1)
      return false;
#ifdef ANDROID_IMAGE_QUEUE_TEST_NO_FENCES
   /* Host-only lifecycle fixtures cannot import native sync fds. */
   if (done_fd >= 0)
      return false;
#else
   if (done_fd >= 0 && fcntl(done_fd, F_GETFD) < 0)
      return false;
#endif
   if (!retro_atomic_cas_int(&t->state, 1, 3))
      return false;
   if (t->generation != generation || (done_fd >= 0 && done_fd == t->ready_fd))
   {
      retro_atomic_store_release_int(&t->state, 1);
      return false;
   }
   t->done_fd = done_fd;
   retro_atomic_store_release_int(&t->state, 2);
   return true;
}
unsigned android_image_queue_poll(android_image_queue_t *q)
{
   unsigned i, released = 0;
   if (!q)
      return 0;
   for (i = 0; i < q->count; i++)
   {
      android_image_ticket_t *t = &q->ticket[i];
      int fd;
#ifndef ANDROID_IMAGE_QUEUE_TEST_NO_FENCES
      int status = 1;
#endif
      if (retro_atomic_load_acquire_int(&t->state) != 2)
         continue;
      fd = t->done_fd >= 0 ? t->done_fd : t->ready_fd;
      if (fd >= 0)
      {
#ifdef ANDROID_IMAGE_QUEUE_TEST_NO_FENCES
         q->failed = true;
         continue;
#else
         struct pollfd p;
         memset(&p, 0, sizeof(p));
         p.fd = fd;
         p.events = POLLIN;
         status = poll(&p, 1, 0);
         if (status < 0 && errno == EINTR)
            continue;
         /* An invalid fence cannot prove GPU completion. Retain the image. */
         if (status < 0 || (p.revents & POLLNVAL))
         {
            q->failed = true;
            continue;
         }
         if (!status)
            continue;
         if (!(p.revents & (POLLIN | POLLERR | POLLHUP)))
            continue;
         if (p.revents & (POLLERR | POLLHUP))
            q->failed = true;
#endif
      }
      if (t->ready_fd >= 0)
         close(t->ready_fd);
      if (t->done_fd >= 0)
         close(t->done_fd);
      q->ops.delete_image(t->image);
      t->image = NULL;
      t->ready_fd = t->done_fd = -1;
      retro_atomic_store_release_int(&t->state, 0);
      released++;
   }
   return released;
}
int android_image_queue_retire_fd(const android_image_queue_t *q)
{
   unsigned i;
   if (!q || !q->closed)
      return -1;
   for (i = 0; i < q->count; i++)
   {
      const android_image_ticket_t *t = &q->ticket[i];
      if (retro_atomic_load_acquire_int(&t->state) == 2)
      {
         int fd = t->done_fd >= 0 ? t->done_fd : t->ready_fd;
         if (fd >= 0)
            return fd;
      }
   }
   return -1;
}
void android_image_queue_close(android_image_queue_t *q)
{
   if (q)
      q->closed = true;
}
bool android_image_queue_idle(const android_image_queue_t *q)
{
   unsigned i;
   if (!q)
      return true;
   if (!q->closed)
      return false;
   for (i = 0; i < q->count; i++)
      if (retro_atomic_load_acquire_int(&q->ticket[i].state)
            || q->ticket[i].image)
         return false;
   return true;
}
bool android_image_queue_free(android_image_queue_t *q)
{
   if (!q)
      return true;
   if (!android_image_queue_idle(q))
      return false;
   q->ops.delete_reader(q->reader);
   free(q);
   return true;
}
