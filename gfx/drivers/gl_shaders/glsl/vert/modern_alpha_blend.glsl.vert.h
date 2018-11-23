#include "shaders_common.h"

static const char *stock_vertex_modern_blend = GLSL(
   attribute vec2 TexCoord;
   attribute vec2 VertexCoord;
   attribute vec4 Color;
   uniform mat4 MVPMatrix;
   varying vec2 tex_coord;
   varying vec4 color;

   void main() {
      gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);
      tex_coord = TexCoord;
      color = Color;
   }
);
