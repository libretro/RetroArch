#version 150

uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
} global;

layout(location = 0) in vec2 VertexCoord;

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

void main()
{
   vec3 v = vec3(VertexCoord.x, 0.0, VertexCoord.y);
   vec3 v2 = v;
   v2.x = v2.x + global.time / 2.0;
   v2.z = v.z * 3.0;
   v.y = cos((v.x + v.z / 3.0 + global.time) * 2.0) / 10.0 + noise(v2.xyz) / 4.0;
   v.y = -v.y;
   gl_Position = vec4(v, 1.0);
}
