#include "shaders_common.h"

static const char *stock_fragment_xmb_ribbon_simple = GLSL(
   uniform float time;

   void main()
   {
     gl_FragColor = vec4(0.05, 0.05, 0.05, 1.0);
   }
);
