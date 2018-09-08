/* credits to: TheTimJames
   https://www.shadertoy.com/view/Md2GRw
*/

#include "shaders_common.h"

static const char* stock_fragment_xmb_snowflake_core = GLSL(
   uniform float time;
   uniform vec2 OutputSize;
   vec2 uv;
   out vec4 FragColor;

   float atime;

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
      float a = atan(d.y,d.x) + sin(time*1.0 + o) * 10.0;

      float dist = d.x*d.x + d.y*d.y;

      if(dist < pos.z/400.0)
      {
         float col = 0.0;
         if(sin(a * 8.0) < 0.0)
         {
            col=1.0;
         }
         if(dist < pos.z/800.0)
         {
            col+=1.0;
         }
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
		   float x = 1.8 - (3.6) * (rand_float(floor((time*((z + 1.0) / 2.0) +o) / 2.0)) + sin(time * o /1000.0) / 10.0);
		   float y = 1.0 - mod((time * ((z + 1.0)/2.0)) + o, 2.0);

		   color += snow(vec3(x,y,z), c, o);
	   }

      return color;
   }

   void main(void)
   {
      uv = gl_FragCoord.xy / OutputSize.xy;
      uv = uv * 2.0 - 1.0;
      vec2 p = uv;
      p.x *= OutputSize.x / OutputSize.y;

      atime = (time + 1.0) / 4.0;

      FragColor = vec4(col(p));
   }

);
