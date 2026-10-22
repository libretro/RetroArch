/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vaapi_vp9_pool.h"
static vaapi_vp9_pool_t pool, saved;
static vaapi_vp9_session_t session;
static rvp9_hw_picture_t picture;
static uint8_t packet[100];
static unsigned live[4096], done[32], made, dropped, checks, fault, target, drop_fault;
static VAStatus create(VADisplay display, VAContextID context,
      VABufferType type, unsigned bytes, unsigned count, void *data,
      VABufferID *id)
{
   assert(display == session.display && context == session.context);
   assert(type == VAPictureParameterBufferType
         || type == VASliceParameterBufferType
         || type == VASliceDataBufferType);
   assert(bytes && count == 1 && data && made < 4096);
   *id = made;
   live[made++] = 1;
   return fault == 1 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus destroy(VADisplay display, VABufferID id)
{
   assert(display == session.display && id < made && live[id]);
   if (id + 1 == drop_fault)
      return VA_STATUS_ERROR_OPERATION_FAILED;
   live[id] = 0;
   dropped++;
   return VA_STATUS_SUCCESS;
}
static VAStatus begin(
      VADisplay display, VAContextID context, VASurfaceID surface)
{
   assert(display == session.display && context == session.context);
   assert(surface == session.surfaces[target]);
   done[target] = 0;
   return fault == 2 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus render(
      VADisplay display, VAContextID context, VABufferID *buffers, int count)
{
   int i;
   assert(display == session.display && context == session.context
         && count == 3);
   for (i = 0; i < count; i++)
      assert(buffers[i] < made && live[buffers[i]]);
   return VA_STATUS_SUCCESS;
}
static VAStatus end(VADisplay display, VAContextID context)
{
   assert(display == session.display && context == session.context);
   return VA_STATUS_SUCCESS;
}
static VAStatus sync_surface(
      VADisplay display, VASurfaceID surface, uint64_t timeout)
{
   assert(display == session.display && surface >= 100 && surface < 132);
   assert(!timeout);
   checks++;
   if (fault == 5)
      return VA_STATUS_ERROR_INVALID_DISPLAY;
   return done[surface - 100] ? VA_STATUS_SUCCESS : VA_STATUS_ERROR_TIMEDOUT;
}
static const vaapi_vp9_submit_ops_t native
      = {create, destroy, begin, render, end, sync_surface};
static void init(unsigned depth)
{
   unsigned i;
   assert(!pool.active && made == dropped);
   memset(&session, 0, sizeof(session));
   session.display = (VADisplay)(uintptr_t)1;
   session.context = 7;
   session.width = session.height = 64;
   session.depth = depth;
   session.profile = depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0;
   session.slots = 10;
   session.ready = true;
   for (i = 0; i < 32; i++)
      session.surfaces[i] = 100 + i;
   assert(vaapi_vp9_pool_init(&pool, &session, &native));
}
static void select_picture(unsigned slot, bool inter)
{
   unsigned i;
   memset(&picture, 0, sizeof(picture));
   picture.slot = (int)slot;
   picture.previous_slot = pool.previous_slot;
   picture.header.w = picture.header.h = 64;
   picture.header.bit_depth = (int)session.depth;
   picture.header.profile = session.depth == 10 ? 2 : 0;
   picture.header.subsampling_x = picture.header.subsampling_y = 1;
   picture.header.show_frame = 1;
   picture.header.uncomp_size = 16;
   picture.header.first_partition_size = 7;
   picture.header.frame_type = inter;
   for (i = 0; i < 8; i++)
      picture.reference_slots[i] = inter ? 0 : -1;
   for (i = 0; i < 3; i++)
   {
      picture.named_slots[i] = inter ? 0 : -1;
      picture.header.ref_idx[i] = (int)i;
   }
   target = slot;
}
int main(void)
{
   unsigned depth, before;
   uint64_t old_ticket, ticket = 999;
   for (depth = 8; depth <= 10; depth += 2)
   {
      init(depth);
      select_picture(0, false);
      assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture)
            == 0);
      assert(!vaapi_vp9_pool_acquire(&pool, 0, &ticket) && ticket == 999);
      before = checks;
      assert(vaapi_vp9_pool_poll(&pool, 0) == 1 && checks == before + 1);
      select_picture(8, true);
      saved = pool;
      picture.reference_slots[0] = 31;
      before = made;
      assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture) == -1);
      assert(made == before && !memcmp(&saved, &pool, sizeof(pool)));
      select_picture(1, true);
      assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture)
            == 0);
      assert(pool.reads[1] == 1 && (vaapi_vp9_pool_held(&pool) & 3) == 3);
      select_picture(0, false);
      saved = pool;
      before = made;
      assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture)
            == 1);
      assert(made == before && !memcmp(&saved, &pool, sizeof(pool)));
      done[0] = 1;
      drop_fault = pool.transactions[0].buffers[1] + 1;
      assert(vaapi_vp9_pool_poll(&pool, 0) == 0 && pool.transactions[0].active);
      before = checks;
      assert(vaapi_vp9_pool_poll(&pool, 0) == 0 && checks == before);
      drop_fault = 0;
      assert(vaapi_vp9_pool_poll(&pool, 0) == 0 && checks == before);

      assert(vaapi_vp9_pool_acquire(&pool, 0, &old_ticket));
      assert(!vaapi_vp9_pool_release(&pool, 0, old_ticket + 1));
      assert(vaapi_vp9_pool_release(&pool, 0, old_ticket));
      assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture)
            == 1);
      done[1] = 1;
      assert(vaapi_vp9_pool_poll(&pool, 1) == 0 && !pool.reads[1]);
      assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture)
            == 0);
      done[0] = 1;
      assert(vaapi_vp9_pool_poll(&pool, 0) == 0);
      assert(vaapi_vp9_pool_acquire(&pool, 0, &ticket) && ticket > old_ticket);
      assert(!vaapi_vp9_pool_release(&pool, 0, old_ticket));
      assert(!vaapi_vp9_pool_close(&pool));
      assert(!vaapi_vp9_pool_terminated(&pool, session.display));
      assert(vaapi_vp9_pool_release(&pool, 0, ticket));
      assert(!vaapi_vp9_pool_release(&pool, 0, ticket));
      assert(vaapi_vp9_pool_close(&pool) && made == dropped);
      init(depth);
      select_picture(0, false);
      assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture)
            == 0);
      done[0] = 1;
      assert(vaapi_vp9_pool_poll(&pool, 0) == 0);
      assert(vaapi_vp9_pool_acquire(&pool, 0, &old_ticket)
            && old_ticket > ticket);
      assert(vaapi_vp9_pool_release(&pool, 0, old_ticket));
      assert(vaapi_vp9_pool_close(&pool));
      ticket = 999;
   }
   init(8);
   select_picture(0, false);
   fault = 1;
   assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture) == -1);
   assert(pool.broken && vaapi_vp9_pool_poll(&pool, 0) == -1);
   assert(!vaapi_vp9_pool_close(&pool));
   assert(!vaapi_vp9_pool_terminated(&pool, (VADisplay)(uintptr_t)99));
   assert(vaapi_vp9_pool_terminated(&pool, session.display));
   fault = 0;
   init(8);
   select_picture(0, false);
   assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture) == 0);
   fault = 5;
   assert(vaapi_vp9_pool_poll(&pool, 0) == -1 && pool.broken);
   assert(!vaapi_vp9_pool_close(&pool) && vaapi_vp9_pool_held(&pool));
   memset(live, 0, sizeof(live));
   dropped = made;
   assert(vaapi_vp9_pool_terminated(&pool, session.display));
   fault = 0;
   pool.next_generation = (uint64_t)-1;
   assert(!vaapi_vp9_pool_init(&pool, &session, &native));
   puts("PASS: VA VP9 pending reads/writes, export generations and termination "
        "recovery");
   return 0;
}
