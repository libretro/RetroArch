#include "shaders_common.h"

static const char *stock_vertex_xmb_modern = GLSL(
   in vec3 VertexCoord;
   uniform float time;
   out vec3 fragVertexEc;

   float iqhash( float n )
   {
     return fract(sin(n)*43758.5453);
   }

   float noise( vec3 x )
   {
     vec3 p = floor(x);
     vec3 f = fract(x);
     f = f*f*(3.0-2.0*f);
     float n = p.x + p.y*57.0 + 113.0*p.z;
     return mix(mix(mix( iqhash(n+0.0 ), iqhash(n+1.0 ),f.x),
     mix( iqhash(n+57.0 ), iqhash(n+58.0 ),f.x),f.y),
     mix(mix( iqhash(n+113.0), iqhash(n+114.0),f.x),
     mix( iqhash(n+170.0), iqhash(n+171.0),f.x),f.y),f.z);
   }

   float xmb_noise2( vec3 x )
   {
     return cos((x.z*1.0)*2.0);
   }

   void main()
   {
     vec3 v = vec3(VertexCoord.x, 0.0, VertexCoord.y);
     vec3 v2 = v;
     vec3 v3 = v;

     v.y = xmb_noise2(v2)/6.0;

     v3.x = v3.x + time/5.0;
     v3.x = v3.x / 2.0;

     v3.z = v3.z + time/10.0;
     v3.y = v3.y + time/100.0;

     v.z = v.z + noise(v3*7.0)/15.0;
     v.y = v.y + noise(v3*7.0)/15.0 + cos(v.x*2.0-time/5.0)/5.0 - 0.3;

     gl_Position = vec4(v, 1.0);
     fragVertexEc = gl_Position.xyz;
   }
);
