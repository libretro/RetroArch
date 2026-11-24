#include "../../../common/menu_ribbon_shader.h"

static const char *stock_fragment_xmb(void)
{
   static char source[4096];
   if (source[0])
      return source;
   return menu_ribbon_shader(source, sizeof(source),
         "#define vec2 float2\n#define vec3 float3\n#define vec4 float4\n#define RIBBON_PARAMS\n#define RIBBON_OUT out float4\n",
         "float4 main(float4 vEC : TEXCOORD0) : COLOR { return ribbon_fragment(vEC); }", 2);
}
