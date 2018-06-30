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

typedef NS_ENUM(NSInteger, ViewDrawState)
{
   ViewDrawStateNone    = 0x00,
   ViewDrawStateContext = 0x01,
   ViewDrawStateEncoder = 0x02,
   
   ViewDrawStateAll     = 0x03,
};

@interface ViewDescriptor : NSObject
@property (readwrite) RPixelFormat format;
@property (readwrite) RTextureFilter filter;
@property (readwrite) CGSize size;

- (instancetype)init;
@end
