#include "../../../common/menu_ribbon_shader.h"

static const char *core_stock_fragment_xmb(void)
{
   static char source[1024];
   if (source[0])
      return source;
   return menu_ribbon_shader(source, sizeof(source),
         "#version 130\n#ifdef GL_ES\nprecision mediump float;\n#endif\nin vec4 fragVertexEc; out vec4 FragColor;",
         "void main() { FragColor = ribbon_fragment(fragVertexEc); }", 2);
}
