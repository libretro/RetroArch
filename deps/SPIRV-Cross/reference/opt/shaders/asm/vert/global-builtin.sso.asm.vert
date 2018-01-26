#version 450

out gl_PerVertex
{
    vec4 gl_Position;
};

struct VSOut
{
    float a;
    vec4 pos;
};

struct VSOut_1
{
    float a;
};

layout(location = 0) out VSOut_1 _entryPointOutput;

void main()
{
    _entryPointOutput.a = 40.0;
    gl_Position = vec4(1.0);
}

