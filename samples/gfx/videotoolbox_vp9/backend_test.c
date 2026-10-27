/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define DISPATCH_MAIN_NAME dispatch_contract_main
#include "dispatch_test.c"
#ifdef VIDEOTOOLBOX_VP9_OWNER
static int native_device, device_refs, setting = 1, acquisition_fail, pool_fail;
static unsigned acquisitions, pools;
static uintptr_t generation = 1;
static bool display_hdr, replace_during_pool;

static bool acquire(void *user, videotoolbox_vp9_device_t *snapshot)
{
   assert(user == &native_device);
   acquisitions++;
   if (acquisition_fail)
      return false;
   snapshot->device     = &native_device;
   snapshot->generation = generation;
   snapshot->hdr        = display_hdr;
   device_refs++;
   return true;
}

static void release(void *device)
{
   assert(device == &native_device && device_refs == 1);
   device_refs--;
}

static bool current(void *user, uintptr_t epoch)
{
   assert(user == &native_device);
   return epoch == generation;
}

static bool enabled(void *user)
{
   assert(user == &native_device);
   return setting != 0;
}

metal_frame_cv_pool_t *metal_frame_cv_pool_create(void *device, unsigned count)
{
   assert(device == &native_device && device_refs == 1 && count == 32);
   pools++;
   if (replace_during_pool)
      generation++;
   if (pool_fail)
      return NULL;
   pool.freed = 0;
   return &pool;
}

static void finish_close(videotoolbox_vp9_backend_t *b, rvp9_hw_ops_t *ops)
{
   b->offer.close(ops);
   assert(!ops->decoder && !device_refs);
   turn(false);
   turn(false);
   assert(!dispatch_objects && !retro_atomic_load_acquire_int(&objects));
}

#ifndef BACKEND_MAIN_NAME
#define BACKEND_MAIN_NAME main
#endif
int BACKEND_MAIN_NAME(void)
{
   videotoolbox_vp9_owner_t *init = open_owner();
   videotoolbox_vp9_backend_t b;
   rvp9_hw_ops_t ops;
   rvp9_config_t record;
   static const uint8_t bytes[12] = {1, 0, 0, 0, 0, 0, 0x80, 1, 1, 1, 0, 0};
   unsigned before;
   close_owner(&init);
   assert(videotoolbox_vp9_backend_init(
         &b, &native_device, acquire, release, current, enabled, NULL, 64, 1));
   assert(b.offer.packet_decoder && b.offer.open && b.offer.drain
          && b.offer.close);
   setting = 0;
   assert(!b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(!acquisitions && !ops.decoder);
   setting          = 1;
   acquisition_fail = 1;
   assert(!b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(!device_refs && !pools);
   acquisition_fail = 0;
   color.transfer   = 0;
   assert(!b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(!device_refs && !pools);
   color.transfer = 1;
   pool_fail      = 1;
   assert(!b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(!device_refs && !ops.decoder && !dispatch_objects);
   pool_fail     = 0;
   session_error = -404;
   assert(!b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(!ops.decoder && !device_refs && pool.freed);
   turn(false);
   turn(false);
   assert(!dispatch_objects && !retro_atomic_load_acquire_int(&objects));
   session_error = 0;
   assert(b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(!device_refs && ops.decoder);
   finish_close(&b, &ops);
   replace_during_pool = true;
   assert(!b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(!ops.decoder && !device_refs && pool.freed);
   turn(false);
   turn(false);
   assert(!dispatch_objects && !retro_atomic_load_acquire_int(&objects));
   replace_during_pool = false;
   assert(rvp9_config_parse(bytes, sizeof(bytes), &record));
   assert(b.offer.open_config(b.offer.user, &header, &color, &record, 10, &ops));
   assert(recorded_configuration[6] == 0x80);
   finish_close(&b, &ops);
   before = pools;
   record.profile = 2;
   assert(!b.offer.open_config(b.offer.user, &header, &color, &record, 10, &ops));
   assert(pools == before && !device_refs);
   record.profile = 0;
   record.transfer = 16;
   assert(!b.offer.open_config(b.offer.user, &header, &color, &record, 10, &ops));
   assert(pools == before && !device_refs);
   header.profile      = 2;
   header.bit_depth    = 10;
   header.color_space  = 5;
   color.transfer      = 16;
   expected_pixel      = 0x78343230;
   before              = pools;
   assert(!b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   assert(pools == before && !device_refs);
   display_hdr = true;
   assert(b.offer.open_color(b.offer.user, &header, &color, 10, &ops));
   finish_close(&b, &ops);
   assert(!videotoolbox_vp9_backend_init(
         &b, &native_device, acquire, release, current, enabled, NULL, 64, 2));
   puts("PASS: VP9 backend policy, owned capability snapshots, setup failures, HDR refusal and stale-generation retirement; host model only");
   return 0;
}
#endif
