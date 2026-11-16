/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_HEVC_PICTURE_H
#define RARCH_VULKAN_ENCODE_HEVC_PICTURE_H
#include "../include/vulkan/vulkan.h"
#include <boolean.h>
#include <retro_inline.h>
#include <stdint.h>
#include <string.h>

typedef struct vulkan_encode_hevc_picture
{
   VkVideoEncodeH265PictureInfoKHR info;
   VkVideoEncodeH265NaluSliceSegmentInfoKHR nalu;
   VkVideoEncodeH265DpbSlotInfoKHR dpb;
   VkVideoEncodeH265GopRemainingFrameInfoKHR remaining;
   StdVideoEncodeH265PictureInfo header;
   StdVideoEncodeH265ReferenceInfo reference;
   StdVideoEncodeH265SliceSegmentHeader slice;
   StdVideoEncodeH265ReferenceListsInfo lists;
   StdVideoH265ShortTermRefPicSet reference_set;
   unsigned slot, position;
} vulkan_encode_hevc_picture_t;

/* Two stable bundles alternate with the DPB. Advance the previous bundle
 * only after successful queue submission; a rejected frame consumes nothing. */
static INLINE bool vulkan_encode_hevc_picture(
      vulkan_encode_hevc_picture_t *out,
      const vulkan_encode_hevc_picture_t *previous,
      unsigned slot, unsigned gop, unsigned qp, bool keyframe)
{
   unsigned i, position;
   uintptr_t a = (uintptr_t)out, b = (uintptr_t)previous;
   if (!out || slot > 1 || !gop || gop > 65536 || qp > 51 ||
       (previous && (a <= b ? b - a < sizeof(*out) : a - b < sizeof(*previous))))
      return false;
   if (previous)
   {
      if (previous->slot > 1 || previous->slot == slot || previous->position > 65535 ||
          previous->header.pRefLists != &previous->lists ||
          previous->header.pShortTermRefPicSet != (previous->position ?
                &previous->reference_set : NULL) ||
          previous->header.flags.short_term_ref_pic_set_sps_flag != !!previous->position ||
          !previous->header.flags.is_reference ||
          !previous->header.flags.pic_output_flag ||
          previous->header.flags.IrapPicFlag != !previous->position ||
          previous->header.PicOrderCntVal != (int32_t)previous->position ||
          previous->header.pic_type != (previous->position ?
                STD_VIDEO_H265_PICTURE_TYPE_P : STD_VIDEO_H265_PICTURE_TYPE_IDR) ||
          previous->reference.PicOrderCntVal != previous->header.PicOrderCntVal ||
          previous->reference.pic_type != previous->header.pic_type)
         return false;
      keyframe = keyframe || previous->position + 1 >= gop;
   }
   else
      keyframe = true;
   position = keyframe ? 0 : previous->position + 1;
   memset(out, 0, sizeof(*out));
   out->slot = slot;
   out->position = position;
   for (i = 0; i < STD_VIDEO_H265_MAX_NUM_LIST_REF; i++)
      out->lists.RefPicList0[i] = out->lists.RefPicList1[i] = STD_VIDEO_H265_NO_REFERENCE_PICTURE;
   if (!keyframe)
      out->lists.RefPicList0[0] = (uint8_t)previous->slot;
   out->header.flags.IrapPicFlag = keyframe;
   out->header.flags.is_reference = 1;
   out->header.pic_type = keyframe ? STD_VIDEO_H265_PICTURE_TYPE_IDR : STD_VIDEO_H265_PICTURE_TYPE_P;
   out->header.PicOrderCntVal = (int32_t)position;
   out->header.flags.pic_output_flag = 1;
   out->header.flags.no_output_of_prior_pics_flag = keyframe;
   out->header.flags.short_term_ref_pic_set_sps_flag = !keyframe;
   out->reference_set.num_negative_pics = 1;
   out->reference_set.used_by_curr_pic_s0_flag = 1;
   out->header.pShortTermRefPicSet = keyframe ? NULL : &out->reference_set;
   out->header.pRefLists = &out->lists;
   out->reference.pic_type = out->header.pic_type;
   out->reference.PicOrderCntVal = out->header.PicOrderCntVal;
   out->slice.slice_type = keyframe ? STD_VIDEO_H265_SLICE_TYPE_I : STD_VIDEO_H265_SLICE_TYPE_P;
   out->slice.flags.first_slice_segment_in_pic_flag = 1;
   out->slice.flags.slice_loop_filter_across_slices_enabled_flag = 1;
   out->slice.MaxNumMergeCand = 5;
   out->nalu.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_NALU_SLICE_SEGMENT_INFO_KHR;
   out->nalu.pNext = NULL;
   out->nalu.constantQp = (int32_t)qp;
   out->nalu.pStdSliceSegmentHeader = &out->slice;
   out->info.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PICTURE_INFO_KHR;
   out->info.pNext = NULL;
   out->info.naluSliceSegmentEntryCount = 1;
   out->info.pNaluSliceSegmentEntries = &out->nalu;
   out->info.pStdPictureInfo = &out->header;
   out->dpb.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_DPB_SLOT_INFO_KHR;
   out->dpb.pNext = NULL;
   out->dpb.pStdReferenceInfo = &out->reference;
   out->remaining.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_GOP_REMAINING_FRAME_INFO_KHR;
   out->remaining.pNext = NULL;
   out->remaining.useGopRemainingFrames = VK_TRUE;
   out->remaining.gopRemainingI = keyframe ? 1 : 0;
   out->remaining.gopRemainingP = keyframe ? gop - 1 : gop - position;
   out->remaining.gopRemainingB = 0;
   return true;
}
#endif
