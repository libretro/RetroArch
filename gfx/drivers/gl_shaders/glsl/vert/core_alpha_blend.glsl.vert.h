#include "shaders_common.h"

static const char *stock_vertex_core_blend = GLSL(
   in vec2 TexCoord;
   in vec2 VertexCoord;
   in vec4 Color;
   uniform mat4 MVPMatrix;
   out vec2 tex_coord;
   out vec4 color;

   void main() {
      gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);
      tex_coord = TexCoord;
      color = Color;
   }
);
