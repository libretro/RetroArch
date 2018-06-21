//
// Created by Stuart Carnie on 6/16/18.
//

#import "View.h"

@class Renderer;

@interface TexturedView : NSObject<View>

@property (readonly) RPixelFormat format;
@property (readonly) RTextureFilter filter;
@property (readwrite) BOOL visible;
@property (readwrite) CGRect frame;
@property (readwrite) CGSize size;

- (instancetype)initWithDescriptor:(ViewDescriptor *)td renderer:(Renderer *)renderer;

- (void)prepareFrame:(Context *)ctx;
- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;

@end
