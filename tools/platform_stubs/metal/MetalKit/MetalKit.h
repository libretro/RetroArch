/* Stand-in for the MetalKit declarations the Metal driver uses */
#ifndef STUB_METALKIT_H
#define STUB_METALKIT_H
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <AppKit/AppKit.h>
@class MTKView;
@protocol MTKViewDelegate <NSObject>
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size;
- (void)drawInMTKView:(MTKView *)view;
@end
@interface MTKView : NSView
@property (nonatomic, assign) id<MTKViewDelegate> delegate;
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic) MTLPixelFormat colorPixelFormat;
@property (nonatomic) MTLClearColor clearColor;
@property (nonatomic) CGSize drawableSize;
@property (nonatomic) BOOL enableSetNeedsDisplay;
@property (nonatomic, getter=isPaused) BOOL paused;
@property (nonatomic) BOOL autoResizeDrawable;
@property (nonatomic) NSInteger preferredFramesPerSecond;
@property (nonatomic, readonly) id<CAMetalDrawable> currentDrawable;
@property (nonatomic, readonly) MTLRenderPassDescriptor *currentRenderPassDescriptor;
- (id)initWithFrame:(CGRect)f device:(id<MTLDevice>)d;
- (void)draw;
@end
#endif
