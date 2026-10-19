/* Several owned IDR submissions before any owner-side result consumption. */
#ifdef VIDEOTOOLBOX_HEVC_SLOTS
static CMSampleTimingInfo batch_timing(unsigned index)
{
   CMSampleTimingInfo timing;
   timing.presentationTimeStamp = slot_time(0, 0);
   timing.presentationTimeStamp.value = (int64_t)index * 1001;
   timing.presentationTimeStamp.flags = 1;
   timing.presentationTimeStamp.epoch = 0;
   timing.duration = slot_time(0, 1);
   timing.decodeTimeStamp = kCMTimeInvalid;
   return timing;
}
#endif

static void test_codec_batch(void)
{
#ifdef VIDEOTOOLBOX_HEVC_SLOTS
   unsigned which;
   for (which = 0; which < sizeof(codec_cases) / sizeof(codec_cases[0]); which++)
   {
      const codec_case_t *fixture = &codec_cases[which];
      rh265_video *source = rh265_video_open();
      videotoolbox_hevc_slots_t *context = videotoolbox_hevc_slots_new(VT_HEVC_TEST_BATCH);
      videotoolbox_hevc_result_t results[VT_HEVC_TEST_BATCH];
      VTDecompressionOutputCallbackRecord callback;
      VTDecompressionSessionRef session = NULL;
      CMVideoFormatDescriptionRef format = NULL;
      rh265_access_unit_t au;
      rh265_nal_t slices[8];
      uint8_t *packet = (uint8_t*)malloc(fixture->sample_size);
      CMSampleTimingInfo timing;
      VTDecodeInfoFlags info;
      OSStatus status;
      unsigned i;
      assert(source && context && packet);
      assert(!rh265_video_retain_parameter_sets(source));
      assert(!rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      memcpy(packet, fixture->sample, fixture->sample_size);
      assert(!rh265_video_get_access_unit(source, packet, fixture->sample_size, 4, slices, 8, &au));
      callback.decompressionOutputCallback = videotoolbox_hevc_slots_output;
      callback.decompressionOutputRefCon = context;
#ifndef VT_HEVC_NATIVE
      assert(!rh265_video_get_parameter_sets(source, (unsigned)au.first.pps_id, wanted_sets));
      wanted_pixel = fixture->depth == 10 ? (fixture->full ? 0x78663230 : 0x78343230)
         : (fixture->full ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                          : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
      assert(!batch_count && !pending_sample);
      decode_mode = 4;
#endif
      status = videotoolbox_hevc_session_create(source, (unsigned)au.first.pps_id,
            &callback, &format, &session);
      if (status) fprintf(stderr, "%s batch: session status %ld\n", fixture->name, (long)status);
      assert(!status);
      for (i = 0; i < VT_HEVC_TEST_BATCH; i++)
      {
         timing = batch_timing(i);
         status = videotoolbox_hevc_slots_submit_timed(context, i, session, format,
               &au, slices, 8, &timing, &info);
         if (status) fprintf(stderr, "%s batch %u: submission status %ld\n",
               fixture->name, i, (long)status);
         assert(!status && !(info & kVTDecodeInfo_FrameDropped));
         memset(&timing, 0xa5, sizeof(timing));
      }
      info = 99;
      assert(videotoolbox_hevc_slots_submit(context, 0, session, format,
               &au, slices, 8, &info) == 1);
      assert(!info);
      videotoolbox_hevc_slots_close(context);
      memset(packet, 0xa5, fixture->sample_size); free(packet);
      rh265_video_close(source); CFRelease(format);
#ifndef VT_HEVC_NATIVE
      assert(batch_count == VT_HEVC_TEST_BATCH);
      for (i = VT_HEVC_TEST_BATCH; i > 0; i--)
      {
         CMSampleBufferRef sample = batch_samples[i - 1];
         CVPixelBufferRef image = slot_image();
         uint8_t *bytes = (uint8_t*)malloc(fixture->sample_size);
         assert(image && bytes && sample);
         assert(CMSampleBufferGetTotalSampleSize(sample) == fixture->sample_size);
         assert(!CMBlockBufferCopyDataBytes(CMSampleBufferGetDataBuffer(sample),
                  0, fixture->sample_size, bytes));
         assert(!memcmp(bytes, fixture->sample, fixture->sample_size)); free(bytes);
         session->callback(context, batch_frames[i - 1], 0, 0, image,
               sample->timing.presentationTimeStamp, sample->timing.duration);
         CVPixelBufferRelease(image); CFRelease(sample);
         batch_samples[i - 1] = NULL; batch_frames[i - 1] = NULL;
      }
      batch_count = 0;
#endif
      for (i = 0; i < VT_HEVC_TEST_BATCH; i++)
      {
         while (!videotoolbox_hevc_slots_take(context, i, &results[i])) sthread_yield();
         if (results[i].status || !results[i].image || (results[i].flags & kVTDecodeInfo_FrameDropped))
            fprintf(stderr, "%s batch %u: callback status %ld, image %p, flags %u\n",
                  fixture->name, i, (long)results[i].status, (void*)results[i].image,
                  (unsigned)results[i].flags);
         assert(!results[i].status && results[i].image && !(results[i].flags & kVTDecodeInfo_FrameDropped));
         timing = batch_timing(i);
#ifdef VT_HEVC_NATIVE
         assert(CMTIME_IS_NUMERIC(results[i].pts) && CMTIME_IS_NUMERIC(results[i].duration));
         assert(!CMTimeCompare(results[i].pts, timing.presentationTimeStamp));
         assert(!CMTimeCompare(results[i].duration, timing.duration));
#else
         slot_check_time(results[i].pts, timing.presentationTimeStamp);
         slot_check_time(results[i].duration, timing.duration);
#endif
      }
      VTDecompressionSessionInvalidate(session); CFRelease(session);
      videotoolbox_hevc_slots_release(context);
      for (i = 0; i < VT_HEVC_TEST_BATCH; i++)
      {
#ifdef VT_HEVC_NATIVE
         codec_native_pixels(fixture, results[i].image);
#endif
         CVPixelBufferRelease(results[i].image);
      }
#ifdef VT_HEVC_NATIVE
      printf("VideoToolbox native batch: %s eight timed images survive source and session release\n", fixture->name);
#else
      assert(!objects);
      printf("VideoToolbox mock batch: %s eight owned samples complete in reverse order\n", fixture->name);
#endif
   }
#endif
}
