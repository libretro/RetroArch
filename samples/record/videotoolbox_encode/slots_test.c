/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <retro_atomic.h>
typedef struct object { retro_atomic_int_t refs; } object_t;
typedef object_t *CVPixelBufferRef;
typedef object_t *CMSampleBufferRef;
typedef int OSStatus;
typedef unsigned VTEncodeInfoFlags;
static const void *CFRetain(const void *p)
{ retro_atomic_fetch_add_int(&((object_t*)p)->refs, 1); return p; }
static void CFRelease(const void *p)
{ assert(retro_atomic_fetch_sub_int(&((object_t*)p)->refs, 1) > 1); }
static CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef p)
{ return (CVPixelBufferRef)CFRetain(p); }
static void CVPixelBufferRelease(CVPixelBufferRef p) { CFRelease(p); }
#define VIDEOTOOLBOX_ENCODE_TEST_TYPES
#include "../../../gfx/common/videotoolbox_encode_slots.h"
#ifdef VT_ENCODE_THREAD_TEST
#include <pthread.h>
static void *complete(void *ticket)
{
   videotoolbox_encode_slot_t *slot = (videotoolbox_encode_slot_t*)ticket;
   videotoolbox_encode_slots_output(NULL, slot, 0, 0, slot->input);
   return NULL;
}
#endif
int main(void)
{
   videotoolbox_encode_slots_t *pool;
   videotoolbox_encode_slot_t *ticket[3];
   const videotoolbox_encode_slot_t *result;
   object_t input, sample;
   unsigned i;
   retro_atomic_int_init(&input.refs, 1); retro_atomic_int_init(&sample.refs, 1);
   assert(!videotoolbox_encode_slots_new(0));
   assert(!videotoolbox_encode_slots_new(VIDEOTOOLBOX_ENCODE_SLOTS + 1));
   pool = videotoolbox_encode_slots_new(3); assert(pool);
   assert(!videotoolbox_encode_slots_begin(pool, 3, &input));
   assert(!videotoolbox_encode_slots_begin(pool, 0, NULL));
   for (i = 0; i < 3; i++)
   {
      ticket[i] = videotoolbox_encode_slots_begin(pool, i, &input); assert(ticket[i]);
      assert(!videotoolbox_encode_slots_begin(pool, i, &input));
      assert(!videotoolbox_encode_slots_peek(pool, i));
      assert(!videotoolbox_encode_slots_ack(pool, i));
   }
   /* Synchronous and out-of-order callback publication do not recycle slots. */
   videotoolbox_encode_slots_output(NULL, ticket[2], 0, 0, &sample);
   assert(!videotoolbox_encode_slots_reject(ticket[2]));
   result = videotoolbox_encode_slots_peek(pool, 2);
   assert(result && result->sample == &sample && result->input == &input);
   assert(videotoolbox_encode_slots_peek(pool, 2) == result);
   assert(retro_atomic_load_acquire_int(&sample.refs) == 2);
   assert(videotoolbox_encode_slots_ack(pool, 2));
   assert(!videotoolbox_encode_slots_ack(pool, 2));
   videotoolbox_encode_slots_output(NULL, ticket[0], -123, 4, &sample);
   result = videotoolbox_encode_slots_peek(pool, 0);
   assert(result && result->status == -123 && result->flags == 4 && !result->sample);
   assert(videotoolbox_encode_slots_ack(pool, 0));
   videotoolbox_encode_slots_output(NULL, ticket[1], 0, 2, NULL);
   result = videotoolbox_encode_slots_peek(pool, 1);
   assert(result && !result->status && result->flags == 2 && !result->sample);
   assert(videotoolbox_encode_slots_ack(pool, 1));
   ticket[0] = videotoolbox_encode_slots_begin(pool, 0, &input);
   assert(videotoolbox_encode_slots_reject(ticket[0]));
   assert(!videotoolbox_encode_slots_reject(ticket[0]));
   ticket[1] = videotoolbox_encode_slots_begin(pool, 1, &input);
   videotoolbox_encode_slots_close(pool);
   assert(!videotoolbox_encode_slots_begin(pool, 0, &input));
   videotoolbox_encode_slots_quiesce(pool);
   assert(!videotoolbox_encode_slots_peek(pool, 1));
   videotoolbox_encode_slots_release(pool);
   assert(retro_atomic_load_acquire_int(&input.refs) == 1);
   /* Late callback owns the pool even after the controller has retired. */
   pool = videotoolbox_encode_slots_new(1); assert(pool);
   ticket[0] = videotoolbox_encode_slots_begin(pool, 0, &input);
   videotoolbox_encode_slots_close(pool); videotoolbox_encode_slots_release(pool);
   videotoolbox_encode_slots_output(NULL, ticket[0], 0, 0, &sample);
   assert(retro_atomic_load_acquire_int(&input.refs) == 1);
   assert(retro_atomic_load_acquire_int(&sample.refs) == 1);
#ifdef VT_ENCODE_THREAD_TEST
   pool = videotoolbox_encode_slots_new(1); assert(pool);
   for (i = 0; i < 1000; i++)
   {
      pthread_t thread;
      ticket[0] = videotoolbox_encode_slots_begin(pool, 0, &input); assert(ticket[0]);
      assert(!pthread_create(&thread, NULL, complete, ticket[0]));
      while (!videotoolbox_encode_slots_peek(pool, 0)) { }
      assert(videotoolbox_encode_slots_ack(pool, 0));
      assert(!pthread_join(thread, NULL));
   }
   videotoolbox_encode_slots_release(pool);
#endif
   assert(retro_atomic_load_acquire_int(&input.refs) == 1);
   assert(retro_atomic_load_acquire_int(&sample.refs) == 1);
   puts("PASS: bounded VT encode callback ownership, errors, drops and retirement");
   return 0;
}
