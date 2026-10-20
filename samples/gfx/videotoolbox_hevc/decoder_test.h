#include "../../../gfx/common/videotoolbox_hevc_registry.h"
#include "registry_test.h"

#if defined(VIDEOTOOLBOX_HEVC_REGISTRY) && defined(VT_HEVC_METAL)
void *videotoolbox_hevc_test_device(void *);
void *videotoolbox_hevc_test_device_retain(void *);
void videotoolbox_hevc_test_device_release(void *);
void videotoolbox_hevc_test_export(video_hw_frame_t*, unsigned, unsigned, const uint32_t*);

int videotoolbox_hevc_test_start(void*, void*, int);
void videotoolbox_hevc_test_stop(void*);
int videotoolbox_hevc_test_owns(void*);
void videotoolbox_hevc_test_retired(void);
void videotoolbox_hevc_test_movie_reset(void);

static void test_native_decoder_adapter(void)
{
   unsigned which;
   unsigned singles = sizeof(codec_cases) / sizeof(codec_cases[0]);
   unsigned sequences = sizeof(sequence_cases) / sizeof(sequence_cases[0]);
   unsigned reorders = sizeof(reorder_cases) / sizeof(reorder_cases[0]);
   unsigned ordinary = singles + sequences + reorders;
   unsigned hdr_count = sizeof(hdr_codec_cases) / sizeof(hdr_codec_cases[0]) / 2;
   /* Apple's "High Dynamic Range Metadata For Apple Devices", HDR10/HLG
    * sample-entry constraints, require Main10. Main8 color-tagged stress
    * cases remain in software/portable admission checks, not this GPU gate. */
   for (which = 0; which < ordinary + hdr_count; which++)
   {
      const hdr_codec_case_t *hdr = which >= ordinary ? &hdr_codec_cases[which - ordinary + hdr_count] : NULL;
      const sequence_case_t *sequence = (hdr || which < singles) ? NULL
         : which < singles + sequences ? &sequence_cases[which - singles]
         : &reorder_cases[which - singles - sequences];
      const codec_case_t *fixture = hdr ? &hdr->picture : sequence ? &sequence->picture : &codec_cases[which];
      unsigned count = sequence ? VT_HEVC_TEST_BATCH : 1, input = 0, output = 0, i;
      rh265_video *source = rh265_video_open();
      video_hw_frame_t *frames = (video_hw_frame_t*)calloc(count, sizeof(*frames));
      void *device = videotoolbox_hevc_test_device(NULL);
      owner_queue_t queue;
      assert(source && frames && device);
      if (hdr) assert(fixture->depth == 10);
      memset(&queue, 0, sizeof(queue));
      assert(!rh265_video_retain_parameter_sets(source));
      assert(!rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      rh265_set_hw_backend(NULL); assert(!rh265_video_set_hw(source, 1));
      assert(videotoolbox_hevc_test_start(&queue, device, 0));
      if (hdr)
      {
         assert(!rh265_video_set_hw(source, 1) && !rh265_video_is_hw(source));
         assert(videotoolbox_hevc_test_start(&queue, device, 1));
      }
      videotoolbox_hevc_test_device_release(device);
      assert(videotoolbox_hevc_test_owns(&queue));
      assert(rh265_video_set_hw(source, 1) && rh265_video_is_hw(source));
      /* Withdrawal stops new activation; the active codec owns its operations. */
      videotoolbox_hevc_test_stop(&queue);
      assert(!videotoolbox_hevc_test_owns(&queue));
      while (input < count)
      {
         const uint8_t *packet = sequence ? sequence->packets[input].data : fixture->sample;
         size_t size = sequence ? sequence->packets[input].size : fixture->sample_size;
         int status = rh265_video_decode(source, packet, size);
         if (status < 0) fprintf(stderr, "%s registered picture %u: decode %d\n", fixture->name, input, status);
         assert(status >= 0 && status <= 3);
         if (status == 1 || status == 3)
         {
            assert(output < count && !rh265_video_hw_frame(source, &frames[output]));
            output++;
         }
         if (status < 2) input++;
         else sthread_yield();
      }
      /* Only the codec's nonblocking drain; no framework flush or wait. */
      while (output < count)
      {
         int status = rh265_video_drain(source);
         assert(status == 0 || status == 2);
         if (!status)
         {
            assert(!rh265_video_hw_frame(source, &frames[output])); output++;
         }
         else sthread_yield();
      }
      assert(rh265_video_drain(source) == -1);
      rh265_video_close(source);
      videotoolbox_hevc_test_retired();
      for (i = 0; i < count; i++)
      {
         unsigned packet = 0;
         if (sequence)
         {
            for (packet = 0; packet < count; packet++)
               if (sequence->packets[packet].display_index == i) break;
            assert(packet < count);
         }
         videotoolbox_hevc_test_export(&frames[i], fixture->depth, fixture->full,
               sequence ? sequence->packets[packet].crc : fixture->crc);
      }
      videotoolbox_hevc_test_movie_reset();
      free(frames);
   }
   puts("Metal HEVC driver: shipping startup, withdrawal and retirement passed");
   puts("VideoToolbox native adapter: sixteen registered SDR/Main10-HDR IDR/I-P/B streams passed");
}
#endif

#if defined(VIDEOTOOLBOX_HEVC_SLOTS) && !defined(VT_HEVC_NATIVE)
/* Export scheduling stand-in; real Metal pool ownership has its own fixture. */
struct metal_frame_cv_pool { int busy, fail; };
static unsigned decoder_pools;
static unsigned backend_acquired, backend_released, backend_failure;
static bool backend_enabled, backend_hdr;
static bool backend_hdr_enabled(void *user) { (void)user; return backend_hdr; }
static int backend_device;
static void *backend_acquire(void *user)
{ (void)user; backend_acquired++; return backend_failure == 1 ? NULL : &backend_device; }
static void backend_release(void *device)
{ assert(device == &backend_device); backend_released++; }
static bool backend_is_enabled(void *user) { (void)user; return backend_enabled; }
metal_frame_cv_pool_t *metal_frame_cv_pool_create(void *device, unsigned count)
{
   metal_frame_cv_pool_t *pool;
   assert(device == &backend_device && count == 32);
   if (backend_failure == 2) return NULL;
   pool = (metal_frame_cv_pool_t*)calloc(1, sizeof(*pool)); assert(pool);
   decoder_pools++; return pool;
}
void metal_frame_cv_pool_free(metal_frame_cv_pool_t *pool)
{ assert(pool && decoder_pools); decoder_pools--; free(pool); }
static void decoder_frame_abandon(void *owner, uint64_t done, uint64_t value)
{ (void)done; (void)value; CVPixelBufferRelease((CVPixelBufferRef)owner); }
int metal_frame_cv_pool_export(metal_frame_cv_pool_t *pool,
      const metal_frame_view_t *view, void *image, video_hw_frame_t *out)
{
   if (pool->busy) return 1;
   if (pool->fail) return -1;
   memset(out, 0, sizeof(*out));
   out->image = (uint64_t)(uintptr_t)image;
   out->owner = CVPixelBufferRetain((CVPixelBufferRef)image);
   out->abandon = decoder_frame_abandon;
   out->width = view->width; out->height = view->height;
   out->pixfmt = view->pixfmt; out->color = view->color;
   return 0;
}

static void decoder_complete(void)
{
   CVPixelBufferRef image = slot_image();
   assert(pending_session && pending_sample && pending_frame && image);
   pending_session->callback(pending_session->callback_ref, pending_frame,
         0, 0, image, kCMTimeInvalid, kCMTimeInvalid);
   CVPixelBufferRelease(image); CFRelease(pending_sample);
   pending_sample = NULL; pending_session = NULL; pending_frame = NULL;
}

static void test_registered_backend(void)
{
   unsigned which, ordinary = sizeof(codec_cases) / sizeof(codec_cases[0]);
   for (which = 0; which < ordinary + sizeof(hdr_codec_cases) / sizeof(hdr_codec_cases[0]); which++)
   {
      const hdr_codec_case_t *hdr = which >= ordinary ? &hdr_codec_cases[which - ordinary] : NULL;
      const codec_case_t *fixture = hdr ? &hdr->picture : &codec_cases[which];
      rh265_video *source = rh265_video_open();
      videotoolbox_hevc_backend_t backend;
      owner_queue_t queue;
      sthread_t *worker;
      video_hw_frame_t frame;
      int status;
      assert(source && !objects && !decoder_pools);
      memset(&queue, 0, sizeof(queue));
      backend_acquired = backend_released = backend_failure = 0;
      backend_enabled = backend_hdr = false;
      assert(!rh265_video_retain_parameter_sets(source));
      assert(!rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      assert(!rh265_video_get_parameter_sets(source, 0, wanted_sets));
      wanted_height = 64;
      wanted_pixel = fixture->depth == 10 ? (fixture->full ? 0x78663230 : 0x78343230)
         : (fixture->full ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                          : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
      rh265_set_hw_backend(NULL); assert(!rh265_video_set_hw(source, 1));
      videotoolbox_hevc_backend_init(&backend, NULL, backend_acquire, backend_release,
            backend_is_enabled, owner_enqueue, &queue, true);
      rh265_set_hw_backend(&backend.offer);
      assert(!rh265_video_set_hw(source, 1) && !backend_acquired);
      backend_enabled = true;
      backend.hdr_enabled = backend_hdr_enabled;
      if (hdr) assert(!rh265_video_set_hw(source, 1) && !backend_acquired);
      backend_hdr = true; backend_failure = 1;
      assert(!rh265_video_set_hw(source, 1) && backend_acquired == 1 && !backend_released);
      backend_failure = 2;
      assert(!rh265_video_set_hw(source, 1) && backend_acquired == 2 && backend_released == 1);
      backend_failure = 0;
      assert(rh265_video_set_hw(source, 1) && rh265_video_is_hw(source));
      assert(backend_acquired == 3 && backend_released == 2 && decoder_pools == 1);
      rh265_set_hw_backend(NULL);
      decode_mode = 0;
      status = rh265_video_decode(source, fixture->sample, fixture->sample_size);
      assert(status == 0 || status == 2);
      decoder_complete();
      if (status == 2) status = rh265_video_decode(source, fixture->sample, fixture->sample_size);
      if (!status) { assert(!rh265_video_drain(source)); status = 1; }
      assert(status == 1 && !rh265_video_hw_frame(source, &frame));
      assert(frame.image && frame.pixfmt == (fixture->depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12));
      if (hdr) assert(frame.color.primaries == 9 && frame.color.matrix == 9 && frame.color.transfer == hdr->transfer);
      assert(!rh265_video_plane(source, 0, NULL, NULL, NULL));
      rh265_video_close(source);
      assert(queue.submitted == 1 && !decoder_pools);
      worker = sthread_create(owner_worker, &queue); assert(worker); sthread_join(worker);
      assert(objects == 1); video_hw_frame_abandon(&frame); assert(!objects);
      source = rh265_video_open(); assert(source);
      assert(!rh265_video_retain_parameter_sets(source));
      assert(!rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      backend_enabled = false; rh265_set_hw_backend(&backend.offer);
      assert(!rh265_video_set_hw(source, 1) && !rh265_video_is_hw(source));
      status = rh265_video_decode(source, fixture->sample, fixture->sample_size);
      if (!status) { assert(!rh265_video_drain(source)); status = 1; }
      assert(status == 1 && rh265_video_plane(source, 0, NULL, NULL, NULL));
      rh265_set_hw_backend(NULL); rh265_video_close(source);
      assert(!objects && !decoder_pools && backend_acquired == 3);
   }
   backend_enabled = false;
   puts("VideoToolbox registration: twelve SDR/HDR codec activations survive capability refusal and withdrawal");
}
#endif

static void test_decoder_adapter(void)
{
#if defined(VIDEOTOOLBOX_HEVC_SLOTS) && !defined(VT_HEVC_NATIVE)
   unsigned which;
   for (which = 0; which < 8; which++)
   {
      const codec_case_t *fixture = &codec_cases[which / 2];
      rh265_video *source = rh265_video_open();
      rh265_video_params_t *params = (rh265_video_params_t*)malloc(sizeof(*params));
      metal_frame_cv_pool_t *pool = (metal_frame_cv_pool_t*)calloc(1, sizeof(*pool));
      rh265_hw_ops_t ops;
      rh265_hw_picture_t picture;
      rh265_nal_t slices[8];
      rh265_access_unit_t au;
      video_hw_frame_t frame, probe, sentinel;
      owner_queue_t queue;
      sthread_t *worker;
      assert(source && params && pool && !objects && !decoder_pools);
      decoder_pools++;
      memset(&queue, 0, sizeof(queue)); memset(&picture, 0, sizeof(picture));
      assert(!rh265_video_retain_parameter_sets(source));
      assert(!rh265_video_set_extradata(source, fixture->hvcc, fixture->hvcc_size));
      assert(!rh265_video_get_params(source, 0, params));
      assert(!rh265_video_get_parameter_sets(source, 0, wanted_sets));
      wanted_height = 64;
      wanted_pixel = fixture->depth == 10 ? (fixture->full ? 0x78663230 : 0x78343230)
         : (fixture->full ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                          : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
      assert(!videotoolbox_hevc_decoder_open(source, params, 3, pool, NULL, &queue, &ops));
      params->width++;
      assert(!videotoolbox_hevc_decoder_open(source, params, 3, pool, owner_enqueue, &queue, &ops));
      params->width--;
      assert(videotoolbox_hevc_decoder_open(source, params, 3, pool, owner_enqueue, &queue, &ops));
      assert(!rh265_video_get_access_unit(source, fixture->sample, fixture->sample_size, 4, slices, 8, &au));
      memset(&probe, 0xa5, sizeof(probe)); sentinel = probe;
      assert(ops.output(ops.decoder, 0, &probe) == 1);
      assert(!memcmp(&probe, &sentinel, sizeof(probe)));
      decode_mode = 2;
      assert(ops.decode(ops.decoder, params, &au, slices, &picture) == -1);
      decode_mode = which & 1 ? 3 : 0;
      if (which & 1)
      {
         assert(ops.decode(ops.decoder, params, &au, slices, &picture) == -1);
         memset(&frame, 0, sizeof(frame));
      }
      else
      {
         assert(!ops.decode(ops.decoder, params, &au, slices, &picture));
         assert(ops.decode(ops.decoder, params, &au, slices, &picture) == 1);
         assert(ops.output(ops.decoder, 0, &probe) == 1);
         decoder_complete(); pool->busy = 1;
         assert(ops.output(ops.decoder, 0, &probe) == 1);
         assert(!memcmp(&probe, &sentinel, sizeof(probe)));
         pool->busy = 0; pool->fail = 1;
         assert(ops.output(ops.decoder, 0, &probe) == -1);
         assert(!memcmp(&probe, &sentinel, sizeof(probe)));
         pool->fail = 0;
         assert(!ops.output(ops.decoder, 0, &frame));
         assert(frame.image && frame.width == 64 && frame.height == 64);
         assert(frame.color.range == (fixture->full ? 2u : 1u));
         assert(frame.pixfmt == (fixture->depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12));
         assert(ops.output(ops.decoder, 0, &probe) == 1);
         assert(!ops.decode(ops.decoder, params, &au, slices, &picture));
      }
      rh265_video_close(source); free(params);
      videotoolbox_hevc_decoder_close(&ops);
      assert(!ops.decoder && !ops.decode && !ops.output && queue.submitted == 1 && !decoder_pools);
      videotoolbox_hevc_decoder_close(&ops); assert(queue.submitted == 1);
      worker = sthread_create(owner_worker, &queue); assert(worker); sthread_join(worker);
      assert(!queue.work && !pending_sample);
      if (!(which & 1))
      {
         assert(objects == 1 && ((CVPixelBufferRef)frame.owner)->refs == 1);
         video_hw_frame_abandon(&frame);
      }
      assert(!objects);
   }
   decode_mode = 0;
   puts("VideoToolbox adapter: eight retry, export pressure, drop and deferred close cases passed");
   test_registered_backend();
#endif
#if defined(VIDEOTOOLBOX_HEVC_REGISTRY) && defined(VT_HEVC_METAL)
   test_native_decoder_adapter();
#endif
   test_backend_withdrawal();
   test_registry_lifetime();
}
