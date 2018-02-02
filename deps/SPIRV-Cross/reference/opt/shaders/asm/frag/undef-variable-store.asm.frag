#version 450

layout(location = 0) out vec4 _entryPointOutput;

vec4 _38;
vec4 _50;

void main()
{
    vec4 _51;
    _51 = _50;
    vec4 _52;
    for (;;)
    {
        if (0.0 != 0.0)
        {
            _52 = vec4(1.0, 0.0, 0.0, 1.0);
            break;
        }
        else
        {
            _52 = vec4(1.0, 1.0, 0.0, 1.0);
            break;
        }
        _52 = _38;
        break;
    }
    _entryPointOutput = _52;
}

