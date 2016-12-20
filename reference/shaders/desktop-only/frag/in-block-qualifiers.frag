#version 450

layout(location = 0) out vec4 FragColor;
in VertexData
{
    layout(location = 0) flat float f;
    layout(location = 1) centroid vec4 g;
    layout(location = 2) flat int h;
    layout(location = 3) float i;
} vin;

layout(location = 4) in flat float f;
layout(location = 5) in centroid vec4 g;
layout(location = 6) in flat int h;
layout(location = 7) in sample float i;

void main()
{
    FragColor = ((((((vec4(vin.f) + vin.g) + vec4(float(vin.h))) + vec4(vin.i)) + vec4(f)) + g) + vec4(float(h))) + vec4(i);
}

