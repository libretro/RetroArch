/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/metal_video_decoder_driver.h"

static bool metal_video_decoder_enabled(void *user)
{
   (void)user;
   return false;
}

metal_frame_cv_pool_t *metal_frame_cv_pool_create(void *device, unsigned count)
{
   (void)device;
   (void)count;
   assert(0 && "lifecycle test must not open a decoder");
   return NULL;
}

void metal_frame_cv_pool_free(metal_frame_cv_pool_t *pool)
{
   (void)pool;
   assert(0 && "lifecycle test must not own a pool");
}

int metal_frame_cv_pool_export(metal_frame_cv_pool_t *pool,
      const metal_frame_view_t *view, void *image, video_hw_frame_t *out)
{
   (void)pool;
   (void)view;
   (void)image;
   (void)out;
   assert(0 && "lifecycle test must not export images");
   return -1;
}

int main(void)
{
   int first, second;
   void *device = (void *)CFSTR("lifecycle device");
   metal_decoder_snapshot_t *snapshot;
   metal_decoder_snapshot_view_t held;
#ifdef HAVE_RVP9
   rvp9_hw_backend_t foreign;
#endif
#ifdef HAVE_RMP4
   rh265_hw_backend_t hevc_foreign;
#endif
#ifdef HAVE_RVP9
   memset(&foreign, 0, sizeof(foreign));
#endif
#ifdef HAVE_RMP4
   memset(&hevc_foreign, 0, sizeof(hevc_foreign));
#endif
   assert(!metal_video_decoder_start_supported(NULL, device, device, false));
   assert(!metal_video_decoder_start_supported(&first, NULL, device, false));
   assert(!metal_video_decoder_start_supported(&first, device, NULL, false));
   assert(metal_video_decoder_start_supported(&first, device, device, false));
   assert(metal_video_decoder_owns(&first));
   snapshot = (metal_decoder_snapshot_t *)retro_atomic_load_acquire_ptr(
         &metal_video_decoder_active);
   assert(metal_decoder_snapshot_acquire(snapshot, &held));
   assert(held.device == device && !held.hdr);
#ifdef HAVE_RVP9
   assert(rvp9_unset_hw_backend(&metal_video_decoder_vp9.offer));
#endif
#ifdef HAVE_RMP4
   assert(rh265_unset_hw_backend(&metal_video_decoder_hevc.backend.offer));
#endif
   assert(metal_video_decoder_start_supported(&second, device, device, true));
   assert(!metal_decoder_snapshot_current(snapshot, held.generation));
   metal_video_decoder_stop(&first);
   assert(metal_video_decoder_owns(&second));
#ifdef HAVE_RVP9
   assert(rvp9_unset_hw_backend(&metal_video_decoder_vp9.offer));
#endif
#ifdef HAVE_RMP4
   assert(rh265_unset_hw_backend(&metal_video_decoder_hevc.backend.offer));
#endif
   assert(metal_video_decoder_start_supported(&second, device, device, true));
#ifdef HAVE_RVP9
   rvp9_set_hw_backend(&foreign);
#endif
#ifdef HAVE_RMP4
   rh265_set_hw_backend(&hevc_foreign);
#endif
   metal_video_decoder_stop(&second);
   assert(!metal_video_decoder_owns(&second));
   assert(!metal_decoder_snapshot_current(snapshot, held.generation));
   assert(CFEqual(held.device, CFSTR("lifecycle device")));
   metal_video_decoder_release(held.device);
#ifdef HAVE_RVP9
   assert(rvp9_unset_hw_backend(&foreign));
#endif
#ifdef HAVE_RMP4
   assert(rh265_unset_hw_backend(&hevc_foreign));
#endif
   puts("PASS: native Metal codec lifecycle, retained snapshots, stale owner and foreign offer withdrawal; no GPU decode");
   return 0;
}
