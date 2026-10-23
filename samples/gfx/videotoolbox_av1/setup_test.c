/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#ifdef NDEBUG
#error Assertions are required for this test
#endif
static INLINE void *setup_malloc(size_t size);
#define malloc setup_malloc
#include "../../../gfx/common/videotoolbox_av1.h"
#undef malloc
#include "../vulkan_video_caps/av1_session_data.h"

static rav1_sequence_t header;
static video_color_metadata_t color;
static int callback_token;
static int malloc_fail;

static INLINE void *setup_malloc(size_t size)
{
   return malloc_fail ? NULL : malloc(size);
}

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

const char kCVImageBufferColorPrimaries_ITU_R_709_2[] = "0";
const char kCVImageBufferColorPrimaries_ITU_R_2020[] = "1";
const char kCVImageBufferTransferFunction_ITU_R_709_2[] = "2";
const char kCVImageBufferTransferFunction_sRGB[] = "3";
const char kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ[] = "4";
const char kCVImageBufferTransferFunction_ITU_R_2100_HLG[] = "5";
const char kCVImageBufferYCbCrMatrix_ITU_R_709_2[] = "6";
const char kCVImageBufferYCbCrMatrix_ITU_R_2020[] = "7";
struct object
{
   const void *keys[6], *values[6];
   struct object *format;
   CMBlockBufferCustomBlockSource source;
   void *data;
   size_t size;
   CMSampleTimingInfo timing;
   VTDecompressionOutputCallbackRecord callback;
   uint8_t bytes[128];
   retro_atomic_int_t refs;
   int kind, count, value, invalidated;
};

static retro_atomic_int_t objects;
static int step, fail_step, format_error, session_error, registers;
static int expected_pixel;
static uint8_t recorded_configuration[128];

static struct object *new_object(int kind)
{
   struct object *p;
   if (++step == fail_step)
      return NULL;
   p = (struct object *)calloc(1, sizeof(*p));
   assert(p);
   p->kind = kind;
   retro_atomic_int_init(&p->refs, 1);
   retro_atomic_fetch_add_int(&objects, 1);
   return p;
}

static bool mock_constant_value(const void *value)
{
   return value == kCFBooleanTrue || value == kCFBooleanFalse
         || value == kCVImageBufferColorPrimaries_ITU_R_709_2
         || value == kCVImageBufferColorPrimaries_ITU_R_2020
         || value == kCVImageBufferTransferFunction_ITU_R_709_2
         || value == kCVImageBufferTransferFunction_sRGB
         || value == kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ
         || value == kCVImageBufferTransferFunction_ITU_R_2100_HLG
         || value == kCVImageBufferYCbCrMatrix_ITU_R_709_2
         || value == kCVImageBufferYCbCrMatrix_ITU_R_2020;
}

static void retain(const void *value)
{
   assert(value);
   if (value == kCFBooleanTrue || value == kCFBooleanFalse)
      return;
   if (!mock_constant_value(value))
      retro_atomic_fetch_add_int(&((struct object *)value)->refs, 1);
}

void CFRelease(void *value)
{
   struct object *p = (struct object *)value;
   int i;
   assert(p && retro_atomic_load_acquire_int(&p->refs) > 0);
   if (retro_atomic_fetch_sub_int(&p->refs, 1) != 1)
      return;
   if (p->kind == 3)
      for (i = 0; i < p->count; i++)
         if (!mock_constant_value(p->values[i]))
            CFRelease((void *)p->values[i]);
   if (p->format)
      CFRelease(p->format);
   if (p->kind == 6)
      p->source.FreeBlock(p->source.refCon, p->data, p->size);
   assert(p->kind != 5 || p->invalidated);
   retro_atomic_fetch_sub_int(&objects, 1);
   free(p);
}

CFDataRef CFDataCreate(void *allocator, const uint8_t *bytes, long size)
{
   struct object *p = new_object(1);
   assert(!allocator && size > 4 && size <= 128);
   if (p)
   {
      memcpy(p->bytes, bytes, (size_t)size);
      p->size = (size_t)size;
      memcpy(recorded_configuration, bytes, (size_t)size);
   }
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
   assert(!allocator && kc && vc && count >= 0 && count <= 6);
   if (!p)
      return NULL;
   p->count = (int)count;
   for (i = 0; i < count; i++)
   {
      p->keys[i] = keys[i];
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
   struct object *data = get(get(extensions, "atoms"), "av1C");
   struct object *depth = get(extensions, "BitsPerComponent");
   rav1_config_t parsed;
   rav1_sequence_t sequence;
   assert(!allocator && codec == VIDEOTOOLBOX_AV1_CODEC);
   assert(width == (int32_t)header.max_width
         && height == (int32_t)header.max_height);
   assert(depth->value == (int)header.bit_depth);
   assert(rav1_config_parse(&parsed, data->bytes, data->size));
   assert(rav1_config_sequence(&sequence, &parsed));
   assert(!memcmp(&sequence, &header, sizeof(header)));
   assert(!parsed.initial_presentation_delay);
   assert(get(extensions, "FullRangeVideo") == kCFBooleanFalse);
   assert((const void *)get(extensions, "ColorPrimaries")
         == kCVImageBufferColorPrimaries_ITU_R_709_2);
   assert((const void *)get(extensions, "TransferFunction")
         == kCVImageBufferTransferFunction_ITU_R_709_2);
   assert((const void *)get(extensions, "YCbCrMatrix")
         == kCVImageBufferYCbCrMatrix_ITU_R_709_2);
   *out = new_object(4);
   if (*out)
   {
      memcpy((*out)->bytes, data->bytes, data->size);
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
   assert(!allocator && format->kind == 4);
#ifdef VT_AV1_OWNER_TEST
   assert(cb->decompressionOutputCallback == videotoolbox_av1_owner_output);
   assert(cb->decompressionOutputRefCon);
#else
   assert(cb->decompressionOutputCallback == callback);
   assert(cb->decompressionOutputRefCon == &callback_token);
#endif
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
   assert(codec == VIDEOTOOLBOX_AV1_CODEC);
   registers++;
}

int main(void)
{
   uint8_t bytes[128];
   rav1_obu_t obu;
   size_t i, offset;
   int fault, allocation_count;
   OSStatus status;
   CMVideoFormatDescriptionRef format;
   VTDecompressionSessionRef session;
   VTDecompressionOutputCallbackRecord cb;
   cb.decompressionOutputCallback = callback;
   cb.decompressionOutputRefCon = &callback_token;
   retro_atomic_int_init(&objects, 0);
   memset(&color, 0, sizeof(color));
   color.primaries = color.transfer = color.matrix = color.range = 1;
   for (i = 0;
         i < sizeof(av1_sequence_fixtures) / sizeof(av1_sequence_fixtures[0]);
         i++)
   {
      const struct av1_sequence_fixture *fixture = &av1_sequence_fixtures[i];
      bytes[0] = 0x0a;
      bytes[1] = (uint8_t)fixture->size;
      memcpy(bytes + 2, fixture->data, fixture->size);
      offset = 0;
      assert(
            rav1_obu_next(&obu, bytes, fixture->size + 2, &offset, false) == 1);
      assert(rav1_sequence_parse(&header, obu.payload, obu.payload_size));
      expected_pixel = fixture->depth == 10
            ? 0x78343230
            : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
      step = 0;
      status = videotoolbox_av1_session_create(
            &header, &color, &obu, register_decoder, &cb, &format, &session);
      assert(!status && format && session);
      allocation_count = step;
      VTDecompressionSessionInvalidate(session);
      CFRelease(session);
      CFRelease(format);
      assert(!retro_atomic_load_acquire_int(&objects));
      for (fault = 1; fault <= allocation_count; fault++)
      {
         step = 0;
         fail_step = fault;
         assert(videotoolbox_av1_session_create(
               &header, &color, &obu, NULL, &cb, &format, &session));
         assert(
               !format && !session && !retro_atomic_load_acquire_int(&objects));
      }
      fail_step = 0;
      malloc_fail = 1;
      assert(videotoolbox_av1_session_create(
            &header, &color, &obu, NULL, &cb, &format, &session));
      assert(!format && !session && !retro_atomic_load_acquire_int(&objects));
      malloc_fail = 0;
      format_error = -7;
      assert(videotoolbox_av1_session_create(
                   &header, &color, &obu, NULL, &cb, &format, &session)
            == -7);
      assert(!format && !session && !retro_atomic_load_acquire_int(&objects));
      format_error = 0;
      session_error = -8;
      assert(videotoolbox_av1_session_create(
                   &header, &color, &obu, NULL, &cb, &format, &session)
            == -8);
      assert(!format && !session && !retro_atomic_load_acquire_int(&objects));
      session_error = 0;
   }
   assert(registers == 6);
   puts("PASS: AV1 format, hardware requirement and setup failure ownership");
   return 0;
}
