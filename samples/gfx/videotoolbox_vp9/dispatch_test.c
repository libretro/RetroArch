/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define DECODER_MAIN_NAME decoder_contract_main
#include "decoder_test.c"
#ifdef VIDEOTOOLBOX_VP9_OWNER
struct mock_dispatch
{
   struct mock_dispatch *queue;
   void *context;
   void (*event)(void *), (*cancel)(void *);
   unsigned refs, pending, resumed, canceled, delivered, armed;
   int type;
};
static struct mock_dispatch *sources[2];
static unsigned dispatch_objects, fail_queue, fail_sources, timer_arms;

void dispatch_retain(void *object)
{
   ((struct mock_dispatch *)object)->refs++;
}

void dispatch_release(void *object)
{
   struct mock_dispatch *p = (struct mock_dispatch *)object;
   unsigned i;
   assert(p && p->refs);
   if (--p->refs)
      return;
   for (i = 0; i < 2; i++)
      if (sources[i] == p)
         sources[i] = NULL;
   if (p->queue)
      dispatch_release(p->queue);
   dispatch_objects--;
   free(p);
}

dispatch_queue_t dispatch_queue_create(const char *name, void *attributes)
{
   struct mock_dispatch *p;
   assert(name && !attributes);
   if (fail_queue)
      return NULL;
   p = (struct mock_dispatch *)calloc(1, sizeof(*p));
   assert(p);
   p->refs = 1;
   dispatch_objects++;
   return p;
}

dispatch_source_t dispatch_source_create(
      int type, uintptr_t handle, unsigned long mask, dispatch_queue_t queue)
{
   struct mock_dispatch *p;
   assert(type >= 1 && type <= 2 && !handle && !mask && queue);
   if (fail_sources & (1u << (type - 1)))
      return NULL;
   p = (struct mock_dispatch *)calloc(1, sizeof(*p));
   assert(p && !sources[type - 1]);
   p->refs  = 1;
   p->type  = type;
   p->queue = queue;
   dispatch_retain(queue);
   sources[type - 1] = p;
   dispatch_objects++;
   return p;
}

void dispatch_set_context(void *object, void *context)
{
   ((struct mock_dispatch *)object)->context = context;
}

void dispatch_source_set_event_handler_f(dispatch_source_t p, void (*f)(void *))
{
   p->event = f;
}

void dispatch_source_set_cancel_handler_f(
      dispatch_source_t p, void (*f)(void *))
{
   p->cancel = f;
}

void dispatch_source_set_timer(dispatch_source_t p, dispatch_time_t start,
      uint64_t interval, uint64_t leeway)
{
   assert(p->type == 2 && interval == DISPATCH_TIME_FOREVER && !leeway);
   p->armed = start != DISPATCH_TIME_FOREVER;
   if (p->armed)
   {
      assert(start == 16 * NSEC_PER_MSEC);
      timer_arms++;
   }
}

void dispatch_resume(void *object)
{
   struct mock_dispatch *p = (struct mock_dispatch *)object;
   assert(!p->resumed);
   p->resumed = 1;
}

void dispatch_source_cancel(dispatch_source_t p)
{
   assert(p->resumed);
   p->canceled = 1;
}

void dispatch_source_merge_data(dispatch_source_t p, unsigned long value)
{
   assert(p && value && p->resumed);
   if (!p->canceled)
      p->pending++;
}

dispatch_time_t dispatch_time(dispatch_time_t base, int64_t delta)
{
   assert(base == DISPATCH_TIME_NOW);
   return (dispatch_time_t)delta;
}

static unsigned turn(bool timer)
{
   unsigned i, count = 0;
   for (i = 0; i < 2; i++)
   {
      struct mock_dispatch *p = sources[i];
      if (!p)
         continue;
      dispatch_retain(p);
      if (p->canceled && !p->delivered)
      {
         p->delivered = 1;
         p->cancel(p->context);
         count++;
      }
      else if (!p->canceled && (p->pending || (timer && p->armed)))
      {
         p->pending = p->armed = 0;
         p->event(p->context);
         count++;
      }
      dispatch_release(p);
   }
   return count;
}

static bool open_dispatch(rvp9_hw_ops_t *ops)
{
   return videotoolbox_vp9_dispatch_open(
         &header, &color, 1, 10, 64, &pool, NULL, ops);
}

#ifndef DISPATCH_MAIN_NAME
#define DISPATCH_MAIN_NAME main
#endif
int DISPATCH_MAIN_NAME(void)
{
   videotoolbox_vp9_owner_t *init = open_owner();
   videotoolbox_vp9_decoder_t *d;
   rvp9_hw_ops_t ops;
   video_hw_frame_t frame;
   unsigned failure;
   close_owner(&init);
   for (failure = 1; failure <= 3; failure++)
   {
      fail_sources = failure;
      assert(!open_dispatch(&ops) && !ops.decoder && !pool.freed);
      turn(false);
      turn(false);
      assert(!dispatch_objects && !retro_atomic_load_acquire_int(&objects));
   }
   fail_sources   = 0;
   header.profile = 1;
   assert(!open_dispatch(&ops) && !ops.decoder && !pool.freed);
   turn(false);
   turn(false);
   assert(!dispatch_objects);
   header.profile = 0;
   fail_queue     = 1;
   assert(!open_dispatch(&ops) && !dispatch_objects);
   fail_queue = 0;
   assert(open_dispatch(&ops));
   d                         = (videotoolbox_vp9_decoder_t *)ops.decoder;
   picture.header            = header;
   picture.header.show_frame = 1;
   picture.slot              = 0;
   assert(!ops.decode(d, (const uint8_t *)"vp9", 3, &picture));
   assert(!ops.decode(d, (const uint8_t *)"vp9", 3, &picture));
   assert(ops.output(d, 0, &frame) == 1);
   assert(turn(false) == 1);
   assert(ops.output(d, 0, &frame) == 1);
   assert(turn(false) == 1);
   assert(!ops.output(d, 0, &frame));
   videotoolbox_vp9_decoder_close(&ops);
   assert(!ops.decoder && !retired && pool.freed);
   turn(false);
   turn(false);
   assert(!dispatch_objects);
   video_hw_frame_abandon(&frame);
   assert(!retro_atomic_load_acquire_int(&objects));

   pool.freed  = 0;
   owner_delay = 1;
   assert(open_dispatch(&ops));
   d = (videotoolbox_vp9_decoder_t *)ops.decoder;
   assert(!ops.decode(d, (const uint8_t *)"vp9", 3, &picture));
   turn(false);
   owner_delayed_session->callback.decompressionOutputCallback(d->owner,
         owner_delayed_cookie, 0, kVTDecodeInfo_FrameDropped, NULL,
         kCMTimeInvalid, kCMTimeInvalid);
   videotoolbox_vp9_decoder_close(&ops);
   turn(false);
   assert(dispatch_objects == 3 && timer_arms == 1);
   assert(!turn(false));
   CFRelease(delayed_sample);
   delayed_sample = NULL;
   owner_delay    = 0;
   turn(true);
   turn(false);
   turn(false);
   assert(!dispatch_objects && !retro_atomic_load_acquire_int(&objects));
   puts("PASS: VP9 executor coalesced wake, cancellation/partial setup, deferred retirement and preallocated timer; mock dispatch only");
   return 0;
}
#endif
