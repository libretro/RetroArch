//
//  Context.m
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "Context.h"
#import <QuartzCore/QuartzCore.h>

@interface Context()
{
   CAMetalLayer *_layer;
   id<CAMetalDrawable> _drawable;
}

@end

@implementation Context

+ (instancetype)newContextWithDevice:(id<MTLDevice>)d
                               layer:(CAMetalLayer *)layer
                             library:(id<MTLLibrary>)l
                        commandQueue:(id<MTLCommandQueue>)q
{
   Context *c = [Context new];
   c->_device = d;
   c->_layer = layer;
   c->_library = l;
   c->_commandQueue = q;

   return c;
}

- (id<CAMetalDrawable>)nextDrawable {
   if (_drawable == nil) {
      _drawable = _layer.nextDrawable;
   }
   return _drawable;
}

- (id<MTLTexture>)renderTexture {
   return self.nextDrawable.texture;
}

- (void)begin
{
   assert(_commandBuffer == nil);
   _commandBuffer = [_commandQueue commandBuffer];
}

- (void)end
{
   assert(self->_commandBuffer != nil);
   [_commandBuffer commit];
   _commandBuffer = nil;
   _drawable = nil;
}

@end
