/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vulkan_encode_h264_picture.h"
static vulkan_encode_h264_picture_t pictures[2], saved;
static void refused(unsigned slot, unsigned gop, unsigned qp)
{
   memcpy(&saved, &pictures[0], sizeof(saved));
   assert(!vulkan_encode_h264_picture(&pictures[0], &pictures[1], slot, gop, qp, false));
   assert(!memcmp(&saved, &pictures[0], sizeof(saved)));
}
int main(void)
{
   unsigned i, position = 0, keys = 0;
   const vulkan_encode_h264_picture_t *previous = NULL;
   for (i = 0; i < 96; i++)
   {
      vulkan_encode_h264_picture_t *current = &pictures[i % 2];
      bool force = i == 13 || i == 57;
      if (!previous || force || position + 1 >= 32)
      {
         position = 0;
         keys++;
      }
      else
         position++;
      assert(vulkan_encode_h264_picture(current, previous, i % 2, 32, 12, force));
      assert(current->position == position);
      assert(current->header.flags.IdrPicFlag == !position);
      assert(current->header.frame_num == (position & 15));
      assert(current->header.PicOrderCnt == (int32_t)(2 * position));
      assert(current->header.idr_pic_id == keys - 1);
      assert(current->info.pStdPictureInfo == &current->header);
      assert(current->dpb.pStdReferenceInfo == &current->reference);
      assert(current->remaining.gopRemainingI + current->remaining.gopRemainingP ==
            (position ? 32 - position : 32));
      assert(current->lists.RefPicList0[0] == (position ? (i + 1) % 2 : STD_VIDEO_H264_NO_REFERENCE_PICTURE));
      previous = current;
   }
   assert(vulkan_encode_h264_picture(&pictures[1], NULL, 1, 1, 0, false));
   assert(vulkan_encode_h264_picture(&pictures[0], &pictures[1], 0, 1, 51, false));
   assert(pictures[0].header.flags.IdrPicFlag && pictures[0].header.idr_pic_id == 1);
   pictures[1].header.idr_pic_id = 65535;
   assert(vulkan_encode_h264_picture(&pictures[0], &pictures[1], 0, 1, 12, false));
   assert(!pictures[0].header.idr_pic_id);
   refused(2, 32, 12); refused(0, 0, 12); refused(0, 65537, 12); refused(0, 32, 52);
   memcpy(&saved, &pictures[0], sizeof(saved));
   assert(!vulkan_encode_h264_picture(&pictures[0], &pictures[0], 0, 32, 12, false));
   assert(!memcmp(&saved, &pictures[0], sizeof(saved)));
   pictures[1].header.frame_num = 3;
   refused(0, 32, 12);
   puts("Vulkan H264 picture GOP/wrap/forced IDR/borrow validity PASS");
   return 0;
}
