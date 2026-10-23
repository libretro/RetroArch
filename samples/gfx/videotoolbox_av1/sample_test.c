/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define main setup_main
#include "setup_test.c"
#undef main
#include "../../../gfx/common/videotoolbox_av1_sample.h"
#ifndef VIDEOTOOLBOX_AV1_SAMPLE
#error Lock-free threaded sample support is required
#endif
static uint8_t sample_bytes[64];
static videotoolbox_av1_packet_t sample_packet;
static videotoolbox_av1_sample_t sample_ticket;
static int block_error, sample_error, decode_error;
static unsigned expected_flags;
static CMSampleBufferRef delayed_sample;
OSStatus CMBlockBufferCreateWithMemoryBlock(void *allocator, void *data,
      size_t size, void *block_allocator,
      const CMBlockBufferCustomBlockSource *source, size_t offset,
      size_t length, unsigned flags, CMBlockBufferRef *out)
{
   assert(!allocator && block_allocator == kCFAllocatorNull && !offset
         && !flags);
   assert(source->version == 0 && !source->AllocateBlock && source->FreeBlock);
   assert(data == sample_packet.data && size == length
         && size == sample_packet.size);
   *out = new_object(6);
   if (*out)
   {
      (*out)->source = *source;
      (*out)->data = data;
      (*out)->size = size;
   }
   return *out ? block_error : -1001;
}

OSStatus CMSampleBufferCreateReady(void *allocator, CMBlockBufferRef block,
      CMVideoFormatDescriptionRef format, long samples, long times,
      const CMSampleTimingInfo *timing, long sizes, const size_t *size,
      CMSampleBufferRef *out)
{
   assert(!allocator && format && samples == 1 && sizes == 1);
   assert(*size == sample_packet.size);
   assert(times == 0 || (times == 1 && timing));
   *out = new_object(7);
   if (*out)
   {
      (*out)->format = block;
      retain(block);
      if (times)
         (*out)->timing = *timing;
   }
   return *out ? sample_error : -1001;
}

OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef session,
      CMSampleBufferRef sample, VTDecodeFrameFlags flags, void *ref,
      VTDecodeInfoFlags *info)
{
   assert(session && ref == &callback_token && flags == expected_flags);
   assert(!delayed_sample);
   if (!decode_error)
   {
      retain(sample);
      delayed_sample = sample;
   }
   if (info)
      *info = 0;
   return decode_error;
}

static void test_sample(CMVideoFormatDescriptionRef format)
{
   CMSampleBufferRef sample, busy;
   CMSampleTimingInfo timing;
   int failure;
   assert(videotoolbox_av1_packet_init(
         &sample_packet, sample_bytes, sizeof(sample_bytes)));
   assert(!videotoolbox_av1_packet_append(
         &sample_packet, (const uint8_t *)"\x32\x01\x80", 3, true, 0));
   retro_atomic_int_init(&sample_ticket.held, 0);
   timing.duration.value = 1;
   timing.duration.timescale = 24;
   timing.duration.flags = 1;
   timing.duration.epoch = 0;
   timing.presentationTimeStamp = timing.duration;
   timing.presentationTimeStamp.value = 48;
   timing.decodeTimeStamp = timing.presentationTimeStamp;
   assert(!videotoolbox_av1_sample_create_worker(
         format, &sample_packet, &sample_ticket, &timing, &sample));
   assert(sample && retro_atomic_load_acquire_int(&sample_ticket.held));
   assert(videotoolbox_av1_sample_create_worker(
                format, &sample_packet, &sample_ticket, NULL, &busy)
               == 1
         && !busy);
   timing.presentationTimeStamp.value = 999;
   assert(sample->timing.presentationTimeStamp.value == 48
         && sample->timing.duration.timescale == 24);
   CFRelease(sample);
   assert(!retro_atomic_load_acquire_int(&sample_ticket.held));
   for (failure = 1; failure <= 2; failure++)
   {
      step = 0;
      fail_step = failure;
      assert(videotoolbox_av1_sample_create_worker(
            format, &sample_packet, &sample_ticket, NULL, &sample));
      assert(!sample && !retro_atomic_load_acquire_int(&sample_ticket.held));
   }
   fail_step = 0;
   block_error = -301;
   assert(videotoolbox_av1_sample_create_worker(
                format, &sample_packet, &sample_ticket, NULL, &sample)
         == -301);
   assert(!sample && !retro_atomic_load_acquire_int(&sample_ticket.held));
   block_error = 0;
   sample_error = -302;
   assert(videotoolbox_av1_sample_create_worker(
                format, &sample_packet, &sample_ticket, NULL, &sample)
         == -302);
   assert(!sample && !retro_atomic_load_acquire_int(&sample_ticket.held));
   sample_error = 0;
}
int main(void)
{
   CMVideoFormatDescriptionRef format;
   VTDecompressionSessionRef session;
   VTDecodeInfoFlags info;
   assert(!setup_main());
   format = new_object(4);
   assert(format);
   test_sample(format);
   session = new_object(5);
   assert(session);
   expected_flags = kVTDecodeFrame_EnableAsynchronousDecompression;
   assert(!videotoolbox_av1_decode_worker(session, format, &sample_packet,
         &sample_ticket, NULL, &callback_token, &info));
   assert(delayed_sample && retro_atomic_load_acquire_int(&sample_ticket.held));
   CFRelease(delayed_sample);
   delayed_sample = NULL;
   assert(!retro_atomic_load_acquire_int(&sample_ticket.held));
   videotoolbox_av1_packet_reset(&sample_packet);
   assert(!videotoolbox_av1_packet_append(
         &sample_packet, (const uint8_t *)"\x32\x01\x00", 3, false, -1));
   assert(!videotoolbox_av1_packet_seal(&sample_packet));
   expected_flags |= kVTDecodeFrame_DoNotOutputFrame;
   assert(!videotoolbox_av1_decode_worker(session, format, &sample_packet,
         &sample_ticket, NULL, &callback_token, &info));
   assert(delayed_sample);
   CFRelease(delayed_sample);
   delayed_sample = NULL;
   decode_error = -303;
   assert(videotoolbox_av1_decode_worker(session, format, &sample_packet,
                &sample_ticket, NULL, &callback_token, &info)
         == -303);
   assert(!retro_atomic_load_acquire_int(&sample_ticket.held));
   VTDecompressionSessionInvalidate(session);
   CFRelease(session);
   CFRelease(format);
   assert(!retro_atomic_load_acquire_int(&objects));
   puts("PASS: AV1 borrowed native sample storage, failure and delayed "
        "release");
   return 0;
}
