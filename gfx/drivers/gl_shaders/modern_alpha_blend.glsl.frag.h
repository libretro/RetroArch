#include "shaders_common.h"

static const char *stock_fragment_modern_blend = GLSL(
   uniform sampler2D Texture;
   varying vec2 tex_coord;
   varying vec4 color;
   void main() {
      gl_FragColor = color * texture2D(Texture, tex_coord);
   }
);
