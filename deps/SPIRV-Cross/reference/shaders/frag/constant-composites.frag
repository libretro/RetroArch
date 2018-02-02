#version 310 es
precision mediump float;
precision highp int;

struct Foo
{
    float a;
    float b;
};

layout(location = 0) out vec4 FragColor;
layout(location = 0) flat in mediump int _line;
float lut[4];
Foo foos[2];

void main()
{
    lut = float[](1.0, 4.0, 3.0, 2.0);
    foos = Foo[](Foo(10.0, 20.0), Foo(30.0, 40.0));
    FragColor = vec4(lut[_line]);
    FragColor += vec4(foos[_line].a * (foos[1 - _line].a));
}

