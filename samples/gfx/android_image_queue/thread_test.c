/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define main serial_contract_main
#include "test.c"
#undef main
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
static retro_atomic_int_t published;
static android_image_view_t shared;
static void render(void *unused)
{
   unsigned i;
   (void)unused;
   for (i = 0; i < 10000; i++)
   {
      android_image_view_t view;
      while (!retro_atomic_load_acquire_int(&published))
      {
      }
      view = shared;
      assert(android_image_queue_return(view.ticket, view.generation, -1));
      retro_atomic_store_release_int(&published, 0);
   }
   return;
}
int main(void)
{
   android_image_ops_t ops;
   android_image_queue_t *q;
   sthread_t *thread;
   unsigned i;
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = release_image;
   ops.delete_reader = release_reader;
   q = android_image_queue_new(&image, 1, &ops);
   assert(q);
   retro_atomic_int_init(&published, 0);
   thread = sthread_create(render, NULL);
   assert(thread);
   for (i = 0; i < 10000; i++)
   {
      android_image_view_t view;
      while (retro_atomic_load_acquire_int(&published))
      {
      }
      while (!android_image_queue_acquire(q, &view))
         android_image_queue_poll(q);
      assert(view.ticket);
      shared = view;
      retro_atomic_store_release_int(&published, 1);
   }
   android_image_queue_close(q);
   assert(!android_image_queue_free(q));
   sthread_join(thread);
   assert(android_image_queue_poll(q) == 1);
   assert(android_image_queue_free(q));
   assert(deleted == 10000 && reader_deleted == 1);
   puts("PASS: 10000 concurrent worker/render image handoffs and close with "
        "outstanding lease");
   return 0;
}
