#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_in
{
    float vIn3 [[user(locn3)]];
    float vIn2 [[user(locn2)]];
    float4 vIn1 [[user(locn1)]];
    float4 vIn0 [[user(locn0)]];
};

struct main0_out
{
    float4 FragColor [[color(0)]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    out.FragColor = float4(bool4(false, true, false, false).x ? in.vIn1.x : in.vIn0.x, bool4(false, true, false, false).y ? in.vIn1.y : in.vIn0.y, bool4(false, true, false, false).z ? in.vIn1.z : in.vIn0.z, bool4(false, true, false, false).w ? in.vIn1.w : in.vIn0.w);
    out.FragColor = float4(true ? in.vIn3 : in.vIn2);
    bool4 _37 = bool4(true);
    out.FragColor = float4(_37.x ? in.vIn0.x : in.vIn1.x, _37.y ? in.vIn0.y : in.vIn1.y, _37.z ? in.vIn0.z : in.vIn1.z, _37.w ? in.vIn0.w : in.vIn1.w);
    out.FragColor = float4(true ? in.vIn2 : in.vIn3);
    return out;
}

