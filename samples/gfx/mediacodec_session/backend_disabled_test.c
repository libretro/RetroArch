/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <string.h>
#include "../../../gfx/common/mediacodec_hevc_backend.h"
#include "../../../gfx/common/mediacodec_vp9_backend.h"
#include "../../../gfx/common/mediacodec_av1_backend.h"

static JNIEnv *environment(void *user)
{
   (void)user;
   assert(0);
   return NULL;
}

int main(void)
{
   mediacodec_hevc_backend_t hevc;
   mediacodec_vp9_backend_t vp9;
   mediacodec_av1_backend_t av1;
   memset(&hevc, 0, sizeof(hevc));
   memset(&vp9, 0, sizeof(vp9));
   memset(&av1, 0, sizeof(av1));
   assert(!mediacodec_hevc_backend_init(&hevc, environment, NULL, 1024, 1));
   assert(!mediacodec_vp9_backend_init(&vp9, environment, NULL, 1024, 1));
   assert(!mediacodec_av1_backend_init(&av1, environment, NULL, 1024, 1));
   assert(!hevc.offer.open && !vp9.offer.open_color && !av1.offer.open_config);
   assert(!mediacodec_hevc_backend_active(&hevc));
   assert(!mediacodec_vp9_backend_active(&vp9));
   assert(!mediacodec_av1_backend_active(&av1));
   return 0;
}
