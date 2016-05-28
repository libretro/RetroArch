#include "shaders_common.h"

static const char *stock_fragment_legacy = GLSL(
   uniform sampler2D Texture;
   varying vec4 color;

   void main() {
      gl_FragColor = color * texture2D(Texture, gl_TexCoord[0].xy);
   }
);
