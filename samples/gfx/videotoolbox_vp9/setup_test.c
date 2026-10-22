/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/videotoolbox_vp9_sample.h"

static rvp9_hdr header;
static video_color_metadata_t color;
static int callback_token;
#ifdef VIDEOTOOLBOX_VP9_SAMPLE
static uint8_t sample_bytes[64];
static videotoolbox_vp9_packet_t sample_packet;
static videotoolbox_vp9_sample_t sample_ticket;
#endif

static void callback(void *user, void *frame, OSStatus status,
      VTDecodeInfoFlags flags, CVImageBufferRef image, CMTime pts,
      CMTime duration)
{
   (void)user;
   (void)frame;
   (void)status;
   (void)flags;
   (void)image;
   (void)pts;
   (void)duration;
   assert(0 && "setup must not deliver a frame");
}

#ifndef VT_VP9_NATIVE
struct object
{
   const void *keys[3], *values[3];
   struct object *format;
   CMBlockBufferCustomBlockSource source;
   void *data;
   size_t size;
   CMSampleTimingInfo timing;
   VTDecompressionOutputCallbackRecord callback;
   uint8_t bytes[12];
   int kind, refs, count, value, invalidated;
};

static int objects, step, fail_step, format_error, session_error, registers;
static int expected_pixel;
#ifdef VIDEOTOOLBOX_VP9_SAMPLE
static CMSampleBufferRef delayed_sample;
static int block_error, sample_error, decode_error;
static unsigned expected_flags;
#endif

static struct object *new_object(int kind)
{
   struct object *p;
   if (++step == fail_step)
      return NULL;
   p = (struct object *)calloc(1, sizeof(*p));
   assert(p);
   p->kind = kind;
   p->refs = 1;
   objects++;
   return p;
}

static void retain(const void *value)
{
   if (value != kCFBooleanTrue)
      ((struct object *)value)->refs++;
}

void CFRelease(void *value)
{
   struct object *p = (struct object *)value;
   int i;
   assert(p && p->refs > 0);
   if (--p->refs)
      return;
   if (p->kind == 3)
      for (i = 0; i < p->count; i++)
         if (p->values[i] != kCFBooleanTrue)
            CFRelease((void *)p->values[i]);
   if (p->format)
      CFRelease(p->format);
   if (p->kind == 6)
      p->source.FreeBlock(p->source.refCon, p->data, p->size);
   assert(p->kind != 5 || p->invalidated);
   objects--;
   free(p);
}

CFDataRef CFDataCreate(void *allocator, const uint8_t *bytes, long size)
{
   struct object *p = new_object(1);
   assert(!allocator && size == 12);
   if (p)
      memcpy(p->bytes, bytes, 12);
   return p;
}

CFNumberRef CFNumberCreate(void *allocator, int type, const void *value)
{
   struct object *p = new_object(2);
   assert(!allocator && type == kCFNumberSInt32Type);
   if (p)
      p->value = *(const int32_t *)value;
   return p;
}

CFDictionaryRef CFDictionaryCreate(void *allocator, const void **keys,
      const void **values, long count, const void *kc, const void *vc)
{
   struct object *p = new_object(3);
   long i;
   assert(!allocator && kc && vc && count >= 0 && count <= 3);
   if (!p)
      return NULL;
   p->count = (int)count;
   for (i = 0; i < count; i++)
   {
      p->keys[i]   = keys[i];
      p->values[i] = values[i];
      retain(values[i]);
   }
   return p;
}

static struct object *get(struct object *dictionary, const char *key)
{
   int i;
   assert(dictionary && dictionary->kind == 3);
   for (i = 0; i < dictionary->count; i++)
      if (!strcmp((const char *)dictionary->keys[i], key))
         return (struct object *)dictionary->values[i];
   assert(0 && "missing attribute");
   return NULL;
}

OSStatus CMVideoFormatDescriptionCreate(void *allocator, CMVideoCodecType codec,
      int32_t width, int32_t height, CFDictionaryRef extensions,
      CMVideoFormatDescriptionRef *out)
{
   struct object *data  = get(get(extensions, "atoms"), "vpcC");
   struct object *depth = get(extensions, "BitsPerComponent");
   assert(!allocator && codec == VIDEOTOOLBOX_VP9_CODEC);
   assert(width == header.w && height == header.h && data->kind == 1);
   assert(depth->kind == 2 && depth->value == header.bit_depth);
   assert(data->bytes[0] == 1 && data->bytes[4] == header.profile);
   assert(data->bytes[8] == color.transfer && !data->bytes[10]
          && !data->bytes[11]);
   *out = new_object(4);
   if (*out)
   {
      memcpy((*out)->bytes, data->bytes, 12);
      (*out)->format = extensions;
      retain(extensions);
   }
   return *out ? format_error : -1000;
}

OSStatus VTDecompressionSessionCreate(void *allocator,
      CMVideoFormatDescriptionRef format, CFDictionaryRef specification,
      CFDictionaryRef attributes, const VTDecompressionOutputCallbackRecord *cb,
      VTDecompressionSessionRef *out)
{
   assert(!allocator && format->kind == 4
          && cb->decompressionOutputCallback == callback);
   assert(cb->decompressionOutputRefCon == &callback_token);
   assert((void *)get(specification, "hardware") == kCFBooleanTrue);
   assert((void *)get(attributes, "metal") == kCFBooleanTrue);
   assert(get(attributes, "surface")->kind == 3
          && !get(attributes, "surface")->count);
   assert(get(attributes, "pixel")->value == expected_pixel);
   *out = new_object(5);
   if (*out)
   {
      (*out)->format = format;
      retain(format);
      (*out)->callback = *cb;
   }
   return *out ? session_error : -1001;
}

void VTDecompressionSessionInvalidate(VTDecompressionSessionRef session)
{
   assert(session->kind == 5 && !session->invalidated);
   session->invalidated = 1;
}

#ifdef VIDEOTOOLBOX_VP9_SAMPLE
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
      (*out)->data   = data;
      (*out)->size   = size;
   }
   return *out ? block_error : -1001;
}

OSStatus CMSampleBufferCreateReady(void *allocator, CMBlockBufferRef block,
      CMVideoFormatDescriptionRef format, long samples, long times,
      const CMSampleTimingInfo *timing, long sizes, const size_t *size,
      CMSampleBufferRef *out)
{
   assert(!allocator && format && samples == 1 && sizes == 1);
   assert(
         *size == sample_packet.size && (times == 0 || (times == 1 && timing)));
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

#endif

static void register_decoder(CMVideoCodecType codec)
{
   assert(codec == VIDEOTOOLBOX_VP9_CODEC);
   registers++;
}
#endif

#ifdef VIDEOTOOLBOX_VP9_SAMPLE
static void test_sample(CMVideoFormatDescriptionRef format)
{
   CMSampleBufferRef sample, busy;
   CMSampleTimingInfo timing;
#ifndef VT_VP9_NATIVE
   int failure;
#endif
   assert(videotoolbox_vp9_packet_init(
         &sample_packet, sample_bytes, sizeof(sample_bytes)));
   assert(!videotoolbox_vp9_packet_append(
         &sample_packet, (const uint8_t *)"vp9", 3, true, 0));
   retro_atomic_int_init(&sample_ticket.held, 0);
   timing.duration.value              = 1;
   timing.duration.timescale          = 24;
   timing.duration.flags              = 1;
   timing.duration.epoch              = 0;
   timing.presentationTimeStamp       = timing.duration;
   timing.presentationTimeStamp.value = 48;
   timing.decodeTimeStamp             = timing.presentationTimeStamp;
   assert(!videotoolbox_vp9_sample_create_worker(
         format, &sample_packet, &sample_ticket, &timing, &sample));
   assert(sample && retro_atomic_load_acquire_int(&sample_ticket.held));
   assert(videotoolbox_vp9_sample_create_worker(format, &sample_packet,
         &sample_ticket, NULL, &busy) == 1 && !busy);
#ifndef VT_VP9_NATIVE
   timing.presentationTimeStamp.value = 999;
   assert(sample->timing.presentationTimeStamp.value == 48
         && sample->timing.duration.timescale == 24);
#endif
   CFRelease(sample);
   assert(!retro_atomic_load_acquire_int(&sample_ticket.held));
#ifndef VT_VP9_NATIVE
   for (failure = 1; failure <= 2; failure++)
   {
      step      = 0;
      fail_step = failure;
      assert(videotoolbox_vp9_sample_create_worker(
            format, &sample_packet, &sample_ticket, NULL, &sample));
      assert(!sample && !retro_atomic_load_acquire_int(&sample_ticket.held));
   }
   fail_step   = 0;
   block_error = -301;
   assert(videotoolbox_vp9_sample_create_worker(
                format, &sample_packet, &sample_ticket, NULL, &sample)
          == -301);
   assert(!sample && !retro_atomic_load_acquire_int(&sample_ticket.held));
   block_error  = 0;
   sample_error = -302;
   assert(videotoolbox_vp9_sample_create_worker(
                format, &sample_packet, &sample_ticket, NULL, &sample)
          == -302);
   assert(!sample && !retro_atomic_load_acquire_int(&sample_ticket.held));
   sample_error = 0;
#endif
}
#endif

int main(void)
{
   CMVideoFormatDescriptionRef format;
   VTDecompressionSessionRef session;
   VTDecompressionOutputCallbackRecord cb;
   OSStatus status;
   unsigned depth, range;
#ifndef VT_VP9_NATIVE
   int failure, before;
#endif
   memset(&header, 0, sizeof(header));
   memset(&color, 0, sizeof(color));
   header.w             = 1920;
   header.h             = 1080;
   header.subsampling_x = header.subsampling_y = 1;
   cb.decompressionOutputCallback              = callback;
   cb.decompressionOutputRefCon                = &callback_token;
   for (depth = 8; depth <= 10; depth += 2)
   {
      header.bit_depth   = (int)depth;
      header.profile     = depth == 10 ? 2 : 0;
      header.color_space = depth == 10 ? 5 : 2;
      color.transfer     = depth == 10 ? 16 : 1;
      for (range = 0; range < 2; range++)
      {
         header.color_range = (int)range;
         status = videotoolbox_vp9_format_create(&header, &color, 1, &format);
         assert(!status && format);
#ifdef VT_VP9_NATIVE
         {
            CFDictionaryRef atoms = (CFDictionaryRef)CMFormatDescriptionGetExtension(
                  format,
                  kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
            CFDataRef data =
                  (CFDataRef)CFDictionaryGetValue(atoms, CFSTR("vpcC"));
            const UInt8 *bytes = CFDataGetBytePtr(data);
            CMVideoDimensions dimensions =
                  CMVideoFormatDescriptionGetDimensions(format);
            assert(dimensions.width == 1920 && dimensions.height == 1080);
            assert(CFDataGetLength(data) == 12 && bytes[0] == 1
                   && bytes[4] == header.profile);
            assert(bytes[8] == color.transfer && (bytes[6] & 1) == range);
         }
#endif
#ifdef VIDEOTOOLBOX_VP9_SAMPLE
         test_sample(format);
#endif
         CFRelease(format);
#ifndef VT_VP9_NATIVE
         expected_pixel = depth == 10 ? (range ? 0x78663230 : 0x78343230)
                                      : (range ? 0x34323066 : 0x34323076);
         step = fail_step = format_error = session_error = 0;
         before                                          = registers;
         status = videotoolbox_vp9_session_create(
               &header, &color, 1, register_decoder, &cb, &format, &session);
         assert(!status && format && session && registers == before + 1);
         assert(objects == 6);
#ifdef VIDEOTOOLBOX_VP9_SAMPLE
         expected_flags = kVTDecodeFrame_EnableAsynchronousDecompression;
         assert(!videotoolbox_vp9_decode_worker(session, format, &sample_packet,
               &sample_ticket, NULL, &callback_token, NULL));
         assert(delayed_sample
                && retro_atomic_load_acquire_int(&sample_ticket.held));
         assert(videotoolbox_vp9_decode_worker(session, format, &sample_packet,
                      &sample_ticket, NULL, &callback_token, NULL)
                == 1);
         CFRelease(delayed_sample);
         delayed_sample = NULL;
         assert(!retro_atomic_load_acquire_int(&sample_ticket.held));
         sample_packet.display_slot = -1;
         expected_flags |= kVTDecodeFrame_DoNotOutputFrame;
         decode_error = -303;
         assert(videotoolbox_vp9_decode_worker(session, format, &sample_packet,
                      &sample_ticket, NULL, &callback_token, NULL)
                == -303);
         assert(!delayed_sample
                && !retro_atomic_load_acquire_int(&sample_ticket.held));
         decode_error = 0;
#endif
         VTDecompressionSessionInvalidate(session);
         CFRelease(session);
         CFRelease(format);
         assert(!objects);
         before = registers;
         assert(!videotoolbox_vp9_session_create(
               &header, &color, 1, NULL, &cb, &format, &session));
         assert(format && session && registers == before);
         VTDecompressionSessionInvalidate(session);
         CFRelease(session);
         CFRelease(format);
         assert(!objects);
         for (failure = 1; failure <= 10; failure++)
         {
            step      = 0;
            fail_step = failure;
            status    = videotoolbox_vp9_session_create(
                  &header, &color, 1, NULL, &cb, &format, &session);
            assert(status && !format && !session && !objects);
         }
         step = fail_step = 0;
         format_error     = -200;
         assert(videotoolbox_vp9_session_create(
                      &header, &color, 1, NULL, &cb, &format, &session)
                == -200);
         assert(!format && !session && !objects);
         format_error  = 0;
         session_error = -201;
         assert(videotoolbox_vp9_session_create(
                      &header, &color, 1, NULL, &cb, &format, &session)
                == -201);
         assert(!format && !session && !objects);
         session_error = 0;
#endif
      }
   }
   assert(videotoolbox_vp9_session_create(
                NULL, NULL, 0, NULL, &cb, &format, &session)
                != 0
          && !format && !session);
   assert(videotoolbox_vp9_session_create(
                &header, &color, 0, NULL, NULL, &format, &session)
                != 0
          && !format && !session);
#ifdef VT_VP9_NATIVE
   puts("PASS: native CoreMedia VP9 formats/samples, storage release and SDK signatures; no hardware decode execution");
#elif defined(VIDEOTOOLBOX_VP9_SAMPLE)
   puts("PASS: VP9 setup/sample contracts, delayed storage release, busy/rejected submission and allocation cleanup; no Apple device execution");
#else
   puts("PASS: VP9 setup; threadless excludes native sample/decode helpers");
#endif
   return 0;
}
