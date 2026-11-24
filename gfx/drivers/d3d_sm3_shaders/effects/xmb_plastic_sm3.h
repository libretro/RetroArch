#include "../../../common/xmb_plastic.h"

#define XMB_PLASTIC_SM3_PREFIX \
   "#define vec2 float2\n#define vec3 float3\n#define vec4 float4\n" \
   "#define mix lerp\n#define XMB_SAMPLE(p) tex2D(s0,p)\n" \
   "#define dFdx ddx\n#define dFdy ddy\n" \
   "#define XMB_COVERAGE(p) XMB_SAMPLE(p).a\n" \
   "sampler2D s0:register(s0);" \
   "float4 main(float2 uv:TEXCOORD0,float4 tint:COLOR0):COLOR0{"
