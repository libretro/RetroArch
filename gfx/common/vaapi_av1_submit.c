/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_av1_submit.h"
#include <string.h>

bool vaapi_av1_submit_start(vaapi_av1_submit_t *t, const vaapi_av1_session_t *s,
      const vaapi_av1_submit_ops_t *ops,
      const VADecPictureParameterBufferAV1 *p,
      const VASliceParameterBufferAV1 *tiles, unsigned count,
      const uint8_t *packet, unsigned size)
{
   vaapi_av1_submit_ops_t native;
   vaapi_hevc_submit_t *n;
   const void *data[3];
   unsigned bytes[3], elements[3], i, j, end = 0;
   bool reference_found = false, display_found = false;
   static const VABufferType types[3] = {VAPictureParameterBufferType,
         VASliceParameterBufferType, VASliceDataBufferType};
   if (!t || t->native.active || !s || !s->ready || s->closing || !s->display
         || s->context == VA_INVALID_ID || s->slots < 9 || s->slots > 32 || !ops
         || !ops->create || !ops->destroy || !ops->begin || !ops->render
         || !ops->end || !ops->sync || !p || !tiles || !packet || !size
         || !count || count > 4096 || p->profile
         || (s->depth != 8 && s->depth != 10)
         || p->bit_depth_idx != (s->depth == 10)
         || p->frame_width_minus1 + 1u > s->width
         || p->frame_height_minus1 + 1u > s->height || !p->tile_cols
         || p->tile_cols > 64 || !p->tile_rows || p->tile_rows > 64
         || p->context_update_tile_id >= count
         || count != (unsigned)p->tile_cols * p->tile_rows
         || p->pic_info_fields.bits.large_scale_tile
         || p->current_frame == VA_INVALID_SURFACE
         || p->current_display_picture == VA_INVALID_SURFACE)
      return false;
#if VA_CHECK_VERSION(1, 8, 0)
   if (s->profile != VAProfileAV1Profile0)
      return false;
#else
   return false;
#endif
   if (vaapi_av1_tiles_overlap(t, sizeof(*t), s, sizeof(*s))
         || vaapi_av1_tiles_overlap(t, sizeof(*t), ops, sizeof(*ops))
         || vaapi_av1_tiles_overlap(t, sizeof(*t), p, sizeof(*p))
         || vaapi_av1_tiles_overlap(
               t, sizeof(*t), tiles, count * sizeof(*tiles))
         || vaapi_av1_tiles_overlap(t, sizeof(*t), packet, size))
      return false;
   if (!!p->film_grain_info.film_grain_info_fields.bits.apply_grain
         != (p->current_display_picture != p->current_frame))
      return false;
   for (i = 0; i < s->slots; i++)
   {
      reference_found |= s->surfaces[i] == p->current_frame;
      display_found |= s->surfaces[i] == p->current_display_picture;
   }
   if (!reference_found || !display_found)
      return false;
   for (i = 0; i < 8; i++)
   {
      VASurfaceID id = p->ref_frame_map[i];
      bool found = false;
      if (id == VA_INVALID_SURFACE)
         continue;
      if (id == p->current_frame || id == p->current_display_picture)
         return false;
      for (j = 0; j < s->slots; j++)
         found |= id == s->surfaces[j];
      if (!found)
         return false;
   }
   if (p->pic_info_fields.bits.frame_type == RAV1_FRAME_INTER
         || p->pic_info_fields.bits.frame_type == RAV1_FRAME_SWITCH)
      for (i = 0; i < 7; i++)
         if (p->ref_frame_idx[i] >= 8
               || p->ref_frame_map[p->ref_frame_idx[i]] == VA_INVALID_SURFACE)
            return false;
   for (i = 0; i < count; i++)
   {
      unsigned at = tiles[i].slice_data_offset;
      unsigned length = tiles[i].slice_data_size;
      if (!length || at < end || at > size || length > size - at
            || tiles[i].slice_data_flag != VA_SLICE_DATA_FLAG_ALL
            || tiles[i].tile_row != i / p->tile_cols
            || tiles[i].tile_column != i % p->tile_cols)
         return false;
      end = at + length;
   }
   data[0] = p;
   data[1] = tiles;
   data[2] = packet;
   bytes[0] = sizeof(*p);
   bytes[1] = sizeof(*tiles);
   bytes[2] = size;
   elements[0] = elements[2] = 1;
   elements[1] = count;
   native = *ops;
   memset(t, 0, sizeof(*t));
   n = &t->native;
   n->ops = native;
   n->display = s->display;
   n->surface = p->current_frame;
   n->active = true;
   t->display_surface = p->current_display_picture;
   t->display_complete = t->display_surface == n->surface;
   for (i = 0; i < 5; i++)
      n->buffers[i] = VA_INVALID_ID;
   for (i = 0; i < 3; i++)
   {
      n->error = n->ops.create(n->display, s->context, types[i], bytes[i],
            elements[i], (void *)data[i], &n->buffers[n->count]);
      n->count++;
      for (j = 0; j + 1 < n->count; j++)
         if (n->buffers[j] == n->buffers[n->count - 1])
         {
            n->buffers[n->count - 1] = VA_INVALID_ID;
            n->error = VA_STATUS_ERROR_OPERATION_FAILED;
            return false;
         }
      if (n->error != VA_STATUS_SUCCESS)
         return false;
      if (n->buffers[n->count - 1] == VA_INVALID_ID)
      {
         n->error = VA_STATUS_ERROR_OPERATION_FAILED;
         return false;
      }
   }
   n->poisoned = true;
   n->error = n->ops.begin(n->display, s->context, n->surface);
   if (n->error != VA_STATUS_SUCCESS)
      return false;
   n->error = n->ops.render(n->display, s->context, n->buffers, (int)n->count);
   if (n->error != VA_STATUS_SUCCESS)
      return false;
   n->error = n->ops.end(n->display, s->context);
   if (n->error != VA_STATUS_SUCCESS)
      return false;
   n->submitted = true;
   n->poisoned = false;
   return true;
}
int vaapi_av1_submit_poll(vaapi_av1_submit_t *t)
{
   int result;
   vaapi_hevc_submit_t *n;
   if (!t)
      return -1;
   n = &t->native;
   result = vaapi_hevc_submit_poll(n);
   if (result == 1 || !n->complete || n->poisoned)
      return result;
   if (!t->display_complete)
   {
      n->error = n->ops.sync(n->display, t->display_surface, 0);
      if (n->error == VA_STATUS_ERROR_TIMEDOUT)
         return 1;
      if (n->error != VA_STATUS_SUCCESS
            && n->error != VA_STATUS_ERROR_DECODING_ERROR)
      {
         n->poisoned = true;
         return -1;
      }
      t->display_completion = n->error;
      t->display_complete = true;
   }
   return result || t->display_completion != VA_STATUS_SUCCESS ? -1 : 0;
}
bool vaapi_av1_submit_retire(vaapi_av1_submit_t *t)
{
   if (!t)
      return true;
   if (t->native.submitted && !t->display_complete)
      return false;
   if (!vaapi_hevc_submit_retire(&t->native))
      return false;
   memset(t, 0, sizeof(*t));
   return true;
}
bool vaapi_av1_submit_terminated(vaapi_av1_submit_t *t, VADisplay display)
{
   if (!t || !vaapi_hevc_submit_terminated(&t->native, display))
      return false;
   memset(t, 0, sizeof(*t));
   return true;
}
