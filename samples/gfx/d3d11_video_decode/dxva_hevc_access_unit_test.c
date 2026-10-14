/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/dxva_hevc_rh265.h"
#include "dxva_hevc_access_unit_data.h"

static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)

static void check_picture(const uint8_t *config, size_t config_size,
      const uint8_t *params, size_t params_size, const uint8_t *sample,
      size_t sample_size, int depth)
{
   static rh265_video_params_t metadata;
   rh265_access_unit_t au, saved;
   rh265_nal_t slices[4];
   uint8_t packed[512], records[40], expected[DXVA_HEVC_PP_SIZE];
   dxva_hevc_picture_t picture;
   size_t written;
   int ret, plane;
   rh265_video *original = rh265_video_open(), *converted = rh265_video_open();
   CHECK(original && converted);
   if (!original || !converted) goto done;
   CHECK(!rh265_video_set_extradata(original, config, config_size));
   CHECK(!rh265_video_get_access_unit(original, sample, sample_size, 4, slices, 4, &au));
   CHECK(au.num_slices == 1 && au.intra && au.first.idr && au.first.first_slice_in_pic);
   CHECK(slices[0].data == sample + 4 && slices[0].size == sample_size - 4);
   saved = au;
   CHECK(!rh265_video_get_access_unit(original, sample, sample_size, 4, slices, 4, &au));
   CHECK(!memcmp(&saved, &au, sizeof(au)));
   CHECK(!rh265_video_get_params(original, (unsigned)au.first.pps_id, &metadata));
   CHECK(metadata.bit_depth == depth && metadata.width == 64 && metadata.height == 64);
   memset(&picture, 0, sizeof(picture));
   CHECK(dxva_hevc_params_from_rh265(&picture, &metadata));
   CHECK(dxva_hevc_slice_from_rh265(&picture, &au.first));
   CHECK(dxva_hevc_pic_params(expected, &picture, 1));
   CHECK(dxva_hevc_pack_slices(packed, sizeof(packed), records, sizeof(records),
            slices, au.num_slices, &written));
   CHECK(written <= sizeof(packed) && !(written & 127));
   CHECK(!memcmp(packed + 3, sample + 4, sample_size - 4));
   CHECK(!rh265_video_plane(original, 0, NULL, NULL, NULL));
   CHECK(!rh265_video_plane(converted, 0, NULL, NULL, NULL));
   ret = rh265_video_decode(original, sample, sample_size);
   CHECK(ret >= 0);
   if (!ret) CHECK(!rh265_video_drain(original));
   CHECK(!rh265_video_decode(converted, params, params_size));
   ret = rh265_video_decode(converted, packed, written);
   CHECK(ret >= 0);
   if (!ret) CHECK(!rh265_video_drain(converted));
   CHECK(rh265_video_bit_depth(original) == depth && rh265_video_bit_depth(converted) == depth);
   for (plane = 0; plane < 3; plane++)
   {
      int as, aw, ah, bs, bw, bh, row, bps = depth == 10 ? 2 : 1;
      const uint8_t *a = rh265_video_plane(original, plane, &as, &aw, &ah);
      const uint8_t *b = rh265_video_plane(converted, plane, &bs, &bw, &bh);
      CHECK(a && b);
      if (!a || !b) continue;
      CHECK(aw == bw && ah == bh && aw == (plane ? 32 : 64));
      if (aw != bw || ah != bh) continue;
      for (row = 0; row < ah; row++)
         CHECK(!memcmp(a + (size_t)row * as * bps, b + (size_t)row * bs * bps, (size_t)aw * bps));
   }
done:
   rh265_video_close(original); rh265_video_close(converted);
}

int main(void)
{
   check_picture(hevc_config8, sizeof(hevc_config8), hevc_params8, sizeof(hevc_params8),
         hevc_sample8, sizeof(hevc_sample8), 8);
   check_picture(hevc_config10, sizeof(hevc_config10), hevc_params10, sizeof(hevc_params10),
         hevc_sample10, sizeof(hevc_sample10), 10);
   printf("HEVC access-unit pipeline: %u failures\n", failures);
   return failures ? 1 : 0;
}
