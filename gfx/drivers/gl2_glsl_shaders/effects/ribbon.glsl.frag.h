#include "../../../common/menu_ribbon_shader.h"

static const char *stock_fragment_xmb(void)
{
   static char source[1024];
   if (source[0])
      return source;
   return menu_ribbon_shader(source, sizeof(source),
         "#ifdef GL_ES\nprecision mediump float;\n#endif\nvarying vec4 fragVertexEc;",
         "void main() { gl_FragColor = ribbon_fragment(fragVertexEc); }", 2);
}
