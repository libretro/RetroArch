/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VA AV1 pool tests require assertions
#endif
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc_submit.c"
#include "../../../gfx/common/vaapi_av1_submit.c"
#include "../../../gfx/common/vaapi_av1_pool.c"
static vaapi_av1_pool_t pool;
static vaapi_av1_session_t session;
static rav1_sequence_t sequence;
static rav1_hw_picture_t picture;
static unsigned created, destroyed, reference_queries, display_queries;
static VAStatus reference_status, display_status;
static const uint8_t packet[] = {0x32, 4, 0, 1, 2, 3};
static VAStatus create(VADisplay d, VAContextID c, VABufferType type,
      unsigned bytes, unsigned count, void *data, VABufferID *id)
{
   assert(d == session.display && c == session.context && bytes && count
         && data);
   assert(type == VAPictureParameterBufferType
         || type == VASliceParameterBufferType
         || type == VASliceDataBufferType);
   *id = ++created;
   return VA_STATUS_SUCCESS;
}
static VAStatus destroy(VADisplay d, VABufferID id)
{
   assert(d == session.display && id && id <= created);
   destroyed++;
   return VA_STATUS_SUCCESS;
}
static VAStatus begin(VADisplay d, VAContextID c, VASurfaceID surface)
{
   assert(d == session.display && c == session.context && surface >= 1000);
   return VA_STATUS_SUCCESS;
}
static VAStatus render(VADisplay d, VAContextID c, VABufferID *ids, int count)
{
   assert(d == session.display && c == session.context && ids && count == 3);
   return VA_STATUS_SUCCESS;
}
static VAStatus end(VADisplay d, VAContextID c)
{
   assert(d == session.display && c == session.context);
   return VA_STATUS_SUCCESS;
}
static VAStatus sync_surface(VADisplay d, VASurfaceID surface, uint64_t timeout)
{
   assert(d == session.display && !timeout);
   if (surface == 1001)
   {
      display_queries++;
      return display_status;
   }
   reference_queries++;
   return reference_status;
}
static const vaapi_av1_submit_ops_t ops
      = {create, destroy, begin, render, end, sync_surface};
static void reset(void)
{
   unsigned i;
   assert(!pool.active);
   memset(&session, 0, sizeof(session));
   memset(&sequence, 0, sizeof(sequence));
   memset(&picture, 0, sizeof(picture));
   session.display = (VADisplay)&session;
   session.context = 1;
   session.ready = true;
   session.slots = 10;
   session.width = sequence.max_width = 128;
   session.height = sequence.max_height = 64;
   session.depth = sequence.bit_depth = 8;
   session.profile = VAProfileAV1Profile0;
   sequence.operating_point_count = 1;
   sequence.subsampling_x = sequence.subsampling_y = 1;
   sequence.film_grain_params_present = 1;
   picture.header.frame_is_intra = 1;
   picture.header.primary_ref_frame = 7;
   picture.setup.size.width = picture.setup.size.upscaled_width = 128;
   picture.setup.size.height = 64;
   picture.setup.size.superres_denom = 8;
   picture.tiles.layout.columns = picture.tiles.layout.rows = 1;
   picture.tiles.mi_col_starts[1] = 32;
   picture.tiles.mi_row_starts[1] = 16;
   picture.grain.params.apply = 1;
   picture.grain.params.scaling_shift = 8;
   picture.grain.params.ar_coeff_shift = 6;
   for (i = 0; i < 10; i++)
      session.surfaces[i] = 1000 + i;
   for (i = 0; i < 8; i++)
      picture.reference_slots[i] = -1;
   assert(rav1_tile_group_init(&picture.tile_group, packet + 2,
         sizeof(packet) - 2, &picture.tiles.layout, true));
   reference_status = display_status = VA_STATUS_SUCCESS;
   created = destroyed = reference_queries = display_queries = 0;
   assert(vaapi_av1_pool_init(&pool, &session, &ops));
}
static int decode(unsigned display_slot)
{
   return vaapi_av1_pool_decode(
         &pool, packet, sizeof(packet), &sequence, &picture, display_slot);
}
int main(void)
{
   uint64_t ticket = 0, later = 0;
   unsigned before;
   reset();
   assert(!decode(1));
   assert(vaapi_av1_pool_held(&pool) == 3);
   assert(!vaapi_av1_pool_acquire(&pool, 1, &ticket) && !ticket);
   display_status = VA_STATUS_ERROR_TIMEDOUT;
   assert(vaapi_av1_pool_poll(&pool, 0) == 1);
   assert(!vaapi_av1_pool_close(&pool) && !destroyed);
   before = created;
   assert(decode(1) == 1 && created == before);
   display_status = VA_STATUS_SUCCESS;
   assert(!vaapi_av1_pool_poll(&pool, 0));
   assert(destroyed == 3 && !vaapi_av1_pool_held(&pool));
   assert(reference_queries == 1 && display_queries == 2);
   assert(vaapi_av1_pool_acquire(&pool, 1, &ticket));
   assert(!vaapi_av1_pool_acquire(&pool, 1, &later) && !later);
   assert(decode(1) == 1 && created == before);
   picture.slot = 2;
   picture.reference_slots[0] = 0;
   assert(!decode(3));
   assert((vaapi_av1_pool_held(&pool) & 13) == 13);
   picture.slot = 0;
   picture.reference_slots[0] = -1;
   assert(decode(4) == 1);
   picture.slot = 2;
   picture.reference_slots[0] = 0;
   assert(!vaapi_av1_pool_poll(&pool, 2));
   picture.slot = 4;
   picture.reference_slots[0] = 1;
   assert(decode(5) == -1);
   assert(vaapi_av1_pool_release(&pool, 1, ticket));
   picture.slot = 0;
   picture.reference_slots[0] = -1;
   assert(!decode(1));
   assert(!vaapi_av1_pool_poll(&pool, 0));
   assert(vaapi_av1_pool_acquire(&pool, 1, &later) && later != ticket);
   assert(!vaapi_av1_pool_release(&pool, 1, ticket));
   assert(!vaapi_av1_pool_close(&pool));
   assert(!vaapi_av1_pool_terminated(&pool, session.display));
   assert(vaapi_av1_pool_release(&pool, 1, later));
   assert(vaapi_av1_pool_close(&pool));
   reset();
   assert(!decode(1));
   display_status = VA_STATUS_ERROR_OPERATION_FAILED;
   assert(vaapi_av1_pool_poll(&pool, 0) == -1 && pool.broken);
   assert(!vaapi_av1_pool_close(&pool) && !destroyed);
   assert(!vaapi_av1_pool_terminated(&pool, (VADisplay)&picture));
   assert(vaapi_av1_pool_terminated(&pool, session.display));
   reset();
   assert(vaapi_av1_pool_close(&pool));
   session.slots = 32;
   for (before = 0; before < 32; before++)
      session.surfaces[before] = 1000 + before;
   assert(vaapi_av1_pool_init(&pool, &session, &ops));
   picture.slot = 31;
   assert(!decode(30));
   assert(vaapi_av1_pool_held(&pool) == UINT32_C(0xc0000000));
   assert(!vaapi_av1_pool_poll(&pool, 31));
   assert(vaapi_av1_pool_acquire(&pool, 30, &ticket));
   assert(vaapi_av1_pool_release(&pool, 30, ticket));
   assert(vaapi_av1_pool_close(&pool));
   puts("PASS: AV1 dual writes, reference reads, retained exports, incarnation "
        "tickets and termination; no native device execution");
   return 0;
}
