#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Foo
{
    float a;
    float b;
};

struct main0_in
{
    int line [[user(locn0)]];
};

struct main0_out
{
    float4 FragColor [[color(0)]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    float lut[4] = {1.0, 4.0, 3.0, 2.0};
    Foo foos[2] = {{10.0, 20.0}, {30.0, 40.0}};
    out.FragColor = float4(lut[in.line]);
    out.FragColor += float4(foos[in.line].a * (foos[1 - in.line].a));
    return out;
}

