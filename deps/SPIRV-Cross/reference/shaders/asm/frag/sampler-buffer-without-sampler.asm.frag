#version 450

layout(rgba32f) uniform writeonly imageBuffer RWTex;
uniform samplerBuffer Tex;

layout(location = 0) out vec4 _entryPointOutput;

vec4 _main()
{
    vec4 storeTemp = vec4(1.0, 2.0, 3.0, 4.0);
    imageStore(RWTex, 20, storeTemp);
    return texelFetch(Tex, 10);
}

void main()
{
    vec4 _28 = _main();
    _entryPointOutput = _28;
}

