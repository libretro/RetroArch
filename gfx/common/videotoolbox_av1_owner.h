/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_AV1_OWNER_H
#define RETROARCH_VIDEOTOOLBOX_AV1_OWNER_H
#include <retro_spsc.h>
#include "videotoolbox_av1_slots.h"
#ifdef VIDEOTOOLBOX_AV1_SLOTS
#define VIDEOTOOLBOX_AV1_OWNER 1

typedef struct videotoolbox_av1_work
{
   videotoolbox_av1_packet_t packet;
   videotoolbox_av1_sample_t sample;
   void *cookie;
   retro_atomic_int_t submitted, hidden;
   int taken, discard;
} videotoolbox_av1_work_t;

typedef struct videotoolbox_av1_owner
{
   retro_spsc_t queue;
   videotoolbox_av1_work_t work[32];
   CMVideoFormatDescriptionRef format;
   VTDecompressionSessionRef session;
   videotoolbox_av1_slots_t *slots;
   uint8_t *storage;
   void (*wake)(void *);
   void *user, *latest_display;
   retro_atomic_int_t broken, closing;
   unsigned count, terminal;
   int building;
} videotoolbox_av1_owner_t;

static void videotoolbox_av1_owner_output(void *ref, void *frame,
      OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef image,
      CMTime pts, CMTime duration)
{
   videotoolbox_av1_owner_t *o = (videotoolbox_av1_owner_t *)ref;
   unsigned index = (unsigned)(((uintptr_t)frame >> 2) & 31);
   if (!status && !image && !(flags & kVTDecodeInfo_FrameDropped)
         && index < o->count
         && retro_atomic_load_acquire_int(&o->work[index].hidden))
      videotoolbox_av1_slots_complete(o->slots, frame, 0);
   else if (videotoolbox_av1_slots_output(
                  o->slots, frame, status, flags, image, pts, duration)
         && status)
      retro_atomic_store_release_int(&o->broken, 1);
}

/* Cold worker factory. wake schedules serialized service without running it
 * inline or allocating on the producer. Total compressed storage <=256 MiB. */
static INLINE videotoolbox_av1_owner_t *videotoolbox_av1_owner_new(
      const rav1_sequence_t *header, const video_color_metadata_t *color,
      const rav1_obu_t *sequence, unsigned count, size_t input,
      videotoolbox_av1_register_t registration, void (*wake)(void *),
      void *user, OSStatus *status)
{
   videotoolbox_av1_owner_t *o;
   VTDecompressionOutputCallbackRecord callback;
   unsigned i;
   OSStatus result = kCMFormatDescriptionError_AllocationFailed;
   if (status)
      *status = kCMFormatDescriptionError_InvalidParameter;
   if (!header || !sequence || !wake || count < 10 || count > 32 || !input
         || input > 64u * 1024 * 1024 || input > 256u * 1024 * 1024 / count)
      return NULL;
   if (status)
      *status = kCMFormatDescriptionError_AllocationFailed;
   o = (videotoolbox_av1_owner_t *)calloc(1, sizeof(*o));
   if (!o)
      return NULL;
   o->count = count;
   o->building = -1;
   o->wake = wake;
   o->user = user;
   retro_atomic_int_init(&o->broken, 0);
   retro_atomic_int_init(&o->closing, 0);
   o->storage = (uint8_t *)malloc(count * input);
   o->slots = videotoolbox_av1_slots_new(count);
   if (!o->storage || !o->slots
         || !retro_spsc_init(&o->queue, count * sizeof(unsigned)))
      goto fail;
   for (i = 0; i < count; i++)
   {
      videotoolbox_av1_packet_init(
            &o->work[i].packet, o->storage + i * input, input);
      retro_atomic_int_init(&o->work[i].sample.held, 0);
      retro_atomic_int_init(&o->work[i].submitted, 0);
      retro_atomic_int_init(&o->work[i].hidden, 0);
   }
   callback.decompressionOutputCallback = videotoolbox_av1_owner_output;
   callback.decompressionOutputRefCon = o;
   result = videotoolbox_av1_session_create(header, color, sequence,
         registration, &callback, &o->format, &o->session);
   if (result)
      goto fail;
   if (status)
      *status = 0;
   return o;
fail:
   retro_spsc_free(&o->queue);
   videotoolbox_av1_slots_free_quiesced(o->slots);
   free(o->storage);
   free(o);
   if (status)
      *status = result;
   return NULL;
}

/* Producer only. Native reads end only after submit return AND block release.
 */
static INLINE void videotoolbox_av1_owner_collect(
      videotoolbox_av1_owner_t *o, bool discard)
{
   unsigned i;
   videotoolbox_av1_result_t result;
   for (i = 0; i < o->count; i++)
   {
      videotoolbox_av1_work_t *w = &o->work[i];
      if (!w->cookie || !retro_atomic_load_acquire_int(&w->submitted))
         continue;
      if (!w->taken && (w->packet.display_slot < 0 || w->discard || discard)
            && videotoolbox_av1_slots_take(o->slots, i, &result) == 1)
      {
         if (result.image)
            CVPixelBufferRelease(result.image);
         w->taken = 1;
      }
      if (w->taken && !retro_atomic_load_acquire_int(&w->sample.held))
      {
         w->cookie = NULL;
         videotoolbox_av1_packet_reset(&w->packet);
      }
   }
}

static INLINE int videotoolbox_av1_owner_publish(videotoolbox_av1_owner_t *o)
{
   unsigned index = (unsigned)o->building;
   videotoolbox_av1_work_t *w;
   if (index >= o->count || !o->work[index].packet.ready)
      return -1;
   if (retro_spsc_write_avail(&o->queue) < sizeof(index))
      return 1;
   w = &o->work[index];
   retro_atomic_store_release_int(&w->hidden, w->packet.display_slot < 0);
   w->cookie = videotoolbox_av1_slots_begin(o->slots, index);
   if (!w->cookie)
      return -1;
   w->taken = w->discard = 0;
   retro_atomic_store_release_int(&w->submitted, 0);
   if (retro_spsc_write(&o->queue, &index, sizeof(index)) != sizeof(index))
   {
      retro_atomic_store_release_int(&o->broken, 1);
      return -1;
   }
   if (w->packet.display_slot >= 0)
      o->latest_display = w->cookie;
   o->building = -1;
   o->wake(o->user);
   return 0;
}

/* Producer: 0 accepts/copies input, 1 busy without acceptance, negative
 * failure. */
static INLINE int videotoolbox_av1_owner_append(videotoolbox_av1_owner_t *o,
      const uint8_t *data, size_t size, bool display, int slot)
{
   unsigned i;
   int result, queued;
   if (!o || o->terminal || retro_atomic_load_acquire_int(&o->closing)
         || retro_atomic_load_acquire_int(&o->broken)
         || (display && (slot < 0 || (unsigned)slot >= o->count)))
      return -1;
   videotoolbox_av1_owner_collect(o, false);
   if (o->building < 0)
   {
      for (i = 0; i < o->count; i++)
         if (!o->work[i].cookie)
         {
            o->building = (int)i;
            break;
         }
      if (o->building < 0)
         return 1;
   }
   result = videotoolbox_av1_packet_append(
         &o->work[o->building].packet, data, size, display, slot);
   if (result < 0)
      return result;
   if (o->work[o->building].packet.ready)
   {
      queued = videotoolbox_av1_owner_publish(o);
      if (queued < 0)
         return -1;
   }
   return result;
}

/* Producer only, after accepting a replacement visible packet. */
static INLINE void videotoolbox_av1_owner_discard_previous(
      videotoolbox_av1_owner_t *o)
{
   unsigned i;
   for (i = 0; i < o->count; i++)
      if (o->work[i].cookie && o->work[i].cookie != o->latest_display)
         o->work[i].discard = 1;
   videotoolbox_av1_owner_collect(o, false);
}

/* Serialized executor: at most one native submission; no caller-thread work. */
static INLINE int videotoolbox_av1_owner_worker(videotoolbox_av1_owner_t *o)
{
   unsigned index;
   videotoolbox_av1_work_t *w;
   VTDecodeInfoFlags info = 0;
   OSStatus status;
   if (!o || retro_spsc_read_avail(&o->queue) < sizeof(index))
      return 0;
   if (retro_spsc_read(&o->queue, &index, sizeof(index)) != sizeof(index)
         || index >= o->count)
      return -1;
   w = &o->work[index];
   status = retro_atomic_load_acquire_int(&o->broken)
         ? -1
         : videotoolbox_av1_decode_worker(o->session, o->format, &w->packet,
                 &w->sample, NULL, w->cookie, &info);
   if (status)
      retro_atomic_store_release_int(&o->broken, 1);
   if (status || (info & kVTDecodeInfo_FrameDropped))
      videotoolbox_av1_slots_complete(o->slots, w->cookie, status);
   retro_atomic_store_release_int(&w->submitted, 1);
   return 1;
}

/* Producer: 0 transfers a result, 1 pending, negative invalid/native failure.
 */
static INLINE int videotoolbox_av1_owner_take(videotoolbox_av1_owner_t *o,
      unsigned slot, videotoolbox_av1_result_t *out)
{
   unsigned i;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   out->pts = out->duration = kCMTimeInvalid;
   if (!o || slot >= o->count || retro_atomic_load_acquire_int(&o->broken))
      return -1;
   for (i = 0; i < o->count; i++)
   {
      videotoolbox_av1_work_t *w = &o->work[i];
      if (!w->cookie || w->taken || w->discard
            || w->packet.display_slot != (int)slot)
         continue;
      if (!retro_atomic_load_acquire_int(&w->submitted)
            || videotoolbox_av1_slots_take(o->slots, i, out) != 1)
         return 1;
      w->taken = 1;
      return 0;
   }
   return 1;
}

/* Producer terminal service. Preserve visible output unless discard is set.
 * Completion releases input/output ownership, not native callback quiescence.
 */
static INLINE int videotoolbox_av1_owner_finish(
      videotoolbox_av1_owner_t *o, bool discard)
{
   unsigned i;
   int result;
   if (!o || retro_atomic_load_acquire_int(&o->closing)
         || retro_atomic_load_acquire_int(&o->broken))
      return -1;
   o->terminal = 1;
   if (o->building >= 0 && o->work[o->building].packet.count)
   {
      if (videotoolbox_av1_packet_seal(&o->work[o->building].packet))
         return -1;
      result = videotoolbox_av1_owner_publish(o);
      if (result)
         return result;
   }
   videotoolbox_av1_owner_collect(o, discard);
   for (i = 0; i < o->count; i++)
      if (o->work[i].cookie)
         return 1;
   return 0;
}

static INLINE int videotoolbox_av1_owner_restart(videotoolbox_av1_owner_t *o)
{
   int result = videotoolbox_av1_owner_finish(o, true);
   if (result)
      return result;
   o->terminal = 0;
   o->building = -1;
   o->latest_display = NULL;
   return 0;
}

static INLINE void videotoolbox_av1_owner_close(videotoolbox_av1_owner_t *o)
{
   if (!o)
      return;
   o->slots->closed = 1;
   retro_atomic_store_release_int(&o->closing, 1);
}

/* Retirement executor only, after producer close and prior service callbacks.
 * Waiting is confined to retirement. A retained native block delays freeing. */
static INLINE bool videotoolbox_av1_owner_retire_poll(
      videotoolbox_av1_owner_t **handle)
{
   videotoolbox_av1_owner_t *o;
   unsigned i;
   if (!handle || !(o = *handle))
      return true;
   if (!retro_atomic_load_acquire_int(&o->closing))
      return false;
   if (o->session)
   {
      for (i = 0; i < o->count; i++)
         if (videotoolbox_av1_owner_worker(o) <= 0)
            break;
      VTDecompressionSessionWaitForAsynchronousFrames(o->session);
      VTDecompressionSessionInvalidate(o->session);
      CFRelease(o->session);
      CFRelease(o->format);
      o->session = NULL;
      o->format = NULL;
   }
   for (i = 0; i < o->count; i++)
      if (retro_atomic_load_acquire_int(&o->work[i].sample.held))
         return false;
   videotoolbox_av1_slots_free_quiesced(o->slots);
   retro_spsc_free(&o->queue);
   free(o->storage);
   free(o);
   *handle = NULL;
   return true;
}
#endif
#endif
