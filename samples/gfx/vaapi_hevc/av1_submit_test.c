/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_av1_submit.h"

#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
static vaapi_av1_session_t session;
static vaapi_av1_submit_t transaction;
static vaapi_av1_params_t params;
static VASliceParameterBufferAV1 slices[2];
static uint8_t packet[128];
static unsigned calls, created, dropped, fault, empty, duplicate, drop_fault;
static unsigned live[3], begun, rendered, ended, synced, grain_mode;
static VAStatus completion;

static VAStatus create(VADisplay display, VAContextID context,
                       VABufferType type, unsigned size, unsigned count,
                       void *data, VABufferID *id)
{
   unsigned i = created++;
   assert(display == session.display && context == session.context);
   assert(i < 3 && !begun && count == (i == 1 ? params.tile_count : 1));
   if (!i)
      assert(type == VAPictureParameterBufferType &&
             size == sizeof(params.picture) &&
             !memcmp(data, &params.picture, size));
   else if (i == 1)
      assert(type == VASliceParameterBufferType && size == sizeof(*slices) &&
             !memcmp(data, slices, size * count));
   else
      assert(type == VASliceDataBufferType && size == sizeof(packet) &&
             !memcmp(data, packet, size));
   *id = empty == i + 1 ? VA_INVALID_ID : duplicate == i + 1 ? 100 : 100 + i;
   if (*id != VA_INVALID_ID)
      live[*id - 100] = 1;
   calls++;
   return fault == i + 1 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus destroy(VADisplay display, VABufferID id)
{
   assert(display == session.display && id >= 100 && id < 103);
   assert(live[id - 100] && !transaction.poisoned &&
          (!transaction.submitted || transaction.complete));
   calls++;
   if (drop_fault == id)
      return VA_STATUS_ERROR_OPERATION_FAILED;
   live[id - 100] = 0;
   dropped++;
   return VA_STATUS_SUCCESS;
}

static VAStatus begin(VADisplay display, VAContextID context,
                      VASurfaceID target)
{
   assert(display == session.display && context == session.context &&
          target == params.target && created == 3 && !begun);
   begun++;
   calls++;
   return fault == 4 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus render(VADisplay display, VAContextID context,
                       VABufferID *buffers, int count)
{
   unsigned i;
   assert(display == session.display && context == session.context &&
          count == 3 && begun == 1 && !rendered);
   for (i = 0; i < 3; i++)
      assert(buffers[i] == 100 + i && live[i]);
   rendered++;
   calls++;
   return fault == 5 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus end(VADisplay display, VAContextID context)
{
   assert(display == session.display && context == session.context &&
          rendered == 1 && !ended);
   ended++;
   calls++;
   return fault == 6 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus sync_surface(VADisplay display, VASurfaceID target, uint64_t ns)
{
   assert(display == session.display && target == (params.picture.current_display_picture != VA_INVALID_SURFACE
             ? params.picture.current_display_picture : params.target) && !ns &&
          ended);
   synced++;
   calls++;
   return completion;
}

static const vaapi_av1_submit_ops_t ops = {create, destroy, begin,
                                           render, end,     sync_surface};

static void reset(unsigned depth)
{
   unsigned i;
   assert(!transaction.active);
   memset(&session, 0, sizeof(session));
   memset(&params, 0, sizeof(params));
   memset(live, 0, sizeof(live));
   calls = created = dropped = fault = empty = duplicate = drop_fault = 0;
   begun = rendered = ended = synced = 0;
   completion = VA_STATUS_SUCCESS;
   session.display = (VADisplay)(uintptr_t)1;
   session.context = 7;
   session.ready = true;
   session.slots = 12;
   session.width = 128;
   session.height = 96;
   session.depth = depth;
   session.profile = VAProfileAV1Profile0;
   for (i = 0; i < session.slots; i++)
      session.surfaces[i] = i + 1;
   params.target = params.picture.current_frame = 12;
   params.picture.current_display_picture = VA_INVALID_SURFACE;
   params.picture.frame_width_minus1 = 127;
   params.picture.frame_height_minus1 = 95;
   params.picture.bit_depth_idx = depth == 10;
   params.picture.primary_ref_frame = 7;
   params.picture.pic_info_fields.bits.show_frame = 1;
   params.picture.seq_info_fields.fields.subsampling_x = 1;
   params.picture.seq_info_fields.fields.subsampling_y = 1;
   params.picture.tile_cols = 2;
   params.picture.tile_rows = 1;
   params.tile_count = 2;
   for (i = 0; i < 8; i++)
      params.picture.ref_frame_map[i] = i ? VA_INVALID_SURFACE : 1;
   if (grain_mode)
   {
      params.picture.seq_info_fields.fields.film_grain_params_present = 1;
      params.picture.film_grain_info.film_grain_info_fields.bits.apply_grain = 1;
      params.picture.current_display_picture = 11;
   }
   memset(slices, 0, sizeof(slices));
   slices[0].slice_data_flag = slices[1].slice_data_flag =
      VA_SLICE_DATA_FLAG_ALL;
   slices[0].slice_data_offset = 16;
   slices[0].slice_data_size = 48;
   slices[1].slice_data_offset = 64;
   slices[1].slice_data_size = 64;
   slices[1].tile_column = 1;
}

static bool start(void)
{
   return vaapi_av1_submit_start(&transaction, &session, &ops, &params, slices,
                                 packet, sizeof(packet));
}

static void failures(unsigned depth)
{
   unsigned i, before;
   for (i = 1; i <= 6; i++)
   {
      reset(depth);
      fault = i;
      assert(!start() && transaction.active);
      if (i < 4)
         assert(vaapi_av1_submit_retire(&transaction) && dropped == i);
      else
      {
         before = calls;
         assert(transaction.poisoned && !vaapi_av1_submit_retire(&transaction));
         assert(vaapi_av1_submit_poll(&transaction) < 0 && calls == before);
         assert(!vaapi_av1_submit_terminated(&transaction, NULL));
         assert(!vaapi_av1_submit_terminated(&transaction,
                                             (VADisplay)(uintptr_t)2));
         assert(vaapi_av1_submit_terminated(&transaction, session.display));
      }
   }
   for (i = 1; i <= 3; i++)
   {
      reset(depth);
      empty = i;
      assert(!start() && vaapi_av1_submit_retire(&transaction) &&
             dropped == i - 1);
   }
   for (i = 2; i <= 3; i++)
   {
      reset(depth);
      duplicate = i;
      assert(!start() && vaapi_av1_submit_retire(&transaction) &&
             dropped == i - 1);
   }
}

static void retirement(unsigned depth)
{
   unsigned before;
   reset(depth);
   assert(start());
   before = calls;
   assert(!start() && !vaapi_av1_submit_retire(&transaction) &&
          calls == before);
   completion = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_av1_submit_poll(&transaction) == 1 && synced == 1);
   assert(!vaapi_av1_submit_retire(&transaction) && !dropped);
   completion = VA_STATUS_SUCCESS;
   assert(!vaapi_av1_submit_poll(&transaction) && synced == 2);
   before = calls;
   assert(!vaapi_av1_submit_poll(&transaction) && calls == before);
   drop_fault = 101;
   assert(!vaapi_av1_submit_retire(&transaction) && dropped == 2);
   drop_fault = 0;
   assert(vaapi_av1_submit_retire(&transaction) && dropped == 3);
   assert(vaapi_av1_submit_retire(&transaction));
   reset(depth);
   assert(start());
   completion = VA_STATUS_ERROR_DECODING_ERROR;
   assert(vaapi_av1_submit_poll(&transaction) < 0 && transaction.complete);
   assert(vaapi_av1_submit_retire(&transaction) && dropped == 3);
   reset(depth);
   transaction.ops = ops;
   assert(vaapi_av1_submit_start(&transaction, &session, &transaction.ops,
                                 &params, slices, packet, sizeof(packet)));
   completion = VA_STATUS_ERROR_INVALID_DISPLAY;
   assert(vaapi_av1_submit_poll(&transaction) < 0 && transaction.poisoned);
   assert(!vaapi_av1_submit_retire(&transaction) && !dropped);
   assert(vaapi_av1_submit_terminated(&transaction, session.display));
}

static void invalid(void)
{
   vaapi_av1_submit_ops_t incomplete;
   vaapi_av1_submit_t saved;
   unsigned i;
#define BAD(change)                                                            \
   do                                                                          \
   {                                                                           \
      reset(8);                                                                \
      change;                                                                  \
      transaction.error = VA_STATUS_ERROR_OPERATION_FAILED;                    \
      memcpy(&saved, &transaction, sizeof(saved));                             \
      assert(!start() && !calls && !transaction.active &&                      \
             !memcmp(&saved, &transaction, sizeof(saved)));                    \
   } while (0)
   BAD(session.ready = false);
   BAD(session.closing = true);
   BAD(session.context = VA_INVALID_ID);
   BAD(session.display = NULL);
   BAD(session.slots = 8);
   BAD(session.slots = 33);
   BAD(session.profile = VAProfileHEVCMain);
   BAD(session.depth = 12);
   BAD(params.picture.profile = 1);
   BAD(params.picture.bit_depth_idx = 1);
   BAD(params.picture.frame_width_minus1 = 128);
   BAD(params.picture.frame_height_minus1 = 96);
   BAD(params.picture.seq_info_fields.fields.subsampling_y = 0);
   BAD(params.picture.seq_info_fields.fields.mono_chrome = 1);
   BAD(params.picture.film_grain_info.film_grain_info_fields.bits.apply_grain =
          1);
   BAD(params.picture.current_display_picture = 1);
   BAD(params.picture.film_grain_info.film_grain_info_fields.bits.apply_grain = 1;
       params.picture.current_display_picture = 11);
   BAD(params.picture.seq_info_fields.fields.film_grain_params_present = 1;
       params.picture.film_grain_info.film_grain_info_fields.bits.apply_grain = 1;
       params.picture.current_display_picture = 12);
   BAD(params.picture.seq_info_fields.fields.film_grain_params_present = 1;
       params.picture.film_grain_info.film_grain_info_fields.bits.apply_grain = 1;
       params.picture.current_display_picture = 13);
   BAD(params.picture.seq_info_fields.fields.film_grain_params_present = 1;
       params.picture.film_grain_info.film_grain_info_fields.bits.apply_grain = 1;
       params.picture.current_display_picture = 1);
   BAD(params.picture.current_frame = 1);
   BAD(params.picture.pic_info_fields.bits.large_scale_tile = 1);
   BAD(params.picture.anchor_frames_num = 1);
   BAD(params.picture.anchor_frames_list = session.surfaces);
   BAD(params.picture.tile_cols = 0);
   BAD(params.picture.tile_rows = 0);
   BAD(params.picture.tile_cols = 65);
   BAD(params.tile_count = 0);
   BAD(params.tile_count = 4097);
   BAD(params.tile_count = 3);
   BAD(params.picture.context_update_tile_id = 2);
   BAD(params.picture.primary_ref_frame = 8);
   BAD(params.target = VA_INVALID_SURFACE);
   BAD(params.target = params.picture.current_frame = 13);
   BAD(params.picture.ref_frame_map[0] = 12);
   BAD(params.picture.ref_frame_map[0] = 13);
   BAD(slices[0].slice_data_flag = VA_SLICE_DATA_FLAG_BEGIN);
   BAD(slices[0].slice_data_offset = 0);
   BAD(slices[0].slice_data_size = 0);
   BAD(slices[1].slice_data_size = 65);
   BAD(slices[1].slice_data_size = 63);
   BAD(slices[1].slice_data_offset = 63);
   BAD(slices[1].tile_row = 1);
   BAD(slices[1].tile_column = 0);
   BAD(params.picture.pic_info_fields.bits.frame_type = RAV1_FRAME_INTER;
       params.picture.ref_frame_idx[0] = 8);
   BAD(params.picture.pic_info_fields.bits.frame_type = RAV1_FRAME_INTER;
       params.picture.ref_frame_idx[0] = 1);
#undef BAD
   for (i = 0; i < 6; i++)
   {
      reset(8);
      incomplete = ops;
      switch (i)
      {
      case 0:
         incomplete.create = NULL;
         break;
      case 1:
         incomplete.destroy = NULL;
         break;
      case 2:
         incomplete.begin = NULL;
         break;
      case 3:
         incomplete.render = NULL;
         break;
      case 4:
         incomplete.end = NULL;
         break;
      case 5:
         incomplete.sync = NULL;
         break;
      }
      assert(!vaapi_av1_submit_start(&transaction, &session, &incomplete,
                                     &params, slices, packet, sizeof(packet)) &&
             !calls);
   }
   reset(8);
   assert(!vaapi_av1_submit_start(NULL, &session, &ops, &params, slices, packet,
                                  128));
   assert(!vaapi_av1_submit_start(&transaction, NULL, &ops, &params, slices,
                                  packet, 128));
   assert(!vaapi_av1_submit_start(&transaction, &session, NULL, &params, slices,
                                  packet, 128));
   assert(!vaapi_av1_submit_start(&transaction, &session, &ops, NULL, slices,
                                  packet, 128));
   assert(!vaapi_av1_submit_start(&transaction, &session, &ops, &params, slices,
                                  NULL, 128));
   assert(!vaapi_av1_submit_start(&transaction, &session, &ops, &params, slices,
                                  packet, 0));
   assert(!vaapi_av1_submit_start(&transaction, &session, &ops, &params, NULL,
                                  packet, 128));
   assert(!vaapi_av1_submit_start(&transaction, &session, &ops, &params, slices,
                                  (const uint8_t *)&transaction,
                                  sizeof(packet)));
   assert(!calls && !transaction.active);
}

int main(void)
{
   failures(8);
   failures(10);
   retirement(8);
   retirement(10);
   grain_mode = 1;
   failures(8);
   failures(10);
   retirement(8);
   retirement(10);
   grain_mode = 0;
   invalid();
   puts("PASS: AV1 buffer submission, zero-timeout completion and failure "
        "retirement");
   return 0;
}

#else
int main(void)
{
   puts("AV1 submission: SKIP (SDK lacks AV1)");
   return 0;
}
#endif
