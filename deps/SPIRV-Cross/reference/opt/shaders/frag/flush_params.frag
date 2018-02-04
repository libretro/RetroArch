#version 310 es
precision mediump float;
precision highp int;

struct Structy
{
    vec4 c;
};

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(10.0);
}

