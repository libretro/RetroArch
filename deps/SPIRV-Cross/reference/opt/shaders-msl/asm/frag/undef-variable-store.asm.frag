#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant float4 _38 = {};
constant float4 _50 = {};

struct main0_out
{
    float4 _entryPointOutput [[color(0)]];
};

fragment main0_out main0()
{
    main0_out out = {};
    float4 _51;
    _51 = _50;
    float4 _52;
    for (;;)
    {
        if (0.0 != 0.0)
        {
            _52 = float4(1.0, 0.0, 0.0, 1.0);
            break;
        }
        else
        {
            _52 = float4(1.0, 1.0, 0.0, 1.0);
            break;
        }
        _52 = _38;
        break;
    }
    out._entryPointOutput = _52;
    return out;
}

