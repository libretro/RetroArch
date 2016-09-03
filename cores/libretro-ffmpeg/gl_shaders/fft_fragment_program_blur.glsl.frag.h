#include "shaders_common.h"

static const char *fft_fragment_program_blur = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;
   in vec2 vTex;
   out vec4 FragColor;
   uniform sampler2D sHeight;

   void main() {
      float k = 0.0;
      float t;
      vec4 res = vec4(0.0);
      \n#define kernel(x, y) t = exp(-0.35 * float((x) * (x) + (y) * (y))); k += t; res += t * textureLodOffset(sHeight, vTex, 0.0, ivec2(x, y))\n
       kernel(-1, -2);
       kernel(-1, -1);
       kernel(-1,  0);
       kernel( 0, -2);
       kernel( 0, -1);
       kernel( 0,  0);
       kernel( 1, -2);
       kernel( 1, -1);
       kernel( 1,  0);
       FragColor = res / k;
   }
);
