#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 FragColor [[color(0)]];
};

fragment main0_out main0()
{
    main0_out out = {};
    out.FragColor = (((float4(1.0) + float4(1.0)) + (float3(1.0).xyzz + float4(1.0))) + (float4(1.0) + float4(2.0))) + (float2(1.0).xyxy + float4(2.0));
    return out;
}

