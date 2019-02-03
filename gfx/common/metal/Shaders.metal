//
//  Shaders.metal
//  MetalRenderer
//
//  Created by Stuart Carnie on 5/31/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

// File for Metal kernel and shader functions

#include <metal_stdlib>
#include <simd/simd.h>

// Including header shared between this Metal shader code and Swift/C code executing Metal API commands
#import "ShaderTypes.h"

using namespace metal;

#pragma mark - functions using projected coordinates

vertex ColorInOut basic_vertex_proj_tex(const Vertex in [[ stage_in ]],
                                        const device Uniforms &uniforms [[ buffer(BufferIndexUniforms) ]])
{
    ColorInOut out;
    out.position = uniforms.projectionMatrix * float4(in.position, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 basic_fragment_proj_tex(ColorInOut in [[stage_in]],
                                        constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
                                        texture2d<half> tex          [[ texture(TextureIndexColor) ]],
                                        sampler samp                 [[ sampler(SamplerIndexDraw) ]])
{
    half4 colorSample = tex.sample(samp, in.texCoord.xy);
    return float4(colorSample);
}

#pragma mark - functions for rendering sprites

vertex FontFragmentIn sprite_vertex(const SpriteVertex in [[ stage_in ]], const device Uniforms &uniforms [[ buffer(BufferIndexUniforms) ]])
{
    FontFragmentIn out;
    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);
    out.texCoord = in.texCoord;
    out.color    = in.color;
    return out;
}

fragment float4 sprite_fragment_a8(FontFragmentIn  in  [[ stage_in ]],
                              texture2d<half> tex [[ texture(TextureIndexColor) ]],
                              sampler samp        [[ sampler(SamplerIndexDraw) ]])
{
    half4 colorSample = tex.sample(samp, in.texCoord.xy);
    return float4(in.color.rgb, in.color.a * colorSample.r);
}

#pragma mark - functions for rendering sprites

vertex FontFragmentIn stock_vertex(const SpriteVertex in [[ stage_in ]], const device Uniforms &uniforms [[ buffer(BufferIndexUniforms) ]])
{
    FontFragmentIn out;
    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);
    out.texCoord = in.texCoord;
    out.color    = in.color;
    return out;
}

fragment float4 stock_fragment(FontFragmentIn  in  [[ stage_in ]],
                              texture2d<float> tex [[ texture(TextureIndexColor) ]],
                              sampler samp         [[ sampler(SamplerIndexDraw) ]])
{
    float4 colorSample = tex.sample(samp, in.texCoord.xy);
    return colorSample * in.color;
}

fragment half4 stock_fragment_color(FontFragmentIn in [[ stage_in ]])
{
    return half4(in.color);
}

#pragma mark - filter kernels

kernel void convert_bgra4444_to_bgra8888(texture2d<ushort, access::read> in  [[ texture(0) ]],
                                         texture2d<half, access::write>  out [[ texture(1) ]],
                                         uint2                           gid [[ thread_position_in_grid ]])
{
   ushort pix  = in.read(gid).r;
   uchar4 pix2 = uchar4(
                        extract_bits(pix,  4, 4),
                        extract_bits(pix,  8, 4),
                        extract_bits(pix, 12, 4),
                        extract_bits(pix,  0, 4)
                        );

   out.write(half4(pix2) / 15.0, gid);
}

kernel void convert_rgb565_to_bgra8888(texture2d<ushort, access::read> in  [[ texture(0) ]],
                                       texture2d<half, access::write>  out [[ texture(1) ]],
                                       uint2                           gid [[ thread_position_in_grid ]])
{
   ushort pix  = in.read(gid).r;
   uchar4 pix2 = uchar4(
                        extract_bits(pix, 11, 5),
                        extract_bits(pix,  5, 6),
                        extract_bits(pix,  0, 5),
                        0xf
                        );

   out.write(half4(pix2) / half4(0x1f, 0x3f, 0x1f, 0xf), gid);
}
