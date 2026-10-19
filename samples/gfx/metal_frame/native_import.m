/* Included by the native lease fixture. */
@interface Texture : NSObject
{
@public
   BOOL _planar;
   unsigned _stagingLent;
}
@property(nonatomic, strong) id<MTLTexture> texture;
@property(nonatomic, strong) id<MTLSamplerState> sampler;
@end
@implementation Texture
@end

@interface NativeContext : NSObject
{
@public
   void *_frameSDRPipeline;
   id<MTLDevice> _device;
   id<MTLSamplerState> _samplers[4];
   id<MTLCommandBuffer> _buffer;
}
@property(readonly) id<MTLDevice> device;
@property(readonly) id<MTLCommandBuffer> blitCommandBuffer;
@property(readonly) BOOL hdrEnabled;
@property(readonly) void *frameSDRPipeline;
@property(readonly) BOOL movieHDRSupported;
- (id)linearShader;
- (Texture *)newImportedTexture:(MTLPixelFormat)format width:(unsigned)width
      height:(unsigned)height filter:(enum texture_filter_type)filter;
@end
@implementation NativeContext
- (void *)frameSDRPipeline { return _frameSDRPipeline; }
- (BOOL)movieHDRSupported { return YES; }
- (id<MTLDevice>)device { return _device; }
- (id<MTLCommandBuffer>)blitCommandBuffer { return _buffer; }
- (BOOL)hdrEnabled { return YES; }
- (id)linearShader { return self; }
/* SHIPPING_IMPORT_ALLOCATION */
@end

@interface MetalDriver : NSObject
@property(nonatomic, strong) NativeContext *context;
@end
@implementation MetalDriver
@end
/* SHIPPING_IMPORT */

static void test_import(id<MTLDevice> device, MTLPixelFormat next_format,
      unsigned next_width, unsigned next_height)
{
   @autoreleasepool
   {
      NativeContext *context = [NativeContext new];
      MetalDriver *driver = [MetalDriver new];
      id<MTLCommandQueue> producer = [device newCommandQueue];
      id<MTLCommandQueue> consumer = [device newCommandQueue];
      id<MTLSharedEvent> ready = [device newSharedEvent];
      id<MTLSharedEvent> done[2];
      MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
         initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
      id<MTLTexture> readback[2];
      unsigned char pixels[2][504], actual[504];
      unsigned widths[2] = { 7, next_width }, heights[2] = { 5, next_height };
      MTLPixelFormat formats[2] = { MTLPixelFormatBGRA8Unorm, next_format };
      uintptr_t handle = 0, original = 0;
      unsigned i, j;
      __weak id<MTLTexture> previous = nil;
      assert(producer && consumer && ready && listener);
      context->_device = device;
      context->_buffer = [consumer commandBuffer];
      assert(context->_buffer && context->_buffer.retainedReferences);
      driver.context = context;
      for (i = 0; i < 2; i++)
      {
         MTLSamplerDescriptor *sampler = [MTLSamplerDescriptor new];
         sampler.minFilter = sampler.magFilter = i == TEXTURE_FILTER_LINEAR
            ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
         context->_samplers[i] = [device newSamplerStateWithDescriptor:sampler];
         assert(context->_samplers[i]);
      }
      for (i = 0; i < 2; i++)
      {
         unsigned bpp = formats[i] == MTLPixelFormatRGBA16Float ? 8 : 4;
         metal_frame_source_t source;
         video_hw_frame_t frame;
         video_frame_import_t imported;
         MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:formats[i]
            width:9 height:7 mipmapped:NO];
         desc.storageMode = MTLStorageModeShared;
         id<MTLTexture> input = [device newTextureWithDescriptor:desc];
         assert(input);
         for (j = 0; j < sizeof(pixels[i]); j++)
            pixels[i][j] = (unsigned char)(j * 17 + i * 31);
         if (bpp == 8)
            for (j = 0; j < sizeof(pixels[i]); j += 2)
            { pixels[i][j] = 0; pixels[i][j + 1] = (unsigned char)(0x3c + ((j / 8) % 8) * 2); }
         [input replaceRegion:MTLRegionMake2D(0, 0, 9, 7) mipmapLevel:0
            withBytes:pixels[i] bytesPerRow:9 * bpp];
         memset(&source, 0, sizeof(source));
         done[i] = [device newSharedEvent];
         assert(done[i]);
         source.device = (__bridge void *)device;
         source.planes[0] = (__bridge void *)input;
         source.ready = (__bridge void *)ready;
         source.done = (__bridge void *)done[i];
         source.listener = (__bridge void *)listener;
         source.ready_value = 3; source.done_value = 7;
         source.width = widths[i]; source.height = heights[i];
         source.crop_x = 2; source.crop_y = 2;
         source.pixfmt = bpp == 8 ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888;
         source.color.primaries = 1; source.color.transfer = bpp == 8 ? 8 : 13;
         source.color.range = 2;
         assert(metal_frame_create(&source, &frame));
         memset(&imported, 0, sizeof(imported));
         imported.image = frame.image; imported.wait = frame.wait; imported.done = frame.done;
         imported.wait_value = frame.wait_value; imported.done_value = frame.done_value;
         imported.owner = frame.owner; imported.abandon = frame.abandon;
         imported.crop_x = frame.crop_x; imported.crop_y = frame.crop_y;
         imported.pixfmt = frame.pixfmt; imported.output_pixfmt = frame.pixfmt;
         imported.dims = VIDEO_SCALE_PACK(widths[i], heights[i]);
         assert(metal_import_texture((__bridge void *)driver, &handle, &imported,
                  i ? TEXTURE_FILTER_NEAREST : TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DONE);
         if (!i) original = handle;
         assert(handle == original);
         Texture *texture = (__bridge Texture *)(void *)handle;
         assert(texture.texture.width == widths[i] && texture.texture.height == heights[i]);
         assert(texture.texture.pixelFormat == formats[i]);
         assert(texture.texture.storageMode == MTLStorageModePrivate);
         if (i)
         {
            bool replaced = widths[i] != widths[0] || heights[i] != heights[0] || formats[i] != formats[0];
            assert(previous && (texture.texture != previous) == replaced);
            assert(texture.sampler == context->_samplers[replaced ? TEXTURE_FILTER_NEAREST : TEXTURE_FILTER_LINEAR]);
         }
         previous = texture.texture;
         desc.width = widths[i]; desc.height = heights[i];
         readback[i] = [device newTextureWithDescriptor:desc];
         assert(readback[i]);
         id<MTLBlitCommandEncoder> blit = [context->_buffer blitCommandEncoder];
         assert(blit);
         [blit copyFromTexture:texture.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
            sourceSize:MTLSizeMake(widths[i], heights[i], 1) toTexture:readback[i]
            destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
         [blit endEncoding];
      }
      /* Drop the public handle before GPU execution; encoded readers own both images. */
      CFRelease((CFTypeRef)(void *)handle);
      /* Test-only bounded completion and readback. */
      dispatch_semaphore_t finished = dispatch_semaphore_create(0);
      [context->_buffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
         (void)buffer; dispatch_semaphore_signal(finished);
      }];
      [context->_buffer commit];
      assert(previous && !done[0].signaledValue && !done[1].signaledValue);
      id<MTLCommandBuffer> write = [producer commandBuffer];
      [write encodeSignalEvent:ready value:3]; [write commit];
      assert(!dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
      assert(context->_buffer.status == MTLCommandBufferStatusCompleted);
      assert(done[0].signaledValue == 7 && done[1].signaledValue == 7);
      for (i = 0; i < 2; i++)
      {
         unsigned bpp = formats[i] == MTLPixelFormatRGBA16Float ? 8 : 4;
         [readback[i] getBytes:actual bytesPerRow:widths[i] * bpp
            fromRegion:MTLRegionMake2D(0, 0, widths[i], heights[i]) mipmapLevel:0];
         for (j = 0; j < heights[i]; j++)
            assert(!memcmp(actual + j * widths[i] * bpp,
                     pixels[i] + ((j + 2) * 9 + 2) * bpp, widths[i] * bpp));
      }
   }
}
