#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant float _17[5] = {1.0, 2.0, 3.0, 4.0, 5.0};

struct main0_out
{
    float4 FragColor [[color(0)]];
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
    float lut[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    for (int _46 = 0; _46 < 4; )
    {
        int _33 = _46 + 1;
        out.FragColor += float4(lut[_33]);
        _46 = _33;
        continue;
    }
    return out;
}

