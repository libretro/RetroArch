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
@property (readonly) ViewDrawState drawState;

- (instancetype)initWithDescriptor:(ViewDescriptor *)td renderer:(Renderer *)renderer;

- (void)drawWithContext:(Context *)ctx;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;
- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch;

@end
