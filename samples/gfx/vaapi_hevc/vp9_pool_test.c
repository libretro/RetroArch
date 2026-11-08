/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_vp9_pool.h"

static vaapi_vp9_pool_t pool;
static vaapi_vp9_session_t session;
static rvp9_hw_picture_t picture;
static uint8_t packet[128];
static VADecPictureParameterBufferVP9 observed;
static unsigned calls, created, dropped, fault, drop_fault, native_syncs;
static unsigned live[256];
static VAStatus completion[32];

static VAStatus create(VADisplay display, VAContextID context,
      VABufferType type, unsigned size, unsigned count, void *data,
      VABufferID *id)
{
   unsigned index = created++;
   assert(display == session.display && context == session.context
         && index < 256 && count == 1 && data);
   if (index % 3 == 0)
   {
      assert(type == VAPictureParameterBufferType && size == sizeof(observed));
      memcpy(&observed, data, size);
   }
   else if (index % 3 == 1)
      assert(type == VASliceParameterBufferType
            && size == sizeof(VASliceParameterBufferVP9));
   else
      assert(type == VASliceDataBufferType && size == sizeof(packet)
            && !memcmp(data, packet, size));
   *id = 100 + index;
   live[index] = 1;
   calls++;
   return fault == index + 1 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus destroy(VADisplay display, VABufferID id)
{
   assert(display == session.display && id >= 100 && id < 356 && live[id - 100]);
   calls++;
   if (id == drop_fault)
      return VA_STATUS_ERROR_OPERATION_FAILED;
   live[id - 100] = 0;
   dropped++;
   return VA_STATUS_SUCCESS;
}

static VAStatus begin(VADisplay display, VAContextID context, VASurfaceID target)
{
   assert(display == session.display && context == session.context
         && target >= 1 && target <= session.slots && created % 3 == 0);
   calls++;
   return fault == 1000 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}

static VAStatus render(VADisplay display, VAContextID context,
      VABufferID *ids, int count)
{
   unsigned i;
   assert(display == session.display && context == session.context && count == 3);
   for (i = 0; i < 3; i++)
      assert(ids[i] >= 100 && ids[i] < 356 && live[ids[i] - 100]);
   calls++;
   return VA_STATUS_SUCCESS;
}

static VAStatus end(VADisplay display, VAContextID context)
{
   assert(display == session.display && context == session.context);
   calls++;
   return VA_STATUS_SUCCESS;
}

static VAStatus sync_surface(VADisplay display, VASurfaceID target, uint64_t ns)
{
   assert(display == session.display && target >= 1 && target <= session.slots && !ns);
   calls++;
   native_syncs++;
   return completion[target - 1];
}

static const vaapi_vp9_submit_ops_t ops = {
      create, destroy, begin, render, end, sync_surface};

static void next(unsigned logical, int previous)
{
   unsigned i;
   memset(&picture, 0, sizeof(picture));
   picture.header.bit_depth = (int)session.depth;
   picture.header.profile = session.depth == 8 ? 0 : 2;
   picture.header.subsampling_x = picture.header.subsampling_y = 1;
   picture.header.w = 128;
   picture.header.h = 96;
   picture.header.uncomp_size = 18;
   picture.header.first_partition_size = 20;
   picture.header.show_frame = 1;
   picture.slot = (int)logical;
   picture.previous_slot = previous;
   for (i = 0; i < 8; i++) picture.reference_slots[i] = -1;
   for (i = 0; i < 3; i++) picture.named_slots[i] = -1;
}

static void setup(unsigned depth, unsigned slots)
{
   unsigned i;
   assert(!pool.active);
   pool.next_generation = 0;
   memset(&session, 0, sizeof(session));
   memset(live, 0, sizeof(live));
   memset(completion, 0, sizeof(completion));
   calls = created = dropped = fault = drop_fault = native_syncs = 0;
   session.display = (VADisplay)(uintptr_t)1;
   session.context = 7;
   session.ready = true;
   session.slots = slots;
   session.width = 128;
   session.height = 96;
   session.depth = depth;
   session.profile = depth == 8 ? VAProfileVP9Profile0 : VAProfileVP9Profile2;
   for (i = 0; i < slots; i++) session.surfaces[i] = i + 1;
   assert(vaapi_vp9_pool_init(&pool, &session, &ops, 10));
   next(0, -1);
}

static int decode(void)
{
   return vaapi_vp9_pool_decode(&pool, &picture, packet, sizeof(packet));
}

static void rename_exports(unsigned depth)
{
   uint64_t first, repeat, fresh;
   unsigned before;
   setup(depth, 12);
   assert(!decode() && pool.mapping[0] == 0);
   completion[0] = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_vp9_pool_poll(&pool, 0) == 1 && native_syncs == 1);
   assert(!vaapi_vp9_pool_acquire(&pool, 0, &first));
   completion[0] = VA_STATUS_SUCCESS;
   assert(!vaapi_vp9_pool_poll(&pool, 0) && dropped == 3);
   assert(vaapi_vp9_pool_acquire(&pool, 0, &first));
   assert(vaapi_vp9_pool_acquire(&pool, 0, &repeat) && repeat == first);
   next(1, 0);
   assert(!decode() && pool.mapping[1] == 1 && pool.mapping[0] == 0);
   assert(!vaapi_vp9_pool_poll(&pool, 1));
   next(0, 1);
   assert(!decode() && pool.mapping[0] == 2);
   assert(!vaapi_vp9_pool_poll(&pool, 2));
   assert(vaapi_vp9_pool_current(&pool, 0, first) && pool.exports[0] == 2);
   assert(vaapi_vp9_pool_release(&pool, 0, first) && pool.exports[0] == 1);
   assert(vaapi_vp9_pool_held(&pool) & 1);
   assert(!vaapi_vp9_pool_release(&pool, 0, first + 1));
   assert(vaapi_vp9_pool_release(&pool, 0, repeat));
   assert(!vaapi_vp9_pool_current(&pool, 0, first));
   next(3, 0);
   assert(!decode() && pool.mapping[3] == 0);
   assert(!vaapi_vp9_pool_poll(&pool, 0));
   assert(vaapi_vp9_pool_acquire(&pool, 0, &fresh) && fresh != first);
   assert(!vaapi_vp9_pool_release(&pool, 0, first));
   before = calls;
   assert(!vaapi_vp9_pool_close(&pool) && calls == before);
   assert(!vaapi_vp9_pool_terminated(&pool, session.display));
   assert(vaapi_vp9_pool_release(&pool, 0, fresh));
   assert(vaapi_vp9_pool_close(&pool));
}

static void reads(void)
{
   unsigned i;
   setup(8, 12);
   assert(!decode());
   next(1, 0);
   picture.header.frame_type = 1;
   picture.use_prev_frame_mvs = 1;
   for (i = 0; i < 8; i++) picture.reference_slots[i] = 0;
   for (i = 0; i < 3; i++) picture.named_slots[i] = 0;
   assert(!decode() && pool.reads[1] == 1);
   for (i = 0; i < 8; i++) assert(observed.reference_frames[i] == 1);
   assert(!vaapi_vp9_pool_poll(&pool, 0));
   completion[1] = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_vp9_pool_poll(&pool, 1) == 1);
   next(0, 1);
   assert(!decode() && pool.mapping[0] == 2);
   assert(!vaapi_vp9_pool_poll(&pool, 2));
   assert(vaapi_vp9_pool_held(&pool) & 1);
   completion[1] = VA_STATUS_SUCCESS;
   assert(!vaapi_vp9_pool_poll(&pool, 1) && !pool.reads[1]);
   assert(!vaapi_vp9_pool_held(&pool) && vaapi_vp9_pool_close(&pool));
}

static void capacity(void)
{
   uint64_t tickets[32], fresh, generation;
   unsigned i, before;
   int mapping[32];
   setup(10, 32);
   for (i = 0; i < 32; i++)
   {
      next(i % 10, i ? (int)((i - 1) % 10) : -1);
      assert(!decode() && pool.mapping[i % 10] == (int)i);
      assert(!vaapi_vp9_pool_poll(&pool, i));
      assert(vaapi_vp9_pool_acquire(&pool, i, &tickets[i]));
   }
   assert(vaapi_vp9_pool_held(&pool) == UINT32_MAX);
   memcpy(mapping, pool.mapping, sizeof(mapping));
   before = calls;
   generation = pool.next_generation;
   next(2, 1);
   assert(decode() == 1 && calls == before && generation == pool.next_generation);
   assert(!memcmp(mapping, pool.mapping, sizeof(mapping)));
   for (i = 0; i < 32; i++) assert(vaapi_vp9_pool_release(&pool, i, tickets[i]));
   assert(vaapi_vp9_pool_close(&pool));
   assert(vaapi_vp9_pool_init(&pool, &session, &ops, 10));
   next(0, -1);
   assert(!decode() && !vaapi_vp9_pool_poll(&pool, 0));
   assert(vaapi_vp9_pool_acquire(&pool, 0, &fresh) && fresh > tickets[31]);
   assert(!vaapi_vp9_pool_release(&pool, 0, tickets[0]));
   assert(vaapi_vp9_pool_release(&pool, 0, fresh));
   pool.next_generation = (uint64_t)-1;
   before = calls;
   next(1, 0);
   assert(decode() < 0 && calls == before);
   assert(vaapi_vp9_pool_close(&pool));
}

static void failures(void)
{
   uint64_t ticket;
   unsigned before;
   setup(8, 12);
   fault = 2;
   assert(decode() < 0 && pool.broken && pool.transactions[0].active);
   assert(vaapi_vp9_pool_poll(&pool, 0) < 0 && dropped == 2);
   assert(!vaapi_vp9_pool_close(&pool));
   assert(vaapi_vp9_pool_terminated(&pool, session.display));
   setup(8, 12);
   fault = 1000;
   assert(decode() < 0 && pool.transactions[0].poisoned);
   before = calls;
   assert(vaapi_vp9_pool_poll(&pool, 0) < 0 && calls == before);
   assert(!vaapi_vp9_pool_terminated(&pool, (VADisplay)(uintptr_t)2));
   assert(vaapi_vp9_pool_terminated(&pool, session.display));
   setup(8, 12);
   assert(!decode());
   drop_fault = 101;
   assert(!vaapi_vp9_pool_poll(&pool, 0) && dropped == 2 && native_syncs == 1);
   assert(pool.transactions[0].active && vaapi_vp9_pool_held(&pool));
   drop_fault = 0;
   assert(!vaapi_vp9_pool_poll(&pool, 0) && dropped == 3 && native_syncs == 1);
   assert(vaapi_vp9_pool_acquire(&pool, 0, &ticket));
   next(1, 0);
   assert(!decode());
   completion[1] = VA_STATUS_ERROR_DECODING_ERROR;
   assert(vaapi_vp9_pool_poll(&pool, 1) < 0 && pool.broken);
   assert(vaapi_vp9_pool_current(&pool, 0, ticket));
   assert(!vaapi_vp9_pool_acquire(&pool, 0, &ticket));
   assert(!vaapi_vp9_pool_terminated(&pool, session.display));
   assert(vaapi_vp9_pool_release(&pool, 0, ticket));
   assert(vaapi_vp9_pool_terminated(&pool, session.display));
}

static void invalid(void)
{
   setup(8, 12);
   picture.reference_slots[0] = 1;
   assert(decode() < 0 && !calls);
   next(0, -1);
   picture.previous_slot = 10;
   assert(decode() < 0 && !calls);
   next(0, -1);
   picture.header.first_partition_size = 200;
   assert(decode() < 0 && !calls);
   assert(vaapi_vp9_pool_close(&pool));
   session.surfaces[1] = session.surfaces[0];
   assert(!vaapi_vp9_pool_init(&pool, &session, &ops, 10));
   session.surfaces[1] = VA_INVALID_SURFACE;
   assert(!vaapi_vp9_pool_init(&pool, &session, &ops, 10));
}

int main(void)
{
   rename_exports(8);
   rename_exports(10);
   reads();
   capacity();
   failures();
   invalid();
   puts("PASS: VP9 physical renaming, independent leases, read hazards and retirement");
   return 0;
}
