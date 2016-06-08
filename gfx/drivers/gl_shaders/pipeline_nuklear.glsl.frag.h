#include "shaders_common.h"

static const char *nuklear_fragment_shader = GLSL_300(
   precision mediump float;
   uniform sampler2D Texture;
   in vec2 Frag_UV;
   in vec4 Frag_Color;
   out vec4 Out_Color;

   void main(){
      Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
   }
);
