#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    int FragColor [[color(0)]];
};

fragment main0_out main0()
{
    main0_out out = {};
    out.FragColor = 16;
    for (int _140 = 0; _140 < 25; _140++)
    {
        out.FragColor += 10;
    }
    for (int _141 = 1; _141 < 30; _141++)
    {
        out.FragColor += 11;
    }
    int _142;
    _142 = 0;
    for (; _142 < 20; _142++)
    {
        out.FragColor += 12;
    }
    int _62 = _142 + 3;
    out.FragColor += _62;
    if (_62 == 40)
    {
        for (int _143 = 0; _143 < 40; _143++)
        {
            out.FragColor += 13;
        }
        return out;
    }
    else
    {
        out.FragColor += _62;
    }
    int2 _144;
    _144 = int2(0);
    int2 _139;
    for (; _144.x < 10; _139 = _144, _139.x = _144.x + 4, _144 = _139)
    {
        out.FragColor += _144.y;
    }
    for (int _145 = _62; _145 < 40; _145++)
    {
        out.FragColor += _145;
    }
    out.FragColor += _62;
    return out;
}

