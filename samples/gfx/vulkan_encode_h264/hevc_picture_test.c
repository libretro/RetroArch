/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vulkan_encode_hevc_picture.h"
static vulkan_encode_hevc_picture_t pictures[2], saved;
static void refused(unsigned slot, unsigned gop, unsigned qp)
{
   memcpy(&saved, &pictures[0], sizeof(saved));
   assert(!vulkan_encode_hevc_picture(&pictures[0], &pictures[1], slot, gop, qp, false));
   assert(!memcmp(&saved, &pictures[0], sizeof(saved)));
}
int main(void)
{
   unsigned i, position = 0;
   const vulkan_encode_hevc_picture_t *previous = NULL;
   for (i = 0; i < 96; i++)
   {
      vulkan_encode_hevc_picture_t *current = &pictures[i % 2];
      bool force = i == 13 || i == 57;
      if (!previous || force || position + 1 >= 32)
      {
         position = 0;
      }
      else
         position++;
      assert(vulkan_encode_hevc_picture(current, previous, i % 2, 32, 12, force));
      assert(current->position == position);
      assert(current->header.flags.IrapPicFlag == !position);
      assert(current->header.PicOrderCntVal == (int32_t)position);
      assert(current->header.flags.pic_output_flag);
      assert(current->header.flags.short_term_ref_pic_set_sps_flag == !!position);
      assert(current->header.pShortTermRefPicSet == (position ? &current->reference_set : NULL));
      assert(current->slice.MaxNumMergeCand == 5);
      assert(current->info.pStdPictureInfo == &current->header);
      assert(current->dpb.pStdReferenceInfo == &current->reference);
      assert(current->remaining.gopRemainingI + current->remaining.gopRemainingP ==
            (position ? 32 - position : 32));
      assert(current->lists.RefPicList0[0] == (position ? (i + 1) % 2 : STD_VIDEO_H265_NO_REFERENCE_PICTURE));
      previous = current;
   }
   assert(vulkan_encode_hevc_picture(&pictures[1], NULL, 1, 1, 0, false));
   assert(vulkan_encode_hevc_picture(&pictures[0], &pictures[1], 0, 1, 51, false));
   assert(pictures[0].header.flags.IrapPicFlag);
   refused(2, 32, 12); refused(0, 0, 12); refused(0, 65537, 12); refused(0, 32, 52);
   memcpy(&saved, &pictures[0], sizeof(saved));
   assert(!vulkan_encode_hevc_picture(&pictures[0], &pictures[0], 0, 32, 12, false));
   assert(!memcmp(&saved, &pictures[0], sizeof(saved)));
   pictures[1].header.PicOrderCntVal = 3;
   refused(0, 32, 12);
   previous = NULL;
   for (i = 0; i < 514; i++)
   {
      vulkan_encode_hevc_picture_t *current = &pictures[i % 2];
      assert(vulkan_encode_hevc_picture(current, previous, i % 2, 512, 22, false));
      assert(current->header.PicOrderCntVal == (int32_t)(i % 512));
      previous = current;
   }
   puts("Vulkan HEVC picture GOP/forced IDR/reference and borrow validity PASS");
   return 0;
}
