#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct D
{
    float4 a;
    float b;
};

struct main0_out
{
    float FragColor [[color(0)]];
};

fragment main0_out main0()
{
    main0_out out = {};
    out.FragColor = 0.0;
    return out;
}

