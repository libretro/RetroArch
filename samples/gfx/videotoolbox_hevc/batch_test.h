/* Several owned submissions before any owner-side result consumption. */
#ifdef VIDEOTOOLBOX_HEVC_SLOTS
static CMSampleTimingInfo batch_timing(unsigned index, const sequence_case_t *sequence)
{
   CMSampleTimingInfo timing;
   timing.presentationTimeStamp = slot_time(0, 0);
   timing.presentationTimeStamp.value = (int64_t)(sequence ? sequence->packets[index].display_index : index) * 1001;
   timing.presentationTimeStamp.flags = 1;
   timing.presentationTimeStamp.epoch = 0;
   timing.duration = slot_time(0, 1);
   timing.decodeTimeStamp = kCMTimeInvalid;
   if (sequence)
   {
      timing.decodeTimeStamp = timing.presentationTimeStamp;
      timing.decodeTimeStamp.value = sequence->packets[index].dts;
   }
   return timing;
}
#endif

static void test_codec_batch(void)
{
#ifdef VIDEOTOOLBOX_HEVC_SLOTS
   unsigned which;
   unsigned singles = sizeof(codec_cases) / sizeof(codec_cases[0]);
   unsigned sequences = sizeof(sequence_cases) / sizeof(sequence_cases[0]);
   unsigned reorders = sizeof(reorder_cases) / sizeof(reorder_cases[0]);
   for (which = 0; which < singles + sequences + reorders; which++)
   {
      const sequence_case_t *sequence = which < singles ? NULL :
         (which < singles + sequences ? &sequence_cases[which - singles] : &reorder_cases[which - singles - sequences]);
      const codec_case_t *fixture = sequence ? &sequence->picture : &codec_cases[which];
      rh265_video *source = rh265_video_open();
      videotoolbox_hevc_slots_t *context = videotoolbox_hevc_slots_new(VT_HEVC_TEST_BATCH);
      videotoolbox_hevc_result_t results[VT_HEVC_TEST_BATCH];
      VTDecompressionOutputCallbackRecord callback;
      VTDecompressionSessionRef session = NULL;
      CMVideoFormatDescriptionRef format = NULL;
      rh265_access_unit_t au;
      rh265_nal_t slices[8];
      size_t packet_capacity = fixture->sample_size;
      uint8_t *packet;
      CMSampleTimingInfo timing;
      VTDecodeInfoFlags info;
      OSStatus status;
      unsigned i;
      for (i = 0; sequence && i < VT_HEVC_TEST_BATCH; i++)
         if (sequence->packets[i].size > packet_capacity) packet_capacity = sequence->packets[i].size;
      packet = (uint8_t*)malloc(packet_capacity);
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
         if (sequence)
         {
            memset(packet, 0xa5, packet_capacity);
            memcpy(packet, sequence->packets[i].data, sequence->packets[i].size);
            sequence_access_unit(source, sequence, packet, sequence->packets[i].size, i, slices, &au);
         }
         timing = batch_timing(i, sequence);
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
      memset(packet, 0xa5, packet_capacity); free(packet);
      rh265_video_close(source); CFRelease(format);
#ifndef VT_HEVC_NATIVE
      assert(batch_count == VT_HEVC_TEST_BATCH);
      for (i = VT_HEVC_TEST_BATCH; i > 0; i--)
      {
         CMSampleBufferRef sample = batch_samples[i - 1];
         CVPixelBufferRef image = slot_image();
         const uint8_t *expected = sequence ? sequence->packets[i - 1].data : fixture->sample;
         size_t expected_size = sequence ? sequence->packets[i - 1].size : fixture->sample_size;
         uint8_t *bytes = (uint8_t*)malloc(expected_size);
         assert(image && bytes && sample);
         assert(CMSampleBufferGetTotalSampleSize(sample) == expected_size);
         assert(!CMBlockBufferCopyDataBytes(CMSampleBufferGetDataBuffer(sample),
                  0, expected_size, bytes));
         assert(!memcmp(bytes, expected, expected_size)); free(bytes);
         timing = batch_timing(i - 1, sequence);
         slot_check_time(sample->timing.decodeTimeStamp, timing.decodeTimeStamp);
         session->callback(context, batch_frames[i - 1], 0, 0, image,
               sample->timing.presentationTimeStamp, sample->timing.duration);
         CVPixelBufferRelease(image); CFRelease(sample);
         batch_samples[i - 1] = NULL; batch_frames[i - 1] = NULL;
      }
      batch_count = 0;
#endif
      /* Temporal processing is disabled at submission. Even the final B
       * pictures must complete without a producer-thread flush or wait. */
      for (i = 0; i < VT_HEVC_TEST_BATCH; i++)
      {
         while (!videotoolbox_hevc_slots_take(context, i, &results[i])) sthread_yield();
         if (results[i].status || !results[i].image || (results[i].flags & kVTDecodeInfo_FrameDropped))
            fprintf(stderr, "%s batch %u: callback status %ld, image %p, flags %u\n",
                  fixture->name, i, (long)results[i].status, (void*)results[i].image,
                  (unsigned)results[i].flags);
         assert(!results[i].status && results[i].image && !(results[i].flags & kVTDecodeInfo_FrameDropped));
         timing = batch_timing(i, sequence);
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
         codec_case_t picture = *fixture;
         if (sequence) memcpy(picture.crc, sequence->packets[i].crc, sizeof(picture.crc));
         codec_native_pixels(&picture, results[i].image);
#ifdef VT_HEVC_METAL
         videotoolbox_hevc_metal_test(results[i].image, picture.depth, picture.full, picture.crc);
#else
         CVPixelBufferRelease(results[i].image);
#endif
#else
         CVPixelBufferRelease(results[i].image);
#endif
      }
#ifdef VT_HEVC_NATIVE
      if (sequence && sequence->reordered)
         printf("VideoToolbox native reorder: %s eight reordered pictures match FFmpeg\n", fixture->name);
      else if (sequence)
         printf("VideoToolbox native sequence: %s eight dependent pictures match FFmpeg\n", fixture->name);
      else
         printf("VideoToolbox native batch: %s eight timed images survive source and session release\n", fixture->name);
#else
      assert(!objects);
      if (sequence && sequence->reordered)
         printf("VideoToolbox mock reorder: %s eight decode-order samples preserve PTS and DTS\n", fixture->name);
      else if (sequence)
         printf("VideoToolbox mock sequence: %s eight owned samples complete in reverse order\n", fixture->name);
      else
         printf("VideoToolbox mock batch: %s eight owned samples complete in reverse order\n", fixture->name);
#endif
   }
#endif
}
