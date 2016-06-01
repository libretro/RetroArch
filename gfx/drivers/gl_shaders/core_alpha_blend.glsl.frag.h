#include "shaders_common.h"

static const char *stock_fragment_core_blend = GLSL(
   uniform sampler2D Texture;
   in vec2 tex_coord;
   in vec4 color;
   out vec4 FragColor;

   void main() {
      FragColor = color * texture(Texture, tex_coord);
   }
);
