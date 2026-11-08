/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_vp9_submit.h"

bool vaapi_vp9_submit_start(vaapi_vp9_submit_t *t,
      const vaapi_vp9_session_t *s, const vaapi_vp9_submit_ops_t *ops,
      const vaapi_vp9_params_t *p, const uint8_t *data, unsigned size)
{
   vaapi_vp9_submit_ops_t native;
   const void *parameters[3];
   unsigned sizes[3], i, j;
   static const VABufferType types[3] = {VAPictureParameterBufferType,
         VASliceParameterBufferType, VASliceDataBufferType};
   bool found = false;
   if (!t || t->active || !s || !s->ready || s->closing || !ops
         || !ops->create || !ops->destroy || !ops->begin || !ops->render
         || !ops->end || !ops->sync || !s->display
         || s->context == VA_INVALID_ID || s->slots < 10 || s->slots > 32
         || !p || !data || !size || p->target == VA_INVALID_SURFACE
         || p->picture.frame_width != s->width
         || p->picture.frame_height != s->height
         || p->picture.bit_depth != s->depth
         || !((s->profile == VAProfileVP9Profile0 && s->depth == 8
                  && p->picture.profile == 0)
               || (s->profile == VAProfileVP9Profile2 && s->depth == 10
                  && p->picture.profile == 2))
         || !p->picture.pic_fields.bits.subsampling_x
         || !p->picture.pic_fields.bits.subsampling_y
         || !p->picture.frame_header_length_in_bytes
         || p->picture.frame_header_length_in_bytes >= size
         || !p->picture.first_partition_size
         || p->picture.first_partition_size
               >= size - p->picture.frame_header_length_in_bytes
         || p->slice.slice_data_size != size || p->slice.slice_data_offset
         || p->slice.slice_data_flag != VA_SLICE_DATA_FLAG_ALL)
      return false;
   for (i = 0; i < s->slots; i++)
      if (s->surfaces[i] == p->target)
         found = true;
   if (!found)
      return false;
   for (i = 0; i < 8; i++)
   {
      VASurfaceID ref = p->picture.reference_frames[i];
      if (ref == VA_INVALID_SURFACE)
         continue;
      if (ref == p->target)
         return false;
      found = false;
      for (j = 0; j < s->slots; j++)
         if (s->surfaces[j] == ref)
            found = true;
      if (!found)
         return false;
   }
   parameters[0] = &p->picture;
   parameters[1] = &p->slice;
   parameters[2] = data;
   sizes[0] = sizeof(p->picture);
   sizes[1] = sizeof(p->slice);
   sizes[2] = size;
   native = *ops;
   memset(t, 0, sizeof(*t));
   t->ops = native;
   t->display = s->display;
   t->surface = p->target;
   t->active = true;
   for (i = 0; i < 5; i++)
      t->buffers[i] = VA_INVALID_ID;
   for (i = 0; i < 3; i++)
   {
      t->error = t->ops.create(t->display, s->context, types[i], sizes[i], 1,
            (void *)parameters[i], &t->buffers[i]);
      t->count++;
      for (j = 0; j < i; j++)
         if (t->buffers[j] == t->buffers[i])
         {
            t->buffers[i] = VA_INVALID_ID;
            t->error = VA_STATUS_ERROR_OPERATION_FAILED;
            return false;
         }
      if (t->error != VA_STATUS_SUCCESS)
         return false;
      if (t->buffers[i] == VA_INVALID_ID)
      {
         t->error = VA_STATUS_ERROR_OPERATION_FAILED;
         return false;
      }
   }
   t->poisoned = true;
   t->error = t->ops.begin(t->display, s->context, t->surface);
   if (t->error != VA_STATUS_SUCCESS)
      return false;
   t->error = t->ops.render(t->display, s->context, t->buffers, 3);
   if (t->error != VA_STATUS_SUCCESS)
      return false;
   t->error = t->ops.end(t->display, s->context);
   if (t->error != VA_STATUS_SUCCESS)
      return false;
   t->submitted = true;
   t->poisoned = false;
   return true;
}
