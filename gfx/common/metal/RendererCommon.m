//
//  RendererCommon.m
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/3/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "RendererCommon.h"
#import <Metal/Metal.h>

NSUInteger RPixelFormatToBPP(RPixelFormat format)
{
    switch (format)
    {
        case RPixelFormatBGRA8Unorm:
        case RPixelFormatBGRX8Unorm:
            return 4;
            
        case RPixelFormatB5G6R5Unorm:
        case RPixelFormatBGRA4Unorm:
            return 2;
            
        default:
            NSLog(@"Unknown format %ld", format);
            abort();
    }
}

static NSString * RPixelStrings[RPixelFormatCount];

NSString *NSStringFromRPixelFormat(RPixelFormat format)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        
#define STRING(literal) RPixelStrings[literal] = @#literal
        STRING(RPixelFormatInvalid);
        STRING(RPixelFormatB5G6R5Unorm);
        STRING(RPixelFormatBGRA4Unorm);
        STRING(RPixelFormatBGRA8Unorm);
        STRING(RPixelFormatBGRX8Unorm);
#undef STRING
        
    });
    
    if (format >= RPixelFormatCount)
    {
        format = 0;
    }
    
    return RPixelStrings[format];
}


