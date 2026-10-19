/* Actual HEVC packets with FFmpeg-derived planar pixel checksums. */
typedef struct codec_case
{
   const char *name;
   const uint8_t *hvcc;
   size_t hvcc_size;
   const uint8_t *sample;
   size_t sample_size;
   unsigned depth, full;
   uint32_t crc[3];
} codec_case_t;

#include "codec_data.h"

static uint32_t codec_crc_sample(uint32_t crc, unsigned value, unsigned depth)
{
   unsigned byte, bit;
   for (byte = 0; byte < (depth == 10 ? 2u : 1u); byte++)
   {
      crc ^= (value >> (byte * 8)) & 255;
      for (bit = 0; bit < 8; bit++)
         crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
   }
   return crc;
}

static void codec_check_crc(const codec_case_t *fixture, unsigned plane, uint32_t actual)
{
   if (actual != fixture->crc[plane])
      fprintf(stderr, "%s plane %u: expected %08lx, got %08lx\n", fixture->name,
            plane, (unsigned long)fixture->crc[plane], (unsigned long)actual);
   assert(actual == fixture->crc[plane]);
}

static void test_codec_software(void)
{
   unsigned i, plane;
   for (i = 0; i < sizeof(codec_cases) / sizeof(codec_cases[0]); i++)
   {
      const codec_case_t *fixture = &codec_cases[i];
      rh265_video *source = rh265_video_open();
      int ready;
      assert(source && !rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      ready = rh265_video_decode(source, fixture->sample, fixture->sample_size);
      assert(ready >= 0 && ready <= 1);
      if (!ready) assert(!rh265_video_drain(source));
      assert((unsigned)rh265_video_bit_depth(source) == fixture->depth);
      for (plane = 0; plane < 3; plane++)
      {
         int stride, width, height, x, y;
         const uint8_t *data = rh265_video_plane(source, (int)plane, &stride, &width, &height);
         uint32_t crc = UINT32_MAX;
         assert(data && width == (plane ? 32 : 64) && height == width && stride >= width);
         for (y = 0; y < height; y++)
            for (x = 0; x < width; x++)
            {
               unsigned value = fixture->depth == 10
                  ? ((const uint16_t*)data)[(size_t)y * stride + x]
                  : data[(size_t)y * stride + x];
               crc = codec_crc_sample(crc, value, fixture->depth);
            }
         codec_check_crc(fixture, plane, crc ^ UINT32_MAX);
      }
      rh265_video_close(source);
   }
   puts("VideoToolbox codec software: four spatial Main/Main10 fixtures match FFmpeg pixels");
}

#ifdef VT_HEVC_METAL
void videotoolbox_hevc_metal_test(CVPixelBufferRef image, unsigned depth,
      unsigned full, const uint32_t expected[3]);
#endif
#if defined(VT_HEVC_NATIVE) && defined(VIDEOTOOLBOX_HEVC_SLOTS)
static void codec_native_pixels(const codec_case_t *fixture, CVPixelBufferRef image)
{
   unsigned plane;
   uint32_t pixel = fixture->depth == 10 ? (fixture->full ? 0x78663230u : 0x78343230u)
      : (fixture->full ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                       : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
   assert(CVPixelBufferGetPixelFormatType(image) == pixel);
   assert(CVPixelBufferGetWidth(image) == 64 && CVPixelBufferGetHeight(image) == 64);
   assert(CVPixelBufferGetPlaneCount(image) == 2);
   assert(!CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly));
   for (plane = 0; plane < 3; plane++)
   {
      unsigned index = plane ? 1 : 0;
      unsigned width = plane ? 32 : 64, step = plane ? 2 : 1;
      unsigned component = plane == 2 ? 1 : 0;
      unsigned x, y, pel = fixture->depth == 10 ? 2 : 1;
      const uint8_t *data = (const uint8_t*)CVPixelBufferGetBaseAddressOfPlane(image, index);
      size_t stride = CVPixelBufferGetBytesPerRowOfPlane(image, index);
      uint32_t crc = UINT32_MAX;
      assert(data && stride >= (size_t)width * step * pel);
      assert(CVPixelBufferGetWidthOfPlane(image, index) == width);
      assert(CVPixelBufferGetHeightOfPlane(image, index) == width);
      for (y = 0; y < width; y++)
         for (x = 0; x < width; x++)
         {
            const uint8_t *row = data + (size_t)y * stride;
            unsigned value = fixture->depth == 10
               ? ((const uint16_t*)row)[x * step + component] >> 6
               : row[x * step + component];
            crc = codec_crc_sample(crc, value, fixture->depth);
         }
      codec_check_crc(fixture, plane, crc ^ UINT32_MAX);
   }
   assert(!CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly));
}
#endif

static void test_codec_submission(void)
{
#ifdef VIDEOTOOLBOX_HEVC_SLOTS
   unsigned i;
   for (i = 0; i < sizeof(codec_cases) / sizeof(codec_cases[0]); i++)
   {
      const codec_case_t *fixture = &codec_cases[i];
      rh265_video *source = rh265_video_open();
      videotoolbox_hevc_slots_t *context = videotoolbox_hevc_slots_new(1);
      videotoolbox_hevc_result_t result;
      VTDecompressionOutputCallbackRecord callback;
      VTDecompressionSessionRef session = NULL;
      CMVideoFormatDescriptionRef format = NULL;
      rh265_access_unit_t au;
      rh265_nal_t slices[8];
      uint8_t *packet = (uint8_t*)malloc(fixture->sample_size);
      VTDecodeInfoFlags info;
      CMSampleTimingInfo timing, expected;
      OSStatus status;
      assert(source && context && packet);
      assert(!rh265_video_retain_parameter_sets(source));
      assert(!rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      memcpy(packet, fixture->sample, fixture->sample_size);
      assert(!rh265_video_get_access_unit(source, packet, fixture->sample_size, 4, slices, 8, &au));
      assert(au.num_slices == 1 && au.intra && au.first.pps_id >= 0);
      callback.decompressionOutputCallback = videotoolbox_hevc_slots_output;
      callback.decompressionOutputRefCon = context;
#ifndef VT_HEVC_NATIVE
      assert(!rh265_video_get_parameter_sets(source, (unsigned)au.first.pps_id, wanted_sets));
      wanted_pixel = fixture->depth == 10 ? (fixture->full ? 0x78663230 : 0x78343230)
         : (fixture->full ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                          : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
      decode_mode = 0;
#endif
      status = videotoolbox_hevc_session_create(source, (unsigned)au.first.pps_id,
            &callback, &format, &session);
      if (status) fprintf(stderr, "%s: session setup status %ld\n", fixture->name, (long)status);
      assert(!status);
      timing.presentationTimeStamp = slot_time(i, 0);
      timing.presentationTimeStamp.value = (int64_t)i * 1001;
      timing.presentationTimeStamp.flags = 1;
      timing.presentationTimeStamp.epoch = 0;
      timing.duration = slot_time(0, 1);
      timing.decodeTimeStamp = kCMTimeInvalid;
      expected = timing;
      status = videotoolbox_hevc_slots_submit_timed(context, 0, session, format,
            &au, slices, 8, &timing, &info);
      if (status) fprintf(stderr, "%s: decode admission status %ld\n", fixture->name, (long)status);
      assert(!status);
      if (info & kVTDecodeInfo_FrameDropped)
         fprintf(stderr, "%s: decode admission dropped the picture, flags %u\n", fixture->name, (unsigned)info);
      assert(!(info & kVTDecodeInfo_FrameDropped));
      memset(&timing, 0xa5, sizeof(timing));
      videotoolbox_hevc_slots_close(context);
      memset(packet, 0xa5, fixture->sample_size); free(packet);
      rh265_video_close(source); CFRelease(format);
#ifndef VT_HEVC_NATIVE
      {
         uint8_t *bytes = (uint8_t*)malloc(fixture->sample_size);
         CVPixelBufferRef image = slot_image();
         assert(bytes && image && pending_sample);
         assert(CMSampleBufferGetTotalSampleSize(pending_sample) == fixture->sample_size);
         assert(!CMBlockBufferCopyDataBytes(CMSampleBufferGetDataBuffer(pending_sample),
                  0, fixture->sample_size, bytes));
         assert(!memcmp(bytes, fixture->sample, fixture->sample_size)); free(bytes);
         slot_check_time(pending_sample->timing.decodeTimeStamp, expected.decodeTimeStamp);
         session->callback(context, pending_frame, 0, 0, image,
               pending_sample->timing.presentationTimeStamp, pending_sample->timing.duration);
         CVPixelBufferRelease(image);
         CFRelease(pending_sample); pending_sample = NULL;
      }
#endif
#ifdef VT_HEVC_NATIVE
      fprintf(stderr, "%s: accepted decode, callback wait (info %u)\n", fixture->name, (unsigned)info);
#endif
      while (!videotoolbox_hevc_slots_take(context, 0, &result)) sthread_yield();
      if (result.status || !result.image || (result.flags & kVTDecodeInfo_FrameDropped))
         fprintf(stderr, "%s: decode callback status %ld, image %p, flags %u\n",
               fixture->name, (long)result.status, (void*)result.image, (unsigned)result.flags);
      assert(!result.status && result.image && !(result.flags & kVTDecodeInfo_FrameDropped));
#ifdef VT_HEVC_NATIVE
      assert(CMTIME_IS_NUMERIC(result.pts) && CMTIME_IS_NUMERIC(result.duration));
      assert(!CMTimeCompare(result.pts, expected.presentationTimeStamp));
      assert(!CMTimeCompare(result.duration, expected.duration));
#else
      slot_check_time(result.pts, expected.presentationTimeStamp);
      slot_check_time(result.duration, expected.duration);
#endif
      printf("VideoToolbox codec timing: %s rational presentation and duration passed\n", fixture->name);
      VTDecompressionSessionInvalidate(session); CFRelease(session);
      videotoolbox_hevc_slots_release(context);
#ifdef VT_HEVC_NATIVE
      codec_native_pixels(fixture, result.image);
      printf("VideoToolbox native codec: %s decoded pixels and released-source ownership passed\n", fixture->name);
#endif
#ifdef VT_HEVC_METAL
      videotoolbox_hevc_metal_test(result.image, fixture->depth, fixture->full, fixture->crc);
#else
      CVPixelBufferRelease(result.image);
#endif
   }
#ifndef VT_HEVC_NATIVE
   puts("VideoToolbox codec mock: four real access units preserve owned submission bytes");
#endif
#elif defined(VT_HEVC_NATIVE)
   puts("VideoToolbox codec: slot callbacks unavailable; native decoding did not run");
   exit(77);
#endif
}
