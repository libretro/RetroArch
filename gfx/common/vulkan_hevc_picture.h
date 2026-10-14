/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_HEVC_PICTURE_H
#define RARCH_VULKAN_HEVC_PICTURE_H
#include <formats/rh265_hw.h>
#include "../include/vk_video/vulkan_video_codec_h265std_decode.h"

static INLINE bool vulkan_hevc_picture(StdVideoDecodeH265PictureInfo *out,
      unsigned vps_id, const rh265_video_params_t *params,
      const rh265_access_unit_t *au, const rh265_hw_picture_t *pic)
{
   const rh265_slice_info_t *h = &au->first;
   unsigned list, i, j;
   memset(out, 0, sizeof(*out));
   if (pic->num_refs > 15 || h->num_negative < 0 || h->num_negative > 16
         || h->num_positive < 0 || h->num_positive > 16 || h->long_term.count > 16
         || h->predictor_delta_pocs < 0 || h->predictor_delta_pocs > 32
         || h->short_term_rps_bits > 65535 || params->log2_max_poc_lsb < 4
         || params->log2_max_poc_lsb > 16 || vps_id > 15) return false;
   out->flags.IrapPicFlag = !!h->irap;
   out->flags.IdrPicFlag = !!h->idr;
   out->flags.IsReference = !!pic->reference;
   out->flags.short_term_ref_pic_set_sps_flag = h->short_term_rps_sps_idx >= 0;
   out->sps_video_parameter_set_id = (uint8_t)vps_id;
   out->pps_seq_parameter_set_id = (uint8_t)params->sps_id;
   out->pps_pic_parameter_set_id = (uint8_t)params->pps_id;
   out->NumDeltaPocsOfRefRpsIdx = (uint8_t)h->predictor_delta_pocs;
   out->NumBitsForSTRefPicSetInSlice = (uint16_t)h->short_term_rps_bits;
   out->PicOrderCntVal = pic->poc;
   memset(out->RefPicSetStCurrBefore, 0xff, sizeof(out->RefPicSetStCurrBefore));
   memset(out->RefPicSetStCurrAfter, 0xff, sizeof(out->RefPicSetStCurrAfter));
   memset(out->RefPicSetLtCurr, 0xff, sizeof(out->RefPicSetLtCurr));
   for (list = 0; list < 2; list++)
   {
      unsigned n = 0, count = (unsigned)(list ? h->num_positive : h->num_negative);
      const int32_t *delta = list ? h->delta_poc_after : h->delta_poc_before;
      const uint8_t *used = list ? h->used_after : h->used_before;
      uint8_t *set = list ? out->RefPicSetStCurrAfter : out->RefPicSetStCurrBefore;
      for (i = 0; i < count; i++)
      {
         int64_t poc = (int64_t)pic->poc + delta[i];
         unsigned found = pic->num_refs;
         if (used[i] > 1) return false;
         if (!used[i]) continue;
         if (n == 8) return false;
         for (j = 0; j < pic->num_refs; j++)
            if (!pic->refs[j].long_term && pic->refs[j].poc == poc)
            {
               if (found != pic->num_refs) return false;
               found = j;
            }
         if (found == pic->num_refs || pic->refs[found].slot > 15) return false;
         set[n++] = (uint8_t)pic->refs[found].slot;
      }
   }
   {
      unsigned n = 0;
      uint32_t max_lsb = UINT32_C(1) << params->log2_max_poc_lsb;
      for (i = 0; i < h->long_term.count; i++)
      {
         int64_t poc = (int64_t)pic->poc - h->poc_lsb + h->long_term.poc_lsb[i]
            - (int64_t)h->long_term.msb_cycle[i] * max_lsb;
         unsigned found = pic->num_refs;
         if (h->long_term.used[i] > 1) return false;
         if (!h->long_term.used[i]) continue;
         if (n == 8) return false;
         for (j = 0; j < pic->num_refs; j++)
            if (pic->refs[j].long_term && (h->long_term.msb_present[i]
                     ? pic->refs[j].poc == poc
                     : ((uint32_t)pic->refs[j].poc & (max_lsb - 1)) == h->long_term.poc_lsb[i]))
            {
               if (found != pic->num_refs) return false;
               found = j;
            }
         if (found == pic->num_refs || pic->refs[found].slot > 15) return false;
         out->RefPicSetLtCurr[n++] = (uint8_t)pic->refs[found].slot;
      }
   }
   return true;
}
#endif
