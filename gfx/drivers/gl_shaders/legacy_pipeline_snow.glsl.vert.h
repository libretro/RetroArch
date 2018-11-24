#include "shaders_common.h"

static const char *stock_vertex_xmb_snow_legacy = GLSL(
   attribute vec3 VertexCoord;
   uniform float time;

   void main()
   {
     vec3 v = vec3(VertexCoord.x, 0.0, VertexCoord.y);
     vec3 v2 = v;
     v.y = v.z;
     gl_Position = vec4(VertexCoord.x, VertexCoord.y, VertexCoord.y, 1.0);
   }
);
