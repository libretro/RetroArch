/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc_submit.c"
static vaapi_hevc_session_t ss_session;
static vaapi_hevc_submit_t ss_tx;
static VAPictureParameterBufferHEVC ss_picture;
static VAIQMatrixBufferHEVC ss_matrix;
static VASliceParameterBufferHEVC ss_long[2];
static VASliceParameterBufferBase ss_short[2];
static uint8_t ss_data[8] = {0, 0, 1, 38, 1, 128, 128, 128};
static unsigned ss_created, ss_dropped, ss_calls, ss_fault, ss_empty,
      ss_duplicate, ss_drop_fault, ss_live[5];
static unsigned ss_subset;
static uint32_t ss_entries[2];
static unsigned ss_begin_calls, ss_render_calls, ss_end_calls, ss_sync_calls;
static VAStatus ss_sync_status;

static VAStatus ss_create(VADisplay display, VAContextID context,
      VABufferType type, unsigned size, unsigned count, void *data,
      VABufferID *id)
{
   unsigned index = ss_created++, kind = index;
   assert(display == ss_session.display && context == ss_session.context
         && index < 5 && data);
   assert(!ss_begin_calls);
   if (!ss_picture.pic_fields.bits.scaling_list_enabled_flag && kind)
      kind++;
   if (kind == 0)
      assert(type == VAPictureParameterBufferType && size == sizeof(ss_picture)
            && count == 1);
   else if (kind == 1)
      assert(type == VAIQMatrixBufferType && size == sizeof(ss_matrix)
            && count == 1);
   else if (kind == 2)
      assert(type == VASliceParameterBufferType && count == 2
            && size
                     == (ss_session.slice_mode == VA_DEC_SLICE_MODE_BASE
                                 ? sizeof(ss_short[0])
                                 : sizeof(ss_long[0])));
   else if (kind == 3 && ss_subset)
      assert(type == VASubsetsParameterBufferType
            && size == sizeof(ss_entries[0]) && count == 2
            && !memcmp(data, ss_entries, sizeof(ss_entries)));
   else
      assert(type == VASliceDataBufferType && size == 8 && count == 1
            && !memcmp(data, ss_data, 8));
   *id = ss_empty == index + 1       ? VA_INVALID_ID
         : ss_duplicate == index + 1 ? 100
                                    : 100 + index;
   if (*id != VA_INVALID_ID)
      ss_live[*id - 100] = 1;
   ss_calls++;
   return ss_fault == index + 1 ? VA_STATUS_ERROR_OPERATION_FAILED
                                : VA_STATUS_SUCCESS;
}

static VAStatus ss_destroy(VADisplay display, VABufferID id)
{
   assert(display == ss_session.display && id >= 100 && id < 105
         && ss_live[id - 100]);
   assert(!ss_tx.poisoned && (!ss_tx.submitted || ss_tx.complete));
   ss_calls++;
   if (ss_drop_fault == id)
      return VA_STATUS_ERROR_OPERATION_FAILED;
   ss_live[id - 100] = 0;
   ss_dropped++;
   return VA_STATUS_SUCCESS;
}

static VAStatus ss_begin(
      VADisplay display, VAContextID context, VASurfaceID surface)
{
   assert(display == ss_session.display && context == ss_session.context
         && surface == 9);
   assert(ss_created
         == 3u + ss_picture.pic_fields.bits.scaling_list_enabled_flag
                  + ss_subset);
   ss_calls++;
   ss_begin_calls++;
   return ss_fault == 5 || ss_fault == 8 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus ss_render(
      VADisplay display, VAContextID context, VABufferID *ids, int count)
{
   unsigned i;
   assert(display == ss_session.display && context == ss_session.context
         && ss_begin_calls == 1
         && count
                  == (int)(3u
                           + ss_picture.pic_fields.bits
                                    .scaling_list_enabled_flag
                           + ss_subset));
   for (i = 0; i < (unsigned)count; i++)
      assert(ids[i] == 100 + i && ss_live[i]);
   ss_calls++;
   ss_render_calls++;
   return ss_fault == 6 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus ss_end(VADisplay display, VAContextID context)
{
   assert(display == ss_session.display && context == ss_session.context
         && ss_render_calls == 1);
   ss_calls++;
   ss_end_calls++;
   return ss_fault == 7 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus ss_sync(
      VADisplay display, VASurfaceID surface, uint64_t timeout)
{
   assert(display == ss_session.display && surface == 9 && timeout == 0
         && ss_end_calls == 1);
   ss_calls++;
   ss_sync_calls++;
   return ss_sync_status;
}

static const vaapi_hevc_submit_ops_t ss_ops = {
      ss_create, ss_destroy, ss_begin, ss_render, ss_end, ss_sync};

static void ss_reset(unsigned depth, unsigned mode)
{
   unsigned i;
   assert(!ss_tx.active);
   memset(&ss_session, 0, sizeof(ss_session));
   memset(&ss_picture, 0, sizeof(ss_picture));
   memset(ss_live, 0, sizeof(ss_live));
   memset(ss_long, 0, sizeof(ss_long));
   memset(ss_short, 0, sizeof(ss_short));
   ss_created = ss_dropped = ss_calls = ss_fault = ss_empty = ss_duplicate =
         ss_drop_fault                                      = 0;
   ss_begin_calls = ss_render_calls = ss_end_calls = ss_sync_calls = 0;
   ss_sync_status = VA_STATUS_SUCCESS;
   ss_subset      = 0;
   memset(ss_entries, 0, sizeof(ss_entries));
   ss_session.display     = (VADisplay)(uintptr_t)1;
   ss_session.context     = 7;
   ss_session.ready       = true;
   ss_session.slots       = 2;
   ss_session.surfaces[0] = 9;
   ss_session.surfaces[1] = 10;
   ss_session.width = ss_session.height = 64;
   ss_session.depth                     = depth;
   ss_session.slice_mode                = mode;
   ss_picture.CurrPic.picture_id        = 9;
   ss_picture.pic_width_in_luma_samples =
         ss_picture.pic_height_in_luma_samples = 64;
   ss_picture.bit_depth_luma_minus8 = ss_picture.bit_depth_chroma_minus8 =
         (uint8_t)(depth - 8);
   ss_picture.pic_fields.bits.scaling_list_enabled_flag = 1;
   for (i = 0; i < 2; i++)
   {
      ss_short[i].slice_data_offset = ss_long[i].slice_data_offset = i * 4;
      ss_short[i].slice_data_size = ss_long[i].slice_data_size = 4;
   }
}

static bool ss_start(void)
{
   return vaapi_hevc_submit_start(&ss_tx, &ss_session, &ss_ops, &ss_picture,
         ss_picture.pic_fields.bits.scaling_list_enabled_flag ? &ss_matrix
                                                              : NULL,
         ss_session.slice_mode == VA_DEC_SLICE_MODE_BASE ? (void *)ss_short
                                                         : (void *)ss_long,
         2, ss_data, 8);
}

static void ss_cases(unsigned depth, unsigned mode)
{
   unsigned i, before;
   ss_reset(depth, mode);
   assert(ss_start());
   before = ss_calls;
   assert(!ss_start() && !vaapi_hevc_submit_retire(&ss_tx)
         && ss_calls == before);
   ss_sync_status = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_hevc_submit_poll(&ss_tx) == 1 && !ss_tx.complete);
   assert(!vaapi_hevc_submit_retire(&ss_tx));
   ss_sync_status = VA_STATUS_SUCCESS;
   assert(!vaapi_hevc_submit_poll(&ss_tx));
   before = ss_calls;
   assert(!vaapi_hevc_submit_poll(&ss_tx) && ss_calls == before);
   ss_drop_fault = 101;
   assert(!vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == 3);
   before = ss_calls;
   assert(!vaapi_hevc_submit_poll(&ss_tx) && ss_calls == before);
   ss_drop_fault = 0;
   assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == 4);
   assert(vaapi_hevc_submit_retire(&ss_tx));
   for (i = 1; i <= 7; i++)
   {
      ss_reset(depth, mode);
      ss_fault = i;
      assert(!ss_start());
      if (i <= 4)
         assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == i);
      else
      {
         before = ss_calls;
         assert(!vaapi_hevc_submit_retire(&ss_tx)
               && vaapi_hevc_submit_poll(&ss_tx) < 0 && ss_calls == before);
         assert(!vaapi_hevc_submit_terminated(&ss_tx, (VADisplay)(uintptr_t)2));
         assert(vaapi_hevc_submit_terminated(&ss_tx, ss_session.display));
      }
   }
   for (i = 1; i <= 4; i++)
   {
      ss_reset(depth, mode);
      ss_empty = i;
      assert(!ss_start());
      assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == i - 1);
   }
   ss_reset(depth, mode);
   ss_duplicate = 3;
   assert(!ss_start());
   assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == 2);
   ss_reset(depth, mode);
   assert(ss_start());
   ss_sync_status = VA_STATUS_ERROR_DECODING_ERROR;
   assert(vaapi_hevc_submit_poll(&ss_tx) < 0 && ss_tx.complete
         && vaapi_hevc_submit_retire(&ss_tx));
   ss_reset(depth, mode);
   assert(ss_start());
   ss_sync_status = VA_STATUS_ERROR_INVALID_DISPLAY;
   assert(vaapi_hevc_submit_poll(&ss_tx) < 0 && ss_tx.poisoned
         && !vaapi_hevc_submit_retire(&ss_tx));
   assert(vaapi_hevc_submit_terminated(&ss_tx, ss_session.display));
   ss_reset(depth, mode);
   ss_short[0].slice_data_offset = ss_long[0].slice_data_offset = 9;
   assert(!ss_start() && !ss_tx.active && !ss_calls);
   ss_reset(depth, mode);
   ss_session.closing = true;
   assert(!ss_start() && !ss_calls);
   ss_reset(depth, mode);
   ss_picture.CurrPic.picture_id = 11;
   assert(!ss_start() && !ss_calls);
   ss_reset(depth, mode);
   ss_picture.bit_depth_luma_minus8 = 3;
   assert(!ss_start() && !ss_calls);
   ss_reset(depth, mode);
   ss_picture.pic_fields.bits.scaling_list_enabled_flag = 0;
   assert(ss_start() && ss_created == 3 && !vaapi_hevc_submit_poll(&ss_tx));
   assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == 3);
   ss_reset(depth, mode);
   ss_short[0].slice_data_size = ss_long[0].slice_data_size = 9;
   assert(!ss_start() && !ss_calls);
   ss_reset(depth, mode);
   ss_short[0].slice_data_flag = ss_long[0].slice_data_flag =
         VA_SLICE_DATA_FLAG_BEGIN;
   assert(!ss_start() && !ss_calls);
   ss_reset(depth, mode);
   ss_tx.ops = ss_ops;
   assert(vaapi_hevc_submit_start(&ss_tx, &ss_session, &ss_tx.ops, &ss_picture,
         &ss_matrix,
         mode == VA_DEC_SLICE_MODE_BASE ? (void *)ss_short : (void *)ss_long, 2,
         ss_data, 8));
   assert(!vaapi_hevc_submit_poll(&ss_tx) && vaapi_hevc_submit_retire(&ss_tx));
}

static void ss_subset_setup(void)
{
   unsigned i;
   ss_reset(10, VA_DEC_SLICE_MODE_NORMAL);
   ss_subset = 1;
   for (i = 0; i < 2; i++)
   {
      ss_long[i].slice_data_byte_offset       = 2;
      ss_long[i].num_entry_point_offsets      = 1;
      ss_long[i].entry_offset_to_subset_array = (uint16_t)i;
   }
}

static bool ss_subset_start(void)
{
   return vaapi_hevc_submit_start_entries(&ss_tx, &ss_session, &ss_ops,
         &ss_picture, &ss_matrix, ss_long, 2, ss_data, 8, ss_entries, 2);
}

static void ss_subset_cases(void)
{
   unsigned i;
   ss_subset_setup();
   assert(ss_subset_start() && ss_created == 5);
   assert(!vaapi_hevc_submit_poll(&ss_tx));
   ss_drop_fault = 104;
   assert(!vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == 4);
   ss_drop_fault = 0;
   assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == 5);
   for (i = 1; i <= 5; i++)
   {
      ss_subset_setup();
      ss_fault = i;
      assert(!ss_subset_start());
      assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == i);
      ss_subset_setup();
      ss_empty = i;
      assert(!ss_subset_start());
      assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == i - 1);
   }
   ss_subset_setup();
   ss_duplicate = 5;
   assert(!ss_subset_start());
   assert(vaapi_hevc_submit_retire(&ss_tx) && ss_dropped == 4);
   for (i = 6; i <= 8; i++)
   {
      unsigned before_calls;
      ss_subset_setup();
      ss_fault = i;
      assert(!ss_subset_start() && ss_created == 5 && ss_tx.poisoned);
      before_calls = ss_calls;
      assert(!vaapi_hevc_submit_retire(&ss_tx) && ss_calls == before_calls);
      assert(vaapi_hevc_submit_terminated(&ss_tx, ss_session.display));
   }
#define SS_SUBSET_BAD(change)                                                  \
   do                                                                          \
   {                                                                           \
      ss_subset_setup();                                                       \
      change;                                                                  \
      assert(!ss_subset_start() && !ss_calls && !ss_tx.active);                \
   } while (0)
   SS_SUBSET_BAD(ss_long[1].entry_offset_to_subset_array = 0);
   SS_SUBSET_BAD(ss_long[0].num_entry_point_offsets = 3);
   SS_SUBSET_BAD(ss_long[1].num_entry_point_offsets = 0);
   SS_SUBSET_BAD(ss_entries[0] = UINT32_MAX);
   SS_SUBSET_BAD(ss_entries[0] = 1);
   SS_SUBSET_BAD(ss_long[0].slice_data_byte_offset = UINT32_MAX);
   SS_SUBSET_BAD(ss_long[0].slice_data_num_emu_prevn_bytes = 3);
   SS_SUBSET_BAD(ss_session.slice_mode = VA_DEC_SLICE_MODE_BASE);
#undef SS_SUBSET_BAD
   ss_subset_setup();
   assert(!vaapi_hevc_submit_start_entries(&ss_tx, &ss_session, &ss_ops,
         &ss_picture, &ss_matrix, ss_long, 2, ss_data, 8, ss_entries, 65536)
         && !ss_calls);
   assert(!vaapi_hevc_submit_start_entries(&ss_tx, &ss_session, &ss_ops,
               &ss_picture, &ss_matrix, ss_long, 2, ss_data, 8, NULL, 2)
         && !ss_calls);
}

int main(void)
{
   unsigned depth, mode;
   for (depth = 8; depth <= 10; depth += 2)
      for (mode = VA_DEC_SLICE_MODE_NORMAL; mode <= VA_DEC_SLICE_MODE_BASE;
            mode++)
         ss_cases(depth, mode);
   ss_subset_cases();
   puts("PASS: bounded VA submission buffers, zero-timeout completion, failure poisoning and retirement; no device execution");
   return 0;
}
