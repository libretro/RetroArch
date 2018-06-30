//
// Created by Stuart Carnie on 6/16/18.
//

#import "View.h"

@interface TexturedView : NSObject

@property (readonly) RPixelFormat format;
@property (readonly) RTextureFilter filter;
@property (readwrite) BOOL visible;
@property (readwrite) CGRect frame;
@property (readwrite) CGSize size;
@property (readonly) ViewDrawState drawState;

- (instancetype)initWithDescriptor:(ViewDescriptor *)td context:(Context *)c;

- (void)drawWithContext:(Context *)ctx;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;
- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch;

@end
