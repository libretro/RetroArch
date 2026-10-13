/* D3D9's views pixel shaders, shader model 2, which take HLSL and Cg
 * alike. Each is main_fragment, its one constant views_c. */

/* A view of the frame. views_c: its first and last texel centres, so
 * filtering stops at its edge as at a texture's. */
static const char d3d9_views_ps_rect[] =
   "uniform sampler2D s0;\n"
   "uniform float4 views_c;\n"
   "float4 main_fragment(float2 uv : TEXCOORD0) : COLOR\n"
   "{\n"
   "   return float4(tex2D(s0, clamp(uv, views_c.xy, views_c.zw)).rgb,\n"
   "         1.0);\n"
   "}\n";

/* The canvas, its left half the left eye: views_c.x 0 for anaglyph, by
 * Dubois' least-squares red/cyan matrices, 1 for even rows from the
 * left half and odd from the right; views_c.y its height. */
static const char d3d9_views_ps_blend[] =
   "uniform sampler2D s0;\n"
   "uniform float4 views_c;\n"
   "float4 main_fragment(float2 uv : TEXCOORD0) : COLOR\n"
   "{\n"
   "   float2 l_uv = float2(uv.x * 0.5, uv.y);\n"
   "   float3 l    = tex2D(s0, l_uv).rgb;\n"
   "   float3 r    = tex2D(s0, l_uv + float2(0.5, 0.0)).rgb;\n"
   "   float  odd  = fmod(floor(uv.y * views_c.y), 2.0);\n"
   "   float3 c    = float3(\n"
   "         dot(l, float3( 0.456100,  0.500484,  0.176381))\n"
   "       + dot(r, float3(-0.0434706, -0.0879388, -0.00155529)),\n"
   "         dot(l, float3(-0.0400822, -0.0378246, -0.0157589))\n"
   "       + dot(r, float3( 0.378476,   0.73364,   -0.0184503)),\n"
   "         dot(l, float3(-0.0152161, -0.0205971, -0.00546856))\n"
   "       + dot(r, float3(-0.0721527, -0.112961,   1.2264)));\n"
   "   float3 row  = lerp(l, r, odd);\n"
   "   return float4(lerp(saturate(c), row, views_c.x), 1.0);\n"
   "}\n";

/* The UI layer, premultiplied */
static const char d3d9_views_ps_layer[] =
   "uniform sampler2D s0;\n"
   "float4 main_fragment(float2 uv : TEXCOORD0) : COLOR\n"
   "{\n"
   "   return tex2D(s0, uv);\n"
   "}\n";
