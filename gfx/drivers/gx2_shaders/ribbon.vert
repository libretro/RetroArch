#version 150

uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
} global;

layout(location = 0) in vec2 VertexCoord;
layout(location = 0) out vec3 vEC;

float iqhash(float n)
{
   return fract(sin(n) * 43758.5453);
}

float noise(vec3 x)
{
   vec3 p = floor(x);
   vec3 f = fract(x);
   f = f * f * (3.0 - 2.0 * f);
   float n = p.x + p.y * 57.0 + 113.0 * p.z;
   return mix(mix(mix(iqhash(n), iqhash(n + 1.0), f.x),
              mix(iqhash(n + 57.0), iqhash(n + 58.0), f.x), f.y),
              mix(mix(iqhash(n + 113.0), iqhash(n + 114.0), f.x),
              mix(iqhash(n + 170.0), iqhash(n + 171.0), f.x), f.y), f.z);
}

float xmb_noise2(vec3 x)
{
   return cos(x.z * 4.0) * cos(x.z + global.time / 10.0 + x.x);
}

void main()
{
   vec3 v = vec3(VertexCoord.x, 0.0, VertexCoord.y);
   vec3 v2 = v;
   vec3 v3 = v;

   v.y = xmb_noise2(v2) / 8.0;

   v3.x -= global.time / 5.0;
   v3.x /= 4.0;

   v3.z -= global.time / 10.0;
   v3.y -= global.time / 100.0;

   v.z -= noise(v3 * 7.0) / 15.0;
   v.y -= noise(v3 * 7.0) / 15.0 + cos(v.x * 2.0 - global.time / 2.0) / 5.0 - 0.3;
   v.y = -v.y;

   vEC = v;
   gl_Position = vec4(v, 1.0);
}
