#version 310 es
precision mediump float;
precision highp int;

layout(binding = 0) uniform mediump sampler2D uSampler;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 vTexCoord;

void main()
{
    FragColor = texture(uSampler, vTexCoord);
}

