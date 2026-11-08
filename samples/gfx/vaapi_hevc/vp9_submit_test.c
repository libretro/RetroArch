/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_vp9_submit.h"

static vaapi_vp9_session_t session;
static vaapi_vp9_submit_t transaction;
static vaapi_vp9_params_t params;
static uint8_t packet[128];
static unsigned calls, created, dropped, fault, empty, duplicate, drop_fault;
static unsigned live[3], begun, rendered, ended, synced;
static VAStatus completion;

static VAStatus create(VADisplay display, VAContextID context,
      VABufferType type, unsigned size, unsigned count, void *data,
      VABufferID *id)
{
   unsigned i = created++;
   assert(display == session.display && context == session.context);
   assert(i < 3 && !begun && count == 1);
   if (!i)
      assert(type == VAPictureParameterBufferType
            && size == sizeof(params.picture)
            && !memcmp(data, &params.picture, size));
   else if (i == 1)
      assert(type == VASliceParameterBufferType
            && size == sizeof(params.slice)
            && !memcmp(data, &params.slice, size));
   else
      assert(type == VASliceDataBufferType && size == sizeof(packet)
            && !memcmp(data, packet, size));
   *id = empty == i + 1 ? VA_INVALID_ID : duplicate == i + 1 ? 100 : 100 + i;
   if (*id != VA_INVALID_ID)
      live[*id - 100] = 1;
   calls++;
   return fault == i + 1 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus destroy(VADisplay display, VABufferID id)
{
   assert(display == session.display && id >= 100 && id < 103);
   assert(live[id - 100] && !transaction.poisoned
         && (!transaction.submitted || transaction.complete));
   calls++;
   if (drop_fault == id)
      return VA_STATUS_ERROR_OPERATION_FAILED;
   live[id - 100] = 0;
   dropped++;
   return VA_STATUS_SUCCESS;
}

static VAStatus begin(VADisplay display, VAContextID context, VASurfaceID target)
{
   assert(display == session.display && context == session.context
         && target == params.target && created == 3 && !begun);
   begun++;
   calls++;
   return fault == 4 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus render(VADisplay display, VAContextID context,
      VABufferID *buffers, int count)
{
   unsigned i;
   assert(display == session.display && context == session.context
         && count == 3 && begun == 1 && !rendered);
   for (i = 0; i < 3; i++)
      assert(buffers[i] == 100 + i && live[i]);
   rendered++;
   calls++;
   return fault == 5 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus end(VADisplay display, VAContextID context)
{
   assert(display == session.display && context == session.context
         && rendered == 1 && !ended);
   ended++;
   calls++;
   return fault == 6 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus sync_surface(VADisplay display, VASurfaceID target, uint64_t ns)
{
   assert(display == session.display && target == params.target && !ns && ended);
   synced++;
   calls++;
   return completion;
}

static const vaapi_vp9_submit_ops_t ops = {
      create, destroy, begin, render, end, sync_surface};

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
   session.profile = depth == 8 ? VAProfileVP9Profile0 : VAProfileVP9Profile2;
   for (i = 0; i < session.slots; i++)
      session.surfaces[i] = i + 1;
   params.target = 12;
   params.picture.frame_width = 128;
   params.picture.frame_height = 96;
   params.picture.bit_depth = (uint8_t)depth;
   params.picture.profile = depth == 8 ? 0 : 2;
   params.picture.pic_fields.bits.subsampling_x = 1;
   params.picture.pic_fields.bits.subsampling_y = 1;
   params.picture.frame_header_length_in_bytes = 18;
   params.picture.first_partition_size = 20;
   for (i = 0; i < 8; i++)
      params.picture.reference_frames[i] = i ? VA_INVALID_SURFACE : 1;
   params.slice.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
   params.slice.slice_data_size = sizeof(packet);
}

static bool start(void)
{
   return vaapi_vp9_submit_start(&transaction, &session, &ops,
         &params, packet, sizeof(packet));
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
         assert(vaapi_vp9_submit_retire(&transaction) && dropped == i);
      else
      {
         before = calls;
         assert(transaction.poisoned && !vaapi_vp9_submit_retire(&transaction));
         assert(vaapi_vp9_submit_poll(&transaction) < 0 && calls == before);
         assert(!vaapi_vp9_submit_terminated(&transaction, NULL));
         assert(!vaapi_vp9_submit_terminated(&transaction, (VADisplay)(uintptr_t)2));
         assert(vaapi_vp9_submit_terminated(&transaction, session.display));
      }
   }
   for (i = 1; i <= 3; i++)
   {
      reset(depth);
      empty = i;
      assert(!start() && vaapi_vp9_submit_retire(&transaction) && dropped == i - 1);
   }
   for (i = 2; i <= 3; i++)
   {
      reset(depth);
      duplicate = i;
      assert(!start() && vaapi_vp9_submit_retire(&transaction) && dropped == i - 1);
   }
}

static void retirement(unsigned depth)
{
   unsigned before;
   reset(depth);
   assert(start());
   before = calls;
   assert(!start() && !vaapi_vp9_submit_retire(&transaction) && calls == before);
   completion = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_vp9_submit_poll(&transaction) == 1 && synced == 1);
   assert(!vaapi_vp9_submit_retire(&transaction) && !dropped);
   completion = VA_STATUS_SUCCESS;
   assert(!vaapi_vp9_submit_poll(&transaction) && synced == 2);
   before = calls;
   assert(!vaapi_vp9_submit_poll(&transaction) && calls == before);
   drop_fault = 101;
   assert(!vaapi_vp9_submit_retire(&transaction) && dropped == 2);
   drop_fault = 0;
   assert(vaapi_vp9_submit_retire(&transaction) && dropped == 3);
   assert(vaapi_vp9_submit_retire(&transaction));
   reset(depth);
   assert(start());
   completion = VA_STATUS_ERROR_DECODING_ERROR;
   assert(vaapi_vp9_submit_poll(&transaction) < 0 && transaction.complete);
   assert(vaapi_vp9_submit_retire(&transaction) && dropped == 3);
   reset(depth);
   transaction.ops = ops;
   assert(vaapi_vp9_submit_start(&transaction, &session, &transaction.ops,
         &params, packet, sizeof(packet)));
   completion = VA_STATUS_ERROR_INVALID_DISPLAY;
   assert(vaapi_vp9_submit_poll(&transaction) < 0 && transaction.poisoned);
   assert(!vaapi_vp9_submit_retire(&transaction) && !dropped);
   assert(vaapi_vp9_submit_terminated(&transaction, session.display));
}

static void invalid(void)
{
#define BAD(change) do { reset(8); change; assert(!start() && !calls \
      && !transaction.active); } while (0)
   BAD(session.ready = false);
   BAD(session.closing = true);
   BAD(session.context = VA_INVALID_ID);
   BAD(session.display = NULL);
   BAD(session.slots = 9);
   BAD(session.slots = 33);
   BAD(session.profile = VAProfileHEVCMain);
   BAD(params.picture.profile = 2);
   BAD(params.picture.bit_depth = 10);
   BAD(params.picture.frame_width = 127);
   BAD(params.picture.frame_height = 95);
   BAD(params.picture.pic_fields.bits.subsampling_y = 0);
   BAD(params.picture.frame_header_length_in_bytes = 0);
   BAD(params.picture.first_partition_size = 110);
   BAD(params.target = VA_INVALID_SURFACE);
   BAD(params.target = 13);
   BAD(params.picture.reference_frames[0] = 12);
   BAD(params.picture.reference_frames[0] = 13);
   BAD(params.slice.slice_data_flag = VA_SLICE_DATA_FLAG_BEGIN);
   BAD(params.slice.slice_data_offset = 1);
   BAD(params.slice.slice_data_size = 127);
#undef BAD
   reset(8);
   assert(!vaapi_vp9_submit_start(NULL, &session, &ops, &params, packet, 128));
   assert(!vaapi_vp9_submit_start(&transaction, NULL, &ops, &params, packet, 128));
   assert(!vaapi_vp9_submit_start(&transaction, &session, NULL, &params, packet, 128));
   assert(!vaapi_vp9_submit_start(&transaction, &session, &ops, NULL, packet, 128));
   assert(!vaapi_vp9_submit_start(&transaction, &session, &ops, &params, NULL, 128));
   assert(!vaapi_vp9_submit_start(&transaction, &session, &ops, &params, packet, 0));
   assert(!calls && !transaction.active);
}

int main(void)
{
   failures(8);
   failures(10);
   retirement(8);
   retirement(10);
   invalid();
   puts("PASS: VP9 buffer submission, zero-timeout completion and failure retirement");
   return 0;
}
