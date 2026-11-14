#version 150

uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
} global;

layout(location = 0) in vec2 VertexCoord;
layout(location = 0) out vec4 vEC;

void main()
{
   float x = VertexCoord.x;
   float row = floor((VertexCoord.y + 1.0) * 31.5 + 0.5);
   float z = min(row, 53.0) * (2.0 / 53.0) - 1.0;
   float a = x * 1.45 + global.time * 0.08;
   float b = z * 1.75 + x * (1.2 + 0.45 * z) - global.time * 0.06;
   float c = z * (5.8 + 1.2 * z) + x * 0.75 - global.time * 0.09;
   float d = z * 10.0 + x * (2.2 - 0.6 * z) + global.time * 0.075;
   float width = 0.13 + 0.05 * cos(a);
   float y = -0.28 + 0.12 * sin(a) + width * cos(b) * cos(c)
         + 0.04 * sin(d) + 0.06 * z;
   float dx = 0.174 * cos(a) - 0.0725 * sin(a) * cos(b) * cos(c)
         - width * ((1.2 + 0.45 * z) * sin(b) * cos(c) + 0.75 * cos(b) * sin(c))
         + 0.04 * (2.2 - 0.6 * z) * cos(d);
   float dz = -width * ((1.75 + 0.45 * x) * sin(b) * cos(c)
         + (5.8 + 2.4 * z) * cos(b) * sin(c))
         + 0.04 * (10.0 - 0.6 * x) * cos(d) + 0.06;
   float stripes = 0.65 + 0.35 * cos(z * 28.0);
   float w = 1.0 + 0.10 * z;
   vec3 normal = vec3(dx, -1.12, 1.12 * dz);
   vec3 eye = vec3(-0.112 * x, -0.10 * y, -w);
   float angle = dot(normal, eye) / sqrt(dot(normal, normal) * dot(eye, eye));
   vEC = vec4(angle, (dz * w - 0.10 * y) / (w * w), z,
         stripes * (1.0 - step(52.5, row)));
   gl_Position = vec4(1.12 * x, y, 0.0, w);
   if (row > 54.5)
   {
      float layer = floor((row - 55.0) / 3.0);
      float t = row - 56.0 - 3.0 * layer;
      vEC = vec4(x, t, 2.0 + layer, 1.0 - abs(t));
      gl_Position = vec4(x, -0.28 + 0.06 * sin(a) + t * (0.70 + 0.12 * layer), 0.0, 1.0);
   }
}
