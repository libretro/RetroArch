#include "../../common/menu_ribbon_shader.h"

static const char *stock_vertex_xmb_ribbon_modern(void)
{
   static char source[4096];
   if (source[0])
      return source;
   return menu_ribbon_shader(source, sizeof(source),
         "#version 130\n#define RIBBON_PARAMS\n#define RIBBON_OUT out vec4\nin vec3 VertexCoord;\nout vec4 fragVertexEc;\nuniform vec4 Ribbon[50];\n",
         "void main() { ribbon_vertex(VertexCoord.xy, gl_Position, fragVertexEc); }", 1);
}
