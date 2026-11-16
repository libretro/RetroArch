#include "../../common/menu_ribbon_shader.h"

static const char *stock_xmb_ribbon(void)
{
   static char source[4096];
   if (source[0])
      return source;
   return menu_ribbon_shader(source, sizeof(source),
         "#define vec2 float2\n#define vec3 float3\n#define vec4 float4\n#define RIBBON_PARAMS\n#define RIBBON_OUT out float4\nuniform float4 Ribbon[50];\n",
         "void main_vertex(float2 p : POSITION, out float4 position : POSITION, out float4 vEC : TEXCOORD0) { ribbon_vertex(p, position, vEC); }\nfloat4 main_fragment(float4 vEC : TEXCOORD0) : COLOR { return ribbon_fragment(vEC); }", 3);
}
