#version 310 es
precision highp float;

layout(std140, set = 0, binding = 0) uniform UBO
{
   float time;
   float yflip;
   float alpha;
} constants;

layout(location = 0) in vec3 vEC;
layout(location = 0) out vec4 FragColor;

void main()
{
   const vec3 up = vec3(0.0, 0.0, 1.0);
   vec3 x = dFdx(vEC);
   vec3 y = dFdy(vEC);
   /* Vulkan dFdy is positive-Y-up while D3D ddy is positive-Y-down.
    * yflip also affects screen-space derivative direction.
    * Multiply by -yflip to correct for both. */
   y *= -constants.yflip;
   vec3 normal = normalize(cross(x, y));
   float c = 1.0 - dot(normal, up);
   c = (1.0 - cos(c * c)) / 13.0;
   FragColor = vec4(c, c, c, constants.alpha);
}
