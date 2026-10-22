/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_SAMPLE_H
#define RETROARCH_VIDEOTOOLBOX_VP9_SAMPLE_H
#include <retro_atomic.h>
#include "videotoolbox_vp9.h"
#include "videotoolbox_vp9_packet.h"

#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
#define VIDEOTOOLBOX_VP9_SAMPLE 1

typedef struct videotoolbox_vp9_sample
{
   retro_atomic_int_t held;
} videotoolbox_vp9_sample_t;

static void videotoolbox_vp9_sample_release(void *ref, void *data, size_t size)
{
   videotoolbox_vp9_sample_t *sample = (videotoolbox_vp9_sample_t *)ref;
   (void)data;
   (void)size;
   retro_atomic_store_release_int(&sample->held, 0);
}

/* Serialized executor only: CoreMedia allocates its sample/block objects.
 * Initialize held to zero at cold setup. Keep the ticket and packet storage
 * alive and immutable until held acquires zero, even after decode callbacks. */
static INLINE OSStatus videotoolbox_vp9_sample_create_worker(
      CMVideoFormatDescriptionRef format, const videotoolbox_vp9_packet_t *p,
      videotoolbox_vp9_sample_t *ticket, const CMSampleTimingInfo *timing,
      CMSampleBufferRef *out)
{
   CMBlockBufferRef block = NULL;
   CMBlockBufferCustomBlockSource source;
   OSStatus status;
   if (!out)
      return kCMFormatDescriptionError_InvalidParameter;
   *out = NULL;
   if (!format || !p || !ticket || !p->data || !p->ready || !p->count
         || p->count > 8 || !p->size || p->size > p->capacity)
      return kCMFormatDescriptionError_InvalidParameter;
   if (retro_atomic_load_acquire_int(&ticket->held))
      return 1;
   source.version       = kCMBlockBufferCustomBlockSourceVersion;
   source.AllocateBlock = NULL;
   source.FreeBlock     = videotoolbox_vp9_sample_release;
   source.refCon        = ticket;
   retro_atomic_store_release_int(&ticket->held, 1);
   status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, p->data,
         p->size, kCFAllocatorNull, &source, 0, p->size, 0, &block);
   if (!status && !block)
      status = kCMFormatDescriptionError_AllocationFailed;
   if (!status)
      status = CMSampleBufferCreateReady(kCFAllocatorDefault, block, format, 1,
            timing ? 1 : 0, timing, 1, &p->size, out);
   if (!status && !*out)
      status = kCMFormatDescriptionError_AllocationFailed;
   if (block)
      CFRelease(block);
   if (status)
   {
      if (*out)
      {
         CFRelease(*out);
         *out = NULL;
      }
      retro_atomic_store_release_int(&ticket->held, 0);
   }
   return status;
}

/* Worker only. Callback/frame_ref lifetime is owned separately by the owner.
 * Do not enable temporal processing: display order belongs to the shared codec.
 */
static INLINE OSStatus videotoolbox_vp9_decode_worker(
      VTDecompressionSessionRef session, CMVideoFormatDescriptionRef format,
      const videotoolbox_vp9_packet_t *p, videotoolbox_vp9_sample_t *ticket,
      const CMSampleTimingInfo *timing, void *frame_ref,
      VTDecodeInfoFlags *info)
{
   CMSampleBufferRef sample = NULL;
   OSStatus status;
   VTDecodeFrameFlags flags = kVTDecodeFrame_EnableAsynchronousDecompression;
   if (info)
      *info = 0;
   if (!session)
      return kCMFormatDescriptionError_InvalidParameter;
   status = videotoolbox_vp9_sample_create_worker(
         format, p, ticket, timing, &sample);
   if (status)
      return status;
   if (p->display_slot < 0)
      flags |= kVTDecodeFrame_DoNotOutputFrame;
   status = VTDecompressionSessionDecodeFrame(
         session, sample, flags, frame_ref, info);
   CFRelease(sample);
   return status;
}
#endif
#endif
