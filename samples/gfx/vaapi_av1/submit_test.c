/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VA AV1 submission tests require assertions
#endif
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc_submit.c"
#include "../../../gfx/common/vaapi_av1_submit.c"
static vaapi_av1_session_t session;
static vaapi_av1_submit_t transaction;
static VADecPictureParameterBufferAV1 picture;
static VASliceParameterBufferAV1 tiles[2];
static unsigned calls, created, destroyed, fault, duplicate, empty, drop_fault;
static unsigned live[3], sync_calls[2];
static VAStatus completion[2];
static const uint8_t packet[8] = {1, 2, 3, 4, 5, 6, 7, 8};
static VAStatus create(VADisplay d, VAContextID context, VABufferType type,
      unsigned bytes, unsigned count, void *data, VABufferID *id)
{
   unsigned i = created++;
   assert(d == session.display && context == session.context && i < 3);
   if (!i)
      assert(type == VAPictureParameterBufferType && count == 1
            && bytes == sizeof(picture) && !memcmp(data, &picture, bytes));
   else if (i == 1)
      assert(type == VASliceParameterBufferType && count == 2
            && bytes == sizeof(tiles[0])
            && !memcmp(data, tiles, sizeof(tiles)));
   else
      assert(type == VASliceDataBufferType && count == 1
            && bytes == sizeof(packet) && !memcmp(data, packet, bytes));
   *id = empty == i + 1 ? VA_INVALID_ID : duplicate == i + 1 ? 100 : 100 + i;
   if (*id != VA_INVALID_ID)
      live[*id - 100] = 1;
   calls++;
   return fault == i + 1 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus destroy(VADisplay d, VABufferID id)
{
   assert(d == session.display && id >= 100 && id < 103 && live[id - 100]);
   assert(!transaction.native.poisoned);
   if (drop_fault == id)
      return VA_STATUS_ERROR_OPERATION_FAILED;
   live[id - 100] = 0;
   destroyed++;
   return VA_STATUS_SUCCESS;
}
static VAStatus begin(VADisplay d, VAContextID context, VASurfaceID surface)
{
   assert(
         d == session.display && context == session.context && surface == 1000);
   assert(created == 3);
   calls++;
   return fault == 4 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus render(VADisplay d, VAContextID context, VABufferID *ids, int n)
{
   unsigned i;
   assert(d == session.display && context == session.context && n == 3);
   for (i = 0; i < 3; i++)
      assert(ids[i] == 100 + i && live[i]);
   calls++;
   return fault == 5 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus end(VADisplay d, VAContextID context)
{
   assert(d == session.display && context == session.context);
   calls++;
   return fault == 6 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus sync_surface(VADisplay d, VASurfaceID surface, uint64_t timeout)
{
   unsigned i = surface == 1001;
   assert(d == session.display && (surface == 1000 || surface == 1001));
   assert(!timeout && transaction.native.submitted);
   sync_calls[i]++;
   return completion[i];
}
static const vaapi_av1_submit_ops_t ops
      = {create, destroy, begin, render, end, sync_surface};
static void reset(bool grain)
{
   unsigned i;
   assert(!transaction.native.active);
   memset(&session, 0, sizeof(session));
   memset(&picture, 0, sizeof(picture));
   memset(tiles, 0, sizeof(tiles));
   memset(live, 0, sizeof(live));
   memset(sync_calls, 0, sizeof(sync_calls));
   completion[0] = completion[1] = VA_STATUS_SUCCESS;
   calls = created = destroyed = fault = duplicate = empty = drop_fault = 0;
   session.display = (VADisplay)&session;
   session.context = 12;
   session.ready = true;
   session.slots = 10;
   session.profile = VAProfileAV1Profile0;
   session.depth = 8;
   session.width = 128;
   session.height = 64;
   for (i = 0; i < 10; i++)
      session.surfaces[i] = 1000 + i;
   picture.current_frame = 1000;
   picture.current_display_picture = grain ? 1001 : 1000;
   picture.film_grain_info.film_grain_info_fields.bits.apply_grain = grain;
   picture.frame_width_minus1 = 127;
   picture.frame_height_minus1 = 63;
   picture.tile_cols = 2;
   picture.tile_rows = 1;
   for (i = 0; i < 8; i++)
      picture.ref_frame_map[i] = VA_INVALID_SURFACE;
   for (i = 0; i < 2; i++)
   {
      tiles[i].slice_data_offset = i * 4;
      tiles[i].slice_data_size = 4;
      tiles[i].slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
      tiles[i].tile_column = (uint16_t)i;
   }
}
static bool start(void)
{
   return vaapi_av1_submit_start(&transaction, &session, &ops, &picture, tiles,
         2, packet, sizeof(packet));
}
static void dual_surface(void)
{
   reset(true);
   assert(start());
   assert(!start());
   assert(!vaapi_av1_submit_retire(&transaction));
   completion[0] = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_av1_submit_poll(&transaction) == 1 && !sync_calls[1]);
   completion[0] = VA_STATUS_SUCCESS;
   completion[1] = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_av1_submit_poll(&transaction) == 1 && sync_calls[1] == 1);
   assert(!vaapi_av1_submit_retire(&transaction) && !destroyed);
   completion[1] = VA_STATUS_SUCCESS;
   assert(!vaapi_av1_submit_poll(&transaction) && sync_calls[0] == 2);
   assert(!vaapi_av1_submit_poll(&transaction) && sync_calls[1] == 2);
   drop_fault = 101;
   assert(!vaapi_av1_submit_retire(&transaction) && destroyed == 2);
   drop_fault = 0;
   assert(vaapi_av1_submit_retire(&transaction) && destroyed == 3);
}
static void failures(void)
{
   unsigned i;
   for (i = 1; i <= 6; i++)
   {
      reset(true);
      fault = i;
      assert(!start());
      if (i <= 3)
         assert(vaapi_av1_submit_retire(&transaction) && destroyed == i);
      else
      {
         assert(!vaapi_av1_submit_retire(&transaction) && !destroyed);
         assert(
               !vaapi_av1_submit_terminated(&transaction, (VADisplay)&picture));
         assert(vaapi_av1_submit_terminated(&transaction, session.display));
      }
   }
   for (i = 1; i <= 3; i++)
   {
      reset(false);
      empty = i;
      assert(!start());
      assert(vaapi_av1_submit_retire(&transaction) && destroyed == i - 1);
   }
   reset(false);
   duplicate = 2;
   assert(!start());
   assert(vaapi_av1_submit_retire(&transaction) && destroyed == 1);
   reset(true);
   assert(start());
   completion[1] = VA_STATUS_ERROR_OPERATION_FAILED;
   assert(vaapi_av1_submit_poll(&transaction) == -1);
   assert(!vaapi_av1_submit_retire(&transaction));
   assert(vaapi_av1_submit_terminated(&transaction, session.display));
}
static void decode_errors(void)
{
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      reset(true);
      assert(start());
      completion[i] = VA_STATUS_ERROR_DECODING_ERROR;
      assert(vaapi_av1_submit_poll(&transaction) == -1);
      assert(sync_calls[0] == 1 && sync_calls[1] == 1);
      assert(!transaction.native.poisoned);
      assert(vaapi_av1_submit_retire(&transaction) && destroyed == 3);
   }
}
static void invalid(void)
{
   reset(false);
   picture.frame_width_minus1 = 128;
   assert(!start() && !calls);
   reset(false);
   picture.frame_height_minus1 = 64;
   assert(!start() && !calls);
   reset(false);
   tiles[1].slice_data_offset = 3;
   assert(!start() && !calls);
   reset(false);
   tiles[1].slice_data_size = 5;
   assert(!start() && !calls);
   reset(false);
   tiles[1].tile_column = 0;
   assert(!start() && !calls);
   reset(false);
   picture.ref_frame_map[0] = 1000;
   assert(!start() && !calls);
   reset(true);
   picture.ref_frame_map[0] = 1001;
   assert(!start() && !calls);
   reset(true);
   picture.current_display_picture = 999;
   assert(!start() && !calls);
   reset(false);
   picture.bit_depth_idx = 1;
   assert(!start() && !calls);
   reset(false);
   picture.tile_cols = 1;
   assert(!start() && !calls);
   reset(false);
   picture.context_update_tile_id = 2;
   assert(!start() && !calls);
   reset(false);
   picture.pic_info_fields.bits.frame_type = RAV1_FRAME_INTER;
   assert(!start() && !calls);
   reset(false);
   assert(!vaapi_av1_submit_start(&transaction, &session, &ops, &picture, tiles,
                4097, packet, sizeof(packet))
         && !calls);
}
int main(void)
{
   dual_surface();
   failures();
   invalid();
   decode_errors();
   reset(false);
   assert(start());
   assert(!vaapi_av1_submit_poll(&transaction) && !sync_calls[1]);
   assert(vaapi_av1_submit_retire(&transaction));
   reset(true);
   session.depth = 10;
   picture.bit_depth_idx = 1;
   assert(start());
   assert(!vaapi_av1_submit_poll(&transaction));
   assert(vaapi_av1_submit_retire(&transaction));
   reset(false);
   picture.frame_width_minus1 = 63;
   picture.frame_height_minus1 = 31;
   assert(start());
   assert(!vaapi_av1_submit_poll(&transaction));
   assert(vaapi_av1_submit_retire(&transaction));
   puts("PASS: VA AV1 bounded submission, dual-surface completion, faults and "
        "retirement; no native device execution");
   return 0;
}
