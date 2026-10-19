#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/videotoolbox_hevc.h"
#include "parameters.h"

#ifndef VT_HEVC_NATIVE
struct object { int kind, value, invalidated; };
static int objects, fail_step, step, wanted_pixel;
static rh265_nal_t wanted_sets[3];

static struct object *create(int kind, int value)
{
   struct object *p;
   if (++step == fail_step) return NULL;
   p = (struct object*)calloc(1, sizeof(*p));
   assert(p);
   p->kind = kind; p->value = value; objects++;
   return p;
}
void CFRelease(void *ref)
{
   struct object *p = (struct object*)ref;
   assert(p && objects > 0);
   assert(p->kind != 4 || p->invalidated);
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
   return *out && fail_step != 6 ? 0 : -1001;
}
void VTDecompressionSessionInvalidate(VTDecompressionSessionRef session)
{
   assert(session->kind == 4 && !session->invalidated);
   session->invalidated = 1;
}
CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef format)
{
   CMVideoDimensions dims;
   assert(format->kind == 1); dims.width = 64; dims.height = 48;
   return dims;
}
#endif

static void output(void *ref, void *frame, OSStatus status, VTDecodeInfoFlags flags,
      CVImageBufferRef image, CMTime pts, CMTime duration)
{
   (void)ref; (void)frame; (void)status; (void)flags;
   (void)image; (void)pts; (void)duration;
   assert(0); /* This fixture never submits compressed frames. */
}

static void test(const uint8_t *data, size_t size, unsigned depth, int full)
{
   rh265_video *source = rh265_video_open();
   rh265_nal_t sets[3];
   CMVideoFormatDescriptionRef format = NULL;
   VTDecompressionSessionRef session = NULL;
   VTDecompressionOutputCallbackRecord callback;
   CMVideoDimensions dims;
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
   assert(dims.width == 64 && dims.height == 48);
   CFRelease(format); format = NULL;
#ifdef VT_HEVC_NATIVE
   if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC))
   {
      rh265_video_close(source);
      puts("VideoToolbox HEVC: no hardware decoder; session check unavailable");
      exit(77);
   }
#endif
   assert(!videotoolbox_hevc_session_create(source, 0, &callback, &format, &session));
   assert(format && session);
   VTDecompressionSessionInvalidate(session); CFRelease(session); CFRelease(format);
   format = NULL; session = NULL;
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
   test(hevc_main8_limited, sizeof(hevc_main8_limited), 8, 0);
   test(hevc_main10_limited, sizeof(hevc_main10_limited), 10, 0);
   test(hevc_main8_full, sizeof(hevc_main8_full), 8, 1);
   test(hevc_main10_full, sizeof(hevc_main10_full), 10, 1);
   puts("VideoToolbox HEVC: owned parameters and Main/Main10 session setup passed");
   return 0;
}
