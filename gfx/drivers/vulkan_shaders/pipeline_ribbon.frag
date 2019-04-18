#version 310 es
precision highp float;

layout(std140, set = 0, binding = 0) uniform UBO
{
   float time;
   float yflip;
} constants;

layout(location = 0) in vec3 vEC;
layout(location = 0) out vec4 FragColor;

void main()
{
   const vec3 up = vec3(0.0, 0.0, 1.0);
   vec3 x = dFdx(vEC);
   vec3 y = dFdy(vEC);
   y *= constants.yflip;
   vec3 normal = normalize(cross(x, y));
   float c = 1.0 - dot(normal, up);
   c = (1.0 - cos(c * c)) / 3.0;
   FragColor = vec4(c, c, c, 1.0);
}
