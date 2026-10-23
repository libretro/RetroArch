/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_vulkan_lease.h"

bool vaapi_hevc_vulkan_lease_bind(vaapi_hevc_vulkan_lease_t *l,
      vaapi_hevc_vulkan_submission_t *e, bool fp16)
{
   if (!l || l->entry || l->owner || l->returned.active || l->broken || !e
         || !e->frame || !e->frame->active || e->frame->source.active
         || e->frame->pending || e->frame->broken)
      return false;
   l->entry = e;
   l->fp16  = fp16;
   return true;
}

static bool vaapi_hevc_vulkan_lease_put(
      vaapi_hevc_vulkan_lease_t *l, bool finish)
{
   bool result;
   if (!l->returned.active
         && !vaapi_hevc_vulkan_frame_detach(l->entry->frame, &l->returned))
      return false;
   result = finish ? vaapi_frame_finish(
                           l->owner, l->index, l->generation, &l->returned)
                   : vaapi_frame_unclaim(
                           l->owner, l->index, l->generation, &l->returned);
   if (!result)
      return false;
   l->owner = NULL;
   l->index = l->generation = 0;
   return true;
}

int vaapi_hevc_vulkan_lease_poll(vaapi_hevc_vulkan_lease_t *l)
{
   int result;
   if (!l || !l->entry || l->broken)
      return -1;
   if (!l->owner)
      return 1;
   result = vaapi_hevc_vulkan_frame_poll(
         l->entry->frame, l->entry->ops.fence_status);
   if (result <= 0)
      return result;
   if (!vaapi_hevc_vulkan_lease_put(l, true))
   {
      l->broken = true;
      return -1;
   }
   return 1;
}

int vaapi_hevc_vulkan_lease_present(vaapi_hevc_vulkan_lease_t *l,
      const video_hw_frame_t *frame, VkImage destination, VkImageView view,
      VkFormat format, unsigned width, unsigned height, VkImageLayout layout)
{
   vaapi_frame_view_t source;
   unsigned i, yuv;
   int result = vaapi_hevc_vulkan_lease_poll(l);
   if (result <= 0)
      return result;
   if (!frame || !vaapi_frame_callback(frame->abandon)
         || !vaapi_frame_claim(frame->owner, frame->done, frame->done_value,
               frame->image, &source))
      return -1;
   if (!frame->width || !frame->height || frame->width > source.width
         || frame->height > source.height || frame->crop_x != source.x
         || frame->crop_y != source.y || frame->pixfmt != source.pixfmt
         || width != frame->width || height != frame->height
         || ((frame->yuv | source.yuv)
               & ~(IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709))
         || !vaapi_hevc_vulkan_frame_matches(l->entry->frame, source.source))
   {
      if (vaapi_frame_unclaim(
                frame->owner, frame->done, frame->done_value, NULL))
         return -1;
      l->owner      = frame->owner;
      l->index      = frame->done;
      l->generation = frame->done_value;
      l->broken     = true;
      return -2;
   }
   yuv = source.yuv | frame->yuv;
   if (!source.color.primaries || source.color.primaries == 2)
      source.color.primaries = frame->color.primaries;
   if (!source.color.transfer || source.color.transfer == 2)
      source.color.transfer = frame->color.transfer;
   if (!source.color.matrix || source.color.matrix == 2)
      source.color.matrix = frame->color.matrix;
   if (!source.color.range)
      source.color.range = frame->color.range;
   for (i = 0; i < 8; i++)
      if (frame->color.mastering.present & (1u << i))
         source.color.mastering.chromaticity[i] =
               frame->color.mastering.chromaticity[i];
   if (frame->color.mastering.present & 0x100u)
      source.color.mastering.luminance_max =
            frame->color.mastering.luminance_max;
   if (frame->color.mastering.present & 0x200u)
      source.color.mastering.luminance_min =
            frame->color.mastering.luminance_min;
   source.color.mastering.present |= frame->color.mastering.present;
   if (frame->color.max_cll)
      source.color.max_cll = frame->color.max_cll;
   if (frame->color.max_fall)
      source.color.max_fall = frame->color.max_fall;
   l->owner      = frame->owner;
   l->index      = frame->done;
   l->generation = frame->done_value;
   if (!vaapi_frame_take(l->owner, l->index, l->generation, &l->returned))
   {
      if (vaapi_frame_unclaim(l->owner, l->index, l->generation, NULL))
      {
         l->owner = NULL;
         l->index = l->generation = 0;
         return -1;
      }
      l->broken = true;
      return -2;
   }
   if (!vaapi_hevc_vulkan_frame_adopt(l->entry->frame, &l->returned)
         || !vaapi_hevc_vulkan_submission_configure_position(l->entry,
               l->entry->frame, source.x, source.y, frame->width, frame->height,
               yuv, source.chroma_position, &source.color, l->fp16))
      result = -1;
   else
      result = vaapi_hevc_vulkan_submission_present(
            l->entry, destination, view, format, width, height, layout);
   if (result == 1)
      return 1;
   if (result == -2 || !vaapi_hevc_vulkan_lease_put(l, false))
   {
      l->broken = true;
      return -2;
   }
   return result;
}

bool vaapi_hevc_vulkan_lease_after_device_destroy(
      vaapi_hevc_vulkan_lease_t *l, VkDevice device)
{
   vaapi_hevc_vulkan_frame_t *frame;
   vaapi_hevc_export_t empty;
   if (!l || !l->entry || !device || l->entry->device != device)
      return false;
   frame = l->entry->frame;
   if (frame && frame->active)
   {
      if (l->returned.active && frame->source.active)
         return false;
      memset(&empty, 0, sizeof(empty));
      if (!vaapi_hevc_vulkan_frame_after_device_destroy(
                frame, device, l->returned.active ? &empty : &l->returned))
         return false;
   }
   if (l->owner
         && !vaapi_frame_finish(l->owner, l->index, l->generation,
               l->returned.active ? &l->returned : NULL))
      return false;
   memset(l, 0, sizeof(*l));
   return true;
}
