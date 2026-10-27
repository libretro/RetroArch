/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "../../../gfx/common/mediacodec_vp9_registry.h"
static retro_atomic_ptr_t registered_offer;

void rvp9_set_hw_backend(const rvp9_hw_backend_t *offer)
{
   retro_atomic_store_release_ptr(&registered_offer, (void *)offer);
}

int rvp9_unset_hw_backend(const rvp9_hw_backend_t *offer)
{
   return offer && retro_atomic_cas_ptr(&registered_offer, (void *)offer, NULL);
}

typedef struct registry_test_open
{
   mediacodec_vp9_registry_t *registry;
   const rvp9_hdr *header;
   rvp9_hw_ops_t ops;
   bool result;
} registry_test_open_t;

static void *registry_test_reader(void *opaque)
{
   registry_test_open_t *work = (registry_test_open_t *)opaque;
   work->result = work->registry->offer.open_color(
         work->registry, work->header, NULL, 10, &work->ops);
   return NULL;
}

static void test_registry(void)
{
   static mediacodec_vp9_registry_t registry;
   static rvp9_hdr header;
   rvp9_hw_ops_t ops;
   rvp9_hw_backend_t saved, foreign;
   video_color_metadata_t color;
   static registry_test_open_t work;
   pthread_t reader;
   unsigned i, j;
   int first, second, opens;
   retro_atomic_ptr_init(&registered_offer, NULL);
   assert(mediacodec_vp9_registry_init(
         &registry, environment, &native_opens, 1048576, 2));
   memset(&header, 0, sizeof(header));
   header.profile = 2;
   header.bit_depth = 10;
   header.subsampling_x = header.subsampling_y = 1;
   header.w = 64;
   header.h = 32;
   header.color_space = 2;
   assert(!mediacodec_vp9_registry_publish(&registry, NULL));
   assert(!registry.offer.open_color(&registry, &header, NULL, 10, &ops));
   assert(mediacodec_vp9_registry_publish(&registry, &first));
   saved = registry.offer;
   header.profile = 2;
   header.bit_depth = 10;
   memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 1;
   color.transfer = 18;
   color.range = 1;
   assert(saved.open_color(saved.user, &header, &color, 10, &ops));
   assert(opened_color.transfer == 18 && opened_color.range == 1);
   assert(!mediacodec_vp9_registry_withdraw(&registry, &second));
   assert(mediacodec_vp9_registry_withdraw(&registry, &first));
   assert(ops.decoder);
   saved.close(&ops);
   wait_idle(&registry.backend);
   assert(!saved.open_color(saved.user, &header, NULL, 10, &ops));
   assert(mediacodec_vp9_registry_publish(&registry, &first));
   assert(mediacodec_vp9_registry_publish(&registry, &second));
   assert(!mediacodec_vp9_registry_withdraw(&registry, &first));
   memset(&foreign, 0, sizeof(foreign));
   rvp9_set_hw_backend(&foreign);
   assert(mediacodec_vp9_registry_withdraw(&registry, &second));
   assert(retro_atomic_load_acquire_ptr(&registered_offer) == &foreign);
   rvp9_set_hw_backend(NULL);
   for (i = 0; i < 20; i++)
   {
      assert(mediacodec_vp9_registry_publish(&registry, &first));
      memset(&work, 0, sizeof(work));
      work.registry = &registry;
      work.header = &header;
      retro_atomic_store_release_int(&open_entered, 0);
      retro_atomic_store_release_int(&open_hold, 1);
      assert(!pthread_create(&reader, NULL, registry_test_reader, &work));
      for (j = 0; j < 2000 && !retro_atomic_load_acquire_int(&open_entered);
            j++)
         retro_sleep(1);
      assert(retro_atomic_load_acquire_int(&open_entered));
      if (i & 1)
         assert(mediacodec_vp9_registry_publish(&registry, &first));
      else
      {
         assert(mediacodec_vp9_registry_withdraw(&registry, &first));
         assert(mediacodec_vp9_registry_publish(&registry, &second));
      }
      retro_atomic_store_release_int(&open_hold, 0);
      assert(!pthread_join(reader, NULL));
      assert(!work.result && !work.ops.decoder);
      wait_idle(&registry.backend);
      assert(mediacodec_vp9_registry_withdraw(
            &registry, i & 1 ? &first : &second));
   }
   retro_atomic_store_release_int(&registry.epoch, INT_MAX - 2);
   opens = native_opens;
   assert(!mediacodec_vp9_registry_publish(&registry, &first));
   assert(!mediacodec_vp9_registry_publish(&registry, &second));
   assert(!saved.open_color(saved.user, &header, NULL, 10, &ops)
         && native_opens == opens);
   assert(!retro_atomic_load_acquire_ptr(&registered_offer));
   puts("PASS: Android registry stable offers, stale/foreign withdrawal, 20 "
        "overlapping native opens and generation exhaustion");
}
