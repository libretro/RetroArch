#include "shaders_common.h"

static const char *stock_fragment_modern = GLSL(
   #ifdef GL_ES
   precision mediump float;
   #endif
   uniform sampler2D Texture;
   varying vec2 tex_coord;

   void main() {
      gl_FragColor = vec4(texture2D(Texture, tex_coord).rgb, 1.0);
   }
);
