#include "shaders_common.h"

static const char *vertex_source = GLSL(
      attribute vec2 aVertex;
      attribute vec2 aTexCoord;
      varying vec2 vTex;

      void main() {
         gl_Position = vec4(aVertex, 0.0, 1.0); vTex = aTexCoord;
      }
);
