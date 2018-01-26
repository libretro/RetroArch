#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant float a_tmp [[function_constant(1)]];
constant float a = is_function_constant_defined(a_tmp) ? a_tmp : 1.0;
constant float b_tmp [[function_constant(2)]];
constant float b = is_function_constant_defined(b_tmp) ? b_tmp : 2.0;
constant int c_tmp [[function_constant(3)]];
constant int c = is_function_constant_defined(c_tmp) ? c_tmp : 3;
constant int d_tmp [[function_constant(4)]];
constant int d = is_function_constant_defined(d_tmp) ? d_tmp : 4;
constant uint e_tmp [[function_constant(5)]];
constant uint e = is_function_constant_defined(e_tmp) ? e_tmp : 5u;
constant uint f_tmp [[function_constant(6)]];
constant uint f = is_function_constant_defined(f_tmp) ? f_tmp : 6u;
constant bool g_tmp [[function_constant(7)]];
constant bool g = is_function_constant_defined(g_tmp) ? g_tmp : false;
constant bool h_tmp [[function_constant(8)]];
constant bool h = is_function_constant_defined(h_tmp) ? h_tmp : true;

struct main0_out
{
    float4 FragColor [[color(0)]];
};

fragment main0_out main0()
{
    main0_out out = {};
    float t0 = a;
    float t1 = b;
    uint c0 = (uint(c) + 0u);
    int c1 = (-c);
    int c2 = (~c);
    int c3 = (c + d);
    int c4 = (c - d);
    int c5 = (c * d);
    int c6 = (c / d);
    uint c7 = (e / f);
    int c8 = (c % d);
    uint c9 = (e % f);
    int c10 = (c >> d);
    uint c11 = (e >> f);
    int c12 = (c << d);
    int c13 = (c | d);
    int c14 = (c ^ d);
    int c15 = (c & d);
    bool c16 = (g || h);
    bool c17 = (g && h);
    bool c18 = (!g);
    bool c19 = (g == h);
    bool c20 = (g != h);
    bool c21 = (c == d);
    bool c22 = (c != d);
    bool c23 = (c < d);
    bool c24 = (e < f);
    bool c25 = (c > d);
    bool c26 = (e > f);
    bool c27 = (c <= d);
    bool c28 = (e <= f);
    bool c29 = (c >= d);
    bool c30 = (e >= f);
    int c31 = c8 + c3;
    int c32 = int(e + 0u);
    bool c33 = (c != int(0u));
    bool c34 = (e != 0u);
    int c35 = int(g);
    uint c36 = uint(g);
    float c37 = float(g);
    out.FragColor = float4(t0 + t1);
    return out;
}

