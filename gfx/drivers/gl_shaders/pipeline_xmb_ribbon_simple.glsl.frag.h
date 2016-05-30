#include "shaders_common.h"

static const char *stock_fragment_xmb_simple = GLSL(
   uniform float time;

   void main()
   {
     gl_FragColor = vec4(1.0, 1.0, 1.0, 0.05);
   }
);
