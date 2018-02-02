#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant uint _49 = {};

struct main0_in
{
    float4 accum [[user(locn0)]];
};

struct main0_out
{
    float4 result [[color(0)]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    out.result = float4(0.0);
    uint _51;
    uint _50;
    for (int _48 = 0; _48 < 4; _48 += int(_51), _50 = _51)
    {
        if (in.accum.y > 10.0)
        {
            _51 = 40u;
        }
        else
        {
            _51 = 30u;
        }
        out.result += in.accum;
    }
    return out;
}

