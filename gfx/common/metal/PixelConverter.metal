//
//  PixelConverter.metal
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#include <metal_stdlib>
using namespace metal;


#pragma mark - filter kernels

kernel void convert_abgr4444_to_bgra8888_tex(texture2d<ushort, access::read> in  [[ texture(0) ]],
                                             texture2d<half, access::write>  out [[ texture(1) ]],
                                             uint2                           gid [[ thread_position_in_grid ]])
{
    ushort pix = in.read(gid).r;
    
    uchar4 pix2 = uchar4(
                         extract_bits(pix,  4, 4),
                         extract_bits(pix,  8, 4),
                         extract_bits(pix, 12, 4),
                         extract_bits(pix,  0, 4)
                         );
    
    out.write(half4(pix2) / 15.0, gid);
}

kernel void convert_abgr4444_to_bgra8888(device uint16_t *              in  [[ buffer(0) ]],
                                         texture2d<half, access::write> out [[ texture(0) ]],
                                         uint                           id  [[ thread_position_in_grid ]])
{
    uint16_t pix = in[id];
    uchar4 pix2 = uchar4(
                         extract_bits(pix,  4, 4),
                         extract_bits(pix,  8, 4),
                         extract_bits(pix, 12, 4),
                         extract_bits(pix,  0, 4)
                         );
    
    uint ypos = id / out.get_width();
    uint xpos = id % out.get_width();
    
    out.write(half4(pix2) / 15.0, uint2(xpos, ypos));
}

kernel void convert_bgr565_to_bgra8888(device uint16_t *                in  [[ buffer(0) ]],
                                         texture2d<half, access::write> out [[ texture(0) ]],
                                         uint                           id  [[ thread_position_in_grid ]])
{
    uint16_t pix = in[id];
    uchar4 pix2 = uchar4(
                         extract_bits(pix, 11, 5),
                         extract_bits(pix,  5, 6),
                         extract_bits(pix,  0, 5),
                         0xf
                         );
    
    uint ypos = id / out.get_width();
    uint xpos = id % out.get_width();
    
    out.write(half4(pix2) / half4(0x1f, 0x3f, 0x1f, 0xf), uint2(xpos, ypos));
}
