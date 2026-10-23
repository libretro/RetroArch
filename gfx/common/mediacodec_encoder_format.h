/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_ENCODER_FORMAT_H
#define MEDIACODEC_ENCODER_FORMAT_H
#include "mediacodec_inventory.h"
#include <string.h>
static int mediacodec_encoder_color_valid(const mediacodec_encoder_request_t *r)
{
   int hevc, av1, vp9;
   if (!r || !r->mime || r->profile <= 0 || r->level <= 0 || r->color > 1
         || (r->range != 1 && r->range != 2))
      return 0;
   hevc = !strcmp(r->mime, "video/hevc");
   av1 = !strcmp(r->mime, "video/av01");
   vp9 = !strcmp(r->mime, "video/x-vnd.on2.vp9");
   if (r->color)
      return (hevc || av1 || vp9) && r->profile == 4096
            && r->hdr_static_size == 25 && !r->hdr_static[0];
   if (r->hdr_static_size)
      return 0;
   return !((hevc || av1) && (r->profile == 4096 || r->profile == 8192))
         && !(vp9
               && (r->profile == 4096 || r->profile == 8192
                     || r->profile == 16384 || r->profile == 32768));
}
#endif
