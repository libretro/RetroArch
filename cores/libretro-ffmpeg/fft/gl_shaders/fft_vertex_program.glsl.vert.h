#include "shaders_common.h"

static const char *fft_vertex_program = GLSL_300(
   precision mediump float;
   layout(location = 0) in vec2 aVertex;
   layout(location = 1) in vec2 aTexCoord;
   uniform vec4 uOffsetScale;
   out vec2 vTex;
   void main() {
      vTex = uOffsetScale.xy + aTexCoord * uOffsetScale.zw;
      gl_Position = vec4(aVertex, 0.0, 1.0);
   }
);
