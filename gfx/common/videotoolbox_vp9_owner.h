/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_OWNER_H
#define RETROARCH_VIDEOTOOLBOX_VP9_OWNER_H
#include <retro_spsc.h>
#include "videotoolbox_vp9_slots.h"
#ifdef VIDEOTOOLBOX_VP9_SLOTS
#define VIDEOTOOLBOX_VP9_OWNER 1

typedef struct videotoolbox_vp9_work
{
   videotoolbox_vp9_packet_t packet;
   videotoolbox_vp9_sample_t sample;
   void *cookie;
   retro_atomic_int_t submitted, hidden;
   int taken, discard;
} videotoolbox_vp9_work_t;

typedef struct videotoolbox_vp9_owner
{
   retro_spsc_t queue;
   videotoolbox_vp9_work_t work[32];
   CMVideoFormatDescriptionRef format;
   VTDecompressionSessionRef session;
   videotoolbox_vp9_slots_t *slots;
   uint8_t *storage;
   void (*wake)(void *);
   void *user, *latest_display;
   retro_atomic_int_t broken, closing;
   unsigned count, terminal;
   int building;
} videotoolbox_vp9_owner_t;

static void videotoolbox_vp9_owner_output(void *ref, void *frame,
      OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef image,
      CMTime pts, CMTime duration)
{
   videotoolbox_vp9_owner_t *o = (videotoolbox_vp9_owner_t *)ref;
   unsigned index              = (unsigned)(((uintptr_t)frame >> 2) & 31);
   if (status)
      retro_atomic_store_release_int(&o->broken, 1);
   if (!status && !image && !(flags & kVTDecodeInfo_FrameDropped)
         && index < o->count
         && retro_atomic_load_acquire_int(&o->work[index].hidden))
      videotoolbox_vp9_slots_complete(o->slots, frame, 0);
   else
      videotoolbox_vp9_slots_output(
            o->slots, frame, status, flags, image, pts, duration);
}

/* Cold worker factory. wake schedules serialized service without running it
 * inline or allocating on the producer. Total compressed storage <=256 MiB. */
static INLINE videotoolbox_vp9_owner_t *videotoolbox_vp9_owner_new(
      const rvp9_hdr *header, const video_color_metadata_t *color,
      unsigned chroma, unsigned count, size_t input,
      videotoolbox_vp9_register_t registration, void (*wake)(void *),
      void *user, OSStatus *status)
{
   videotoolbox_vp9_owner_t *o;
   VTDecompressionOutputCallbackRecord callback;
   unsigned i;
   OSStatus result = kCMFormatDescriptionError_AllocationFailed;
   if (status)
      *status = kCMFormatDescriptionError_InvalidParameter;
   if (!header || !wake || count < 10 || count > 32 || input <= 34
         || input > 64u * 1024 * 1024 || input > 256u * 1024 * 1024 / count)
      return NULL;
   if (status)
      *status = kCMFormatDescriptionError_AllocationFailed;
   o = (videotoolbox_vp9_owner_t *)calloc(1, sizeof(*o));
   if (!o)
      return NULL;
   o->count    = count;
   o->building = -1;
   o->wake     = wake;
   o->user     = user;
   retro_atomic_int_init(&o->broken, 0);
   retro_atomic_int_init(&o->closing, 0);
   o->storage = (uint8_t *)malloc(count * input);
   o->slots   = videotoolbox_vp9_slots_new(count);
   if (!o->storage || !o->slots
         || !retro_spsc_init(&o->queue, count * sizeof(unsigned)))
      goto fail;
   for (i = 0; i < count; i++)
   {
      videotoolbox_vp9_packet_init(
            &o->work[i].packet, o->storage + i * input, input);
      retro_atomic_int_init(&o->work[i].sample.held, 0);
      retro_atomic_int_init(&o->work[i].submitted, 0);
      retro_atomic_int_init(&o->work[i].hidden, 0);
   }
   callback.decompressionOutputCallback = videotoolbox_vp9_owner_output;
   callback.decompressionOutputRefCon   = o;
   result = videotoolbox_vp9_session_create(header, color, chroma, registration,
         &callback, &o->format, &o->session);
   if (result)
      goto fail;
   if (status)
      *status = 0;
   return o;
fail:
   retro_spsc_free(&o->queue);
   videotoolbox_vp9_slots_free_quiesced(o->slots);
   free(o->storage);
   free(o);
   if (status)
      *status = result;
   return NULL;
}

/* Producer only. Native reads end only after submit return AND block release.
 */
static INLINE void videotoolbox_vp9_owner_collect(
      videotoolbox_vp9_owner_t *o, bool discard)
{
   unsigned i;
   videotoolbox_vp9_result_t result;
   for (i = 0; i < o->count; i++)
   {
      videotoolbox_vp9_work_t *w = &o->work[i];
      if (!w->cookie || !retro_atomic_load_acquire_int(&w->submitted))
         continue;
      if (!w->taken && (w->packet.display_slot < 0 || w->discard || discard)
            && videotoolbox_vp9_slots_take(o->slots, i, &result) == 1)
      {
         if (result.image)
            CVPixelBufferRelease(result.image);
         w->taken = 1;
      }
      if (w->taken && !retro_atomic_load_acquire_int(&w->sample.held))
      {
         w->cookie = NULL;
         videotoolbox_vp9_packet_reset(&w->packet);
      }
   }
}

static INLINE int videotoolbox_vp9_owner_publish(videotoolbox_vp9_owner_t *o)
{
   unsigned index = (unsigned)o->building;
   videotoolbox_vp9_work_t *w;
   if (index >= o->count || !o->work[index].packet.ready)
      return -1;
   if (retro_spsc_write_avail(&o->queue) < sizeof(index))
      return 1;
   w = &o->work[index];
   retro_atomic_store_release_int(&w->hidden, w->packet.display_slot < 0);
   w->cookie = videotoolbox_vp9_slots_begin(o->slots, index);
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
static INLINE int videotoolbox_vp9_owner_append(videotoolbox_vp9_owner_t *o,
      const uint8_t *data, size_t size, bool display, int slot)
{
   unsigned i;
   int result, queued;
   if (!o || o->terminal || retro_atomic_load_acquire_int(&o->closing)
         || retro_atomic_load_acquire_int(&o->broken)
         || (display && (slot < 0 || (unsigned)slot >= o->count)))
      return -1;
   videotoolbox_vp9_owner_collect(o, false);
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
   result = videotoolbox_vp9_packet_append(
         &o->work[o->building].packet, data, size, display, slot);
   if (result < 0)
      return result;
   if (o->work[o->building].packet.ready)
   {
      queued = videotoolbox_vp9_owner_publish(o);
      if (queued < 0)
         return -1;
   }
   return result;
}

/* Producer only, after accepting a replacement visible packet. */
static INLINE void videotoolbox_vp9_owner_discard_previous(
      videotoolbox_vp9_owner_t *o)
{
   unsigned i;
   for (i = 0; i < o->count; i++)
      if (o->work[i].cookie && o->work[i].cookie != o->latest_display)
         o->work[i].discard = 1;
   videotoolbox_vp9_owner_collect(o, false);
}

/* Serialized executor: at most one native submission; no caller-thread work. */
static INLINE int videotoolbox_vp9_owner_worker(videotoolbox_vp9_owner_t *o)
{
   unsigned index;
   videotoolbox_vp9_work_t *w;
   VTDecodeInfoFlags info = 0;
   OSStatus status;
   if (!o || retro_spsc_read_avail(&o->queue) < sizeof(index))
      return 0;
   if (retro_spsc_read(&o->queue, &index, sizeof(index)) != sizeof(index)
         || index >= o->count)
      return -1;
   w      = &o->work[index];
   status = retro_atomic_load_acquire_int(&o->broken)
                  ? -1
                  : videotoolbox_vp9_decode_worker(o->session, o->format,
                          &w->packet, &w->sample, NULL, w->cookie, &info);
   if (status)
      retro_atomic_store_release_int(&o->broken, 1);
   if (status || (info & kVTDecodeInfo_FrameDropped))
      videotoolbox_vp9_slots_complete(o->slots, w->cookie, status);
   retro_atomic_store_release_int(&w->submitted, 1);
   return 1;
}

/* Producer: 0 transfers a result, 1 pending, negative invalid/native failure.
 */
static INLINE int videotoolbox_vp9_owner_take(videotoolbox_vp9_owner_t *o,
      unsigned slot, videotoolbox_vp9_result_t *out)
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
      videotoolbox_vp9_work_t *w = &o->work[i];
      if (!w->cookie || w->taken || w->discard
            || w->packet.display_slot != (int)slot)
         continue;
      if (!retro_atomic_load_acquire_int(&w->submitted)
            || videotoolbox_vp9_slots_take(o->slots, i, out) != 1)
         return 1;
      w->taken = 1;
      return 0;
   }
   return 1;
}

/* Producer terminal service. Preserve visible output unless discard is set.
 * Completion releases input/output ownership, not native callback quiescence.
 */
static INLINE int videotoolbox_vp9_owner_finish(
      videotoolbox_vp9_owner_t *o, bool discard)
{
   unsigned i;
   int result;
   if (!o || retro_atomic_load_acquire_int(&o->closing)
         || retro_atomic_load_acquire_int(&o->broken))
      return -1;
   o->terminal = 1;
   if (o->building >= 0 && o->work[o->building].packet.count)
   {
      if (videotoolbox_vp9_packet_seal(&o->work[o->building].packet))
         return -1;
      result = videotoolbox_vp9_owner_publish(o);
      if (result)
         return result;
   }
   videotoolbox_vp9_owner_collect(o, discard);
   for (i = 0; i < o->count; i++)
      if (o->work[i].cookie)
         return 1;
   return 0;
}

static INLINE int videotoolbox_vp9_owner_restart(videotoolbox_vp9_owner_t *o)
{
   int result = videotoolbox_vp9_owner_finish(o, true);
   if (result) return result;
   o->terminal = 0;
   o->building = -1;
   o->latest_display = NULL;
   return 0;
}

static INLINE void videotoolbox_vp9_owner_close(videotoolbox_vp9_owner_t *o)
{
   if (!o)
      return;
   o->slots->closed = 1;
   retro_atomic_store_release_int(&o->closing, 1);
}

/* Retirement executor only, after producer close and prior service callbacks.
 * Waiting is confined to retirement. A retained native block delays freeing. */
static INLINE bool videotoolbox_vp9_owner_retire_poll(
      videotoolbox_vp9_owner_t **handle)
{
   videotoolbox_vp9_owner_t *o;
   unsigned i;
   if (!handle || !(o = *handle))
      return true;
   if (!retro_atomic_load_acquire_int(&o->closing))
      return false;
   if (o->session)
   {
      for (i = 0; i < o->count; i++)
         if (videotoolbox_vp9_owner_worker(o) <= 0)
            break;
      VTDecompressionSessionWaitForAsynchronousFrames(o->session);
      VTDecompressionSessionInvalidate(o->session);
      CFRelease(o->session);
      CFRelease(o->format);
      o->session = NULL;
      o->format  = NULL;
   }
   for (i = 0; i < o->count; i++)
      if (retro_atomic_load_acquire_int(&o->work[i].sample.held))
         return false;
   videotoolbox_vp9_slots_free_quiesced(o->slots);
   retro_spsc_free(&o->queue);
   free(o->storage);
   free(o);
   *handle = NULL;
   return true;
}
#endif
#endif
