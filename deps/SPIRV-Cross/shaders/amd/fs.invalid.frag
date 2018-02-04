#version 450
#extension GL_AMD_shader_fragment_mask : require
#extension GL_AMD_shader_explicit_vertex_parameter : require

uniform sampler2DMS texture1;
layout(location = 0) in vec4 vary;

void main()
{
    uint testi1 = fragmentMaskFetchAMD(texture1, ivec2(0));
    vec4 test1 = fragmentFetchAMD(texture1, ivec2(1), 2);

    vec4 pos =  interpolateAtVertexAMD(vary, 0u);
}
