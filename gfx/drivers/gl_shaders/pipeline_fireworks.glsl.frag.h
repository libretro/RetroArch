#include "shaders_common.h"

static const char* stock_fragment_xmb_fireworks = GLSL(
   uniform float time;
   uniform vec2 OutputSize;

   vec4 N(float h){
      return fract(sin(vec4(6.,9.,1.,0.)*h) * 200.);
   }

   void main(void)
   {
      float iTime = time * 2.;
      vec4 o = vec4(0.,0.,0.,0.);
      vec2 uv = gl_FragCoord.xy - vec2(500.,-500.);
      uv /= OutputSize * vec2(0.5, 1.);

      float e;
      float d;
      float i;
      vec4 j = vec4(0.,0.,0.,0.);
      vec4 p;

      for(i=0.; i < 4.; i++){
         e = i * 93.1 + iTime + 200.;
         d = floor(e);
         p = N(d)+.3;
         e -= d;
         for(d=0.; d < 12.; d++)
            {
               j = N(d*i);
               o += p*(9.-e) / 1000. / length(vec2(uv.x-(p.x-e*(j.x+0.5)), uv.y-(p.y-e*(j.y-0.75))));
            }
         }
      gl_FragColor = clamp(o, 0., 0.75);
   }

);
