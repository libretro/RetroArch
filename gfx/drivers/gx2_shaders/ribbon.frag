#version 150

uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
} global;

layout(location = 0) in vec3 vEC;
layout(location = 0) out vec4 FragColor;

void main()
{
   const vec3 up = vec3(0.0, 0.0, 1.0);
   vec3 x = dFdx(vEC);
   vec3 y = dFdy(vEC);
   vec3 normal = normalize(cross(x, y));
   float c = 1.0 - dot(normal, up);
   c = (1.0 - cos(c * c)) / 20.0;
//   FragColor = vec4(c, c, c, 1.0);
   FragColor = vec4(1.0, 1.0, 1.0, c);
}
