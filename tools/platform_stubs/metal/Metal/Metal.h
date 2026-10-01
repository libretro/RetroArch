/* Stand-in for the Metal declarations gfx/drivers/metal.m uses, in the
 * shapes the SDK gives them, so the compile matrix can check the
 * driver's syntax on a host without the macOS SDK. Nothing links. */
#ifndef STUB_METAL_H
#define STUB_METAL_H
#include <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, MTLPixelFormat) {
   MTLPixelFormatInvalid,
   MTLPixelFormatBC1_RGBA,
   MTLPixelFormatBC2_RGBA,
   MTLPixelFormatBC3_RGBA,
   MTLPixelFormatBC7_RGBAUnorm,
   MTLPixelFormatBGR10A2Unorm,
   MTLPixelFormatBGRA8Unorm,
   MTLPixelFormatBGRA8Unorm_sRGB,
   MTLPixelFormatR16Uint,
   MTLPixelFormatR16Unorm,
   MTLPixelFormatR8Unorm,
   MTLPixelFormatRGB10A2Unorm,
   MTLPixelFormatRGBA16Float,
   MTLPixelFormatRGBA8Unorm,
   MTLPixelFormatRGBA8Unorm_sRGB,
   MTLPixelFormatRGBA8Uint,
   MTLPixelFormatRGBA8Sint,
   MTLPixelFormatRGBA32Uint,
   MTLPixelFormatRGBA32Sint,
   MTLPixelFormatRGBA32Float,
   MTLPixelFormatRGBA16Uint,
   MTLPixelFormatRGBA16Sint,
   MTLPixelFormatRGB10A2Uint,
   MTLPixelFormatRG8Unorm,
   MTLPixelFormatRG8Uint,
   MTLPixelFormatRG8Sint,
   MTLPixelFormatRG32Uint,
   MTLPixelFormatRG32Sint,
   MTLPixelFormatRG32Float,
   MTLPixelFormatRG16Uint,
   MTLPixelFormatRG16Sint,
   MTLPixelFormatRG16Float,
   MTLPixelFormatR8Uint,
   MTLPixelFormatR8Sint,
   MTLPixelFormatR32Uint,
   MTLPixelFormatR32Sint,
   MTLPixelFormatR32Float,
   MTLPixelFormatR16Sint,
   MTLPixelFormatR16Float
};
typedef NS_ENUM(NSUInteger, MTLBlendFactor) {
   MTLBlendFactorOne,
   MTLBlendFactorOneMinusSourceAlpha,
   MTLBlendFactorSourceAlpha,
   MTLBlendFactorZero
};
typedef NS_ENUM(NSUInteger, MTLLoadAction) {
   MTLLoadActionClear,
   MTLLoadActionDontCare,
   MTLLoadActionLoad
};
typedef NS_ENUM(NSUInteger, MTLStoreAction) {
   MTLStoreActionStore,
   MTLStoreActionDontCare
};
typedef NS_ENUM(NSUInteger, MTLPrimitiveType) {
   MTLPrimitiveTypeTriangle,
   MTLPrimitiveTypeTriangleStrip,
   MTLPrimitiveTypePoint,
   MTLPrimitiveTypeLine,
   MTLPrimitiveTypeLineStrip
};
typedef NS_ENUM(NSUInteger, MTLSamplerAddressMode) {
   MTLSamplerAddressModeClampToBorderColor,
   MTLSamplerAddressModeClampToEdge,
   MTLSamplerAddressModeClampToZero,
   MTLSamplerAddressModeMirrorRepeat,
   MTLSamplerAddressModeRepeat
};
typedef NS_ENUM(NSUInteger, MTLSamplerMinMagFilter) {
   MTLSamplerMinMagFilterLinear,
   MTLSamplerMinMagFilterNearest
};
typedef NS_ENUM(NSUInteger, MTLSamplerMipFilter) {
   MTLSamplerMipFilterLinear,
   MTLSamplerMipFilterNearest,
   MTLSamplerMipFilterNotMipmapped
};
typedef NS_ENUM(NSUInteger, MTLStorageMode) {
   MTLStorageModeManaged,
   MTLStorageModePrivate,
   MTLStorageModeShared
};
typedef NS_ENUM(NSUInteger, MTLVertexFormat) {
   MTLVertexFormatFloat2,
   MTLVertexFormatFloat3,
   MTLVertexFormatFloat4,
   MTLVertexFormatUShort4Normalized
};
typedef NS_ENUM(NSUInteger, MTLVertexStepFunction) {
   MTLVertexStepFunctionPerVertex
};
typedef NS_ENUM(NSUInteger, MTLDataType) {
   MTLDataTypeFloat
};
typedef NS_ENUM(NSUInteger, MTLCPUCacheMode) {
   MTLCPUCacheModeDefaultCache
};
typedef NS_ENUM(NSUInteger, MTLTextureType) {
   MTLTextureTypeType2D
};
typedef NS_ENUM(NSUInteger, MTLCullMode) {
   MTLCullModeNone
};
typedef NS_ENUM(NSUInteger, MTLWinding) {
   MTLWindingClockwise
};
typedef NS_OPTIONS(NSUInteger, MTLTextureUsage) {
   MTLTextureUsageRenderTarget = 1,
   MTLTextureUsageShaderRead = 2,
   MTLTextureUsageShaderWrite = 4
};
typedef NS_OPTIONS(NSUInteger, MTLResourceOptions) {
   MTLResourceStorageModeManaged = 1,
   MTLResourceStorageModeShared = 2,
   MTLResourceCPUCacheModeDefaultCache = 4
};

typedef struct { double originX, originY, width, height, znear, zfar; } MTLViewport;
typedef struct { double red, green, blue, alpha; } MTLClearColor;
typedef struct { NSUInteger x, y, z; } MTLOrigin;
typedef struct { NSUInteger width, height, depth; } MTLSize;
typedef struct { MTLOrigin origin; MTLSize size; } MTLRegion;
typedef struct { NSUInteger x, y, width, height; } MTLScissorRect;
static inline MTLClearColor MTLClearColorMake(double r, double g, double b, double a) { MTLClearColor c; c.red = r; c.green = g; c.blue = b; c.alpha = a; return c; }
static inline MTLOrigin MTLOriginMake(NSUInteger x, NSUInteger y, NSUInteger z) { MTLOrigin o; o.x = x; o.y = y; o.z = z; return o; }
static inline MTLSize MTLSizeMake(NSUInteger w, NSUInteger h, NSUInteger d) { MTLSize s; s.width = w; s.height = h; s.depth = d; return s; }
static inline MTLRegion MTLRegionMake2D(NSUInteger x, NSUInteger y, NSUInteger w, NSUInteger h) { MTLRegion r; r.origin.x = x; r.origin.y = y; r.origin.z = 0; r.size.width = w; r.size.height = h; r.size.depth = 1; return r; }

@protocol MTLDevice, MTLTexture, MTLBuffer, MTLCommandQueue, MTLCommandBuffer, MTLFunction, MTLLibrary;
@protocol MTLResource <NSObject>
@property (copy) NSString *label;
@property (readonly) MTLStorageMode storageMode;
@property (readonly) id<MTLDevice> device;
@end
@protocol MTLTexture <MTLResource>
@property (readonly) NSUInteger width;
@property (readonly) NSUInteger height;
@property (readonly) MTLPixelFormat pixelFormat;
@property (readonly) NSUInteger mipmapLevelCount;
- (void)replaceRegion:(MTLRegion)r mipmapLevel:(NSUInteger)m withBytes:(const void *)b bytesPerRow:(NSUInteger)p;
- (void)getBytes:(void *)b bytesPerRow:(NSUInteger)p fromRegion:(MTLRegion)r mipmapLevel:(NSUInteger)m;
- (id<MTLBuffer>)buffer;
@end
@protocol MTLBuffer <MTLResource>
- (void *)contents;
- (NSUInteger)length;
- (void)didModifyRange:(NSRange)r;
- (id<MTLTexture>)newTextureWithDescriptor:(id)d offset:(NSUInteger)o bytesPerRow:(NSUInteger)b;
@end
@protocol MTLFunction <NSObject>
@end
@protocol MTLLibrary <NSObject>
- (id<MTLFunction>)newFunctionWithName:(NSString *)n;
- (id<MTLFunction>)newFunctionWithName:(NSString *)n constantValues:(id)v error:(NSError **)e;
@end
@protocol MTLRenderPipelineState <NSObject>
@property (readonly) NSString *label;
@end
@protocol MTLComputePipelineState <NSObject>
@end
@protocol MTLSamplerState <NSObject>
@end
@protocol MTLDrawable <NSObject>
- (void)present;
@end
@protocol MTLCommandEncoder <NSObject>
- (void)endEncoding;
@property (copy) NSString *label;
- (void)pushDebugGroup:(NSString *)s;
- (void)popDebugGroup;
@end
@protocol MTLRenderCommandEncoder <MTLCommandEncoder>
- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)s;
- (void)setVertexBytes:(const void *)b length:(NSUInteger)l atIndex:(NSUInteger)i;
- (void)setFragmentBytes:(const void *)b length:(NSUInteger)l atIndex:(NSUInteger)i;
- (void)setVertexBuffer:(id<MTLBuffer>)b offset:(NSUInteger)o atIndex:(NSUInteger)i;
- (void)setFragmentBuffer:(id<MTLBuffer>)b offset:(NSUInteger)o atIndex:(NSUInteger)i;
- (void)setFragmentTexture:(id<MTLTexture>)t atIndex:(NSUInteger)i;
- (void)setFragmentSamplerState:(id<MTLSamplerState>)s atIndex:(NSUInteger)i;
- (void)setFragmentSamplerStates:(const id<MTLSamplerState> *)s withRange:(NSRange)r;
- (void)setFragmentTextures:(const id<MTLTexture> *)t withRange:(NSRange)r;
- (void)setViewport:(MTLViewport)v;
- (void)setScissorRect:(MTLScissorRect)r;
- (void)drawPrimitives:(MTLPrimitiveType)t vertexStart:(NSUInteger)s vertexCount:(NSUInteger)c;
@end
@protocol MTLBlitCommandEncoder <MTLCommandEncoder>
- (void)synchronizeResource:(id<MTLResource>)r;
- (void)copyFromBuffer:(id<MTLBuffer>)b sourceOffset:(NSUInteger)o sourceBytesPerRow:(NSUInteger)r sourceBytesPerImage:(NSUInteger)i sourceSize:(MTLSize)sz toTexture:(id<MTLTexture>)t destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl destinationOrigin:(MTLOrigin)d;
- (void)copyFromTexture:(id<MTLTexture>)t sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl sourceOrigin:(MTLOrigin)so sourceSize:(MTLSize)sz toTexture:(id<MTLTexture>)dt destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl destinationOrigin:(MTLOrigin)d;
- (void)generateMipmapsForTexture:(id<MTLTexture>)t;
- (void)copyFromTexture:(id<MTLTexture>)t sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl sourceOrigin:(MTLOrigin)so sourceSize:(MTLSize)sz toBuffer:(id<MTLBuffer>)b destinationOffset:(NSUInteger)o destinationBytesPerRow:(NSUInteger)r destinationBytesPerImage:(NSUInteger)i;
@end
@protocol MTLComputeCommandEncoder <MTLCommandEncoder>
- (void)setComputePipelineState:(id<MTLComputePipelineState>)s;
- (void)setBuffer:(id<MTLBuffer>)b offset:(NSUInteger)o atIndex:(NSUInteger)i;
- (void)setTexture:(id<MTLTexture>)t atIndex:(NSUInteger)i;
- (void)dispatchThreadgroups:(MTLSize)g threadsPerThreadgroup:(MTLSize)t;
@end
@protocol MTLCommandBuffer <NSObject>
- (id<MTLRenderCommandEncoder>)renderCommandEncoderWithDescriptor:(id)d;
- (id<MTLBlitCommandEncoder>)blitCommandEncoder;
- (id<MTLComputeCommandEncoder>)computeCommandEncoder;
- (void)presentDrawable:(id<MTLDrawable>)d;
- (void)commit;
- (void)waitUntilCompleted;
- (void)addCompletedHandler:(void (^)(id<MTLCommandBuffer>))h;
- (void)addScheduledHandler:(void (^)(id<MTLCommandBuffer>))h;
@property (copy) NSString *label;
- (void)enqueue;
@end
@protocol MTLCommandQueue <NSObject>
- (id<MTLCommandBuffer>)commandBuffer;
- (id<MTLCommandBuffer>)commandBufferWithUnretainedReferences;
@end
@protocol MTLDevice <NSObject>
- (NSString *)name;
- (id<MTLCommandQueue>)newCommandQueue;
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)l options:(MTLResourceOptions)o;
- (id<MTLBuffer>)newBufferWithBytes:(const void *)b length:(NSUInteger)l options:(MTLResourceOptions)o;
- (id<MTLTexture>)newTextureWithDescriptor:(id)d;
- (id<MTLSamplerState>)newSamplerStateWithDescriptor:(id)d;
- (id<MTLLibrary>)newDefaultLibrary;
- (id<MTLLibrary>)newLibraryWithSource:(NSString *)s options:(id)o error:(NSError **)e;
- (id<MTLLibrary>)newLibraryWithData:(id)d error:(NSError **)e;
- (id<MTLLibrary>)newLibraryWithURL:(NSURL *)u error:(NSError **)e;
- (id<MTLComputePipelineState>)newComputePipelineStateWithFunction:(id<MTLFunction>)f error:(NSError **)e;
- (id<MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(id)d error:(NSError **)e;
- (BOOL)supportsFamily:(NSInteger)f;
@property (readonly) BOOL supportsBCTextureCompression;
- (NSUInteger)minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)f;
@end
@interface MTLTextureDescriptor : NSObject
+ (MTLTextureDescriptor *)texture2DDescriptorWithPixelFormat:(MTLPixelFormat)f width:(NSUInteger)w height:(NSUInteger)h mipmapped:(BOOL)m;
@property (nonatomic) MTLTextureUsage usage;
@property (nonatomic) MTLStorageMode storageMode;
@property (nonatomic) NSUInteger mipmapLevelCount;
@property (nonatomic) MTLPixelFormat pixelFormat;
@property (nonatomic) NSUInteger width;
@property (nonatomic) NSUInteger height;
@end
@interface MTLSamplerDescriptor : NSObject
@property (copy) NSString *label;
@property (nonatomic) MTLSamplerMinMagFilter minFilter;
@property (nonatomic) MTLSamplerMinMagFilter magFilter;
@property (nonatomic) MTLSamplerMipFilter mipFilter;
@property (nonatomic) MTLSamplerAddressMode sAddressMode;
@property (nonatomic) MTLSamplerAddressMode tAddressMode;
@property (nonatomic) MTLSamplerAddressMode rAddressMode;
@end
@interface MTLRenderPipelineColorAttachmentDescriptor : NSObject
@property (nonatomic) MTLPixelFormat pixelFormat;
@property (nonatomic, getter=isBlendingEnabled) BOOL blendingEnabled;
@property (nonatomic) MTLBlendFactor sourceRGBBlendFactor;
@property (nonatomic) MTLBlendFactor destinationRGBBlendFactor;
@property (nonatomic) MTLBlendFactor sourceAlphaBlendFactor;
@property (nonatomic) MTLBlendFactor destinationAlphaBlendFactor;
@end
@interface MTLRenderPipelineColorAttachmentDescriptorArray : NSObject
- (MTLRenderPipelineColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
@end
@interface MTLVertexAttributeDescriptor : NSObject
@property (nonatomic) MTLVertexFormat format;
@property (nonatomic) NSUInteger offset;
@property (nonatomic) NSUInteger bufferIndex;
@end
@interface MTLVertexAttributeDescriptorArray : NSObject
- (MTLVertexAttributeDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
@end
@interface MTLVertexBufferLayoutDescriptor : NSObject
@property (nonatomic) NSUInteger stride;
@property (nonatomic) MTLVertexStepFunction stepFunction;
@end
@interface MTLVertexBufferLayoutDescriptorArray : NSObject
- (MTLVertexBufferLayoutDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
@end
@interface MTLVertexDescriptor : NSObject
+ (MTLVertexDescriptor *)vertexDescriptor;
@property (readonly) MTLVertexAttributeDescriptorArray *attributes;
@property (readonly) MTLVertexBufferLayoutDescriptorArray *layouts;
@end
@interface MTLRenderPipelineDescriptor : NSObject
@property (nonatomic, strong) NSString *label;
@property (nonatomic, strong) id<MTLFunction> vertexFunction;
@property (nonatomic, strong) id<MTLFunction> fragmentFunction;
@property (nonatomic, copy) MTLVertexDescriptor *vertexDescriptor;
@property (readonly) MTLRenderPipelineColorAttachmentDescriptorArray *colorAttachments;
@property (nonatomic) NSUInteger sampleCount;
@end
@interface MTLRenderPassColorAttachmentDescriptor : NSObject
@property (nonatomic, strong) id<MTLTexture> texture;
@property (nonatomic) MTLLoadAction loadAction;
@property (nonatomic) MTLStoreAction storeAction;
@property (nonatomic) MTLClearColor clearColor;
@end
@interface MTLRenderPassColorAttachmentDescriptorArray : NSObject
- (MTLRenderPassColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
@end
@interface MTLRenderPassDescriptor : NSObject
+ (MTLRenderPassDescriptor *)renderPassDescriptor;
@property (readonly) MTLRenderPassColorAttachmentDescriptorArray *colorAttachments;
@end
@interface MTLFunctionConstantValues : NSObject
- (void)setConstantValue:(const void *)v type:(MTLDataType)t atIndex:(NSUInteger)i;
- (void)setConstantValue:(const void *)v type:(MTLDataType)t withName:(NSString *)n;
@end
@interface MTLCompileOptions : NSObject
@end
id<MTLDevice> MTLCreateSystemDefaultDevice(void);
NSArray *MTLCopyAllDevices(void);
#endif
