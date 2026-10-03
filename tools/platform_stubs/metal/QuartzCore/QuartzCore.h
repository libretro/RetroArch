/* Stand-in for the Core Animation declarations the Metal driver uses */
#ifndef STUB_QUARTZCORE_H
#define STUB_QUARTZCORE_H
#include <Metal/Metal.h>
@interface CALayer : NSObject
@end
@protocol CAMetalDrawable <MTLDrawable>
- (id<MTLTexture>)texture;
@end
@interface CAEDRMetadata : NSObject
+ (CAEDRMetadata *)HDR10MetadataWithMinLuminance:(float)a maxLuminance:(float)b opticalOutputScale:(float)c;
@end
@interface CAMetalLayer : CALayer
- (id<CAMetalDrawable>)nextDrawable;
@property (nonatomic) MTLPixelFormat pixelFormat;
@property (nonatomic) CGSize drawableSize;
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic) BOOL framebufferOnly;
@property (nonatomic) BOOL displaySyncEnabled;
@property (nonatomic) BOOL wantsExtendedDynamicRangeContent;
@property (nonatomic, strong) CAEDRMetadata *EDRMetadata;
@property (nonatomic) NSUInteger maximumDrawableCount;
@property (nonatomic) CGFloat contentsScale;
@property (nonatomic) CGColorSpaceRef colorspace;
@end
double CACurrentMediaTime(void);
#endif
