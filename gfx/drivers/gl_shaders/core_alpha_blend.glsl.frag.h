#include "shaders_common.h"

static const char *stock_fragment_core_blend = GLSL(
   uniform sampler2D Texture;
   uniform vec4 bgcolor;
   in vec2 tex_coord;
   in vec4 color;
   out vec4 FragColor;

   void main() {
      if (bgcolor.a > 0.0)
         FragColor = bgcolor;
      else
         FragColor = color * texture(Texture, tex_coord);
   }
);
