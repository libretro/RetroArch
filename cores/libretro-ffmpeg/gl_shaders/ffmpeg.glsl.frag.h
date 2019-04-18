#include "shaders_common.h"

static const char *fragment_source = GLSL(
      varying vec2 vTex;
      uniform sampler2D sTex0;
      uniform sampler2D sTex1;
      uniform float uMix;

      void main() {
         gl_FragColor = vec4(pow(mix(pow(texture2D(sTex0, vTex).rgb, vec3(2.2)), pow(texture2D(sTex1, vTex).rgb, vec3(2.2)), uMix), vec3(1.0 / 2.2)), 1.0);
     }
);
