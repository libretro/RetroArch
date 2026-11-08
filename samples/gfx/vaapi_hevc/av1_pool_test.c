/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_av1_pool.h"
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE

static vaapi_av1_pool_t pool;
static vaapi_av1_session_t session;
static rav1_hw_picture_t picture;
static rav1_sequence_t sequence;
static const uint8_t packet[] = {0x32, 8, 0, 0, 0, 0, 0, 0, 0, 0};
static VADecPictureParameterBufferAV1 observed;
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
            && size == sizeof(VASliceParameterBufferAV1));
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

static const vaapi_av1_submit_ops_t ops = {
      create, destroy, begin, render, end, sync_surface};

static void next(unsigned logical, int previous)
{
   unsigned i;
   memset(&picture, 0, sizeof(picture));
   picture.header.frame_is_intra = picture.header.show_frame = 1;
   picture.header.primary_ref_frame = 7;
   picture.header.refresh_frame_flags = 255;
   picture.setup.size.width = picture.setup.size.upscaled_width =
      picture.setup.size.render_width = 192;
   picture.setup.size.height = picture.setup.size.render_height = 128;
   picture.setup.size.mi_cols = 48;
   picture.setup.size.mi_rows = 32;
   picture.setup.size.superres_denom = 8;
   picture.tiles.layout.columns = picture.tiles.layout.rows = 1;
   picture.tiles.mi_col_starts[1] = 48;
   picture.tiles.mi_row_starts[1] = 32;
   picture.picture.cdef_damping = 3;
   picture.slot = (int)logical;
   for (i = 0; i < 8; i++) picture.reference_slots[i] = -1;
   if (previous >= 0)
   {
      picture.reference_slots[0] = previous;
      picture.reference_info[0].valid = 1;
   }
   for (i = 0; i < 7; i++)
      picture.motion.global.params[i][2] = picture.motion.global.params[i][5] = 65536;
   assert(rav1_tile_group_init(&picture.tile_group, packet + 3,
         sizeof(packet) - 3, &picture.tiles.layout, true));
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
   session.width = 192;
   session.height = 128;
   session.depth = depth;
   session.profile = VAProfileAV1Profile0;
   for (i = 0; i < slots; i++) session.surfaces[i] = i + 1;
   assert(vaapi_av1_pool_init(&pool, &session, &ops, slots == 9 ? 9 : 10));
   memset(&sequence, 0, sizeof(sequence));
   sequence.operating_point_count = 1;
   sequence.bit_depth = depth;
   sequence.subsampling_x = sequence.subsampling_y = 1;
   sequence.max_width = 192;
   sequence.max_height = 128;
   sequence.frame_width_bits = 8;
   sequence.frame_height_bits = 7;
   next(0, -1);
}

static int decode(void)
{
   return vaapi_av1_pool_decode(&pool, &sequence, &picture, packet, sizeof(packet));
}

static void rename_exports(unsigned depth)
{
   uint64_t first, repeat, fresh;
   unsigned before;
   setup(depth, 12);
   assert(!decode() && pool.mapping[0] == 0);
   completion[0] = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_av1_pool_poll(&pool, 0) == 1 && native_syncs == 1);
   assert(!vaapi_av1_pool_acquire(&pool, 0, &first));
   completion[0] = VA_STATUS_SUCCESS;
   assert(!vaapi_av1_pool_poll(&pool, 0) && dropped == 3);
   assert(vaapi_av1_pool_acquire(&pool, 0, &first));
   assert(vaapi_av1_pool_acquire(&pool, 0, &repeat) && repeat == first);
   next(1, 0);
   assert(!decode() && pool.mapping[1] == 1 && pool.mapping[0] == 0);
   assert(!vaapi_av1_pool_poll(&pool, 1));
   next(0, 1);
   assert(!decode() && pool.mapping[0] == 2);
   assert(!vaapi_av1_pool_poll(&pool, 2));
   assert(vaapi_av1_pool_current(&pool, 0, first) && pool.exports[0] == 2);
   assert(vaapi_av1_pool_release(&pool, 0, first) && pool.exports[0] == 1);
   assert(vaapi_av1_pool_held(&pool) & 1);
   assert(!vaapi_av1_pool_release(&pool, 0, first + 1));
   assert(vaapi_av1_pool_release(&pool, 0, repeat));
   assert(!vaapi_av1_pool_current(&pool, 0, first));
   next(3, 0);
   assert(!decode() && pool.mapping[3] == 0);
   assert(!vaapi_av1_pool_poll(&pool, 0));
   assert(vaapi_av1_pool_acquire(&pool, 0, &fresh) && fresh != first);
   assert(!vaapi_av1_pool_release(&pool, 0, first));
   before = calls;
   assert(!vaapi_av1_pool_close(&pool) && calls == before);
   assert(!vaapi_av1_pool_terminated(&pool, session.display));
   assert(vaapi_av1_pool_release(&pool, 0, fresh));
   assert(vaapi_av1_pool_close(&pool));
}

static void reads(void)
{
   unsigned i;
   setup(8, 12);
   assert(!decode());
   next(1, 0);
   picture.header.frame_type = 1;
   picture.header.frame_is_intra = 0;
   picture.header.primary_ref_frame = 0;
   picture.setup.valid_mask = 255;
   for (i = 0; i < 8; i++)
   {
      picture.reference_slots[i] = 0;
      picture.reference_info[i].valid = 1;
   }
   for (i = 0; i < 7; i++) picture.setup.ref_frame_idx[i] = i;
   assert(!decode() && pool.reads[1] == 1);
   for (i = 0; i < 8; i++) assert(observed.ref_frame_map[i] == 1);
   assert(!vaapi_av1_pool_poll(&pool, 0));
   completion[1] = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_av1_pool_poll(&pool, 1) == 1);
   next(0, 1);
   assert(!decode() && pool.mapping[0] == 2);
   assert(!vaapi_av1_pool_poll(&pool, 2));
   assert(vaapi_av1_pool_held(&pool) & 1);
   completion[1] = VA_STATUS_SUCCESS;
   assert(!vaapi_av1_pool_poll(&pool, 1) && !pool.reads[1]);
   assert(!vaapi_av1_pool_held(&pool) && vaapi_av1_pool_close(&pool));
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
      assert(!vaapi_av1_pool_poll(&pool, i));
      assert(vaapi_av1_pool_acquire(&pool, i, &tickets[i]));
   }
   assert(vaapi_av1_pool_held(&pool) == UINT32_MAX);
   memcpy(mapping, pool.mapping, sizeof(mapping));
   before = calls;
   generation = pool.next_generation;
   next(2, 1);
   assert(decode() == 1 && calls == before && generation == pool.next_generation);
   assert(!memcmp(mapping, pool.mapping, sizeof(mapping)));
   for (i = 0; i < 32; i++) assert(vaapi_av1_pool_release(&pool, i, tickets[i]));
   assert(vaapi_av1_pool_close(&pool));
   assert(vaapi_av1_pool_init(&pool, &session, &ops, 10));
   next(0, -1);
   assert(!decode() && !vaapi_av1_pool_poll(&pool, 0));
   assert(vaapi_av1_pool_acquire(&pool, 0, &fresh) && fresh > tickets[31]);
   assert(!vaapi_av1_pool_release(&pool, 0, tickets[0]));
   assert(vaapi_av1_pool_release(&pool, 0, fresh));
   pool.next_generation = (uint64_t)-1;
   before = calls;
   next(1, 0);
   assert(decode() < 0 && calls == before);
   assert(vaapi_av1_pool_close(&pool));
}

static void failures(void)
{
   uint64_t ticket;
   unsigned before;
   setup(8, 12);
   fault = 2;
   assert(decode() < 0 && pool.broken && pool.transactions[0].active);
   assert(vaapi_av1_pool_poll(&pool, 0) < 0 && dropped == 2);
   assert(!vaapi_av1_pool_close(&pool));
   assert(vaapi_av1_pool_terminated(&pool, session.display));
   setup(8, 12);
   fault = 1000;
   assert(decode() < 0 && pool.transactions[0].poisoned);
   before = calls;
   assert(vaapi_av1_pool_poll(&pool, 0) < 0 && calls == before);
   assert(!vaapi_av1_pool_terminated(&pool, (VADisplay)(uintptr_t)2));
   assert(vaapi_av1_pool_terminated(&pool, session.display));
   setup(8, 12);
   assert(!decode());
   drop_fault = 101;
   assert(!vaapi_av1_pool_poll(&pool, 0) && dropped == 2 && native_syncs == 1);
   assert(pool.transactions[0].active && vaapi_av1_pool_held(&pool));
   drop_fault = 0;
   assert(!vaapi_av1_pool_poll(&pool, 0) && dropped == 3 && native_syncs == 1);
   assert(vaapi_av1_pool_acquire(&pool, 0, &ticket));
   next(1, 0);
   assert(!decode());
   completion[1] = VA_STATUS_ERROR_DECODING_ERROR;
   assert(vaapi_av1_pool_poll(&pool, 1) < 0 && pool.broken);
   assert(vaapi_av1_pool_current(&pool, 0, ticket));
   assert(!vaapi_av1_pool_acquire(&pool, 0, &ticket));
   assert(!vaapi_av1_pool_terminated(&pool, session.display));
   assert(vaapi_av1_pool_release(&pool, 0, ticket));
   assert(vaapi_av1_pool_terminated(&pool, session.display));
}

static void eight_references(void)
{
   unsigned i, j, before;
   setup(10, 9);
   assert(vaapi_av1_pool_close(&pool));
   assert(vaapi_av1_pool_init(&pool, &session, &ops, 9));
   for (i = 0; i < 8; i++)
   {
      next(i, -1);
      for (j = 0; j < i; j++)
      {
         picture.reference_slots[j] = (int)j;
         picture.reference_info[j].valid = 1;
      }
      assert(!decode() && pool.mapping[i] == (int)i);
      assert(!vaapi_av1_pool_poll(&pool, i));
   }
   next(8, -1);
   picture.header.frame_type = RAV1_FRAME_INTER;
   picture.header.frame_is_intra = 0;
   picture.header.primary_ref_frame = 0;
   picture.setup.valid_mask = 255;
   for (i = 0; i < 8; i++)
   {
      picture.reference_slots[i] = (int)i;
      picture.reference_info[i].valid = 1;
   }
   for (i = 0; i < 7; i++) picture.setup.ref_frame_idx[i] = i;
   assert(!decode() && pool.mapping[8] == 8 && pool.reads[8] == 255);
   for (i = 0; i < 8; i++) assert(observed.ref_frame_map[i] == i + 1);
   next(0, -1);
   before = calls;
   assert(decode() == 1 && calls == before);
   assert(!vaapi_av1_pool_poll(&pool, 8));
   assert(!decode() && pool.mapping[0] == 0);
   assert(!vaapi_av1_pool_poll(&pool, 0));
   assert(vaapi_av1_pool_close(&pool));
}

static void invalid(void)
{
   setup(8, 12);
   picture.reference_slots[0] = 1;
   assert(decode() < 0 && !calls);
   next(0, -1);
   picture.reference_slots[0] = 10;
   assert(decode() < 0 && !calls);
   next(0, -1);
   picture.setup.size.width = 200;
   assert(decode() < 0 && !calls);
   next(0, -1);
   sequence.bit_depth = 10;
   assert(decode() < 0 && !calls);
   sequence.bit_depth = 8;
   sequence.max_width++;
   assert(decode() < 0 && !calls);
   sequence.max_width--;
   assert(vaapi_av1_pool_decode(&pool, &sequence, &picture,
         (const uint8_t *)&pool.params, sizeof(packet)) < 0 && !calls);
   assert(vaapi_av1_pool_close(&pool));
   assert(!vaapi_av1_pool_init(&pool, &session, &ops, 8));
   assert(!vaapi_av1_pool_init(&pool, &session, &pool.ops, 10));
   assert(!vaapi_av1_pool_init(&pool,
         (const vaapi_av1_session_t *)&pool, &ops, 10));
   session.surfaces[1] = session.surfaces[0];
   assert(!vaapi_av1_pool_init(&pool, &session, &ops, 10));
   session.surfaces[1] = VA_INVALID_SURFACE;
   assert(!vaapi_av1_pool_init(&pool, &session, &ops, 10));
}

int main(void)
{
   rename_exports(8);
   rename_exports(10);
   reads();
   eight_references();
   capacity();
   failures();
   invalid();
   puts("PASS: AV1 physical renaming, independent leases, read hazards and retirement");
   return 0;
}

#else
int main(void)
{
   puts("SKIP: VA AV1 parameter SDK unavailable");
   return 0;
}
#endif
