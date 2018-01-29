static const float a = 1.0f;
static const float b = 2.0f;
static const int c = 3;
static const int d = 4;
static const uint e = 5u;
static const uint f = 6u;
static const bool g = false;
static const bool h = true;

struct Foo
{
    float elems[(d + 2)];
};

static float4 FragColor;

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

void frag_main()
{
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
    float vec0[(c + 3)][8];
    vec0[0][0] = 10.0f;
    float vec1[(c + 2)];
    vec1[0] = 20.0f;
    Foo foo;
    foo.elems[c] = 10.0f;
    FragColor = (((t0 + t1).xxxx + vec0[0][0].xxxx) + vec1[0].xxxx) + foo.elems[c].xxxx;
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
