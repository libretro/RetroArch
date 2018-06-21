//
//  Renderer.h
//  MetalRenderer
//
//  Created by Stuart Carnie on 5/31/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//


#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import "Context.h"
#import "PixelConverter.h"

@class ViewDescriptor;
@protocol View;

@interface Renderer : NSObject

@property (readonly) Context*         context;
@property (readonly) PixelConverter*  conv;

- (instancetype)initWithDevice:(id<MTLDevice>)device layer:(CAMetalLayer *)layer;
- (void)drawableSizeWillChange:(CGSize)size;

- (void)beginFrame;
- (void)drawFrame;

#pragma mark - view management

- (void)addView:(id<View>)view;
- (void)removeView:(id<View>)view;
- (void)bringViewToFront:(id<View>)view;
- (void)sendViewToBack:(id<View>)view;

@end

