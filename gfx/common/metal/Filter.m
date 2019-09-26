//
//  Filter.m
//  MetalByExampleObjC
//
//  Created by Stuart Carnie on 5/15/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "Filter.h"
#import <Metal/Metal.h>

@interface Filter()
- (instancetype)initWithKernel:(id<MTLComputePipelineState>)kernel sampler:(id<MTLSamplerState>)sampler;
@end

@implementation Filter
{
   id<MTLComputePipelineState> _kernel;
}

+ (instancetype)newFilterWithFunctionName:(NSString *)name device:(id<MTLDevice>)device library:(id<MTLLibrary>)library error:(NSError **)error
{
   id<MTLFunction> function = [library newFunctionWithName:name];
   id<MTLComputePipelineState> kernel = [device newComputePipelineStateWithFunction:function error:error];
   if (*error != nil)
   {
      return nil;
   }

   MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
   sd.minFilter = MTLSamplerMinMagFilterNearest;
   sd.magFilter = MTLSamplerMinMagFilterNearest;
   sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
   sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
   sd.mipFilter = MTLSamplerMipFilterNotMipmapped;
   id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];

   return [[Filter alloc] initWithKernel:kernel sampler:sampler];
}

- (instancetype)initWithKernel:(id<MTLComputePipelineState>)kernel sampler:(id<MTLSamplerState>)sampler
{
   if (self = [super init])
   {
      _kernel = kernel;
      _sampler = sampler;
   }
   return self;
}

- (void)apply:(id<MTLCommandBuffer>)cb in:(id<MTLTexture>)tin out:(id<MTLTexture>)tout
{
   id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
   ce.label = @"filter kernel";

   [ce setComputePipelineState:_kernel];

   [ce setTexture:tin atIndex:0];
   [ce setTexture:tout atIndex:1];

   [self.delegate configure:ce];

   MTLSize size = MTLSizeMake(16, 16, 1);
   MTLSize count = MTLSizeMake((tin.width + size.width + 1) / size.width, (tin.height + size.height + 1) / size.height,
                               1);

   [ce dispatchThreadgroups:count threadsPerThreadgroup:size];

   [ce endEncoding];
}

- (void)apply:(id<MTLCommandBuffer>)cb inBuf:(id<MTLBuffer>)tin outTex:(id<MTLTexture>)tout
{
   id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
   ce.label = @"filter kernel";
 
   [ce setComputePipelineState:_kernel];

   [ce setBuffer:tin offset:0 atIndex:0];
   [ce setTexture:tout atIndex:0];

   [self.delegate configure:ce];

   MTLSize size = MTLSizeMake(32, 1, 1);
   MTLSize count = MTLSizeMake((tin.length + 00) / 32, 1, 1);

   [ce dispatchThreadgroups:count threadsPerThreadgroup:size];

   [ce endEncoding];
}

@end
