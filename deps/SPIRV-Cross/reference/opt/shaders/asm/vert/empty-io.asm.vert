#version 450

struct VSInput
{
    vec4 position;
};

struct VSOutput
{
    vec4 position;
};

layout(location = 0) in vec4 position;

void main()
{
    gl_Position = position;
}

