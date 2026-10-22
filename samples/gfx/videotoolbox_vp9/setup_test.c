/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/videotoolbox_vp9.h"

static rvp9_hdr header;
static video_color_metadata_t color;
static int callback_token;

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
   VTDecompressionOutputCallbackRecord callback;
   uint8_t bytes[12];
   int kind, refs, count, value, invalidated;
};

static int objects, step, fail_step, format_error, session_error, registers;
static int expected_pixel;

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

static void register_decoder(CMVideoCodecType codec)
{
   assert(codec == VIDEOTOOLBOX_VP9_CODEC);
   registers++;
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
   puts("PASS: native CoreMedia VP9 formats and SDK signatures; no hardware session or decode execution");
#else
   puts("PASS: VP9 native setup contract, hardware/Metal/IOSurface requests and every allocation/partial-error cleanup; no Apple device execution");
#endif
   return 0;
}
