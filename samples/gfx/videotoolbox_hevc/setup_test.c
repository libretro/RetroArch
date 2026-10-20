#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/videotoolbox_hevc.h"
#include "../../../gfx/common/videotoolbox_hevc_slots.h"
#include "parameters.h"

enum { VT_HEVC_TEST_BATCH = 8 };

#ifndef VT_HEVC_NATIVE
struct object
{
   CMSampleTimingInfo timing;
   struct object *block, *format;
   uint8_t *bytes;
   size_t size;
   VTDecompressionOutputCallback callback;
   void *callback_ref;
   int kind, value, invalidated, refs;
};
static int objects, fail_step, step, wanted_pixel, wanted_height;
static int sample_fail_step, sample_step, decode_mode, callbacks;
static CMSampleBufferRef pending_sample;
static void *pending_frame;
static VTDecompressionSessionRef pending_session;
static CMSampleBufferRef batch_samples[VT_HEVC_TEST_BATCH];
static void *batch_frames[VT_HEVC_TEST_BATCH];
static unsigned batch_count;
static int owner_drain_allowed;
static rh265_nal_t wanted_sets[3];

static struct object *create(int kind, int value)
{
   struct object *p;
   if (++step == fail_step) return NULL;
   p = (struct object*)calloc(1, sizeof(*p));
   assert(p);
   p->kind = kind; p->value = value; p->refs = 1; objects++;
   return p;
}
void CFRelease(void *ref)
{
   struct object *p = (struct object*)ref;
   assert(p && objects > 0);
   assert(p->refs > 0);
   if (--p->refs) return;
   assert(p->kind != 4 || p->invalidated);
   if (p->block) CFRelease(p->block);
   if (p->format) CFRelease(p->format);
   free(p->bytes);
   objects--; free(p);
}
OSStatus CMVideoFormatDescriptionCreateFromHEVCParameterSets(void *allocator,
      size_t count, const uint8_t *const *ptrs, const size_t *sizes,
      int length, void *extensions, CMVideoFormatDescriptionRef *out)
{
   unsigned i;
   assert(!allocator && !extensions && count == 3 && length == 4);
   for (i = 0; i < 3; i++)
   {
      assert(sizes[i] == wanted_sets[i].size);
      assert(!memcmp(ptrs[i], wanted_sets[i].data, sizes[i]));
      assert(((ptrs[i][0] >> 1) & 63) == 32 + i);
   }
   *out = create(1, 0);
   return *out ? 0 : -1000;
}
CFDictionaryRef CFDictionaryCreate(void *allocator, const void **keys,
      const void **values, int count, const void *key_callbacks,
      const void *value_callbacks)
{
   assert(!allocator && key_callbacks && value_callbacks);
   if (count == 1)
   {
      assert(keys[0] == kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder);
      assert(values[0] == kCFBooleanTrue);
   }
   else
   {
      const struct object *number = (const struct object*)values[0];
      assert(count == 2 && keys[0] == kCVPixelBufferPixelFormatTypeKey);
      assert(keys[1] == kCVPixelBufferMetalCompatibilityKey);
      assert(values[1] == kCFBooleanTrue);
      assert(number->kind == 2 && number->value == wanted_pixel);
   }
   return create(3, count);
}
CFNumberRef CFNumberCreate(void *allocator, int type, const void *value)
{
   assert(!allocator && type == kCFNumberSInt32Type);
   return create(2, *(const int32_t*)value);
}
OSStatus VTDecompressionSessionCreate(void *allocator,
      CMVideoFormatDescriptionRef format, CFDictionaryRef specification,
      CFDictionaryRef attributes, const VTDecompressionOutputCallbackRecord *callback,
      VTDecompressionSessionRef *out)
{
   assert(!allocator && format->kind == 1 && specification->value == 1);
   assert(attributes->value == 2 && callback->decompressionOutputCallback);
   *out = create(4, 0);
   if (*out)
   {
      (*out)->callback = callback->decompressionOutputCallback;
      (*out)->callback_ref = callback->decompressionOutputRefCon;
   }
   return *out && fail_step != 6 ? 0 : -1001;
}
void VTDecompressionSessionInvalidate(VTDecompressionSessionRef session)
{
   assert(session->kind == 4 && !session->invalidated);
   session->invalidated = 1;
}
OSStatus VTDecompressionSessionWaitForAsynchronousFrames(VTDecompressionSessionRef session)
{
   unsigned i;
   assert(owner_drain_allowed && session->kind == 4 && !session->invalidated);
   if (pending_sample)
   {
      CVPixelBufferRef image = create(7, 0);
      assert(pending_session == session && image);
      session->callback(session->callback_ref, pending_frame, 0, 0, image,
            pending_sample->timing.presentationTimeStamp, pending_sample->timing.duration);
      CVPixelBufferRelease(image); CFRelease(pending_sample);
      pending_sample = NULL; pending_frame = NULL; pending_session = NULL;
   }
   for (i = batch_count; i > 0; i--)
   {
      CMSampleBufferRef sample = batch_samples[i - 1];
      CVPixelBufferRef image = create(7, 0);
      assert(sample && image);
      session->callback(session->callback_ref, batch_frames[i - 1], 0, 0, image,
            sample->timing.presentationTimeStamp, sample->timing.duration);
      CVPixelBufferRelease(image); CFRelease(sample);
      batch_samples[i - 1] = NULL; batch_frames[i - 1] = NULL;
   }
   batch_count = 0;
   return 0;
}
CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef format)
{
   CMVideoDimensions dims;
   assert(format->kind == 1); dims.width = 64; dims.height = wanted_height;
   return dims;
}

static int sample_failure(void) { return ++sample_step == sample_fail_step; }
OSStatus CMBlockBufferCreateWithMemoryBlock(void *allocator, void *memory,
      size_t size, void *block_allocator, const void *custom,
      size_t offset, size_t length, unsigned flags, CMBlockBufferRef *out)
{
   assert(!allocator && !memory && !block_allocator && !custom);
   assert(!offset && length == size && !flags && size);
   if (sample_failure()) return -2001;
   *out = create(5, 0); assert(*out); (*out)->size = size;
   return 0;
}
OSStatus CMBlockBufferAssureBlockMemory(CMBlockBufferRef block)
{
   if (sample_failure()) return -2002;
   block->bytes = (uint8_t*)malloc(block->size); assert(block->bytes);
   return 0;
}
OSStatus CMBlockBufferReplaceDataBytes(const void *data, CMBlockBufferRef block,
      size_t offset, size_t size)
{
   assert(offset <= block->size && size <= block->size - offset);
   if (sample_failure()) return -2003;
   memcpy(block->bytes + offset, data, size);
   return 0;
}
OSStatus CMBlockBufferCopyDataBytes(CMBlockBufferRef block, size_t offset,
      size_t size, void *out)
{
   assert(offset <= block->size && size <= block->size - offset);
   memcpy(out, block->bytes + offset, size); return 0;
}
OSStatus CMSampleBufferCreateReady(void *allocator, CMBlockBufferRef block,
      CMVideoFormatDescriptionRef format, long count, long timings,
      const CMSampleTimingInfo *timing, long sizes, const size_t *size, CMSampleBufferRef *out)
{
   assert(!allocator && count == 1 && timings == (timing ? 1 : 0) && sizes == 1);
   assert(*size == block->size && block->bytes);
   if (sample_failure()) return -2004;
   *out = create(6, 0); assert(*out);
   (*out)->block = block; block->refs++;
   (*out)->format = format; format->refs++;
   (*out)->size = *size;
   if (timing) (*out)->timing = *timing;
   return sample_fail_step == 8 ? -2005 : 0;
}
CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef sample) { return sample->block; }
size_t CMSampleBufferGetTotalSampleSize(CMSampleBufferRef sample) { return sample->size; }
OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef session,
      CMSampleBufferRef sample, unsigned flags, void *frame, VTDecodeInfoFlags *info)
{
   assert(flags == kVTDecodeFrame_EnableAsynchronousDecompression);
   if (decode_mode == 4)
   {
      assert(batch_count < VT_HEVC_TEST_BATCH && !pending_sample);
      sample->refs++;
      batch_samples[batch_count] = sample;
      batch_frames[batch_count++] = frame;
      if (info) *info = kVTDecodeInfo_Asynchronous;
      return 0;
   }
   if (decode_mode == 2) return -2006;
   if (decode_mode == 3)
   {
      if (info) *info = kVTDecodeInfo_FrameDropped;
      return 0;
   }
   if (decode_mode == 1)
      session->callback(session->callback_ref, frame, 0, 0, NULL,
            sample->timing.presentationTimeStamp, sample->timing.duration);
   else
   {
      assert(!pending_sample);
      sample->refs++; pending_sample = sample; pending_frame = frame;
      pending_session = session;
      if (info) *info = kVTDecodeInfo_Asynchronous;
   }
   return 0;
}
static void sample_output(void *ref, void *frame, OSStatus status,
      VTDecodeInfoFlags flags, CVImageBufferRef image, CMTime pts, CMTime duration)
{
   assert(ref == &callbacks && frame == &decode_mode && !status);
   assert(!image); (void)flags; (void)pts; (void)duration;
   callbacks++;
}
CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef image)
{
   assert(image && image->kind == 7); image->refs++; return image;
}
void CVPixelBufferRelease(CVPixelBufferRef image) { CFRelease(image); }
#endif

#include "slots_test.h"
#include "codec_test.h"
#include "hdr_codec_test.h"
#include "sequence_test.h"
#include "batch_test.h"
#include "owner_test.h"
#include "decoder_test.h"

static void test_samples(CMVideoFormatDescriptionRef format)
{
   static const uint8_t payload[15] = {38, 1, 0x80, 0, 0, 3, 1, 38, 1, 0x20, 0, 0, 3, 2, 0x80};
   static const uint8_t expected[23] = {0, 0, 0, 7, 38, 1, 0x80, 0, 0, 3, 1,
      0, 0, 0, 8, 38, 1, 0x20, 0, 0, 3, 2, 0x80};
   uint8_t *input = (uint8_t*)malloc(sizeof(payload));
   uint8_t actual[23];
   rh265_nal_t slices[2];
   rh265_access_unit_t au;
   CMSampleBufferRef sample = NULL;
   assert(input); memcpy(input, payload, sizeof(payload));
   memset(&au, 0, sizeof(au)); au.num_slices = 2; au.coded_bytes = 15;
   au.first.nal_type = 19;
   slices[0].data = input; slices[0].size = 7;
   slices[1].data = input + 7; slices[1].size = 8;
#ifndef VT_HEVC_NATIVE
   {
      unsigned i;
      int baseline = objects;
      for (i = 1; i <= 8; i++)
      {
         sample_fail_step = i; sample_step = 0;
         assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
         assert(!sample && objects == baseline);
      }
      sample_fail_step = sample_step = 0;
   }
#endif
   assert(!videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
   memset(input, 0xcc, sizeof(payload)); free(input);
   assert(CMSampleBufferGetTotalSampleSize(sample) == sizeof(expected));
   assert(!CMBlockBufferCopyDataBytes(CMSampleBufferGetDataBuffer(sample),
            0, sizeof(actual), actual));
   assert(!memcmp(actual, expected, sizeof(actual)));
   CFRelease(sample); sample = NULL;
   slices[0].data = payload; slices[1].data = payload + 7;
   assert(videotoolbox_hevc_sample_create(format, &au, slices, 1, &sample));
   au.coded_bytes++;
   assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
   au.coded_bytes--; slices[0].size = 2;
   assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
   slices[0].size = 7; slices[0].data = NULL;
   assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
   slices[0].data = payload;
   assert(videotoolbox_hevc_sample_create(NULL, &au, slices, 2, &sample));
   assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, NULL));
   assert(videotoolbox_hevc_sample_create(format, NULL, slices, 2, &sample));
   au.num_slices = 0;
   assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
   au.num_slices = 2;
   {
      static const uint8_t invalid[7][2] = {{0xa6, 1}, {39, 1}, {64, 1},
         {18, 1}, {38, 0}, {38, 9}, {38, 2}};
      uint8_t nal[7];
      unsigned i;
      memcpy(nal, payload, sizeof(nal)); slices[0].data = nal;
      for (i = 0; i < 7; i++)
      {
         nal[0] = invalid[i][0]; nal[1] = invalid[i][1];
         assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
         assert(!sample);
      }
      slices[0].data = payload;
   }
   if ((size_t)-1 > UINT32_MAX)
   {
      slices[0].size = (size_t)UINT32_MAX + 1;
      assert(videotoolbox_hevc_sample_create(format, &au, slices, 2, &sample));
      slices[0].size = 7;
   }
   au.num_slices = 1; au.coded_bytes = 7;
   assert(!videotoolbox_hevc_sample_create(format, &au, slices, 1, &sample));
   assert(CMSampleBufferGetTotalSampleSize(sample) == 11);
   assert(!CMBlockBufferCopyDataBytes(CMSampleBufferGetDataBuffer(sample), 0, 11, actual));
   assert(!memcmp(actual, expected, 11)); CFRelease(sample); sample = NULL;
   au.num_slices = 2; au.coded_bytes = 15;
#ifndef VT_HEVC_NATIVE
   {
      VTDecompressionSessionRef session = create(4, 0);
      VTDecodeInfoFlags info = 99;
      int baseline = objects;
      assert(session); session->callback = sample_output; session->callback_ref = &callbacks;
      input = (uint8_t*)malloc(sizeof(payload)); assert(input);
      memcpy(input, payload, sizeof(payload));
      slices[0].data = input; slices[1].data = input + 7;
      callbacks = decode_mode = 0;
      assert(!videotoolbox_hevc_decode(session, format, &au, slices, 2, &decode_mode, &info));
      assert(info == kVTDecodeInfo_Asynchronous && !callbacks && pending_sample);
      memset(input, 0xdd, sizeof(payload)); free(input);
      assert(!CMBlockBufferCopyDataBytes(CMSampleBufferGetDataBuffer(pending_sample),
               0, sizeof(actual), actual));
      assert(!memcmp(actual, expected, sizeof(actual)));
      pending_session->callback(pending_session->callback_ref, pending_frame, 0, 0, NULL, kCMTimeInvalid, kCMTimeInvalid);
      CFRelease(pending_sample); pending_sample = NULL;
      assert(callbacks == 1 && objects == baseline);
      slices[0].data = payload; slices[1].data = payload + 7;
      decode_mode = 1; info = 99;
      assert(!videotoolbox_hevc_decode(session, format, &au, slices, 2, &decode_mode, &info));
      assert(callbacks == 2 && !info && objects == baseline);
      decode_mode = 2;
      assert(videotoolbox_hevc_decode(session, format, &au, slices, 2, &decode_mode, &info));
      assert(callbacks == 2 && !info && objects == baseline);
      info = 99;
      assert(videotoolbox_hevc_decode(NULL, format, &au, slices, 2, NULL, &info));
      assert(!info);
      VTDecompressionSessionInvalidate(session); CFRelease(session);
   }
#endif
   puts("VideoToolbox HEVC: owned sample bytes and submission lifetime passed");
}

static void output(void *ref, void *frame, OSStatus status, VTDecodeInfoFlags flags,
      CVImageBufferRef image, CMTime pts, CMTime duration)
{
   (void)ref; (void)frame; (void)status; (void)flags;
   (void)image; (void)pts; (void)duration;
   assert(0); /* This fixture never submits compressed frames. */
}

static void test(const uint8_t *data, size_t size, unsigned depth, int full,
      unsigned height, bool require_hardware)
{
   rh265_video *source = rh265_video_open();
   rh265_nal_t sets[3];
   CMVideoFormatDescriptionRef format = NULL;
   VTDecompressionSessionRef session = NULL;
   VTDecompressionOutputCallbackRecord callback;
   CMVideoDimensions dims;
   OSStatus status;
   uint8_t *copy = (uint8_t*)malloc(size);
   unsigned i;
   size_t pos;
   assert(source && copy);
   memcpy(copy, data, size);
   assert(rh265_video_get_parameter_sets(source, 0, sets) == -1);
   assert(!rh265_video_retain_parameter_sets(source));
   assert(!rh265_video_set_extradata(source, copy, size));
   assert(!rh265_video_get_parameter_sets(source, 0, sets));
   memset(copy, 0xa5, size); free(copy);
   pos = 23;
   for (i = 0; i < 3; i++)
   {
      size_t len = ((size_t)data[pos + 3] << 8) | data[pos + 4];
      assert(sets[i].size == len && !memcmp(sets[i].data, data + pos + 5, len));
      pos += 5 + len;
   }
   assert(rh265_video_retain_parameter_sets(source) == -1);
   callback.decompressionOutputCallback = output;
   callback.decompressionOutputRefCon = source;
#ifndef VT_HEVC_NATIVE
   wanted_height = (int)height;
   memcpy(wanted_sets, sets, sizeof(sets));
   wanted_pixel = depth == 10 ? (full ? 0x78663230 : 0x78343230)
      : (full ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
              : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
   for (i = 1; i <= 6; i++)
   {
      fail_step = i; step = 0;
      assert(videotoolbox_hevc_session_create(source, 0, &callback, &format, &session));
      assert(!format && !session && !objects);
   }
   fail_step = step = 0;
#else
   (void)full;
#endif
   assert(!videotoolbox_hevc_format_create(source, 0, &format));
   dims = CMVideoFormatDescriptionGetDimensions(format);
   assert(dims.width == 64 && dims.height == (int)height);
   test_samples(format);
   test_slots(format);
   CFRelease(format); format = NULL;
#ifdef VT_HEVC_NATIVE
   if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC))
   {
      rh265_video_close(source);
      puts("VideoToolbox HEVC: no hardware decoder; session check unavailable");
      exit(77);
   }
#endif
   if (require_hardware)
   {
      status = videotoolbox_hevc_session_create(source, 0, &callback, &format, &session);
      if (status)
         fprintf(stderr, "VideoToolbox setup %dx%d depth %u full %d: session status %ld\n",
               (int)dims.width, (int)dims.height, depth, full, (long)status);
      assert(!status);
      assert(format && session);
      VTDecompressionSessionInvalidate(session); CFRelease(session); CFRelease(format);
      format = NULL; session = NULL;
   }
   assert(videotoolbox_hevc_session_create(source, 63, &callback, &format, &session));
   assert(!format && !session);
   assert(videotoolbox_hevc_session_create(source, 0, NULL, &format, &session));
   assert(!format && !session);
   {
      rh265_video_params_t *params = (rh265_video_params_t*)malloc(sizeof(*params));
      uint8_t *packet = (uint8_t*)malloc(65541);
      assert(params && packet);
      assert(!rh265_video_get_parameter_sets(source, 0, sets));
      memset(packet, 0x80, 65541);
      packet[0] = 0; packet[1] = 1; packet[2] = 0; packet[3] = 1;
      memcpy(packet + 4, sets[1].data, sets[1].size);
      assert(rh265_video_decode(source, packet, 65541) == 0);
      assert(!rh265_video_get_params(source, 0, params));
      assert((unsigned)params->bit_depth == depth);
      assert(params->width == 64 && params->height == (int)height);
      memset(sets, 0xaa, sizeof(sets));
      assert(rh265_video_get_parameter_sets(source, 0, sets) == -1);
      for (i = 0; i < 3; i++) assert(!sets[i].data && !sets[i].size);
      assert(!rh265_video_set_extradata(source, data, size));
      assert(!rh265_video_get_parameter_sets(source, 0, sets));
      free(packet); free(params);
   }
   rh265_video_close(source);
   source = rh265_video_open();
   assert(source && !rh265_video_set_extradata(source, data, size));
   assert(rh265_video_retain_parameter_sets(source) == -1);
   assert(rh265_video_get_parameter_sets(source, 0, sets) == -1);
   rh265_video_close(source);
#ifndef VT_HEVC_NATIVE
   assert(!objects);
#endif
}

int main(void)
{
   unsigned i;
   bool parameter_session = true;
#ifdef VT_HEVC_NATIVE
   parameter_session = false;
#endif
   test_codec_software();
   test_hdr_codec_software();
   test_sequence_software();
   test_sequence_hardware_state();
#ifdef VT_HEVC_NATIVE
   if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC))
   {
      puts("VideoToolbox HEVC: no hardware decoder; native codec check unavailable");
      return 77;
   }
   test_codec_submission();
   test_codec_batch();
   test_session_owner();
#endif
   test(hevc_main8_limited, sizeof(hevc_main8_limited), 8, 0, 48, parameter_session);
   test(hevc_main10_limited, sizeof(hevc_main10_limited), 10, 0, 48, parameter_session);
   test(hevc_main8_full, sizeof(hevc_main8_full), 8, 1, 48, parameter_session);
   test(hevc_main10_full, sizeof(hevc_main10_full), 10, 1, 48, parameter_session);
   for (i = 0; i < sizeof(codec_cases) / sizeof(codec_cases[0]); i++)
      test(codec_cases[i].hvcc, codec_cases[i].hvcc_size, codec_cases[i].depth,
            (int)codec_cases[i].full, 64, true);
#ifndef VT_HEVC_NATIVE
   test_codec_submission();
   test_codec_batch();
   test_session_owner();
#endif
   puts("VideoToolbox HEVC: owned parameters and Main/Main10 session setup passed");
   test_decoder_adapter();
   return 0;
}
