static const float a = 1.0f;
static const float b = 2.0f;
static const int c = 3;
static const int d = 4;

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
    float vec0[(c + 3)][8];
    vec0[0][0] = 10.0f;
    Foo foo;
    foo.elems[c] = 10.0f;
    FragColor = (((a + b).xxxx + vec0[0][0].xxxx) + 20.0f.xxxx) + foo.elems[c].xxxx;
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
