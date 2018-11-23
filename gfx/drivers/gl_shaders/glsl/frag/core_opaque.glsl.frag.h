#include "shaders_common.h"

static const char *stock_fragment_core = GLSL(
   uniform sampler2D Texture;
   in vec2 tex_coord;
   out vec4 FragColor;

   void main() {
      FragColor = vec4(texture(Texture, tex_coord).rgb, 1.0);
   }
);
