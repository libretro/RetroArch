#import <Foundation/Foundation.h>
#include <Block.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef const void *CFTypeRef;
static CFTypeRef CFRetain(CFTypeRef value) { return [(id)value retain]; }
static void CFRelease(CFTypeRef value) { [(id)value release]; }
typedef enum { MTLPixelFormatInvalid, MTLPixelFormatBGRA8Unorm,
   MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA16Float, MTLPixelFormatR8Unorm,
   MTLPixelFormatRG8Unorm, MTLPixelFormatR16Unorm, MTLPixelFormatRG16Unorm } MTLPixelFormat;
enum { MTLTextureType2D = 2, MTLCommandBufferStatusNotEnqueued = 0,
   MTLCommandBufferStatusEnqueued = 1, MTLCommandBufferStatusCompleted = 4,
   MTLCommandBufferStatusError = 5 };
@protocol MTLDevice <NSObject> @end
@protocol MTLTexture <NSObject>
@property(readonly) id<MTLDevice> device;
@property(readonly) NSUInteger textureType, sampleCount, arrayLength, depth, width, height;
@property(readonly) MTLPixelFormat pixelFormat;
@end
@protocol MTLCommandBuffer <NSObject>
@property(readonly) id<MTLDevice> device;
@property(readonly) NSUInteger status;
- (void)addCompletedHandler:(void (^)(id<MTLCommandBuffer>))block;
@end
@interface MTLSharedEventListener : NSObject @end
@implementation MTLSharedEventListener @end
static unsigned live_textures, live_backing;
@interface MockDevice : NSObject <MTLDevice> @end
@implementation MockDevice @end
@interface MockTexture : NSObject <MTLTexture>
{
@public
   id<MTLDevice> _device;
   NSUInteger _width, _height;
   MTLPixelFormat _pixelFormat;
}
@end
@implementation MockTexture
- (id)init { if ((self = [super init])) live_textures++; return self; }
- (void)dealloc { live_textures--; [_device release]; [super dealloc]; }
- (id<MTLDevice>)device { return _device; }
- (NSUInteger)textureType { return MTLTextureType2D; }
- (NSUInteger)sampleCount { return 1; }
- (NSUInteger)arrayLength { return 1; }
- (NSUInteger)depth { return 1; }
- (NSUInteger)width { return _width; }
- (NSUInteger)height { return _height; }
- (MTLPixelFormat)pixelFormat { return _pixelFormat; }
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
@interface MockBuffer : NSObject <MTLCommandBuffer>
{
@public
   id<MTLDevice> _device;
   NSUInteger _status;
   unsigned sequence;
   MockEvent *wait, *signal;
   uint64_t wait_value, signal_value;
   void (^completion)(id<MTLCommandBuffer>);
}
- (void)encodeWaitForEvent:(id)event value:(uint64_t)value;
- (void)encodeSignalEvent:(id)event value:(uint64_t)value;
- (void)complete:(NSUInteger)status;
@end
@implementation MockBuffer
- (void)dealloc { assert(!completion); [_device release]; [super dealloc]; }
- (id<MTLDevice>)device { return _device; }
- (NSUInteger)status { return _status; }
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
}
@end

/* SHIPPING_LEASE */

static void encode(void *user, const metal_frame_view_t *view, void *buffer)
{
   MockBuffer *commands = buffer;
   unsigned *count = user;
   assert(commands->sequence++ == 1);
   assert(view->planes[0] && view->width == 7 && view->height == 5);
   assert(view->yuv == 123 && view->color.max_cll == 1500);
   (*count)++;
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
      y->_pixelFormat = format == IMAGE_PIXFMT_8888 ? MTLPixelFormatBGRA8Unorm
         : format == IMAGE_PIXFMT_FP16 ? MTLPixelFormatRGBA16Float
         : format == IMAGE_PIXFMT_NV12 ? MTLPixelFormatR8Unorm : MTLPixelFormatR16Unorm;
      if (format == IMAGE_PIXFMT_NV12 || format == IMAGE_PIXFMT_P010)
      {
         uv = [MockTexture new]; uv->_device = [device retain];
         uv->_width = 4; uv->_height = 3;
         uv->_pixelFormat = format == IMAGE_PIXFMT_NV12 ? MTLPixelFormatRG8Unorm : MTLPixelFormatRG16Unorm;
      }
      ready->_device = [device retain]; done->_device = [device retain];
      buffer->_device = [device retain];
      source.device = device; source.planes[0] = y; source.planes[1] = uv;
      source.ready = ready; source.done = done; source.listener = listener;
      source.ready_value = 3; source.done_value = 7;
      source.color.max_cll = 1500;
      source.width = 7; source.height = 5; source.yuv = 123; source.pixfmt = format;
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
      if (action == 0)
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
      assert(!live_backing && !live_textures);
      [buffer release]; [ready release]; [done release];
      [device release]; [other release];
   }
}
int main(void)
{
   unsigned i, j;
   const unsigned formats[] = { IMAGE_PIXFMT_8888, IMAGE_PIXFMT_FP16, IMAGE_PIXFMT_NV12, IMAGE_PIXFMT_P010 };
   for (i = 0; i < 4; i++) for (j = 0; j < 3; j++) test(formats[i], j);
   puts("Metal lease mock: 12 format/completion combinations passed");
   return 0;
}
