/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/metal_decoder_snapshot.h"
#ifdef METAL_DECODER_SNAPSHOT
struct device
{
   retro_atomic_int_t refs;
   bool hdr;
};

static metal_decoder_snapshot_t state;
static struct device first, second;
static retro_atomic_int_t ready, done;
static unsigned offers;

static void *retain_device(void *opaque)
{
   struct device *d = (struct device *)opaque;
   assert(retro_atomic_fetch_add_int(&d->refs, 1) > 0);
   return d;
}

static void release_device(void *opaque)
{
   struct device *d = (struct device *)opaque;
   assert(retro_atomic_fetch_sub_int(&d->refs, 1) > 1);
}

static void offer(void *user)
{
   assert(user == &offers);
   offers++;
}

static void writer(void *user)
{
   unsigned i;
   (void)user;
   while (!retro_atomic_load_acquire_int(&ready))
      sthread_yield();
   for (i = 0; i < 1000; i++)
   {
      struct device *d = (i & 1) ? &first : &second;
      assert(metal_decoder_snapshot_publish(
            &state, &state, d, d->hdr, offer, &offers));
      if (!(i & 7))
         assert(
               metal_decoder_snapshot_withdraw(&state, &state, offer, &offers));
   }
   retro_atomic_store_release_int(&done, 1);
}

static void reader(void *user)
{
   metal_decoder_snapshot_view_t view;
   unsigned i;
   (void)user;
   retro_atomic_store_release_int(&ready, 1);
   for (i = 0; i < 1000; i++)
      if (metal_decoder_snapshot_acquire(&state, &view))
      {
         struct device *d = (struct device *)view.device;
         assert((d == &first || d == &second) && view.hdr == d->hdr
                && (view.generation & 1));
         (void)metal_decoder_snapshot_current(&state, view.generation);
         release_device(d);
      }
   while (!retro_atomic_load_acquire_int(&done))
      sthread_yield();
}

int main(void)
{
   slock_t *lock = slock_new();
   sthread_t *one, *two;
   metal_decoder_snapshot_view_t old, current;
   unsigned before;
   assert(lock);
   retro_atomic_int_init(&first.refs, 1);
   retro_atomic_int_init(&second.refs, 1);
   first.hdr  = false;
   second.hdr = true;
   assert(metal_decoder_snapshot_init(
         &state, lock, retain_device, release_device));
   assert(!metal_decoder_snapshot_acquire(&state, &old) && !old.device);
   assert(metal_decoder_snapshot_publish(
         &state, &first, &first, false, offer, &offers));
   assert(metal_decoder_snapshot_acquire(&state, &old) && old.device == &first
          && !old.hdr);
   assert(metal_decoder_snapshot_current(&state, old.generation));
   assert(metal_decoder_snapshot_publish(
         &state, &second, &second, true, offer, &offers));
   assert(!metal_decoder_snapshot_current(&state, old.generation));
   assert(metal_decoder_snapshot_acquire(&state, &current)
          && current.device == &second && current.hdr);
   assert(metal_decoder_snapshot_is_owner(&state, &second));
   before = offers;
   assert(!metal_decoder_snapshot_withdraw(&state, &first, offer, &offers)
          && offers == before);
   assert(metal_decoder_snapshot_withdraw(&state, &second, offer, &offers));
   assert(!metal_decoder_snapshot_current(&state, current.generation));
   assert(retro_atomic_load_acquire_int(&first.refs) == 2);
   assert(retro_atomic_load_acquire_int(&second.refs) == 2);
   release_device(old.device);
   release_device(current.device);
   assert(!metal_decoder_snapshot_acquire(&state, &current) && !current.device
          && !current.hdr);
   retro_atomic_int_init(&ready, 0);
   retro_atomic_int_init(&done, 0);
   one = sthread_create(writer, NULL);
   two = sthread_create(reader, NULL);
   assert(one && two);
   sthread_join(one);
   sthread_join(two);
   assert(metal_decoder_snapshot_withdraw(&state, &state, offer, &offers));
   assert(retro_atomic_load_acquire_int(&first.refs) == 1);
   assert(retro_atomic_load_acquire_int(&second.refs) == 1);
   before = offers;
   retro_atomic_store_release_size(&state.generation, (size_t)-1 - 1);
   assert(!metal_decoder_snapshot_publish(
         &state, &first, &first, false, offer, &offers));
   assert(offers == before && retro_atomic_load_acquire_int(&first.refs) == 1);
   slock_free(lock);
   puts("PASS: shared Metal snapshot pairs, retained devices, stale owners/generations, exhaustion and 1000 concurrent replacements");
   return 0;
}
#else
int main(void)
{
   puts("PASS: shared Metal snapshot excluded without threads");
   return 0;
}
#endif
