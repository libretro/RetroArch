#version 310 es
/* credits to: TheTimJames
   https://www.shadertoy.com/view/Md2GRw
*/

precision highp float;

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
   float yflip;
} constants;

layout(location = 0) out vec4 FragColor;

float rand(vec2 co)
{
   return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float rand_float(float x)
{
   return rand(vec2(x, 1.0));
}

float snow(vec3 pos, vec2 uv, float o)
{
   vec2 d = (pos.xy - uv);
   float a = atan(d.y, d.x) + sin(constants.time * 1.0 + o) * 10.0;

   float dist = d.x * d.x + d.y * d.y;

   if (dist < pos.z / 400.0)
   {
      float col = 0.0;
      if (sin(a * 8.0) < 0.0)
         col = 1.0;
      if (dist < pos.z / 800.0)
         col += 1.0;
      return col * pos.z;
   }

   return 0.0;
}

float col(vec2 c)
{
   float color = 0.0;
   for (int i = 1; i < 15; i++)
   {
      float o = rand_float(float(i) / 3.0) * 15.0;
      float z = rand_float(float(i) + 13.0);
      float x = 1.8 - (3.6) * (rand_float(floor((constants.time * ((z + 1.0) / 2.0) + o) / 2.0)) + sin(constants.time * o / 1000.0) / 10.0);
      float y = 1.0 - mod((constants.time * ((z + 1.0) / 2.0)) + o, 2.0);

      color += snow(vec3(x, y, z), c, o);
   }

   return color;
}

void main(void)
{
   vec2 uv = gl_FragCoord.xy / constants.OutputSize.xy;
   uv = uv * 2.0 - 1.0;
   uv.y *= constants.yflip;
   vec2 p = uv;
   p.x *= constants.OutputSize.x / constants.OutputSize.y;

   float c = col(p);
   FragColor = vec4(c, c, c, c);
}
