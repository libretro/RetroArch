#version 310 es
precision mediump float;
precision highp int;

layout(location = 0) out mediump int FragColor;

void main()
{
    FragColor = 16;
    for (int _140 = 0; _140 < 25; _140++)
    {
        FragColor += 10;
    }
    for (int _141 = 1; _141 < 30; _141++)
    {
        FragColor += 11;
    }
    int _142;
    _142 = 0;
    for (; _142 < 20; _142++)
    {
        FragColor += 12;
    }
    mediump int _62 = _142 + 3;
    FragColor += _62;
    if (_62 == 40)
    {
        for (int _143 = 0; _143 < 40; _143++)
        {
            FragColor += 13;
        }
        return;
    }
    else
    {
        FragColor += _62;
    }
    ivec2 _144;
    _144 = ivec2(0);
    ivec2 _139;
    for (; _144.x < 10; _139 = _144, _139.x = _144.x + 4, _144 = _139)
    {
        FragColor += _144.y;
    }
    for (int _145 = _62; _145 < 40; _145++)
    {
        FragColor += _145;
    }
    FragColor += _62;
}

