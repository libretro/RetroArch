/* Exercise the shipping slot state machine with real or mock CV references. */
#ifdef VIDEOTOOLBOX_HEVC_SLOTS
#include <rthreads/rthreads.h>

static CVPixelBufferRef slot_image(void)
{
#ifdef VT_HEVC_NATIVE
   CVPixelBufferRef image = NULL;
   assert(!CVPixelBufferCreate(kCFAllocatorDefault, 2, 2,
            kCVPixelFormatType_32BGRA, NULL, &image));
   return image;
#else
   return create(7, 0);
#endif
}

static CMTime slot_time(unsigned serial, int duration)
{
   CMTime time;
   time.value = duration ? 2002 : ((int64_t)serial - 50000) * 1001001;
   time.timescale = duration ? 60000 : 30000;
   time.flags = 1u | (serial & 2u);
   time.epoch = duration ? 0 : serial / 3;
   return time;
}

static void slot_check_time(CMTime actual, CMTime expected)
{
   assert(actual.value == expected.value && actual.timescale == expected.timescale);
   assert(actual.flags == expected.flags && actual.epoch == expected.epoch);
}

static void slot_complete(videotoolbox_hevc_slots_t *context,
      videotoolbox_hevc_slot_t *slot, OSStatus status,
      VTDecodeInfoFlags flags, CVPixelBufferRef image)
{
   unsigned serial = status < 0 ? (unsigned)-(int64_t)status : flags;
   videotoolbox_hevc_slots_output(context, slot, status, flags, image,
         slot_time(serial, 0), slot_time(serial, 1));
}

static void slot_worker(void *ref)
{
   videotoolbox_hevc_slots_t *context = (videotoolbox_hevc_slots_t*)ref;
   unsigned i;
   for (i = 0; i < 100000; i++)
   {
      while (retro_atomic_load_acquire_int(&context->slot[0].state) != 1)
         sthread_yield();
      slot_complete(context, &context->slot[0], -(OSStatus)i - 1, i & 1, NULL);
   }
}

static void slot_unload_worker(void *ref)
{
   videotoolbox_hevc_slots_t *context = (videotoolbox_hevc_slots_t*)ref;
   slot_complete(context, &context->slot[0], 0, kVTDecodeInfo_FrameDropped, NULL);
}

static void test_slots(CMVideoFormatDescriptionRef format)
{
   videotoolbox_hevc_slots_t *context;
   videotoolbox_hevc_slot_t *tickets[32];
   videotoolbox_hevc_result_t result;
   CVPixelBufferRef image;
   sthread_t *thread;
   unsigned i;
   (void)format;
   assert(!videotoolbox_hevc_slots_new(0) && !videotoolbox_hevc_slots_new(33));
   context = videotoolbox_hevc_slots_new(32); assert(context);
   assert(videotoolbox_hevc_slots_take(context, 32, &result) == -1);
   assert(!result.image && !result.status && !result.flags);
   slot_check_time(result.pts, kCMTimeInvalid);
   slot_check_time(result.duration, kCMTimeInvalid);
   assert(!videotoolbox_hevc_slots_begin(context, 32));
   for (i = 0; i < 32; i++)
   {
      tickets[i] = videotoolbox_hevc_slots_begin(context, i); assert(tickets[i]);
      assert(!videotoolbox_hevc_slots_begin(context, i));
      assert(!videotoolbox_hevc_slots_take(context, i, &result));
      slot_check_time(result.pts, kCMTimeInvalid);
      slot_check_time(result.duration, kCMTimeInvalid);
   }
   videotoolbox_hevc_slots_close(NULL);
   videotoolbox_hevc_slots_close(context);
   videotoolbox_hevc_slots_close(context);
   for (i = 0; i < 32; i++)
      assert(!videotoolbox_hevc_slots_begin(context, i));
   for (i = 32; i > 0; i--)
      slot_complete(context, tickets[i - 1], -(OSStatus)i, i, NULL);
   for (i = 0; i < 32; i++)
   {
      assert(videotoolbox_hevc_slots_take(context, i, &result) == 1);
      assert(result.status == -(OSStatus)i - 1 && result.flags == i + 1 && !result.image);
      slot_check_time(result.pts, slot_time(i + 1, 0));
      slot_check_time(result.duration, slot_time(i + 1, 1));
      assert(!videotoolbox_hevc_slots_take(context, i, &result));
      slot_check_time(result.pts, kCMTimeInvalid);
      slot_check_time(result.duration, kCMTimeInvalid);
      assert(!videotoolbox_hevc_slots_begin(context, i));
   }
   videotoolbox_hevc_slots_release(context);
   context = videotoolbox_hevc_slots_new(1); assert(context);
   image = slot_image(); assert(image);
   tickets[0] = videotoolbox_hevc_slots_begin(context, 0); assert(tickets[0]);
   slot_complete(context, tickets[0], 0, 0, image);
   CVPixelBufferRelease(image);
   videotoolbox_hevc_slots_close(context);
   assert(videotoolbox_hevc_slots_take(context, 0, &result) == 1);
   assert(result.image && !result.status && !result.flags);
   slot_check_time(result.pts, slot_time(0, 0));
   slot_check_time(result.duration, slot_time(0, 1));
   image = result.image;
   videotoolbox_hevc_slots_release(context);
   CVPixelBufferRelease(image);

   context = videotoolbox_hevc_slots_new(2); assert(context);
   tickets[0] = videotoolbox_hevc_slots_begin(context, 0);
   tickets[1] = videotoolbox_hevc_slots_begin(context, 1);
   videotoolbox_hevc_slots_release(context); /* Owner unloads before callbacks. */
   for (i = 0; i < 2; i++)
   {
      image = slot_image(); assert(image);
      slot_complete(context, tickets[i], 0, 0, image);
      CVPixelBufferRelease(image);
   }

   context = videotoolbox_hevc_slots_new(2); assert(context);
   tickets[0] = videotoolbox_hevc_slots_begin(context, 0);
   tickets[1] = videotoolbox_hevc_slots_begin(context, 1);
   image = slot_image(); assert(image);
   slot_complete(context, tickets[0], -42, 0, image);
   CVPixelBufferRelease(image);
   assert(videotoolbox_hevc_slots_take(context, 0, &result) == 1);
   assert(result.status == -42 && !result.image);
   slot_check_time(result.pts, slot_time(42, 0));
   slot_check_time(result.duration, slot_time(42, 1));
   slot_complete(context, tickets[1], 0, kVTDecodeInfo_FrameDropped, NULL);
   assert(videotoolbox_hevc_slots_take(context, 1, &result) == 1);
   assert(!result.status && !result.image && result.flags == kVTDecodeInfo_FrameDropped);
   assert(videotoolbox_hevc_slots_begin(context, 0));
   assert(videotoolbox_hevc_slots_begin(context, 1));
   videotoolbox_hevc_slots_quiesce(context); /* Mock session has stopped. */
   videotoolbox_hevc_slots_quiesce(context);
   videotoolbox_hevc_slots_release(context);

#ifndef VT_HEVC_NATIVE
   {
      static const uint8_t payload[3] = {38, 1, 0x80};
      rh265_nal_t nal;
      rh265_access_unit_t au;
      VTDecompressionSessionRef session = create(4, 0);
      VTDecodeInfoFlags info;
      context = videotoolbox_hevc_slots_new(1); assert(context && session);
      session->callback = videotoolbox_hevc_slots_output; session->callback_ref = context;
      memset(&au, 0, sizeof(au)); au.num_slices = 1; au.coded_bytes = 3;
      au.first.nal_type = 19; nal.data = payload; nal.size = sizeof(payload);
      decode_mode = 1;
      assert(!videotoolbox_hevc_slots_submit(context, 0, session, format, &au, &nal, 1, &info));
      assert(videotoolbox_hevc_slots_take(context, 0, &result) == 1);
      slot_check_time(result.pts, kCMTimeInvalid);
      slot_check_time(result.duration, kCMTimeInvalid);
      decode_mode = 2;
      assert(videotoolbox_hevc_slots_submit(context, 0, session, format, &au, &nal, 1, &info));
      decode_mode = 0;
      assert(!videotoolbox_hevc_slots_submit(context, 0, session, format, &au, &nal, 1, &info));
      assert(info == kVTDecodeInfo_Asynchronous);
      assert(videotoolbox_hevc_slots_submit(context, 0, session, format, &au, &nal, 1, &info) == 1);
      assert(!info);
      session->callback(context, pending_frame, 0, 0, NULL, kCMTimeInvalid, kCMTimeInvalid);
      CFRelease(pending_sample); pending_sample = NULL;
      assert(videotoolbox_hevc_slots_take(context, 0, &result) == 1);
      slot_check_time(result.pts, kCMTimeInvalid);
      slot_check_time(result.duration, kCMTimeInvalid);
      decode_mode = 3;
      assert(!videotoolbox_hevc_slots_submit(context, 0, session, format, &au, &nal, 1, &info));
      assert(info == kVTDecodeInfo_FrameDropped);
      assert(!videotoolbox_hevc_slots_take(context, 0, &result));
      videotoolbox_hevc_slots_close(context);
      info = 99;
      assert(videotoolbox_hevc_slots_submit(context, 0, session, format, &au, &nal, 1, &info)
            == kCMFormatDescriptionError_InvalidParameter);
      assert(!info);
      VTDecompressionSessionInvalidate(session);
      videotoolbox_hevc_slots_quiesce(context);
      videotoolbox_hevc_slots_release(context);
      CFRelease(session);
   }
#endif

   context = videotoolbox_hevc_slots_new(1); assert(context);
   thread = sthread_create(slot_worker, context); assert(thread);
   for (i = 0; i < 100000; i++)
   {
      assert(videotoolbox_hevc_slots_begin(context, 0));
      while (!videotoolbox_hevc_slots_take(context, 0, &result))
         sthread_yield();
      assert(!result.image && result.status == -(OSStatus)i - 1 && result.flags == (i & 1));
      slot_check_time(result.pts, slot_time(i + 1, 0));
      slot_check_time(result.duration, slot_time(i + 1, 1));
   }
   sthread_join(thread);
   videotoolbox_hevc_slots_release(context);
   for (i = 0; i < 100; i++)
   {
      context = videotoolbox_hevc_slots_new(1); assert(context);
      assert(videotoolbox_hevc_slots_begin(context, 0));
      thread = sthread_create(slot_unload_worker, context); assert(thread);
      videotoolbox_hevc_slots_close(context);
      videotoolbox_hevc_slots_release(context);
      sthread_join(thread);
   }
   puts("VideoToolbox HEVC slots: ownership and 100000 concurrent publications passed");
   puts("VideoToolbox HEVC slots: closed admission preserves completion and drain");
   puts("VideoToolbox HEVC slots: exact callback timing survives reordered and concurrent publication");
}
#else
static void test_slots(CMVideoFormatDescriptionRef format)
{
   (void)format;
   puts("VideoToolbox HEVC slots: unavailable without threads and lock-free atomics");
}
#endif
