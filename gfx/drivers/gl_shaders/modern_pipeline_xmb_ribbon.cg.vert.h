#include "../../common/menu_ribbon_shader.h"

static const char *stock_vertex_xmb_ribbon_modern(void)
{
   static char source[4096];
   if (source[0])
      return source;
   return menu_ribbon_shader(source, sizeof(source),
         "#define vec2 float2\n#define vec3 float3\n#define vec4 float4\n#define RIBBON_PARAMS\n#define RIBBON_OUT out float4\nuniform float4 Ribbon[50];\n",
         "void main(float3 VertexCoord, out float4 TexCoord : TEXCOORD0, out float4 oPosition : POSITION) { ribbon_vertex(VertexCoord.xy, oPosition, TexCoord); }", 1);
}
