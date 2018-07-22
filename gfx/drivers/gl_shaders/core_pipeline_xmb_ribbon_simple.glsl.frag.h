#include "shaders_common.h"

static const char *stock_fragment_xmb_ribbon_simple_core = GLSL(
   uniform float time;
   out vec4 FragColor;

   void main()
   {
     FragColor = vec4(0.05, 0.05, 0.05, 1.0);
   }
);
