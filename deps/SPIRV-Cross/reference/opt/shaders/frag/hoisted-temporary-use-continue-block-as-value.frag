#version 310 es
precision mediump float;
precision highp int;

layout(location = 0) out vec4 FragColor;
layout(location = 0) flat in mediump int vA;
layout(location = 1) flat in mediump int vB;

void main()
{
    FragColor = vec4(0.0);
    mediump int _49;
    int _60;
    for (int _57 = 0, _58 = 0; _58 < vA; _57 = _60, _58 += _49)
    {
        if ((vA + _58) == 20)
        {
            _60 = 50;
        }
        else
        {
            int _59;
            if ((vB + _58) == 40)
            {
                _59 = 60;
            }
            else
            {
                _59 = _57;
            }
            _60 = _59;
        }
        _49 = _60 + 10;
        FragColor += vec4(1.0);
    }
}

