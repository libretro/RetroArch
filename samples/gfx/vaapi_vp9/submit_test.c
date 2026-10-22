/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define main hevc_submit_contract_suite
#include "../vaapi_hevc/submit_test.c"
#undef main
#include "../../../gfx/common/vaapi_vp9_submit.c"
static vaapi_vp9_picture_t parameters;
static VAStatus vp9_create(VADisplay display, VAContextID context,
      VABufferType type, unsigned bytes, unsigned count, void *data,
      VABufferID *id)
{
   unsigned index = ss_created++;
   assert(display == ss_session.display && context == ss_session.context);
   assert(index < 3 && count == 1 && data && !ss_begin_calls);
   if (index == 0)
      assert(type == VAPictureParameterBufferType
            && bytes == sizeof(parameters.picture)
            && !memcmp(data, &parameters.picture, bytes));
   else if (index == 1)
      assert(type == VASliceParameterBufferType
            && bytes == sizeof(parameters.slice)
            && !memcmp(data, &parameters.slice, bytes));
   else
      assert(type == VASliceDataBufferType && bytes == sizeof(ss_data)
            && !memcmp(data, ss_data, bytes));
   *id = ss_empty == index + 1       ? VA_INVALID_ID
         : ss_duplicate == index + 1 ? 100
                                     : 100 + index;
   if (*id != VA_INVALID_ID)
      ss_live[*id - 100] = 1;
   ss_calls++;
   return ss_fault == index + 1 ? VA_STATUS_ERROR_OPERATION_FAILED
                                : VA_STATUS_SUCCESS;
}
static void vp9_reset(unsigned depth)
{
   unsigned i;
   ss_reset(depth, VA_DEC_SLICE_MODE_BASE);
   ss_picture.pic_fields.bits.scaling_list_enabled_flag = 0;
   ss_session.slots = 10;
   ss_session.profile
         = depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0;
   for (i = 0; i < 10; i++)
      ss_session.surfaces[i] = 9 + i;
   memset(&parameters, 0, sizeof(parameters));
   parameters.picture.frame_width = parameters.picture.frame_height = 64;
   parameters.picture.bit_depth = (uint8_t)depth;
   parameters.picture.profile = depth == 10 ? 2 : 0;
   parameters.picture.frame_header_length_in_bytes = 1;
   parameters.picture.first_partition_size = 1;
   parameters.slice.slice_data_size = sizeof(ss_data);
   parameters.slice.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
   for (i = 0; i < 8; i++)
      parameters.picture.reference_frames[i] = VA_INVALID_SURFACE;
}
static bool vp9_start(void)
{
   vaapi_vp9_submit_ops_t native = ss_ops;
   native.create = vp9_create;
   return vaapi_vp9_submit_start(&ss_tx, &ss_session, &native, 0, &parameters,
         ss_data, sizeof(ss_data));
}
int main(void)
{
   unsigned depth, f;
   assert(!hevc_submit_contract_suite());
   for (depth = 8; depth <= 10; depth += 2)
   {
      vp9_reset(depth);
      parameters.picture.frame_width--;
      assert(!vp9_start() && !ss_calls);
      parameters.picture.frame_width++;
      parameters.picture.reference_frames[0] = 9;
      assert(!vp9_start() && !ss_calls);
      parameters.picture.reference_frames[0] = 777;
      assert(!vp9_start() && !ss_calls);
      parameters.picture.reference_frames[0] = VA_INVALID_SURFACE;
      assert(vp9_start() && ss_created == 3);
      assert(!vp9_start());
      ss_sync_status = VA_STATUS_ERROR_TIMEDOUT;
      assert(vaapi_vp9_submit_poll(&ss_tx) == 1 && ss_sync_calls == 1);
      assert(!vaapi_vp9_submit_retire(&ss_tx));
      ss_sync_status = VA_STATUS_SUCCESS;
      assert(vaapi_vp9_submit_poll(&ss_tx) == 0 && ss_sync_calls == 2);
      assert(vaapi_vp9_submit_poll(&ss_tx) == 0 && ss_sync_calls == 2);
      ss_drop_fault = 101;
      assert(!vaapi_vp9_submit_retire(&ss_tx) && ss_dropped == 2);
      ss_drop_fault = 0;
      assert(vaapi_vp9_submit_retire(&ss_tx) && ss_dropped == 3);
      for (f = 1; f <= 3; f++)
      {
         vp9_reset(depth);
         ss_fault = f;
         assert(!vp9_start() && !ss_begin_calls);
         assert(vaapi_vp9_submit_retire(&ss_tx) && ss_dropped == f);
         vp9_reset(depth);
         ss_empty = f;
         assert(!vp9_start() && !ss_begin_calls);
         assert(vaapi_vp9_submit_retire(&ss_tx) && ss_dropped == f - 1);
      }
      vp9_reset(depth);
      ss_duplicate = 2;
      assert(!vp9_start() && !ss_begin_calls);
      assert(vaapi_vp9_submit_retire(&ss_tx) && ss_dropped == 1);
      for (f = 5; f <= 7; f++)
      {
         vp9_reset(depth);
         ss_fault = f;
         assert(!vp9_start() && ss_tx.poisoned);
         assert(!vaapi_vp9_submit_retire(&ss_tx) && !ss_dropped);
         assert(!vaapi_vp9_submit_terminated(&ss_tx, (VADisplay)(uintptr_t)99));
         assert(vaapi_vp9_submit_terminated(&ss_tx, ss_session.display));
         memset(ss_live, 0, sizeof(ss_live));
      }
      vp9_reset(depth);
      assert(vp9_start());
      ss_sync_status = VA_STATUS_ERROR_DECODING_ERROR;
      assert(vaapi_vp9_submit_poll(&ss_tx) == -1 && ss_tx.complete);
      assert(vaapi_vp9_submit_retire(&ss_tx));
   }
   puts("PASS: VA VP9 native buffers, zero-timeout completion and poisoned "
        "ownership");
   return 0;
}
