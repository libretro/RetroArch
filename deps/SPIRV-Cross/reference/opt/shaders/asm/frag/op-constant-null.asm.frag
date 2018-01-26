#version 310 es
precision mediump float;
precision highp int;

struct D
{
    vec4 a;
    float b;
};

layout(location = 0) out float FragColor;

void main()
{
    FragColor = 0.0;
}

