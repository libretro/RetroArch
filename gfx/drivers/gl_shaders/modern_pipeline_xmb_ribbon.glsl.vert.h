#include "shaders_common.h"

static const char *stock_vertex_xmb_ribbon_modern = GLSL(
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

  float height(vec3 pos )
  {
    const float twoPi = 2.0 * 3.14159;
    float k = twoPi / 20.0;
    float omega = twoPi / 15.0;
    float y = sin( k * pos.x - omega * time );
    y += noise( vec3(0.27) * vec3( 0.4 * pos.x, 3.0, 2.0 * pos.z - 0.5 * time ) );
    return y;
  }

   void main()
   {
    vec3 pos = VertexCoord;
    pos.y = height( pos );
    gl_Position = vec4(pos, 1.0);
    fragVertexEc =pos;
   }
);
