#include "shaders_common.h"

static const char *stock_vertex_legacy = GLSL(
   varying vec4 color;

   void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
      gl_TexCoord[0] = gl_MultiTexCoord0;
      color = gl_Color;
   }
);
