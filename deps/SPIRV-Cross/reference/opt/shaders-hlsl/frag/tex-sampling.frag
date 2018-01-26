Texture1D<float4> tex1d;
SamplerState _tex1d_sampler;
Texture2D<float4> tex2d;
SamplerState _tex2d_sampler;
Texture3D<float4> tex3d;
SamplerState _tex3d_sampler;
TextureCube<float4> texCube;
SamplerState _texCube_sampler;
Texture1D<float4> tex1dShadow;
SamplerComparisonState _tex1dShadow_sampler;
Texture2D<float4> tex2dShadow;
SamplerComparisonState _tex2dShadow_sampler;
TextureCube<float4> texCubeShadow;
SamplerComparisonState _texCubeShadow_sampler;
Texture1DArray<float4> tex1dArray;
SamplerState _tex1dArray_sampler;
Texture2DArray<float4> tex2dArray;
SamplerState _tex2dArray_sampler;
TextureCubeArray<float4> texCubeArray;
SamplerState _texCubeArray_sampler;
Texture2D<float4> separateTex2d;
SamplerState samplerNonDepth;
Texture2D<float4> separateTex2dDepth;
SamplerComparisonState samplerDepth;

static float texCoord1d;
static float2 texCoord2d;
static float3 texCoord3d;
static float4 texCoord4d;
static float4 FragColor;

struct SPIRV_Cross_Input
{
    float texCoord1d : TEXCOORD0;
    float2 texCoord2d : TEXCOORD1;
    float3 texCoord3d : TEXCOORD2;
    float4 texCoord4d : TEXCOORD3;
};

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

float SPIRV_Cross_projectTextureCoordinate(float2 coord)
{
    return coord.x / coord.y;
}

float2 SPIRV_Cross_projectTextureCoordinate(float3 coord)
{
    return float2(coord.x, coord.y) / coord.z;
}

float3 SPIRV_Cross_projectTextureCoordinate(float4 coord)
{
    return float3(coord.x, coord.y, coord.z) / coord.w;
}

void frag_main()
{
    float4 _162 = (((((((((((((((((((tex1d.Sample(_tex1d_sampler, texCoord1d) + tex1d.Sample(_tex1d_sampler, texCoord1d, 1)) + tex1d.SampleLevel(_tex1d_sampler, texCoord1d, 2.0f)) + tex1d.SampleGrad(_tex1d_sampler, texCoord1d, 1.0f, 2.0f)) + tex1d.Sample(_tex1d_sampler, SPIRV_Cross_projectTextureCoordinate(float2(texCoord1d, 2.0f)))) + tex1d.SampleBias(_tex1d_sampler, texCoord1d, 1.0f)) + tex2d.Sample(_tex2d_sampler, texCoord2d)) + tex2d.Sample(_tex2d_sampler, texCoord2d, int2(1, 2))) + tex2d.SampleLevel(_tex2d_sampler, texCoord2d, 2.0f)) + tex2d.SampleGrad(_tex2d_sampler, texCoord2d, float2(1.0f, 2.0f), float2(3.0f, 4.0f))) + tex2d.Sample(_tex2d_sampler, SPIRV_Cross_projectTextureCoordinate(float3(texCoord2d, 2.0f)))) + tex2d.SampleBias(_tex2d_sampler, texCoord2d, 1.0f)) + tex3d.Sample(_tex3d_sampler, texCoord3d)) + tex3d.Sample(_tex3d_sampler, texCoord3d, int3(1, 2, 3))) + tex3d.SampleLevel(_tex3d_sampler, texCoord3d, 2.0f)) + tex3d.SampleGrad(_tex3d_sampler, texCoord3d, float3(1.0f, 2.0f, 3.0f), float3(4.0f, 5.0f, 6.0f))) + tex3d.Sample(_tex3d_sampler, SPIRV_Cross_projectTextureCoordinate(float4(texCoord3d, 2.0f)))) + tex3d.SampleBias(_tex3d_sampler, texCoord3d, 1.0f)) + texCube.Sample(_texCube_sampler, texCoord3d)) + texCube.SampleLevel(_texCube_sampler, texCoord3d, 2.0f)) + texCube.SampleBias(_texCube_sampler, texCoord3d, 1.0f);
    float _178 = _162.w + tex1dShadow.SampleCmp(_tex1dShadow_sampler, float3(texCoord1d, 0.0f, 0.0f).x, 0.0f);
    float4 _327 = _162;
    _327.w = _178;
    float _193 = _178 + tex2dShadow.SampleCmp(_tex2dShadow_sampler, float3(texCoord2d, 0.0f).xy, 0.0f);
    float4 _331 = _327;
    _331.w = _193;
    float4 _335 = _331;
    _335.w = _193 + texCubeShadow.SampleCmp(_texCubeShadow_sampler, float4(texCoord3d, 0.0f).xyz, 0.0f);
    float4 _308 = ((((((((((((((_335 + tex1dArray.Sample(_tex1dArray_sampler, texCoord2d)) + tex2dArray.Sample(_tex2dArray_sampler, texCoord3d)) + texCubeArray.Sample(_texCubeArray_sampler, texCoord4d)) + tex2d.GatherRed(_tex2d_sampler, texCoord2d)) + tex2d.GatherRed(_tex2d_sampler, texCoord2d)) + tex2d.GatherGreen(_tex2d_sampler, texCoord2d)) + tex2d.GatherBlue(_tex2d_sampler, texCoord2d)) + tex2d.GatherAlpha(_tex2d_sampler, texCoord2d)) + tex2d.GatherRed(_tex2d_sampler, texCoord2d, int2(1, 1))) + tex2d.GatherRed(_tex2d_sampler, texCoord2d, int2(1, 1))) + tex2d.GatherGreen(_tex2d_sampler, texCoord2d, int2(1, 1))) + tex2d.GatherBlue(_tex2d_sampler, texCoord2d, int2(1, 1))) + tex2d.GatherAlpha(_tex2d_sampler, texCoord2d, int2(1, 1))) + tex2d.Load(int3(int2(1, 2), 0))) + separateTex2d.Sample(samplerNonDepth, texCoord2d);
    float4 _339 = _308;
    _339.w = _308.w + separateTex2dDepth.SampleCmp(samplerDepth, texCoord3d.xy, texCoord3d.z);
    FragColor = _339;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    texCoord1d = stage_input.texCoord1d;
    texCoord2d = stage_input.texCoord2d;
    texCoord3d = stage_input.texCoord3d;
    texCoord4d = stage_input.texCoord4d;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
