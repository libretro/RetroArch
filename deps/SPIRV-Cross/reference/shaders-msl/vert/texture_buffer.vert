#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 gl_Position [[position]];
};

vertex main0_out main0(texture2d<float> uSamp [[texture(0)]], texture2d<float> uSampo [[texture(1)]])
{
    main0_out out = {};
    out.gl_Position = uSamp.read(uint2(10, 0)) + uSampo.read(uint2(100, 0));
    return out;
}

