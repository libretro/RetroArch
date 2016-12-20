#version 450

out VertexData
{
    layout(location = 0) flat float f;
    layout(location = 1) centroid vec4 g;
    layout(location = 2) flat int h;
    layout(location = 3) float i;
} vout;

layout(location = 4) out flat float f;
layout(location = 5) out centroid vec4 g;
layout(location = 6) out flat int h;
layout(location = 7) out float i;

void main()
{
    vout.f = 10.0;
    vout.g = vec4(20.0);
    vout.h = 20;
    vout.i = 30.0;
    f = 10.0;
    g = vec4(20.0);
    h = 20;
    i = 30.0;
}

