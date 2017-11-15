#include "shaders_common.h"

static const char *stock_fragment_xmb = GLSL(
   uniform float time;
   varying vec3 fragVertexEc;
   vec3 up = vec3(0, 0, 1);

   void main()
   {
     vec3 X = dFdx(fragVertexEc);
     vec3 Y = dFdy(fragVertexEc);
     vec3 normal=normalize(cross(X,Y));
     float c = (1.0 - dot(normal, up));
     c = (1.0 - cos(c*c))/3.0;
     gl_FragColor = vec4(c, c, c, 1.0);
   }
);
