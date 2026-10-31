/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <retro_atomic.h>
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_HAS_PTR)                     \
      && defined(RETRO_ATOMIC_LOCK_FREE)
/* Replaces the device between an acquire's load and its retain. */
struct decoder_device_slot;
static void replace_while_loading(struct decoder_device_slot *slot);
#define DECODER_DEVICE_SLOT_TEST_LOADED(s) replace_while_loading(s)
#endif
#include "../../../gfx/common/metal_decoder_snapshot.h"
#include <rthreads/rthreads.h>
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

/* Devices that count a retain after their last release. */
#define POOL 32768
static struct device pool[POOL];
static unsigned stale, pooled;
static bool replacing;
static metal_decoder_snapshot_t strict;

static void *retain_pooled(void *opaque)
{
   struct device *d = (struct device *)opaque;
   if (retro_atomic_fetch_add_int(&d->refs, 1) <= 0)
   {
      retro_atomic_fetch_sub_int(&d->refs, 1);
      stale++;
      return NULL;
   }
   return d;
}

static void release_pooled(void *opaque)
{
   struct device *d = (struct device *)opaque;
   retro_atomic_fetch_sub_int(&d->refs, 1);
}

static void publish_pooled(void)
{
   struct device *d = &pool[pooled++ % POOL];
   retro_atomic_int_init(&d->refs, 1);
   assert(metal_decoder_snapshot_publish(&strict, &strict, d, false, NULL, NULL));
   release_pooled(d);
}

static void replace_while_loading(struct decoder_device_slot *slot)
{
   if (!replacing || slot != &strict.slot)
      return;
   replacing = false;
   publish_pooled();
   assert(metal_decoder_snapshot_withdraw(&strict, &strict, NULL, NULL));
}

static void strict_reader(void *user)
{
   metal_decoder_snapshot_view_t view;
   unsigned i;
   (void)user;
   for (i = 0; i < 20000; i++)
      if (metal_decoder_snapshot_acquire(&strict, &view))
         release_pooled(view.device);
}

/* A device replaced while an acquire is between loading and retaining
 * it stays alive until that acquire is out. */
static unsigned strict_lanes(void)
{
   metal_decoder_snapshot_view_t view;
   sthread_t *one, *two;
   unsigned i, live = 0;
   assert(metal_decoder_snapshot_init(&strict, retain_pooled, release_pooled));
   publish_pooled();
   replacing = true;
   assert(metal_decoder_snapshot_acquire(&strict, &view) || stale);
   if (view.device)
   {
      assert(view.device == &pool[0]
            && retro_atomic_load_acquire_int(&pool[0].refs) == 1);
      release_pooled(view.device);
   }
   assert(retro_atomic_load_acquire_int(&pool[0].refs) == 0
         && retro_atomic_load_acquire_int(&pool[1].refs) == 0);
   if (stale)
      return stale;

   one = sthread_create(strict_reader, NULL);
   two = sthread_create(strict_reader, NULL);
   assert(one && two);
   for (i = 0; i < 20000; i++)
   {
      publish_pooled();
      if (!(i & 15))
         assert(metal_decoder_snapshot_withdraw(&strict, &strict, NULL, NULL));
   }
   sthread_join(one);
   sthread_join(two);
   assert(metal_decoder_snapshot_withdraw(&strict, &strict, NULL, NULL)
         || !metal_decoder_snapshot_is_owner(&strict, &strict));
   for (i = 0; i < POOL; i++)
      live += retro_atomic_load_acquire_int(&pool[i].refs) != 0;
   assert(!live);
   return stale;
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
   sthread_t *one, *two;
   metal_decoder_snapshot_view_t old, current;
   unsigned before;
#ifdef DECODER_DEVICE_SLOT_TEST_RELEASE_EARLY
   /* negative: releasing on replacement hands out a dead device */
   if (!strict_lanes())
   {
      puts("FAIL: a device released early went unnoticed");
      return 1;
   }
   puts("PASS: early release caught");
   return 0;
#else
   if (strict_lanes())
   {
      printf("FAIL: %u retains of a released device\n", stale);
      return 1;
   }
#endif
   retro_atomic_int_init(&first.refs, 1);
   retro_atomic_int_init(&second.refs, 1);
   first.hdr  = false;
   second.hdr = true;
   assert(metal_decoder_snapshot_init(
         &state, retain_device, release_device));
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
   puts("PASS: replaced devices live until the last acquire is out");
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
