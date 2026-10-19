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
}
