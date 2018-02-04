#version 450
#extension GL_AMD_shader_fragment_mask : require

layout(binding = 0) uniform sampler2DMS t;

void main()
{
    vec4 test2 = fragmentFetchAMD(t, 4u);
    uint testi2 = fragmentMaskFetchAMD(t);
}

