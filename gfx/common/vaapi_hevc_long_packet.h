/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_LONG_PACKET_H
#define RARCH_VAAPI_HEVC_LONG_PACKET_H

#include "vaapi_hevc_packet.h"
#include "vaapi_hevc_slice.h"

#define VAAPI_HEVC_SUBSET_MAX 65535

/* Allocate workspace and data on the cold owner, never on a decoder stack.
 * The entry array stores coded offsets, not cumulative byte positions. */
typedef struct vaapi_hevc_long_work
{
   uint8_t *data;
   size_t capacity;
   rh265_slice_decode_t parsed;
   VAPictureParameterBufferHEVC picture;
   VAIQMatrixBufferHEVC matrix;
   VASliceParameterBufferHEVC slices[256];
   uint32_t lengths[RH265_SLICE_ENTRY_POINTS_MAX];
   uint32_t entries[VAAPI_HEVC_SUBSET_MAX];
   unsigned used, num_entries;
} vaapi_hevc_long_work_t;

/* Borrowed validated AU/NALs and source belong to this decoder owner thread.
 * All input/work/data storage must be disjoint. Refusal resets used/count;
 * scratch fields may change, but no payload is copied until all headers pass.
 * No allocation, VA call or shared DPB advancement. */
static INLINE bool vaapi_hevc_long_packet(rh265_video *source,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      const rh265_hw_picture_t *picture, const VASurfaceID *surfaces,
      unsigned slots, const rh265_nal_t *slices, vaapi_hevc_long_work_t *work)
{
   unsigned i, j, entries = 0;
   size_t total = 0, padded, offset = 0;
   if (!work)
      return false;
   work->used = work->num_entries = 0;
   if (!source || !params || !au || !picture || !surfaces || !slices
         || !work->data || !slots || slots > 32 || !au->num_slices
         || au->num_slices > 256)
      return false;
   for (i = 0; i < au->num_slices; i++)
   {
      const rh265_nal_t *nal = &slices[i];
      if (!nal->data || nal->size < 3 || nal->size > UINT_MAX
            || total > UINT_MAX - nal->size)
         return false;
      total += nal->size;
   }
   if (total != au->coded_bytes || total > UINT_MAX - 127u)
      return false;
   padded = (total + 127u) & ~(size_t)127u;
   if (padded > work->capacity
         || vaapi_hevc_packet_overlap(work, sizeof(*work), work->data, padded)
         || vaapi_hevc_packet_overlap(
               work, sizeof(*work), params, sizeof(*params))
         || vaapi_hevc_packet_overlap(work, sizeof(*work), au, sizeof(*au))
         || vaapi_hevc_packet_overlap(
               work, sizeof(*work), picture, sizeof(*picture))
         || vaapi_hevc_packet_overlap(
               work, sizeof(*work), slices, au->num_slices * sizeof(*slices))
         || vaapi_hevc_packet_overlap(
               work, sizeof(*work), surfaces, slots * sizeof(*surfaces))
         || vaapi_hevc_packet_overlap(
               work->data, padded, params, sizeof(*params))
         || vaapi_hevc_packet_overlap(work->data, padded, au, sizeof(*au))
         || vaapi_hevc_packet_overlap(
               work->data, padded, picture, sizeof(*picture))
         || vaapi_hevc_packet_overlap(
               work->data, padded, surfaces, slots * sizeof(*surfaces))
         || vaapi_hevc_packet_overlap(
               work->data, padded, slices, au->num_slices * sizeof(*slices)))
      return false;
   for (i = 0; i < au->num_slices; i++)
      if (vaapi_hevc_packet_overlap(
               work, sizeof(*work), slices[i].data, slices[i].size)
            || vaapi_hevc_packet_overlap(
                  work->data, padded, slices[i].data, slices[i].size))
         return false;
   if (!vaapi_hevc_picture(&work->picture, params, au, picture, surfaces, slots)
         || (params->scaling_list_enabled
               && !vaapi_hevc_matrix(&work->matrix, params)))
      return false;
   for (i = 0; i < au->num_slices; i++)
   {
      const rh265_nal_t *nal = &slices[i];
      const rh265_slice_info_t *h;
      size_t bytes;
      if (rh265_video_get_slice_decode(source, nal->data, nal->size,
               &work->parsed, work->lengths, RH265_SLICE_ENTRY_POINTS_MAX))
         return false;
      h = &work->parsed.picture;
      if (h->nal_type != au->first.nal_type
            || h->temporal_id != au->first.temporal_id
            || !!h->first_slice_in_pic != (i == 0)
            || h->poc_lsb != au->first.poc_lsb
            || !(au->slice_types & (1u << h->slice_type))
            || work->parsed.num_entry_points > VAAPI_HEVC_SUBSET_MAX - entries
            || !vaapi_hevc_long_slice(&work->slices[i], params, &work->parsed,
                  &work->picture, nal->size, (unsigned)offset,
                  i + 1 == au->num_slices))
         return false;
      bytes = h->header_bytes;
      for (j = 0; j < work->parsed.num_entry_points; j++)
      {
         uint32_t length = work->lengths[j];
         if (!length || length >= nal->size - bytes)
            return false;
         bytes += length;
         work->entries[entries + j] = length - 1;
      }
      work->slices[i].entry_offset_to_subset_array = (uint16_t)entries;
      entries += work->parsed.num_entry_points;
      offset += nal->size;
   }
   offset = 0;
   for (i = 0; i < au->num_slices; i++)
   {
      memcpy(work->data + offset, slices[i].data, slices[i].size);
      offset += slices[i].size;
   }
   memset(work->data + offset, 0, padded - offset);
   work->used        = (unsigned)padded;
   work->num_entries = entries;
   return true;
}

static INLINE bool vaapi_hevc_long_submit(vaapi_hevc_submit_t *t,
      const vaapi_hevc_session_t *session, const vaapi_hevc_submit_ops_t *ops,
      rh265_video *source, const rh265_video_params_t *params,
      const rh265_access_unit_t *au, const rh265_hw_picture_t *picture,
      const rh265_nal_t *slices, vaapi_hevc_long_work_t *work)
{
   if (!t || t->active || !session || !session->ready || session->closing
         || session->slice_mode != VA_DEC_SLICE_MODE_NORMAL
         || !vaapi_hevc_long_packet(source, params, au, picture,
               session->surfaces, session->slots, slices, work))
      return false;
   return vaapi_hevc_submit_start_entries(t, session, ops, &work->picture,
         params->scaling_list_enabled ? &work->matrix : NULL, work->slices,
         au->num_slices, work->data, work->used, work->entries,
         work->num_entries);
}
#endif
