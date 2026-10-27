/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define BACKEND_MAIN_NAME backend_contract_main
#include "backend_test.c"
#include "../../../gfx/common/metal_vp9_driver.h"
#ifdef METAL_VP9_DRIVER
static const rvp9_hw_backend_t *published;
static unsigned driver_refs = 1;

void rvp9_set_hw_backend(const rvp9_hw_backend_t *offer)
{
   published = offer;
}

int rvp9_unset_hw_backend(const rvp9_hw_backend_t *offer)
{
   if (published != offer)
      return 0;
   published = NULL;
   return 1;
}

static void *driver_retain(void *device)
{
   assert(device == &native_device);
   driver_refs++;
   return device;
}

static void driver_release(void *device)
{
   assert(device == &native_device && driver_refs > 1);
   driver_refs--;
}

int main(void)
{
   static metal_vp9_driver_t driver;
   metal_decoder_snapshot_view_t held, refused;
   rvp9_hw_backend_t foreign;
   retro_atomic_ptr_t current_owner;
   int first, second;
   slock_t *lock = slock_new();
   assert(!backend_contract_main() && lock);
   memset(&foreign, 0, sizeof(foreign));
   retro_atomic_ptr_init(&current_owner, &first);
   assert(metal_vp9_driver_init(&driver, lock, driver_retain, driver_release,
         NULL, NULL, &current_owner));
   assert(metal_vp9_driver_start(&driver, &first, &native_device, false));
   assert(published == &driver.backend.offer && driver_refs == 2);
   assert(metal_decoder_snapshot_acquire(&driver.snapshot, &held));
   assert(driver_refs == 3 && !held.hdr);
   retro_atomic_store_release_ptr(&current_owner, &second);
   assert(!metal_decoder_snapshot_current(&driver.snapshot, held.generation));
   assert(!metal_decoder_snapshot_acquire(&driver.snapshot, &refused));
   assert(!refused.device && driver_refs == 3);
   assert(!metal_vp9_driver_start(&driver, &first, &native_device, false));
   assert(driver_refs == 3);
   assert(metal_vp9_driver_start(&driver, &second, &native_device, true));
   assert(driver_refs == 3);
   assert(!metal_vp9_driver_stop(&driver, &first));
   rvp9_set_hw_backend(&foreign);
   assert(metal_vp9_driver_stop(&driver, &second));
   assert(published == &foreign && driver_refs == 2);
   driver_release(held.device);
   assert(driver_refs == 1 && rvp9_unset_hw_backend(&foreign));
   slock_free(lock);
   puts("PASS: VP9 Metal lifecycle, borrowed setup lock, late renderer refusal, "
        "retained snapshot and foreign offer; host model only");
   return 0;
}
#endif
