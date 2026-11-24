#ifndef D3D_SHADERS_RIBBON_SM3_HLSL_H
#define D3D_SHADERS_RIBBON_SM3_HLSL_H

#include "../../../common/menu_ribbon_shader.h"

static const char *hlsl_ribbon_program(void)
{
   static char ribbon[4096];
   if (ribbon[0])
      return ribbon;
   return menu_ribbon_shader(ribbon, sizeof(ribbon),
      "#define vec2 float2\n#define vec3 float3\n#define vec4 float4\n#define RIBBON_PARAMS\n#define RIBBON_OUT out float4\nuniform float4 Ribbon[50];\n",
      "void main_vertex(float2 p : POSITION, out float4 position : POSITION, out float4 vEC : TEXCOORD0) { ribbon_vertex(p, position, vEC); }\nfloat4 main_fragment(float4 vEC : TEXCOORD0) : COLOR { return ribbon_fragment(vEC); }", 3)
;
}

#endif
