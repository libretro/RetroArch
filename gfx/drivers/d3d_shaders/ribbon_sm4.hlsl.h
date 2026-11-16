ribbon[0] ? ribbon : menu_ribbon_shader(ribbon, sizeof(ribbon),
      "#define vec2 float2\n#define vec3 float3\n#define vec4 float4\n#define RIBBON_PARAMS\n#define RIBBON_OUT out float4\ncbuffer UBO { float4x4 modelViewProj; float2 OutputSize; float time; float alpha; float4 Ribbon[50]; };\n",
      "void VSMain(float2 p : POSITION, out float4 position : SV_POSITION, out float4 vEC : TEXCOORD0) { ribbon_vertex(p, position, vEC); }\nfloat4 PSMain(float4 pos : SV_POSITION, float4 vEC : TEXCOORD0) : SV_TARGET { return ribbon_fragment(vEC); }", 3)
