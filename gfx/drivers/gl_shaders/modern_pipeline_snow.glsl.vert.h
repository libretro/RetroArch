#include "shaders_common.h"

static const char *stock_vertex_xmb_snow = GLSL(
   in vec3 VertexCoord;
   uniform float time;

   void main()
   {
     vec3 v = vec3(VertexCoord.x, 0.0, VertexCoord.y);
     gl_Position = vec4(v, 1.0);
   }
);
