#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Light
{
    packed_float3 Position;
    float Radius;
    float4 Color;
};

struct UBO
{
    float4x4 uMVP;
    Light lights[4];
};

struct main0_in
{
    float3 aNormal [[attribute(1)]];
    float4 aVertex [[attribute(0)]];
};

struct main0_out
{
    float4 vColor [[user(locn0)]];
    float4 gl_Position [[position]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant UBO& _21 [[buffer(0)]])
{
    main0_out out = {};
    out.gl_Position = _21.uMVP * in.aVertex;
    out.vColor = float4(0.0);
    for (int _103 = 0; _103 < 4; _103++)
    {
        float3 _68 = in.aVertex.xyz - _21.lights[_103].Position;
        out.vColor += ((_21.lights[_103].Color * clamp(1.0 - (length(_68) / _21.lights[_103].Radius), 0.0, 1.0)) * dot(in.aNormal, normalize(_68)));
    }
    return out;
}

