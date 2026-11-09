/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_av1_submit.h"

#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
bool vaapi_av1_submit_start(vaapi_av1_submit_t *t, const vaapi_av1_session_t *s,
                            const vaapi_av1_submit_ops_t *ops,
                            const vaapi_av1_params_t *p,
                            const VASliceParameterBufferAV1 *slices,
                            const uint8_t *data, unsigned size)
{
   vaapi_av1_submit_ops_t native;
   const void *parameters[3];
   unsigned sizes[3], counts[3], i, j, previous = 0;
   static const VABufferType types[3] = {VAPictureParameterBufferType,
                                         VASliceParameterBufferType,
                                         VASliceDataBufferType};
   bool found = false, display_found = false;
   VASurfaceID display;
   if (!t || t->active || !s || !s->ready || s->closing || !ops ||
       !ops->create || !ops->destroy || !ops->begin || !ops->render ||
       !ops->end || !ops->sync || !s->display || s->context == VA_INVALID_ID ||
       s->slots < 9 || s->slots > 32 || !p || !slices || !data || !size ||
       p->target == VA_INVALID_SURFACE ||
       p->target != p->picture.current_frame ||
       s->profile != VAProfileAV1Profile0 || p->picture.profile ||
       (s->depth != 8 && s->depth != 10) ||
       p->picture.bit_depth_idx != (s->depth == 10 ? 1 : 0) ||
       (unsigned)p->picture.frame_width_minus1 + 1 > s->width ||
       (unsigned)p->picture.frame_height_minus1 + 1 > s->height ||
       !p->picture.seq_info_fields.fields.subsampling_x ||
       !p->picture.seq_info_fields.fields.subsampling_y ||
       p->picture.seq_info_fields.fields.mono_chrome ||
       (p->picture.film_grain_info.film_grain_info_fields.bits.apply_grain !=
        (unsigned)(p->picture.current_display_picture != VA_INVALID_SURFACE)) ||
       (p->picture.film_grain_info.film_grain_info_fields.bits.apply_grain &&
        !p->picture.seq_info_fields.fields.film_grain_params_present) ||
       p->picture.current_display_picture == p->target ||
       p->picture.pic_info_fields.bits.large_scale_tile ||
       p->picture.anchor_frames_num || p->picture.anchor_frames_list ||
       !p->picture.tile_cols || p->picture.tile_cols > 64 ||
       !p->picture.tile_rows || p->picture.tile_rows > 64 || !p->tile_count ||
       p->tile_count > 4096 ||
       p->tile_count != (unsigned)p->picture.tile_cols * p->picture.tile_rows ||
       p->picture.context_update_tile_id >= p->tile_count ||
       p->picture.primary_ref_frame > 7)
      return false;
   if (vaapi_av1_overlap(t, sizeof(*t), s, sizeof(*s)) ||
       vaapi_av1_overlap(t, sizeof(*t), p, sizeof(*p)) ||
       vaapi_av1_overlap(t, sizeof(*t), slices,
                         p->tile_count * sizeof(*slices)) ||
       vaapi_av1_overlap(t, sizeof(*t), data, size))
      return false;
   for (i = 0; i < p->tile_count; i++)
   {
      if (!slices[i].slice_data_size || slices[i].slice_data_offset >= size ||
          slices[i].slice_data_size > size - slices[i].slice_data_offset ||
          slices[i].slice_data_flag != VA_SLICE_DATA_FLAG_ALL ||
          slices[i].tile_row != i / p->picture.tile_cols ||
          slices[i].tile_column != i % p->picture.tile_cols ||
          (!i && !slices[i].slice_data_offset) ||
          slices[i].slice_data_offset < previous)
         return false;
      previous = slices[i].slice_data_offset + slices[i].slice_data_size;
   }
   if (previous != size)
      return false;
   if (p->picture.pic_info_fields.bits.frame_type == RAV1_FRAME_INTER ||
       p->picture.pic_info_fields.bits.frame_type == RAV1_FRAME_SWITCH)
      for (i = 0; i < 7; i++)
         if (p->picture.ref_frame_idx[i] > 7 ||
             p->picture.ref_frame_map[p->picture.ref_frame_idx[i]] ==
                VA_INVALID_SURFACE)
            return false;
   display = p->picture.current_display_picture;
   for (i = 0; i < s->slots; i++)
   {
      if (s->surfaces[i] == p->target)
         found = true;
      if (s->surfaces[i] == display)
         display_found = true;
   }
   if (!found || (display != VA_INVALID_SURFACE && !display_found))
      return false;
   for (i = 0; i < 8; i++)
   {
      VASurfaceID ref = p->picture.ref_frame_map[i];
      if (ref == VA_INVALID_SURFACE)
         continue;
      if (ref == p->target || ref == display)
         return false;
      found = false;
      for (j = 0; j < s->slots; j++)
         if (s->surfaces[j] == ref)
            found = true;
      if (!found)
         return false;
   }
   parameters[0] = &p->picture;
   parameters[1] = slices;
   parameters[2] = data;
   sizes[0] = sizeof(p->picture);
   sizes[1] = sizeof(*slices);
   sizes[2] = size;
   counts[0] = counts[2] = 1;
   counts[1] = p->tile_count;
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
      t->error =
         t->ops.create(t->display, s->context, types[i], sizes[i], counts[i],
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
   if (display != VA_INVALID_SURFACE)
      t->surface = display;
   t->submitted = true;
   t->poisoned = false;
   return true;
}
#endif
