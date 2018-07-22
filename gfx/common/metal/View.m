//
//  RView.m
//  MetalRenderer
//
//  Created by Stuart Carnie on 5/31/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "View.h"
#import "RendererCommon.h"

@implementation ViewDescriptor

- (instancetype)init
{
   self = [super init];
   if (self)
   {
      _format = RPixelFormatBGRA8Unorm;
   }
   return self;
}

- (NSString *)debugDescription
{
   return [NSString stringWithFormat:@"( format = %@, frame = %@ )",
                                     NSStringFromRPixelFormat(_format),
                                     NSStringFromSize(_size)];
}

@end
