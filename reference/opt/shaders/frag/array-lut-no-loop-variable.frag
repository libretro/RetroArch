#version 310 es
precision mediump float;
precision highp int;

layout(location = 0) out vec4 FragColor;

void main()
{
    float lut[5] = float[](1.0, 2.0, 3.0, 4.0, 5.0);
    for (int _46 = 0; _46 < 4; )
    {
        mediump int _33 = _46 + 1;
        FragColor += vec4(lut[_33]);
        _46 = _33;
        continue;
    }
}

