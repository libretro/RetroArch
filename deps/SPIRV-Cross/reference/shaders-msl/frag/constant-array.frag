#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Foobar
{
    float a;
    float b;
};

struct main0_in
{
    int index [[user(locn0)]];
};

struct main0_out
{
    float4 FragColor [[color(0)]];
};

float4 resolve(thread const Foobar& f)
{
    return float4(f.a + f.b);
}

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    float4 indexable[3] = {float4(1.0), float4(2.0), float4(3.0)};
    float4 indexable_1[2][2] = {{float4(1.0), float4(2.0)}, {float4(8.0), float4(10.0)}};
    Foobar param = {10.0, 20.0};
    Foobar indexable_2[2] = {{10.0, 40.0}, {90.0, 70.0}};
    Foobar param_1 = indexable_2[in.index];
    out.FragColor = ((indexable[in.index] + (indexable_1[in.index][in.index + 1])) + resolve(param)) + resolve(param_1);
    return out;
}

