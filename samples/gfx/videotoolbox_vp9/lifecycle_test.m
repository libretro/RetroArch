/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/metal_hevc_driver.h"

static bool metal_hevc_decode_enabled(void *user)
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
   rh265_hw_backend_t hevc_foreign;
#ifdef METAL_VP9_DRIVER
   rvp9_hw_backend_t foreign;
   metal_decoder_snapshot_view_t held;
   memset(&foreign, 0, sizeof(foreign));
#endif
   memset(&hevc_foreign, 0, sizeof(hevc_foreign));
   assert(!metal_hevc_start_supported(NULL, device, device, false));
   assert(!metal_hevc_start_supported(&first, NULL, device, false));
   assert(!metal_hevc_start_supported(&first, device, NULL, false));
   assert(metal_hevc_start_supported(&first, device, device, false));
   assert(metal_hevc_owns(&first));
#ifdef METAL_VP9_DRIVER
   assert(metal_decoder_snapshot_acquire(&metal_hevc_vp9.snapshot, &held));
   assert(held.device == device && !held.hdr);
   assert(rvp9_unset_hw_backend(&metal_hevc_vp9.backend.offer));
#endif
   assert(metal_hevc_start_supported(&second, device, device, true));
#ifdef METAL_VP9_DRIVER
   assert(!metal_decoder_snapshot_current(&metal_hevc_vp9.snapshot,
         held.generation));
#endif
   metal_hevc_stop(&first);
   assert(metal_hevc_owns(&second));
#ifdef METAL_VP9_DRIVER
   assert(rvp9_unset_hw_backend(&metal_hevc_vp9.backend.offer));
#endif
   assert(metal_hevc_start_supported(&second, device, device, true));
#ifdef METAL_VP9_DRIVER
   rvp9_set_hw_backend(&foreign);
#endif
   rh265_set_hw_backend(&hevc_foreign);
   metal_hevc_stop(&second);
   assert(!metal_hevc_owns(&second));
#ifdef METAL_VP9_DRIVER
   assert(!metal_decoder_snapshot_current(&metal_hevc_vp9.snapshot,
         held.generation));
   assert(CFEqual(held.device, CFSTR("lifecycle device")));
   metal_hevc_release(held.device);
   assert(rvp9_unset_hw_backend(&foreign));
#endif
   assert(rh265_unset_hw_backend(&hevc_foreign));
   puts("PASS: native Metal HEVC/VP9 lifecycle, retained snapshots, stale owner "
        "and foreign offer withdrawal; no GPU decode");
   return 0;
}
