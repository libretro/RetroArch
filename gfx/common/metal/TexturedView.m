//
// Created by Stuart Carnie on 6/16/18.
//

#import "TexturedView.h"
#import "RendererCommon.h"
#import "View.h"
#import "Filter.h"

@implementation TexturedView
{
   Context *_context;
   id<MTLTexture> _texture; // optimal render texture
   Vertex _v[4];
   CGSize _size; // size of view in pixels
   CGRect _frame;
   NSUInteger _bpp;

   id<MTLTexture> _src;    // source texture
   bool _srcDirty;
}

- (instancetype)initWithDescriptor:(ViewDescriptor *)d context:(Context *)c
{
   self = [super init];
   if (self)
   {
      _format = d.format;
      _bpp = RPixelFormatToBPP(_format);
      _filter = d.filter;
      _context = c;
      _visible = YES;
      if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
      {
         _drawState = ViewDrawStateEncoder;
      }
      else
      {
         _drawState = ViewDrawStateAll;
      }
      self.size = d.size;
      self.frame = CGRectMake(0, 0, 1, 1);
   }
   return self;
}

- (void)setSize:(CGSize)size
{
   if (CGSizeEqualToSize(_size, size))
   {
      return;
   }

   _size = size;

   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                    width:(NSUInteger)size.width
                                                                                   height:(NSUInteger)size.height
                                                                                mipmapped:NO];
      td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      _texture = [_context.device newTextureWithDescriptor:td];
   }

   if (_format != RPixelFormatBGRA8Unorm && _format != RPixelFormatBGRX8Unorm)
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
                                                                                    width:(NSUInteger)size.width
                                                                                   height:(NSUInteger)size.height
                                                                                mipmapped:NO];
      _src = [_context.device newTextureWithDescriptor:td];
   }
}

- (CGSize)size
{
   return _size;
}

- (void)setFrame:(CGRect)frame
{
   if (CGRectEqualToRect(_frame, frame))
   {
      return;
   }

   _frame = frame;

   float l = (float)CGRectGetMinX(frame);
   float t = (float)CGRectGetMinY(frame);
   float r = (float)CGRectGetMaxX(frame);
   float b = (float)CGRectGetMaxY(frame);

   Vertex v[4] = {
      {simd_make_float3(l, b, 0), simd_make_float2(0, 1)},
      {simd_make_float3(r, b, 0), simd_make_float2(1, 1)},
      {simd_make_float3(l, t, 0), simd_make_float2(0, 0)},
      {simd_make_float3(r, t, 0), simd_make_float2(1, 0)},
   };
   memcpy(_v, v, sizeof(_v));
}

- (CGRect)frame
{
   return _frame;
}

- (void)_convertFormat
{
   if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
      return;

   if (!_srcDirty)
      return;

   [_context convertFormat:_format from:_src to:_texture];
   _srcDirty = NO;
}

- (void)drawWithContext:(Context *)ctx
{
   [self _convertFormat];
}

- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce
{
   [rce setVertexBytes:&_v length:sizeof(_v) atIndex:BufferIndexPositions];
   [rce setFragmentTexture:_texture atIndex:TextureIndexColor];
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
}

- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch
{
   if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
   {
      [_texture replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
                  mipmapLevel:0 withBytes:src
                  bytesPerRow:(NSUInteger)(4 * pitch)];
   }
   else
   {
      [_src replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
              mipmapLevel:0 withBytes:src
              bytesPerRow:(NSUInteger)(pitch)];
      _srcDirty = YES;
   }
}

@end
