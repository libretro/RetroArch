/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_vp9_submit.h"
#include <string.h>

bool vaapi_vp9_submit_start(vaapi_vp9_submit_t *t, const vaapi_vp9_session_t *s,
      const vaapi_vp9_submit_ops_t *ops, unsigned slot,
      const vaapi_vp9_picture_t *picture, const uint8_t *packet, unsigned size)
{
   vaapi_vp9_submit_ops_t native;
   const void *data[3];
   unsigned bytes[3], i, j;
   static const VABufferType types[3] = {VAPictureParameterBufferType,
         VASliceParameterBufferType, VASliceDataBufferType};
   if (!t || t->active || !s || !s->ready || s->closing || !s->display
         || s->context == VA_INVALID_ID || s->slots < 10 || s->slots > 32
         || slot >= s->slots || s->surfaces[slot] == VA_INVALID_SURFACE || !ops
         || !ops->create || !ops->destroy || !ops->begin || !ops->render
         || !ops->end || !ops->sync || !picture || !packet || !size
         || (s->depth != 8 && s->depth != 10)
         || s->profile
               != (s->depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0)
         || picture->picture.profile != (s->depth == 10 ? 2 : 0)
         || picture->picture.bit_depth != s->depth
         || picture->picture.frame_width != s->width
         || picture->picture.frame_height != s->height
         || picture->slice.slice_data_size != size
         || picture->slice.slice_data_offset
         || picture->slice.slice_data_flag != VA_SLICE_DATA_FLAG_ALL
         || !picture->picture.frame_header_length_in_bytes
         || picture->picture.frame_header_length_in_bytes >= size
         || !picture->picture.first_partition_size
         || picture->picture.first_partition_size
               >= size - picture->picture.frame_header_length_in_bytes)
      return false;
   for (i = 0; i < 8; i++)
   {
      VASurfaceID reference = picture->picture.reference_frames[i];
      bool found = false;
      if (reference == VA_INVALID_SURFACE)
         continue;
      if (reference == s->surfaces[slot])
         return false;
      for (j = 0; j < s->slots; j++)
         if (reference == s->surfaces[j])
            found = true;
      if (!found)
         return false;
   }
   data[0] = &picture->picture;
   data[1] = &picture->slice;
   data[2] = packet;
   bytes[0] = sizeof(picture->picture);
   bytes[1] = sizeof(picture->slice);
   bytes[2] = size;
   native = *ops;
   memset(t, 0, sizeof(*t));
   t->ops = native;
   t->display = s->display;
   t->surface = s->surfaces[slot];
   t->active = true;
   for (i = 0; i < 5; i++)
      t->buffers[i] = VA_INVALID_ID;
   for (i = 0; i < 3; i++)
   {
      t->error = t->ops.create(t->display, s->context, types[i], bytes[i], 1,
            (void *)data[i], &t->buffers[t->count]);
      t->count++;
      for (j = 0; j + 1 < t->count; j++)
         if (t->buffers[j] == t->buffers[t->count - 1])
         {
            t->buffers[t->count - 1] = VA_INVALID_ID;
            t->error = VA_STATUS_ERROR_OPERATION_FAILED;
            return false;
         }
      if (t->error != VA_STATUS_SUCCESS)
         return false;
      if (t->buffers[t->count - 1] == VA_INVALID_ID)
      {
         t->error = VA_STATUS_ERROR_OPERATION_FAILED;
         return false;
      }
   }
   t->poisoned = true;
   t->error = t->ops.begin(t->display, s->context, t->surface);
   if (t->error != VA_STATUS_SUCCESS)
      return false;
   t->error = t->ops.render(t->display, s->context, t->buffers, (int)t->count);
   if (t->error != VA_STATUS_SUCCESS)
      return false;
   t->error = t->ops.end(t->display, s->context);
   if (t->error != VA_STATUS_SUCCESS)
      return false;
   t->submitted = true;
   t->poisoned = false;
   return true;
}
