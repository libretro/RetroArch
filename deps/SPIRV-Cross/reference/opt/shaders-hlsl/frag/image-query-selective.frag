Texture1D<uint4> uSampler1DUint : register(t0);
SamplerState _uSampler1DUint_sampler : register(s0);
Texture1D<int4> uSampler1DInt : register(t0);
SamplerState _uSampler1DInt_sampler : register(s0);

uint SPIRV_Cross_textureSize(Texture1D<int4> Tex, uint Level, out uint Param)
{
    uint ret;
    Tex.GetDimensions(Level, ret.x, Param);
    return ret;
}

uint SPIRV_Cross_textureSize(Texture1D<uint4> Tex, uint Level, out uint Param)
{
    uint ret;
    Tex.GetDimensions(Level, ret.x, Param);
    return ret;
}

void frag_main()
{
    uint _17_dummy_parameter;
    uint _24_dummy_parameter;
}

void main()
{
    frag_main();
}
