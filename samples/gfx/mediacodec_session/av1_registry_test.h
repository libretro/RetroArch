/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "../../../gfx/common/mediacodec_av1_registry.h"
static retro_atomic_ptr_t registered_offer;

void rav1_set_hw_backend(const rav1_hw_backend_t *offer)
{
   retro_atomic_store_release_ptr(&registered_offer, (void *)offer);
}

int rav1_unset_hw_backend(const rav1_hw_backend_t *offer)
{
   return offer && retro_atomic_cas_ptr(&registered_offer, (void *)offer, NULL);
}

static mediacodec_av1_registry_t registry;
static rav1_hw_ops_t registry_ops;
static bool registry_result;

static void *registry_open_thread(void *opaque)
{
   (void)opaque;
   registry_result = registry.offer.open_config(
         &registry, &sequence, &sequence_obu, NULL, 9, &registry_ops);
   return NULL;
}

static void test_registry(void)
{
   rav1_hw_backend_t saved, foreign;
   pthread_t reader;
   unsigned i;
   int first, second;
   retro_atomic_ptr_init(&registered_offer, NULL);
   assert(mediacodec_av1_registry_init(
         &registry, environment, &native_opens, 1048576, 2));
   assert(!registry.offer.open_config(
         &registry, &sequence, &sequence_obu, NULL, 9, &registry_ops));
   assert(mediacodec_av1_registry_publish(&registry, &first));
   saved = registry.offer;
   assert(saved.open_config(
         saved.user, &sequence, &sequence_obu, NULL, 9, &registry_ops));
   assert(!mediacodec_av1_registry_withdraw(&registry, &second));
   assert(mediacodec_av1_registry_withdraw(&registry, &first));
   saved.close(&registry_ops);
   wait_idle(&registry.backend);
   assert(!saved.open_config(
         saved.user, &sequence, &sequence_obu, NULL, 9, &registry_ops));
   for (i = 0; i < 20; i++)
   {
      assert(mediacodec_av1_registry_publish(&registry, &first));
      retro_atomic_store_release_int(&open_entered, 0);
      retro_atomic_store_release_int(&open_hold, 1);
      assert(!pthread_create(&reader, NULL, registry_open_thread, NULL));
      while (!retro_atomic_load_acquire_int(&open_entered))
         retro_sleep(1);
      assert(mediacodec_av1_registry_publish(&registry, &second));
      retro_atomic_store_release_int(&open_hold, 0);
      assert(!pthread_join(reader, NULL));
      assert(!registry_result && !registry_ops.decoder);
      wait_idle(&registry.backend);
      assert(!mediacodec_av1_registry_withdraw(&registry, &first));
      memset(&foreign, 0, sizeof(foreign));
      rav1_set_hw_backend(&foreign);
      assert(mediacodec_av1_registry_withdraw(&registry, &second));
      assert(retro_atomic_load_acquire_ptr(&registered_offer) == &foreign);
   }
   retro_atomic_store_release_int(&registry.epoch, INT_MAX - 2);
   assert(!mediacodec_av1_registry_publish(&registry, &first));
   assert(!mediacodec_av1_registry_publish(&registry, &first));
   rav1_set_hw_backend(NULL);
}
