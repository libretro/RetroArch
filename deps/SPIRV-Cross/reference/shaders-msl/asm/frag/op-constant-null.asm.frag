#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct D
{
    float4 a;
    float b;
};

constant float4 _14[4] = {float4(0.0), float4(0.0), float4(0.0), float4(0.0)};

struct main0_out
{
    float FragColor [[color(0)]];
};

// Implementation of an array copy function to cover GLSL's ability to copy an array via assignment.
template<typename T, uint N>
void spvArrayCopy(thread T (&dst)[N], thread const T (&src)[N])
{
    for (uint i = 0; i < N; dst[i] = src[i], i++);
}

// An overload for constant arrays.
template<typename T, uint N>
void spvArrayCopyConstant(thread T (&dst)[N], constant T (&src)[N])
{
    for (uint i = 0; i < N; dst[i] = src[i], i++);
}

fragment main0_out main0()
{
    main0_out out = {};
    float a = 0.0;
    float4 b = float4(0.0);
    float2x3 c = float2x3(float3(0.0), float3(0.0));
    D d = {float4(0.0), 0.0};
    float4 e[4] = {float4(0.0), float4(0.0), float4(0.0), float4(0.0)};
    out.FragColor = a;
    return out;
}

