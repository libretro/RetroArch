#version 310 es
precision mediump float;
precision highp int;

layout(location = 0) out vec4 result;
layout(location = 0) in vec4 accum;

uint _49;

void main()
{
    result = vec4(0.0);
    uint _51;
    uint _50;
    for (int _48 = 0; _48 < 4; _48 += int(_51), _50 = _51)
    {
        if (accum.y > 10.0)
        {
            _51 = 40u;
        }
        else
        {
            _51 = 30u;
        }
        result += accum;
    }
}

