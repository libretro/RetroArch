#include "../../../gfx/common/videotoolbox_hevc_owner.h"

#ifdef VIDEOTOOLBOX_HEVC_SLOTS
typedef struct owner_queue
{
   void (*run)(void *);
   void *work;
   unsigned submitted, late;
} owner_queue_t;

static void owner_enqueue(void *user, void (*run)(void *), void *work)
{
   owner_queue_t *queue = (owner_queue_t*)user;
   assert(!queue->run && !queue->work && run && work);
   queue->run = run; queue->work = work; queue->submitted++;
}

static void owner_worker(void *user)
{
   owner_queue_t *queue = (owner_queue_t*)user;
   if (queue->late)
   {
      videotoolbox_hevc_owner_t *owner = (videotoolbox_hevc_owner_t*)queue->work;
      slot_complete(owner->slots, &owner->slots->slot[1], 0, kVTDecodeInfo_FrameDropped, NULL);
   }
   queue->run(queue->work);
   queue->run = NULL; queue->work = NULL;
}
#endif

static void test_session_owner(void)
{
#ifdef VIDEOTOOLBOX_HEVC_SLOTS
   unsigned which;
   for (which = 0; which < 2 * sizeof(codec_cases) / sizeof(codec_cases[0]); which++)
   {
      const codec_case_t *fixture = &codec_cases[which / 2];
      rh265_video *source = rh265_video_open();
      videotoolbox_hevc_owner_t *owner;
      videotoolbox_hevc_slots_t *slots;
      videotoolbox_hevc_result_t result;
      CVPixelBufferRef image;
      owner_queue_t queue;
      sthread_t *thread;
      OSStatus status;
#ifdef VT_HEVC_NATIVE
      rh265_nal_t slices[8];
      rh265_access_unit_t au;
      VTDecodeInfoFlags info;
#endif
      memset(&queue, 0, sizeof(queue));
      queue.late = which & 1;
      assert(source && !rh265_video_retain_parameter_sets(source));
      assert(!rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
#ifndef VT_HEVC_NATIVE
      assert(!rh265_video_get_parameter_sets(source, 0, wanted_sets));
      wanted_pixel = fixture->depth == 10 ? (fixture->full ? 0x78663230 : 0x78343230)
         : (fixture->full ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                          : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
      {
         unsigned i;
         for (i = 1; i <= 6; i++)
         {
            fail_step = i; step = 0;
            assert(!videotoolbox_hevc_owner_new(source, 0, 3, owner_enqueue, &queue, &status));
            assert(status && !objects && !queue.submitted);
         }
         fail_step = step = 0;
      }
#endif
      assert(!videotoolbox_hevc_owner_new(source, 0, 3, NULL, &queue, &status));
      assert(status);
      owner = videotoolbox_hevc_owner_new(source, 0, 3, owner_enqueue, &queue, &status);
      if (!owner) fprintf(stderr, "%s owner: session status %ld\n", fixture->name, (long)status);
      assert(owner && !status);
      slots = owner->slots;
#ifdef VT_HEVC_NATIVE
      assert(!rh265_video_get_access_unit(source, fixture->sample, fixture->sample_size,
               4, slices, 8, &au));
      status = videotoolbox_hevc_slots_submit(slots, 0, owner->session, owner->format,
            &au, slices, 8, &info);
      if (status) fprintf(stderr, "%s owner: admission status %ld\n", fixture->name, (long)status);
      assert(!status);
      assert(!(info & kVTDecodeInfo_FrameDropped));
      while (!videotoolbox_hevc_slots_take(slots, 0, &result)) sthread_yield();
      if (result.status || !result.image || (result.flags & kVTDecodeInfo_FrameDropped))
         fprintf(stderr, "%s owner: callback status %ld, image %p, flags %u\n",
               fixture->name, (long)result.status, (void*)result.image, (unsigned)result.flags);
      assert(!result.status && result.image && !(result.flags & kVTDecodeInfo_FrameDropped));
#else
      image = slot_image(); assert(image);
      assert(videotoolbox_hevc_slots_begin(slots, 0));
      slot_complete(slots, &slots->slot[0], 0, 0, image);
      assert(videotoolbox_hevc_slots_take(slots, 0, &result) == 1);
      CVPixelBufferRelease(image);
#endif
      rh265_video_close(source);
      image = slot_image(); assert(image);
      assert(videotoolbox_hevc_slots_begin(slots, 1));
      assert(videotoolbox_hevc_slots_begin(slots, 2));
      slot_complete(slots, &slots->slot[2], 0, 0, image);
      CVPixelBufferRelease(image);
      videotoolbox_hevc_owner_retire(&owner);
      assert(!owner && queue.submitted == 1 && queue.run && queue.work);
      assert(slots->closed && !videotoolbox_hevc_slots_begin(slots, 0));
#ifndef VT_HEVC_NATIVE
      assert(!((videotoolbox_hevc_owner_t*)queue.work)->session->invalidated);
#endif
      videotoolbox_hevc_owner_retire(&owner);
      assert(queue.submitted == 1);
      thread = sthread_create(owner_worker, &queue); assert(thread);
      sthread_join(thread);
      assert(!queue.run && !queue.work && result.image);
#ifdef VT_HEVC_NATIVE
      codec_native_pixels(fixture, result.image);
#ifdef VT_HEVC_METAL
      videotoolbox_hevc_metal_test(result.image, fixture->depth, fixture->full, fixture->crc);
#else
      CVPixelBufferRelease(result.image);
#endif
#else
      assert(objects == 1 && result.image->refs == 1);
      CVPixelBufferRelease(result.image);
      assert(!objects);
#endif
   }
   puts("VideoToolbox owner: eight deferred retirements preserve exports, late callbacks and stopped tickets");
#else
   puts("VideoToolbox owner: deferred retirement unavailable without lock-free threads");
#endif
}
