@protocol MTLComputePipelineState <NSObject>
@property(readonly) id<MTLDevice> device;
@property(readonly) NSUInteger maxTotalThreadsPerThreadgroup;
@end
@protocol MTLComputeCommandEncoder <NSObject>
- (void)setComputePipelineState:(id<MTLComputePipelineState>)pipeline;
- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)dispatchThreadgroups:(MTLSize)groups threadsPerThreadgroup:(MTLSize)threads;
- (void)endEncoding;
@end
static unsigned live_sdr_pipelines;
@interface MockSDRPipeline : NSObject <MTLComputePipelineState>
{ @public id<MTLDevice> _device; NSUInteger maximum; }
@end
@implementation MockSDRPipeline
- (id)init { if ((self = [super init])) { maximum = 64; live_sdr_pipelines++; } return self; }
- (void)dealloc { live_sdr_pipelines--; [_device release]; [super dealloc]; }
- (id<MTLDevice>)device { return _device; }
- (NSUInteger)maxTotalThreadsPerThreadgroup { return maximum; }
@end
@interface MockSDREncoder : NSObject <MTLComputeCommandEncoder>
{ @public NSMutableArray *resources; unsigned bindings; unsigned char params[64]; }
@end
@implementation MockSDREncoder
- (void)setComputePipelineState:(id<MTLComputePipelineState>)pipeline
{ assert(pipeline); [resources addObject:pipeline]; }
- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index
{ assert(texture && index == bindings++); [resources addObject:texture]; }
- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{ assert(length == sizeof(params) && !index); memcpy(params, bytes, length); }
- (void)dispatchThreadgroups:(MTLSize)groups threadsPerThreadgroup:(MTLSize)threads
{
   uint32_t dims[4], mode;
   memcpy(dims, params, sizeof(dims)); memcpy(&mode, params + 48, sizeof(mode));
   assert(bindings == 3 && dims[0] == 7 && dims[1] == 5 && dims[2] == 2 && dims[3] == 2);
   assert(mode == 1 || mode == 13);
   assert(groups.width == 1 && groups.height == 1 && groups.depth == 1);
   assert(threads.width == 8 && threads.height == 8 && threads.depth == 1);
}
- (void)endEncoding { }
@end
static id mock_compute_encoder(MockBuffer *buffer)
{
   if (buffer->fail_encoder) return nil;
   assert(buffer->sequence++ == 1);
   if (!buffer->resources) buffer->resources = [NSMutableArray new];
   MockSDREncoder *encoder = [[[MockSDREncoder alloc] init] autorelease];
   encoder->resources = buffer->resources;
   return encoder;
}
/* SHIPPING_SDR */
static void test_sdr_mock(void)
{
   unsigned ten, full, srgb, action;
   for (ten = 0; ten < 2; ten++) for (full = 0; full < 2; full++)
      for (srgb = 0; srgb < 2; srgb++) for (action = 0; action < 4; action++)
   {
      @autoreleasepool
      {
         MockDevice *device = [MockDevice new], *other = [MockDevice new];
         MockTexture *y = [MockTexture new], *uv = [MockTexture new], *out = [MockTexture new];
         MockSDRPipeline *pipeline = [MockSDRPipeline new];
         MockEvent *ready = [MockEvent new], *done = [MockEvent new];
         MTLSharedEventListener *listener = [MTLSharedEventListener new];
         MockBuffer *buffer = [MockBuffer new];
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
         source.color.primaries = 1; source.color.matrix = 1;
         source.color.transfer = action == 3 ? 16 : srgb ? 13 : 1;
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
         imported.output_pixfmt = IMAGE_PIXFMT_FP16;
         assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out));
         imported.output_pixfmt = IMAGE_PIXFMT_8888;
         buffer->unretained = YES;
         assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out)); buffer->unretained = NO;
         out->_usage = MTLTextureUsageShaderRead;
         assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out)); out->_usage |= MTLTextureUsageShaderWrite;
         out->_mips = 2;
         assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out)); out->_mips = 1;
         out->_width = 6;
         assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out)); out->_width = 7;
         pipeline->maximum = 32;
         assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out)); pipeline->maximum = 64;
         [pipeline->_device release]; pipeline->_device = [other retain];
         assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out));
         [pipeline->_device release]; pipeline->_device = [device retain];
         assert(!buffer->sequence && !buffer->completion && live_backing == 3);
         buffer->fail_encoder = action == 1;
         if (action == 1 || action == 3)
         {
            assert(!metal_frame_convert_sdr(&imported, device, buffer, pipeline, out));
            assert(buffer->sequence == (action == 1 ? 1u : 0u) && !buffer->completion);
            video_hw_frame_abandon(&frame);
         }
         else
         {
            assert(metal_frame_convert_sdr(&imported, device, buffer, pipeline, out));
            assert(buffer->sequence == 3 && buffer->completion);
         }
         [out release]; [pipeline release];
         if (action == 2) [buffer complete:MTLCommandBufferStatusError];
         assert(!done.signaledValue && live_backing == 3);
         [ready setSignaledValue:3];
         if (!action) [buffer complete:MTLCommandBufferStatusCompleted];
         assert(done.signaledValue == 7 && !live_backing && !live_textures && !live_sdr_pipelines);
         [buffer release]; [ready release]; [done release]; [listener release];
         [device release]; [other release];
      }
   }
   puts("Metal SDR conversion mock: 32 encode/refusal/retirement cases passed");
}
