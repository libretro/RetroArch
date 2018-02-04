#version 310 es
precision mediump float;
precision highp int;

uniform mediump sampler2D uTex;

layout(location = 0) in vec2 vTex;
layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(vTex.xxy, 1.0) + vec4(texture(uTex, vTex).xyz, 1.0);
}

