static void test_sdr_import_mock(void)
{
   unsigned ten, full, color, action;
   for (ten = 0; ten < 2; ten++) for (full = 0; full < 2; full++)
      for (color = 0; color < 4; color++) for (action = 0; action < 6; action++)
   {
      @autoreleasepool
      {
         bool hdr = color >= 2;
         MockDevice *device = [MockDevice new], *other = [MockDevice new];
         MockTexture *y = [MockTexture new], *uv = [MockTexture new], *out = [MockTexture new];
         MockSDRPipeline *pipeline = [MockSDRPipeline new];
         MockEvent *ready = [MockEvent new], *done = [MockEvent new];
         MTLSharedEventListener *listener = [MTLSharedEventListener new];
         MockBuffer *buffer = [MockBuffer new];
         MockContext *context = [MockContext new];
         MetalDriver *driver = [MetalDriver new];
         uintptr_t handle = 0;
         Texture *previous = [Texture new];
         previous.texture = out;
         handle = (uintptr_t)previous;
         context->_device = device; context->_buffer = buffer;
         context->_frameSDRPipeline = pipeline;
         driver->_context = context;
         metal_frame_source_t source;
         video_hw_frame_t frame;
         video_frame_import_t imported;
         memset(&source, 0, sizeof(source));
         y->_device = [device retain]; y->_width = 10; y->_height = 8;
         uv->_device = [device retain]; uv->_width = 5; uv->_height = 4;
         out->_device = [device retain]; out->_width = 7; out->_height = 5;
         y->_pixelFormat = ten ? MTLPixelFormatR16Unorm : MTLPixelFormatR8Unorm;
         uv->_pixelFormat = ten ? MTLPixelFormatRG16Unorm : MTLPixelFormatRG8Unorm;
         out->_pixelFormat = MTLPixelFormatRGBA8Unorm;
         pipeline->_device = [device retain]; buffer->_device = [device retain];
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
         imported.crop_x = 2; imported.crop_y = 2;
         imported.output_pixfmt = hdr ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888;
         out->_width = 6;
         out->_pixelFormat = MTLPixelFormatBGRA8Unorm;
         if (action == 4) context->_frameSDRPipeline = NULL;
         if (action == 5) context->fail_texture = YES;
         buffer->fail_encoder = action == 1;
         if (action == 1 || action >= 3)
         {
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
            assert(handle == (uintptr_t)previous && previous.texture == out);
            assert(context->allocations == (action == 3 || action == 4 ? 0u : 1u));
            assert(buffer->sequence == (action == 1 ? 1u : 0u) && !buffer->completion);
            video_hw_frame_abandon(&frame);
         }
         else
         {
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DONE);
            assert(handle == (uintptr_t)previous && previous.texture != out);
            assert(previous.texture.pixelFormat == (hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm));
            assert(buffer->sequence == 3 && buffer->completion);
         }
         [previous release]; [out release]; [pipeline release];
         if (action == 2) [buffer complete:MTLCommandBufferStatusError];
         assert(!done.signaledValue && live_backing == 3);
         [ready setSignaledValue:3];
         if (!action) [buffer complete:MTLCommandBufferStatusCompleted];
         assert(done.signaledValue == 7 && !live_backing && !live_textures && !live_sdr_pipelines);
         [buffer release]; [ready release]; [done release]; [listener release];
         [context release]; [driver release];
         [device release]; [other release];
      }
   }
   puts("Metal planar importer mock: 96 SDR/PQ/HLG replacement/refusal/retirement cases passed");
}
