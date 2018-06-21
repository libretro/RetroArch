//
//  RView.h
//  MetalRenderer
//
//  Created by Stuart Carnie on 5/31/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "RendererCommon.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

@protocol View<NSObject>

@property (readonly) RPixelFormat format;
@property (readonly) RTextureFilter filter;
@property (readwrite) BOOL visible;
@property (readwrite) CGRect frame;
@property (readwrite) CGSize size;

@optional
- (void)prepareFrame:(Context *)ctx;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;

@end

@interface ViewDescriptor : NSObject
@property (readwrite) RPixelFormat format;
@property (readwrite) RTextureFilter filter;
@property (readwrite) CGSize size;

- (instancetype)init;
@end
