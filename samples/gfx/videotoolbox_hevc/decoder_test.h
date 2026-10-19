#include "../../../gfx/common/videotoolbox_hevc_decoder.h"

#if defined(VIDEOTOOLBOX_HEVC_SLOTS) && !defined(VT_HEVC_NATIVE)
/* Export scheduling stand-in; real Metal pool ownership has its own fixture. */
struct metal_frame_cv_pool { int busy, fail; };
static unsigned decoder_pools;
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
#endif
}
