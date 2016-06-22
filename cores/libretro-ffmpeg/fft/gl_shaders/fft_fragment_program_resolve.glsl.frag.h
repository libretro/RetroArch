#include "shaders_common.h"

static const char *fft_fragment_program_resolve = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;
   in vec2 vTex;
   out vec4 FragColor;
   uniform usampler2D sFFT;

   vec4 get_heights(highp uvec2 h) {
     vec2 l = unpackHalf2x16(h.x);
     vec2 r = unpackHalf2x16(h.y);
     vec2 channels[4] = vec2[4](
        l, 0.5 * (l + r), r, 0.5 * (l - r));
     vec4 amps;
     for (int i = 0; i < 4; i++)
        amps[i] = dot(channels[i], channels[i]);

     return 9.0 * log(amps + 0.0001) - 22.0;
   }

   void main() {
      uvec2 h = textureLod(sFFT, vTex, 0.0).rg;
      vec4 height = get_heights(h);
      height = (height + 40.0) / 80.0;
      FragColor = height;
   }
);
