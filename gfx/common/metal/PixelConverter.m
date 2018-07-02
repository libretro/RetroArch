//
//  PixelConverter.m
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "PixelConverter+private.h"
#import "Filter.h"
#import "Context.h"

@implementation PixelConverter {
    Context *_context;
    Filter  *_filters[RPixelFormatCount]; // convert to bgra8888
}

- (instancetype)initWithContext:(Context *)c
{
    if (self = [super init])
    {
        _context = c;
        NSError *err = nil;
        _filters[RPixelFormatBGRA4Unorm]  = [Filter newFilterWithFunctionName:@"convert_bgra4444_to_bgra8888"
                                                                       device:c.device library:c.library
                                                                        error:&err];
        _filters[RPixelFormatB5G6R5Unorm] = [Filter newFilterWithFunctionName:@"convert_rgb565_to_bgra8888"
                                                                       device:c.device
                                                                      library:c.library
                                                                        error:&err];
        if (err)
        {
            NSLog(@"unable to create pixel conversion filter: %@", err.localizedDescription);
            abort();
        }
    }
    return self;
}

- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLBuffer>)src to:(id<MTLTexture>)dst
{
    assert(dst.width*dst.height == src.length/RPixelFormatToBPP(fmt));
    assert(fmt >= 0 && fmt < RPixelFormatCount);
    Filter *conv = _filters[fmt];
    assert(conv != nil);
    [conv apply:_context.commandBuffer inBuf:src outTex:dst];
}

@end
