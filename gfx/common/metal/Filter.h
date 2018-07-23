//
//  Filter.h
//  MetalByExampleObjC
//
//  Created by Stuart Carnie on 5/15/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

@protocol FilterDelegate
- (void)configure:(id<MTLCommandEncoder>)encoder;
@end

@interface Filter : NSObject

@property (nonatomic, readwrite) id<FilterDelegate> delegate;
@property (nonatomic, readonly) id<MTLSamplerState> sampler;

- (void)apply:(id<MTLCommandBuffer>)cb in:(id<MTLTexture>)tin out:(id<MTLTexture>)tout;
- (void)apply:(id<MTLCommandBuffer>)cb inBuf:(id<MTLBuffer>)tin outTex:(id<MTLTexture>)tout;

+ (instancetype)newFilterWithFunctionName:(NSString *)name device:(id<MTLDevice>)device library:(id<MTLLibrary>)library error:(NSError **)error;

@end
