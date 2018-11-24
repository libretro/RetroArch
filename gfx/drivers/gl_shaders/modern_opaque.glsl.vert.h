#include "shaders_common.h"

/* Need to duplicate these to work around broken stuff on Android.
 * Must enforce alpha = 1.0 or 32-bit games can potentially go black. */
static const char *stock_vertex_modern = GLSL(
   attribute vec2 TexCoord;
   attribute vec2 VertexCoord;
   attribute vec4 Color;
   uniform mat4 MVPMatrix;
   varying vec2 tex_coord;

   void main() {
      gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);
      tex_coord = TexCoord;
   }
);
