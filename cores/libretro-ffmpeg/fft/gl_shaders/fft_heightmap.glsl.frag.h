#include "shaders_common.h"

static const char *fft_fragment_program_heightmap = GLSL_300(
   precision mediump float;
   out vec4 FragColor;
   in vec3 vWorldPos;
   in vec3 vHeight;

   vec3 colormap(vec3 height) {
      return 1.0 / (1.0 + exp(-0.08 * height));
   }

   void main() {
      vec3 color = mix(vec3(1.0, 0.7, 0.7) * colormap(vHeight), vec3(0.1, 0.15, 0.1), clamp(vWorldPos.z / 400.0, 0.0, 1.0));
      color = mix(color, vec3(0.1, 0.15, 0.1), clamp(1.0 - vWorldPos.z / 2.0, 0.0, 1.0));
      FragColor = vec4(color, 1.0);
   }
);
