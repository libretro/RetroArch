/* SHIPPING_FRAME_VIEW */

static void test_movie_history_storage_mock(void)
{
   MockDevice *device = [MockDevice new];
   @autoreleasepool
   {
   MockContext *context = [MockContext new];
   FrameView *view = [FrameView new];
   struct frame_shader shader = { 2 };
   id<MTLTexture> first[3];
   context->_device = device; view->_context = context;
   view->_shader = &shader; view->init_history = YES;
   view->_movieFrame = YES; view.size = CGSizeMake(7, 5);
   [view _updateHistory];
   for (unsigned i = 0; i < 3; i++)
   {
      first[i] = view->_engine.frame.texture[i].view;
      assert(first[i] && first[i].pixelFormat == MTLPixelFormatRGBA8Unorm);
      for (unsigned j = 0; j < i; j++) assert(first[i] != first[j]);
   }
   [view _updateHistory];
   assert(view->_engine.frame.texture[0].view == first[2]);
   assert(view->_engine.frame.texture[1].view == first[0]);
   assert(view->_engine.frame.texture[2].view == first[1]);
   view->_movieFrame = NO; view->_format = RPixelFormatBGRA8Unorm;
   [view _ensureFront];
   assert(view->_engine.frame.texture[0].view.pixelFormat == MTLPixelFormatBGRA8Unorm);
   assert(view->_engine.frame.texture[1].view == first[0]);
   view->_format = RPixelFormatBGR10A2Unorm; [view _ensureFront];
   assert(view->_engine.frame.texture[0].view.pixelFormat == MTLPixelFormatBGR10A2Unorm);
   view->_movieFrame = YES; view.size = CGSizeMake(9, 6); [view _updateHistory];
   assert(view->_engine.frame.texture[0].view.pixelFormat == MTLPixelFormatRGBA8Unorm);
   assert(view->_engine.frame.texture[0].view.width == 9 && view->_engine.frame.texture[0].view.height == 6);
   view->_movieHDR = YES; view->init_history = YES; [view _updateHistory];
   for (unsigned i = 0; i < 3; i++)
      assert(view->_engine.frame.texture[i].view.pixelFormat == MTLPixelFormatRGBA16Float);
   view->_format = RPixelFormatBGRA8Unorm;
   [view _clearMovieFrame];
   assert(!view->_movieFrame && !view->_movieHDR && view->init_history);
   [view _updateHistory];
   for (unsigned i = 0; i < 3; i++)
      assert(view->_engine.frame.texture[i].view.pixelFormat == MTLPixelFormatBGRA8Unorm);
   view->_movieFrame = YES; view->_movieHDR = NO; view->init_history = YES; [view _updateHistory];
   for (unsigned i = 0; i < 3; i++)
      assert(view->_engine.frame.texture[i].view.pixelFormat == MTLPixelFormatRGBA8Unorm);
   [view release]; [context release];
   }
   assert(!live_textures && [device retainCount] == 1); [device release];
   puts("Metal movie history mock: independent slots, rotation, resize and same-size CPU formats passed");
}

static unsigned planes_released;
static void planes_release(void *owner, uint64_t done, uint64_t value)
{
   assert(owner == &planes_released && done == 5 && value == 6);
   planes_released++;
}

/* Movie frames in memory: the planar conversion stood in for, the
 * shipping importer's checks, front slot, restore and release real */
static void test_movie_planes_mock(void)
{
   static const uint8_t planes[12] = { 0 };
   static const enum video_texture_update results[3] = {
      VIDEO_TEXTURE_UPDATE_REFUSED, VIDEO_TEXTURE_UPDATE_DROPPED,
      VIDEO_TEXTURE_UPDATE_DONE };
   unsigned r;
   MockDevice *device = [MockDevice new];
   for (r = 0; r < 3; r++)
   {
      @autoreleasepool
      {
         MockContext *context = [MockContext new];
         FrameView *view = [FrameView new];
         video_frame_import_t f;
         context->_device = device; view->_context = context;
         view->_pendingSrc = (const void*)1; view->_srcDirty = YES; view->_loanLast = 3;
         view.size = CGSizeMake(3, 3);
         memset(&f, 0, sizeof(f));
         f.planar.planes[0] = planes; f.planar.planes[1] = planes + 8;
         f.planar.planes[2] = planes + 10; f.planar.strides[0] = 4;
         f.planar.strides[1] = f.planar.strides[2] = 2;
         f.planar.chroma_step = 1;
         f.dims = VIDEO_SCALE_PACK(4, 2); f.pixfmt = IMAGE_PIXFMT_I420;
         f.abandon = planes_release; f.owner = &planes_released;
         f.done = 5; f.done_value = 6;
         planes_released = 0;
         /* Refused before anything moves: no kernel, no size, not SDR */
         context->disable_planar = YES;
         assert([view importMovieFrame:&f] == VIDEO_TEXTURE_UPDATE_REFUSED);
         context->disable_planar = NO;
         f.dims = VIDEO_SCALE_PACK(0, 2);
         assert([view importMovieFrame:&f] == VIDEO_TEXTURE_UPDATE_REFUSED);
         f.dims = VIDEO_SCALE_PACK(4, 2); f.output_pixfmt = IMAGE_PIXFMT_FP16;
         assert([view importMovieFrame:&f] == VIDEO_TEXTURE_UPDATE_REFUSED);
         f.output_pixfmt = IMAGE_PIXFMT_8888;
         assert(!view->planes_calls && !view->history_updates && !planes_released);
         /* The conversion's answer */
         view->planes_result = results[r];
         assert([view importMovieFrame:&f] == results[r]);
         assert(view->planes_calls == 1 && view->planes_frame == &f && view->planes_into);
         if (results[r] == VIDEO_TEXTURE_UPDATE_DONE)
         {
            /* Converted into the front slot, RGBA8 at the movie's size */
            assert(view->planes_into == view->_engine.frame.texture[0].view);
            assert(view->planes_into.pixelFormat == MTLPixelFormatRGBA8Unorm);
            assert(view->planes_into.width == 4 && view->planes_into.height == 2);
            assert(view->_movieFrame && !view->_movieHDR && planes_released == 1);
            assert(view.size.width == 4 && view.size.height == 2);
            assert(!view->_srcDirty && !view->_pendingSrc && view->_loanLast == -1);
            assert([view importMovieFrame:&f] == VIDEO_TEXTURE_UPDATE_DONE
                  && planes_released == 2 && view->planes_calls == 2);
            assert([view importMovieFrame:NULL] == VIDEO_TEXTURE_UPDATE_DONE
                  && !view->_movieFrame && planes_released == 2);
         }
         else
         {
            /* Put back as it was, the planes still the producer's to release */
            assert(!view->_movieFrame && !planes_released);
            assert(!view->_engine.frame.texture[0].view);
            assert(view.size.width == 3 && view.size.height == 3);
            assert(view->_srcDirty && view->_pendingSrc == (const void*)1 && view->_loanLast == 3);
         }
         [view release]; [context release];
      }
   }
   assert(!live_textures && [device retainCount] == 1); [device release];
   puts("Metal movie planes mock: kernel, size and format refusals, drop, refusal and release passed");
}

static void test_movie_import_mock(void)
{
   unsigned ten, full, color, action;
   for (ten = 0; ten < 2; ten++) for (full = 0; full < 2; full++)
      for (color = 0; color < 4; color++) for (action = 0; action < 8; action++)
   {
      @autoreleasepool
      {
         bool hdr = color >= 2;
         bool refused = action == 1 || (action >= 3 && action <= 5) || (action == 6 && hdr) || action == 7;
         MockDevice *device = [MockDevice new];
         MockTexture *y = [MockTexture new], *uv = [MockTexture new], *out = [MockTexture new];
         MockSDRPipeline *pipeline = [MockSDRPipeline new];
         MockEvent *ready = [MockEvent new], *done = [MockEvent new];
         MTLSharedEventListener *listener = [MTLSharedEventListener new];
         MockBuffer *buffer = [MockBuffer new];
         MockContext *context = [MockContext new];
         FrameView *view = [FrameView new];
         metal_frame_source_t source;
         video_hw_frame_t frame;
         video_frame_import_t imported;
         context->_device = device; context->_buffer = buffer;
         context->disable_hdr = action == 6; device->fail_alloc = action == 7;
         context->_frameSDRPipeline = action == 4 ? NULL : pipeline;
         view->_context = context; RARCH_STRUCT_ASSIGN(view->_engine.frame.texture[0].view, out);
         view->_pendingSrc = (const void*)1; view->_srcDirty = YES; view->_loanLast = 3;
         assert([view importMovieFrame:NULL] == VIDEO_TEXTURE_UPDATE_DONE);
         assert(view->_srcDirty && view->_pendingSrc == (const void*)1);
         y->_device = [device retain]; y->_width = 10; y->_height = 8;
         uv->_device = [device retain]; uv->_width = 5; uv->_height = 4;
         out->_device = [device retain]; out->_width = 7; out->_height = 5;
         y->_pixelFormat = ten ? MTLPixelFormatR16Unorm : MTLPixelFormatR8Unorm;
         uv->_pixelFormat = ten ? MTLPixelFormatRG16Unorm : MTLPixelFormatRG8Unorm;
         out->_pixelFormat = MTLPixelFormatRGBA8Unorm;
         pipeline->_device = [device retain]; buffer->_device = [device retain];
         memset(&source, 0, sizeof(source));
         source.device = device; source.planes[0] = y; source.planes[1] = uv;
         source.ready = ready; source.done = done; source.listener = listener;
         source.ready_value = 3; source.done_value = 7;
         source.width = 7; source.height = 5; source.crop_x = 2; source.crop_y = 2;
         source.pixfmt = ten ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
         source.color.primaries = source.color.matrix = hdr ? 9 : 1;
         source.color.transfer = color == 3 ? 18 : color == 2 ? 16 : color == 1 ? 13 : 1;
         if (action == 3) source.color.matrix = 10;
         source.color.range = full ? 2 : 1;
         for (unsigned i = 0; i < 3; i++) source.backing[i] = [MockBacking new];
         assert(metal_frame_create(&source, &frame));
         [y release]; [uv release];
         for (unsigned i = 0; i < 3; i++) [(id)source.backing[i] release];
         memset(&imported, 0, sizeof(imported));
         imported.image = frame.image; imported.owner = frame.owner; imported.abandon = frame.abandon;
         imported.wait = frame.wait; imported.wait_value = frame.wait_value;
         imported.done = frame.done; imported.done_value = frame.done_value;
         imported.pixfmt = frame.pixfmt; imported.dims = VIDEO_SCALE_PACK(7, 5);
         imported.crop_x = imported.crop_y = 2;
         imported.output_pixfmt = (hdr != (action == 5)) ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888;
         buffer->fail_encoder = action == 1;
         if (refused)
         {
            assert([view importMovieFrame:&imported] == VIDEO_TEXTURE_UPDATE_REFUSED);
            assert(!view->_movieFrame && view->_srcDirty && view->_pendingSrc == (const void*)1);
            assert(view->_loanLast == 3 && view->history_updates == ((action == 1 || action == 7) ? 1u : 0u));
            assert(!buffer->completion && view->_engine.frame.texture[0].view == out);
            video_hw_frame_abandon(&frame);
         }
         else
         {
            assert([view importMovieFrame:&imported] == VIDEO_TEXTURE_UPDATE_DONE);
            assert(view->_movieFrame && view->_movieHDR == hdr && !view->_srcDirty && !view->_pendingSrc && view->_loanLast == -1);
            assert(view->history_updates == 1 && view->viewport_updates == 1);
            assert(view.size.width == 7 && view.size.height == 5);
            assert(buffer->sequence == 3 && buffer->completion);
            assert([view importMovieFrame:NULL] == VIDEO_TEXTURE_UPDATE_DONE && !view->_movieFrame);
         }
         [out release]; [pipeline release]; [view release]; [context release];
         if (action == 2) [buffer complete:MTLCommandBufferStatusError];
         assert(!done.signaledValue && live_backing == 3);
         [ready setSignaledValue:3];
         if (!refused && action != 2) [buffer complete:MTLCommandBufferStatusCompleted];
         assert(done.signaledValue == 7 && !live_backing && !live_textures && !live_sdr_pipelines);
         [buffer release]; [ready release]; [done release]; [listener release]; [device release];
      }
   }
   puts("Metal movie importer mock: 128 SDR/PQ/HLG conversion, capability, refusal, clear and retirement cases passed");
   test_movie_history_storage_mock();
   test_movie_planes_mock();
}
