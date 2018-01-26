#version 310 es
precision mediump float;
precision highp int;

struct Foobar
{
    float a;
    float b;
};

layout(location = 0) out vec4 FragColor;
layout(location = 0) flat in mediump int index;

vec4 resolve(Foobar f)
{
    return vec4(f.a + f.b);
}

void main()
{
    highp vec4 indexable[3] = vec4[](vec4(1.0), vec4(2.0), vec4(3.0));
    highp vec4 indexable_1[2][2] = vec4[][](vec4[](vec4(1.0), vec4(2.0)), vec4[](vec4(8.0), vec4(10.0)));
    Foobar param = Foobar(10.0, 20.0);
    Foobar indexable_2[2] = Foobar[](Foobar(10.0, 40.0), Foobar(90.0, 70.0));
    Foobar param_1 = indexable_2[index];
    FragColor = ((indexable[index] + (indexable_1[index][index + 1])) + resolve(param)) + resolve(param_1);
}

