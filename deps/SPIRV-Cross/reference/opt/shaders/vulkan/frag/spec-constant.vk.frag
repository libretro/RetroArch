#version 310 es
precision mediump float;
precision highp int;

struct Foo
{
    float elems[(4 + 2)];
};

layout(location = 0) out vec4 FragColor;

float _146[(3 + 2)];

void main()
{
    float vec0[(3 + 3)][8];
    Foo foo;
    FragColor = ((vec4(1.0 + 2.0) + vec4(vec0[0][0])) + vec4(_146[0])) + vec4(foo.elems[3]);
}

