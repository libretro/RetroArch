#import <Foundation/Foundation.h>
#include <Block.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

typedef const void *CFTypeRef;
static CFTypeRef CFRetain(CFTypeRef value) { return [(id)value retain]; }
static void CFRelease(CFTypeRef value) { [(id)value release]; }
typedef enum { MTLPixelFormatInvalid, MTLPixelFormatBGRA8Unorm,
   MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA16Float, MTLPixelFormatR8Unorm,
   MTLPixelFormatRG8Unorm, MTLPixelFormatR16Unorm, MTLPixelFormatRG16Unorm,
   MTLPixelFormatBGR10A2Unorm } MTLPixelFormat;
enum { MTLTextureUsageShaderRead = 1, MTLTextureUsageShaderWrite = 2, MTLTextureUsageRenderTarget = 4 };
enum { MTLTextureType2D = 2, MTLCommandBufferStatusNotEnqueued = 0,
   MTLCommandBufferStatusEnqueued = 1, MTLCommandBufferStatusCompleted = 4,
   MTLCommandBufferStatusError = 5 };
typedef struct { NSUInteger x, y, z; } MTLOrigin;
typedef struct { NSUInteger width, height, depth; } MTLSize;
static MTLOrigin MTLOriginMake(NSUInteger x, NSUInteger y, NSUInteger z)
{ MTLOrigin result = {x, y, z}; return result; }
static MTLSize MTLSizeMake(NSUInteger x, NSUInteger y, NSUInteger z)
{ MTLSize result = {x, y, z}; return result; }
@protocol MTLTexture;
@class MTLTextureDescriptor;
@protocol MTLDevice <NSObject>
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor*)descriptor;
@end
@protocol MTLBlitCommandEncoder <NSObject>
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice
   sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size
   toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)outSlice
   destinationLevel:(NSUInteger)outLevel destinationOrigin:(MTLOrigin)outOrigin;
- (void)endEncoding;
@end
@protocol MTLTexture <NSObject>
@property(readonly) id<MTLDevice> device;
@property(readonly) NSUInteger textureType, sampleCount, arrayLength, depth, width, height, mipmapLevelCount, usage;
@property(readonly) MTLPixelFormat pixelFormat;
@end
@protocol MTLCommandBuffer <NSObject>
@property(readonly) id<MTLDevice> device;
@property(readonly) NSUInteger status;
@property(readonly) BOOL retainedReferences;
- (id)computeCommandEncoder;
- (id<MTLBlitCommandEncoder>)blitCommandEncoder;
- (void)addCompletedHandler:(void (^)(id<MTLCommandBuffer>))block;
@end
@interface MTLSharedEventListener : NSObject @end
@implementation MTLSharedEventListener @end
static unsigned live_textures, live_backing;
@interface MockDevice : NSObject <MTLDevice>
{ @public BOOL fail_alloc; }
@end
@interface MockTexture : NSObject <MTLTexture>
{
@public
   id<MTLDevice> _device;
   NSUInteger _width, _height, _mips, _usage;
   MTLPixelFormat _pixelFormat;
   unsigned char pixels[1024];
}
@end
@implementation MockTexture
- (id)init { if ((self = [super init])) { live_textures++; _mips = 1; _usage = 3; } return self; }
- (void)dealloc { live_textures--; [_device release]; [super dealloc]; }
- (id<MTLDevice>)device { return _device; }
- (NSUInteger)textureType { return MTLTextureType2D; }
- (NSUInteger)sampleCount { return 1; }
- (NSUInteger)arrayLength { return 1; }
- (NSUInteger)depth { return 1; }
- (NSUInteger)mipmapLevelCount { return _mips; }
- (NSUInteger)usage { return _usage; }
- (NSUInteger)width { return _width; }
- (NSUInteger)height { return _height; }
- (MTLPixelFormat)pixelFormat { return _pixelFormat; }
@end
@interface MTLTextureDescriptor : NSObject
{
   NSUInteger _width, _height, _usage;
   MTLPixelFormat _pixelFormat;
}
@property(nonatomic) NSUInteger width, height, usage;
@property(nonatomic) MTLPixelFormat pixelFormat;
+ (id)texture2DDescriptorWithPixelFormat:(MTLPixelFormat)format
      width:(NSUInteger)width height:(NSUInteger)height mipmapped:(BOOL)mipmapped;
@end
@implementation MTLTextureDescriptor
@synthesize width = _width, height = _height, usage = _usage, pixelFormat = _pixelFormat;
+ (id)texture2DDescriptorWithPixelFormat:(MTLPixelFormat)format
      width:(NSUInteger)w height:(NSUInteger)h mipmapped:(BOOL)mipmapped
{
   MTLTextureDescriptor *descriptor = [[self new] autorelease];
   (void)mipmapped;
   descriptor.width = w; descriptor.height = h; descriptor.pixelFormat = format;
   return descriptor;
}
@end
@implementation MockDevice
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor*)descriptor
{
   if (fail_alloc) return nil;
   MockTexture *texture = [MockTexture new];
   texture->_device = [self retain]; texture->_width = descriptor.width;
   texture->_height = descriptor.height; texture->_usage = descriptor.usage;
   texture->_pixelFormat = descriptor.pixelFormat;
   return texture;
}
@end
@interface MockBacking : NSObject @end
@implementation MockBacking
- (id)init { if ((self = [super init])) live_backing++; return self; }
- (void)dealloc { live_backing--; [super dealloc]; }
@end
@interface MockEvent : NSObject
{
@public
   id<MTLDevice> _device;
   uint64_t _value, _threshold;
   void (^_notify)(id, uint64_t);
}
- (id<MTLDevice>)device;
- (uint64_t)signaledValue;
- (void)setSignaledValue:(uint64_t)value;
- (void)notifyListener:(id)listener atValue:(uint64_t)value
      block:(void (^)(id, uint64_t))block;
@end
@implementation MockEvent
- (void)dealloc { assert(!_notify); [_device release]; [super dealloc]; }
- (id<MTLDevice>)device { return _device; }
- (uint64_t)signaledValue { return _value; }
- (void)setSignaledValue:(uint64_t)value
{
   void (^notify)(id, uint64_t);
   if (value > _value) _value = value;
   if (!_notify || _value < _threshold) return;
   notify = _notify;
   _notify = NULL;
   notify(self, _value);
   Block_release(notify);
}
- (void)notifyListener:(id)listener atValue:(uint64_t)value
      block:(void (^)(id, uint64_t))block
{
   (void)listener;
   assert(!_notify);
   if (_value >= value) { block(self, _value); return; }
   _threshold = value;
   _notify = Block_copy(block);
}
@end
@interface MockBlit : NSObject <MTLBlitCommandEncoder>
{ @public NSMutableArray *resources; }
@end
@implementation MockBlit
- (void)copyFromTexture:(id<MTLTexture>)source sourceSlice:(NSUInteger)slice
   sourceLevel:(NSUInteger)level sourceOrigin:(MTLOrigin)origin sourceSize:(MTLSize)size
   toTexture:(id<MTLTexture>)destination destinationSlice:(NSUInteger)outSlice
   destinationLevel:(NSUInteger)outLevel destinationOrigin:(MTLOrigin)outOrigin
{
   MockTexture *input = (MockTexture *)source, *output = (MockTexture *)destination;
   unsigned bpp = source.pixelFormat == MTLPixelFormatRGBA16Float ? 8 : 4;
   NSUInteger row;
   [resources addObject:source]; [resources addObject:destination];
   assert(!slice && !level && !outSlice && !outLevel && !origin.z
         && !outOrigin.x && !outOrigin.y && !outOrigin.z && size.depth == 1);
   assert(origin.x + size.width <= input.width && origin.y + size.height <= input.height);
   for (row = 0; row < size.height; row++)
      memcpy(output->pixels + row * output.width * bpp,
         input->pixels + ((origin.y + row) * input.width + origin.x) * bpp, size.width * bpp);
}
- (void)endEncoding { }
@end
@interface MockBuffer : NSObject <MTLCommandBuffer>
{
@public
   id<MTLDevice> _device;
   NSUInteger _status;
   unsigned sequence;
   BOOL fail_encoder, unretained;
   NSMutableArray *resources;
   MockEvent *wait, *signal;
   uint64_t wait_value, signal_value;
   void (^completion)(id<MTLCommandBuffer>);
}
- (void)encodeWaitForEvent:(id)event value:(uint64_t)value;
- (void)encodeSignalEvent:(id)event value:(uint64_t)value;
- (void)complete:(NSUInteger)status;
@end
static id mock_compute_encoder(MockBuffer *buffer);
@implementation MockBuffer
- (void)dealloc { assert(!completion); [resources release]; [_device release]; [super dealloc]; }
- (id<MTLDevice>)device { return _device; }
- (NSUInteger)status { return _status; }
- (BOOL)retainedReferences { return !unretained; }
- (id)computeCommandEncoder { return mock_compute_encoder(self); }
- (id<MTLBlitCommandEncoder>)blitCommandEncoder
{
   if (fail_encoder) return nil;
   assert(sequence++ == 1);
   if (!resources) resources = [NSMutableArray new];
   MockBlit *encoder = [[[MockBlit alloc] init] autorelease];
   encoder->resources = resources;
   return encoder;
}
- (void)addCompletedHandler:(void (^)(id<MTLCommandBuffer>))block
{ assert(!completion); completion = Block_copy(block); }
- (void)encodeWaitForEvent:(id)event value:(uint64_t)value
{ assert(sequence++ == 0); wait = event; wait_value = value; }
- (void)encodeSignalEvent:(id)event value:(uint64_t)value
{ assert(sequence++ == 2); signal = event; signal_value = value; }
- (void)complete:(NSUInteger)status
{
   void (^block)(id<MTLCommandBuffer>) = completion;
   assert(block);
   _status = status;
   if (status == MTLCommandBufferStatusCompleted)
   {
      assert(wait.signaledValue >= wait_value);
      [signal setSignaledValue:signal_value];
   }
   completion = NULL;
   block(self);
   Block_release(block);
   [resources removeAllObjects];
}
@end

/* SHIPPING_LEASE */
/* COREVIDEO */
/* SDR_CONVERT */

@interface Texture : NSObject
{
@public
   BOOL _planar;
   unsigned _stagingLent;
   id<MTLTexture> _texture;
   id _sampler;
}
@property(nonatomic, retain) id<MTLTexture> texture;
@property(nonatomic, retain) id sampler;
@end
@implementation Texture
@synthesize texture = _texture;
@synthesize sampler = _sampler;
- (void)dealloc { [_texture release]; [_sampler release]; [super dealloc]; }
@end
@interface MockContext : NSObject
{
@public
   id<MTLDevice> _device;
   MockBuffer *_buffer;
   void *_frameSDRPipeline;
   BOOL fail_texture, disable_hdr;
   unsigned allocations;
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
@implementation MockContext
- (void *)frameSDRPipeline { return _frameSDRPipeline; }
- (BOOL)movieHDRSupported { return !disable_hdr; }
- (BOOL)hdrEnabled { return !disable_hdr; }
- (id)linearShader { return disable_hdr ? nil : self; }
- (id<MTLDevice>)device { return _device; }
- (id<MTLCommandBuffer>)blitCommandBuffer { return _buffer; }
- (Texture *)newImportedTexture:(MTLPixelFormat)format width:(unsigned)width
      height:(unsigned)height filter:(enum texture_filter_type)filter
{
   Texture *result;
   MockTexture *image;
   (void)filter;
   allocations++;
   if (fail_texture) return nil;
   result = [Texture new]; image = [MockTexture new];
   image->_device = [_device retain]; image->_width = width; image->_height = height;
   image->_pixelFormat = format; result->_texture = image;
   return result;
}
@end
@interface MetalDriver : NSObject
{
@public
   MockContext *_context;
}
@property(readonly) MockContext *context;
@end
@implementation MetalDriver
- (MockContext *)context { return _context; }
@end
/* SHIPPING_IMPORT */
/* SDR_IMPORT_TEST */
/* MOVIE_IMPORT_TEST */

static bool refuse_encode(void *user, const metal_frame_view_t *view, void *buffer)
{ (void)user; (void)view; (void)buffer; return false; }

static bool encode(void *user, const metal_frame_view_t *view, void *buffer)
{
   MockBuffer *commands = buffer;
   unsigned *count = user;
   assert(commands->sequence++ == 1);
   assert(view->planes[0] && view->width == 7 && view->height == 5);
   assert(view->yuv == 123 && view->color.max_cll == 1500);
   assert(view->chroma_position == (view->planes[1] ? 2u : 0u));
   (*count)++;
   return true;
}
static void foreign_abandon(void *owner, uint64_t done, uint64_t value)
{ (void)owner; (void)done; (void)value; }

static void test(unsigned format, unsigned action)
{
   @autoreleasepool
   {
      MockDevice *device = [MockDevice new];
      MockDevice *other = [MockDevice new];
      MockTexture *y = [MockTexture new], *uv = nil;
      MockEvent *ready = [MockEvent new], *done = [MockEvent new];
      MTLSharedEventListener *listener = [MTLSharedEventListener new];
      MockBuffer *buffer = [MockBuffer new];
      metal_frame_source_t source;
      video_hw_frame_t frame, untouched;
      video_frame_import_t imported;
      unsigned i, count = 0;
      memset(&source, 0, sizeof(source));
      y->_device = [device retain]; y->_width = 7; y->_height = 5;
      y->_pixelFormat = format == IMAGE_PIXFMT_8888 ? (action == 6 ? MTLPixelFormatRGBA8Unorm : MTLPixelFormatBGRA8Unorm)
         : format == IMAGE_PIXFMT_FP16 ? MTLPixelFormatRGBA16Float
         : format == IMAGE_PIXFMT_NV12 ? MTLPixelFormatR8Unorm : MTLPixelFormatR16Unorm;
      if (format == IMAGE_PIXFMT_NV12 || format == IMAGE_PIXFMT_P010)
      {
         uv = [MockTexture new]; uv->_device = [device retain];
         uv->_width = 4; uv->_height = 3;
         uv->_pixelFormat = format == IMAGE_PIXFMT_NV12 ? MTLPixelFormatRG8Unorm : MTLPixelFormatRG16Unorm;
      }
      /* Native shared events may have no device. Exercise both forms. */
      if (action & 1)
      { ready->_device = [device retain]; done->_device = [device retain]; }
      buffer->_device = [device retain];
      source.device = device; source.planes[0] = y; source.planes[1] = uv;
      source.ready = ready; source.done = done; source.listener = listener;
      source.ready_value = 3; source.done_value = 7;
      source.color.max_cll = 1500;
      source.width = 7; source.height = 5; source.yuv = 123; source.pixfmt = format;
      source.chroma_position = 3;
      assert(!metal_frame_create(&source, &frame));
      source.chroma_position = uv ? 2 : 0;
      for (i = 0; i < 3; i++) source.backing[i] = [MockBacking new];
      memset(&frame, 0xa5, sizeof(frame)); untouched = frame;
      source.device = other;
      assert(!metal_frame_create(&source, &frame));
      assert(!memcmp(&frame, &untouched, sizeof(frame)));
      source.device = device;
      source.crop_x = 1;
      assert(!metal_frame_create(&source, &frame)); source.crop_x = 0;
      source.done_value = 0;
      assert(!metal_frame_create(&source, &frame)); source.done_value = 7;
      source.done = ready; source.done_value = 3;
      assert(!metal_frame_create(&source, &frame)); source.done = done; source.done_value = 7;
      source.ready = other;
      assert(!metal_frame_create(&source, &frame)); source.ready = ready;
      {
         MockEvent *foreign = [MockEvent new];
         foreign->_device = [other retain];
         source.ready = foreign;
         assert(!metal_frame_create(&source, &frame)); source.ready = ready;
         source.done = foreign;
         assert(!metal_frame_create(&source, &frame)); source.done = done;
         [foreign release];
      }
      source.listener = other;
      assert(!metal_frame_create(&source, &frame)); source.listener = listener;
      done->_value = 7;
      assert(!metal_frame_create(&source, &frame)); done->_value = 0;
      if (uv)
      {
         source.planes[1] = NULL;
         assert(!metal_frame_create(&source, &frame)); source.planes[1] = uv;
         uv->_width = 3;
         assert(!metal_frame_create(&source, &frame)); uv->_width = 4;
      }
      if (action >= 4 && action != 10)
      {
         y->_width = 9; y->_height = 7; source.crop_x = 2; source.crop_y = 2;
         source.color.primaries = 1; source.color.transfer = format == IMAGE_PIXFMT_FP16 ? 8 : 13;
         source.color.range = 2;
         if (action == 7) source.color.primaries = 9;
         for (i = 0; i < sizeof(y->pixels); i++) y->pixels[i] = (unsigned char)(i * 17);
      }
      assert(metal_frame_create(&source, &frame));
      assert([ready retainCount] == 2 && [done retainCount] == 2
            && [listener retainCount] == 2);
      source.color.max_cll = 0;
      for (i = 0; i < 3; i++) [(id)source.backing[i] release];
      [y release]; [uv release]; [listener release];
      assert(live_backing == 3 && live_textures == (uv ? 2u : 1u));
      memset(&imported, 0, sizeof(imported));
      imported.image = frame.image; imported.wait = frame.wait; imported.done = frame.done;
      imported.wait_value = frame.wait_value; imported.done_value = frame.done_value;
      imported.crop_x = frame.crop_x; imported.crop_y = frame.crop_y;
      imported.pixfmt = frame.pixfmt; imported.dims = VIDEO_SCALE_PACK(7, 5);
      imported.abandon = frame.abandon; imported.owner = frame.owner;
      imported.abandon = foreign_abandon; imported.owner = (void *)(uintptr_t)1;
      assert(!metal_frame_encode(&imported, device, buffer, encode, &count));
      imported.abandon = frame.abandon; imported.owner = frame.owner;
      assert(!metal_frame_encode(&imported, other, buffer, encode, &count));
      imported.done_value++;
      assert(!metal_frame_encode(&imported, device, buffer, encode, &count)); imported.done_value--;
      buffer->_status = MTLCommandBufferStatusCompleted;
      assert(!metal_frame_encode(&imported, device, buffer, encode, &count)); buffer->_status = 0;
      assert(!count && !buffer->sequence && !buffer->completion);
      if (action == 3 || action == 10)
      {
         assert(!metal_frame_encode(&imported, device, buffer, refuse_encode, NULL));
         assert(buffer->sequence == 1 && !buffer->completion && done.signaledValue == 0);
         if (action == 10)
         {
            MockBuffer *retry = [MockBuffer new]; retry->_device = [device retain];
            assert(metal_frame_encode(&imported, device, retry, encode, &count));
            assert(live_backing == 3 && count == 1);
            [ready setSignaledValue:3]; [retry complete:MTLCommandBufferStatusCompleted];
            [retry release];
         }
         else
         {
            video_hw_frame_abandon(&frame);
            assert(live_backing == 3); [ready setSignaledValue:3];
         }
      }
      else if (action >= 4)
      {
         MockContext *context = [MockContext new];
         MetalDriver *driver = [MetalDriver new];
         uintptr_t handle = 0, original_handle;
         MockTexture *old_image = nil;
         Texture *texture;
         context->_device = device; context->_buffer = buffer; driver->_context = context;
         imported.output_pixfmt = format;
         {
            MockTexture *target = [MockTexture new];
            target->_device = [device retain]; target->_width = 7; target->_height = 5;
            target->_pixelFormat = y->_pixelFormat;
            buffer->unretained = YES;
            assert(!metal_frame_copy(&imported, device, buffer, target));
            buffer->unretained = NO;
            target->_mips = 2;
            assert(!metal_frame_copy(&imported, device, buffer, target)); target->_mips = 1;
            target->_width = 6;
            assert(!metal_frame_copy(&imported, device, buffer, target)); target->_width = 7;
            target->_pixelFormat = format == IMAGE_PIXFMT_FP16 ? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA16Float;
            assert(!metal_frame_copy(&imported, device, buffer, target)); target->_pixelFormat = y->_pixelFormat;
            imported.output_pixfmt = IMAGE_PIXFMT_FP32;
            assert(!metal_frame_copy(&imported, device, buffer, target)); imported.output_pixfmt = format;
            assert(!buffer->sequence && !buffer->completion);
            [target release];
         }
         if (format == IMAGE_PIXFMT_FP16)
         {
            context->disable_hdr = YES;
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR)
                  == VIDEO_TEXTURE_UPDATE_REFUSED && !handle && !buffer->sequence);
            context->disable_hdr = NO;
         }
         context->fail_texture = YES;
         assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR)
               == VIDEO_TEXTURE_UPDATE_REFUSED && !handle && !buffer->sequence);
         context->fail_texture = NO;
         if (action >= 8)
         {
            handle = (uintptr_t)[context newImportedTexture:y->_pixelFormat width:7 height:5 filter:TEXTURE_FILTER_LINEAR];
            memset(((MockTexture *)((Texture *)handle).texture)->pixels, 0xa5, 1024);
            if (action >= 11)
            {
               old_image = [(MockTexture *)((Texture *)handle).texture retain];
               if (action <= 13) old_image->_width = 6;
               else if (action <= 16) old_image->_height = 4;
               else old_image->_pixelFormat = format == IMAGE_PIXFMT_FP16
                  ? MTLPixelFormatRGBA8Unorm : MTLPixelFormatRGBA16Float;
            }
         }
         original_handle = handle;
         if (handle)
         {
            Texture *held = (Texture *)handle;
            held->_stagingLent = 1;
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR)
                  == VIDEO_TEXTURE_UPDATE_REFUSED && handle == original_handle && !buffer->sequence);
            held->_stagingLent = 0; held->_planar = YES;
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR)
                  == VIDEO_TEXTURE_UPDATE_REFUSED && handle == original_handle && !buffer->sequence);
            held->_planar = NO;
         }
         context->fail_texture = action == 12 || action == 15 || action == 18;
         buffer->fail_encoder = action == 5 || action == 9
            || action == 13 || action == 16 || action == 19;
         assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_MIPMAP_LINEAR)
               == VIDEO_TEXTURE_UPDATE_REFUSED && handle == original_handle);
         if (action == 7)
         {
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR)
                  == VIDEO_TEXTURE_UPDATE_REFUSED && !handle && !buffer->sequence);
            video_hw_frame_abandon(&frame); [ready setSignaledValue:3];
         }
         else if (action == 5 || action == 9
               || action == 12 || action == 13 || action == 15
               || action == 16 || action == 18 || action == 19)
         {
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR)
                  == VIDEO_TEXTURE_UPDATE_REFUSED && handle == original_handle && !buffer->completion);
            assert(buffer->sequence == (context->fail_texture ? 0u : 1u) && live_backing == 3);
            if (handle)
            {
               texture = (Texture *)handle;
               for (i = 0; i < 1024; i++) assert(((MockTexture *)texture.texture)->pixels[i] == 0xa5);
               [texture release];
            }
            video_hw_frame_abandon(&frame); [ready setSignaledValue:3];
         }
         else
         {
            assert(metal_import_texture(driver, &handle, &imported, TEXTURE_FILTER_LINEAR)
                  == VIDEO_TEXTURE_UPDATE_DONE && handle);
            assert(!original_handle || handle == original_handle);
            assert(context->allocations == (old_image ? 3u : 2u));
            texture = (Texture *)handle;
            if (old_image)
            {
               assert(texture.texture != old_image);
               assert(context->allocations == 3);
               for (i = 0; i < 1024; i++) assert(old_image->pixels[i] == 0xa5);
            }
            for (i = 0; i < 5; i++)
            {
               unsigned bpp = format == IMAGE_PIXFMT_FP16 ? 8 : 4;
               assert(!memcmp(((MockTexture *)texture.texture)->pixels + i * 7 * bpp,
                     ((MockTexture *)source.planes[0])->pixels + ((i + 2) * 9 + 2) * bpp, 7 * bpp));
            }
            if (old_image) [texture release];
            [ready setSignaledValue:3]; [buffer complete:MTLCommandBufferStatusCompleted];
            if (!old_image) [texture release];
         }
         if (old_image)
         {
            for (i = 0; i < 1024; i++) assert(old_image->pixels[i] == 0xa5);
            [old_image release];
         }
         [context release]; [driver release];
      }
      else if (action == 0)
      {
         video_hw_frame_abandon(&frame);
         assert(done.signaledValue == 0 && live_backing == 3);
         [ready setSignaledValue:3];
      }
      else
      {
         assert(metal_frame_encode(&imported, device, buffer, encode, &count));
         assert(count == 1 && buffer->sequence == 3);
         assert(live_backing == 3 && done.signaledValue == 0);
         if (action == 1)
         {
            [ready setSignaledValue:3];
            [buffer complete:MTLCommandBufferStatusCompleted];
         }
         else
         {
            [buffer complete:MTLCommandBufferStatusError];
            assert(done.signaledValue == 0 && live_backing == 3);
            [ready setSignaledValue:3];
         }
      }
      assert(done.signaledValue == 7);
      if (live_backing || live_textures) fprintf(stderr, "format %u action %u: %u textures, %u backing\n", format, action, live_textures, live_backing);
      assert(!live_backing && !live_textures);
      [buffer release]; [ready release]; [done release];
      [device release]; [other release];
   }
}
int main(void)
{
   test_sdr_mock();
   test_sdr_import_mock();
   test_movie_import_mock();
   test_cv_mock();
   test_cv_color_mock();
   unsigned i, j;
   const unsigned formats[] = { IMAGE_PIXFMT_8888, IMAGE_PIXFMT_FP16, IMAGE_PIXFMT_NV12, IMAGE_PIXFMT_P010 };
   for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) test(formats[i], j);
   test(IMAGE_PIXFMT_8888, 4); test(IMAGE_PIXFMT_8888, 5);
   test(IMAGE_PIXFMT_8888, 6);
   test(IMAGE_PIXFMT_FP16, 4); test(IMAGE_PIXFMT_FP16, 5);
   test(IMAGE_PIXFMT_FP16, 7);
   test(IMAGE_PIXFMT_8888, 8); test(IMAGE_PIXFMT_8888, 9);
   test(IMAGE_PIXFMT_FP16, 8); test(IMAGE_PIXFMT_FP16, 9);
   for (i = 0; i < 4; i++) test(formats[i], 10);
   for (j = 11; j < 20; j++)
   {
      test(IMAGE_PIXFMT_8888, j);
      test(IMAGE_PIXFMT_FP16, j);
   }
   puts("Metal packed import: 48 ownership/copy/refusal/replacement combinations passed");
   return 0;
}
