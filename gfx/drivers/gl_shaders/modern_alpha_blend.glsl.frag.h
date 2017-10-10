#include "shaders_common.h"

static const char *stock_fragment_modern_blend = GLSL(
   uniform sampler2D Texture;
   uniform vec4 bgcolor;
   varying vec2 tex_coord;
   varying vec4 color;
   void main() {
      if (bgcolor == vec4(0.0,0.0,1.0,1.0))
      {
         gl_FragColor = bgcolor;
      }
      else
      {
         gl_FragColor = color * texture2D(Texture, tex_coord);
      }
   }
);
