#version 310 es
precision mediump float;
precision highp int;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = (((vec4(1.0) + vec4(1.0)) + (vec3(1.0).xyzz + vec4(1.0))) + (vec4(1.0) + vec4(2.0))) + (vec2(1.0).xyxy + vec4(2.0));
}

