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

VSOutput _main(VSInput _input)
{
    VSOutput _out;
    _out.position = _input.position;
    return _out;
}

void main()
{
    VSInput _input;
    _input.position = position;
    VSInput param = _input;
    gl_Position = _main(param).position;
}

