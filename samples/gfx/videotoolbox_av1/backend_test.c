/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define DISPATCH_MAIN_NAME dispatch_contract_main
#include "dispatch_test.c"
#include "../../../gfx/common/videotoolbox_av1_backend.h"
#ifdef VIDEOTOOLBOX_AV1_OWNER
static int native_device, device_refs, setting = 1, acquisition_fail, pool_fail;
static unsigned acquisitions, pools;
static uintptr_t generation = 1;
static bool display_hdr, replace_during_pool;

static bool acquire(void *user, videotoolbox_av1_device_t *snapshot)
{
   assert(user == &native_device);
   acquisitions++;
   if (acquisition_fail)
      return false;
   snapshot->device = &native_device;
   snapshot->generation = generation;
   snapshot->hdr = display_hdr;
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

static void finish_close(videotoolbox_av1_backend_t *b, rav1_hw_ops_t *ops)
{
   b->offer.close(ops);
   assert(!ops->decoder && !device_refs);
   turn(false);
   turn(false);
   assert(!dispatch_objects && !retro_atomic_load_acquire_int(&objects));
}

int main(void)
{
   videotoolbox_av1_owner_t *init = open_owner();
   videotoolbox_av1_backend_t b;
   rav1_hw_ops_t ops;
   static uint8_t bytes[128];
   rav1_obu_t sequence;
   size_t offset = 0;
   close_owner(&init);
   bytes[0] = 0x0a;
   bytes[1] = (uint8_t)sizeof(av1_sequence_8_normal);
   memcpy(bytes + 2, av1_sequence_8_normal, sizeof(av1_sequence_8_normal));
   assert(rav1_obu_next(&sequence, bytes, sizeof(av1_sequence_8_normal) + 2,
                &offset, false)
         == 1);
   assert(videotoolbox_av1_backend_init(
         &b, &native_device, acquire, release, current, enabled, NULL, 64));
   assert(b.offer.packet_decoder && b.offer.open_config && b.offer.close
         && b.offer.drain);
   setting = 0;
   assert(!b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   assert(!acquisitions && !ops.decoder);
   setting = 1;
   acquisition_fail = 1;
   assert(!b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   assert(!device_refs && !pools);
   acquisition_fail = 0;
   pool_fail = 1;
   assert(!b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   assert(!device_refs && !ops.decoder);
   pool_fail = 0;
   assert(b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   assert(((videotoolbox_av1_decoder_t *)ops.decoder)->owner->count == 10);
   finish_close(&b, &ops);
   replace_during_pool = true;
   assert(!b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   assert(!device_refs && !ops.decoder && pool.freed);
   turn(false);
   turn(false);
   assert(!dispatch_objects && !retro_atomic_load_acquire_int(&objects));
   replace_during_pool = false;
   color.primaries = color.matrix = 9;
   color.transfer = 16;
   assert(!b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   assert(!device_refs && !ops.decoder);
   display_hdr = true;
   assert(b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   finish_close(&b, &ops);
   header.max_width++;
   assert(!b.offer.open_config(
         b.offer.user, &header, &sequence, &color, 9, &ops));
   assert(!device_refs && !ops.decoder);
   puts("PASS: AV1 backend snapshot admission, bounded setup, HDR policy and "
        "renderer replacement");
   return 0;
}
#endif
