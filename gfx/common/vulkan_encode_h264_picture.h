/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_H264_PICTURE_H
#define RARCH_VULKAN_ENCODE_H264_PICTURE_H
#include "../include/vulkan/vulkan.h"
#include <boolean.h>
#include <retro_inline.h>
#include <stdint.h>
#include <string.h>

typedef struct vulkan_encode_h264_picture
{
   VkVideoEncodeH264PictureInfoKHR info;
   VkVideoEncodeH264NaluSliceInfoKHR nalu;
   VkVideoEncodeH264DpbSlotInfoKHR dpb;
   VkVideoEncodeH264GopRemainingFrameInfoKHR remaining;
   StdVideoEncodeH264PictureInfo header;
   StdVideoEncodeH264ReferenceInfo reference;
   StdVideoEncodeH264SliceHeader slice;
   StdVideoEncodeH264ReferenceListsInfo lists;
   unsigned slot, position;
} vulkan_encode_h264_picture_t;

/* Two stable bundles alternate with the DPB. Advance the previous bundle
 * only after successful queue submission; a rejected frame consumes nothing. */
static INLINE bool vulkan_encode_h264_picture(
      vulkan_encode_h264_picture_t *out,
      const vulkan_encode_h264_picture_t *previous,
      unsigned slot, unsigned gop, unsigned qp, bool keyframe)
{
   unsigned i, position;
   uint16_t idr = 0;
   uintptr_t a = (uintptr_t)out, b = (uintptr_t)previous;
   if (!out || slot > 1 || !gop || gop > 65536 || qp > 51 ||
       (previous && (a <= b ? b - a < sizeof(*out) : a - b < sizeof(*previous))))
      return false;
   if (previous)
   {
      if (previous->slot > 1 || previous->slot == slot || previous->position > 65535 ||
          previous->header.pRefLists != &previous->lists ||
          !previous->header.flags.is_reference ||
          previous->header.flags.IdrPicFlag != !previous->position ||
          previous->header.frame_num != (previous->position & 15) ||
          previous->header.PicOrderCnt != (int32_t)(2 * previous->position) ||
          previous->header.primary_pic_type != (previous->position ?
                STD_VIDEO_H264_PICTURE_TYPE_P : STD_VIDEO_H264_PICTURE_TYPE_IDR) ||
          previous->reference.FrameNum != previous->header.frame_num ||
          previous->reference.PicOrderCnt != previous->header.PicOrderCnt ||
          previous->reference.primary_pic_type != previous->header.primary_pic_type)
         return false;
      keyframe = keyframe || previous->position + 1 >= gop;
      idr = (uint16_t)(previous->header.idr_pic_id + (keyframe ? 1 : 0));
   }
   else
      keyframe = true;
   position = keyframe ? 0 : previous->position + 1;
   memset(out, 0, sizeof(*out));
   out->slot = slot;
   out->position = position;
   for (i = 0; i < STD_VIDEO_H264_MAX_NUM_LIST_REF; i++)
      out->lists.RefPicList0[i] = out->lists.RefPicList1[i] = STD_VIDEO_H264_NO_REFERENCE_PICTURE;
   if (!keyframe)
      out->lists.RefPicList0[0] = (uint8_t)previous->slot;
   out->header.flags.IdrPicFlag = keyframe;
   out->header.flags.is_reference = 1;
   out->header.idr_pic_id = idr;
   out->header.primary_pic_type = keyframe ? STD_VIDEO_H264_PICTURE_TYPE_IDR : STD_VIDEO_H264_PICTURE_TYPE_P;
   out->header.frame_num = position & 15;
   out->header.PicOrderCnt = (int32_t)(2 * position);
   out->header.pRefLists = &out->lists;
   out->reference.primary_pic_type = out->header.primary_pic_type;
   out->reference.FrameNum = out->header.frame_num;
   out->reference.PicOrderCnt = out->header.PicOrderCnt;
   out->slice.slice_type = keyframe ? STD_VIDEO_H264_SLICE_TYPE_I : STD_VIDEO_H264_SLICE_TYPE_P;
   out->nalu.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_NALU_SLICE_INFO_KHR;
   out->nalu.pNext = NULL;
   out->nalu.constantQp = (int32_t)qp;
   out->nalu.pStdSliceHeader = &out->slice;
   out->info.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PICTURE_INFO_KHR;
   out->info.pNext = NULL;
   out->info.naluSliceEntryCount = 1;
   out->info.pNaluSliceEntries = &out->nalu;
   out->info.pStdPictureInfo = &out->header;
   out->info.generatePrefixNalu = VK_FALSE;
   out->dpb.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_KHR;
   out->dpb.pNext = NULL;
   out->dpb.pStdReferenceInfo = &out->reference;
   out->remaining.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_GOP_REMAINING_FRAME_INFO_KHR;
   out->remaining.pNext = NULL;
   out->remaining.useGopRemainingFrames = VK_TRUE;
   out->remaining.gopRemainingI = keyframe ? 1 : 0;
   out->remaining.gopRemainingP = keyframe ? gop - 1 : gop - position;
   out->remaining.gopRemainingB = 0;
   return true;
}
#endif
