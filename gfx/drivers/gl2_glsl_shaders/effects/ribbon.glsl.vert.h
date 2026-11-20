#include "../../../common/menu_ribbon_shader.h"

static const char *stock_vertex_xmb_ribbon_legacy(void)
{
   static char source[4096];
   if (source[0])
      return source;
   return menu_ribbon_shader(source, sizeof(source),
         "#define RIBBON_PARAMS\n#define RIBBON_OUT out vec4\nattribute vec3 VertexCoord;\nvarying vec4 fragVertexEc;\nuniform vec4 Ribbon[50];\n",
         "void main() { ribbon_vertex(VertexCoord.xy, gl_Position, fragVertexEc); }", 1);
}
