static const char *zahnrad_shader = 
"struct input\n"
"{\n"
"  float time;\n"
"};\n"

"void main_vertex\n"
"(\n"
"	float4 position	: POSITION,\n"
"	float4 color	: COLOR,\n"
"	float2 texCoord : TEXCOORD0,\n"

"    uniform float4x4 modelViewProj,\n"

"	out float4 oPosition : POSITION,\n"
"	out float4 oColor    : COLOR,\n"
"	out float2 otexCoord : TEXCOORD\n"
")\n"
"{\n"
"	oPosition = mul(modelViewProj, position);\n"
"	oColor = color;\n"
"	otexCoord = texCoord;\n"
"}\n"

"struct output \n"
"{\n"
"  float4 color    : COLOR;\n"
"};\n"


"output main_fragment(float2 texCoord : TEXCOORD0, uniform sampler2D Texture : TEXUNIT0, uniform input IN)\n" 
"{\n"
"   output OUT;\n"
"   OUT.color = tex2D(Texture, texCoord);\n"
"   return OUT;\n"
"}\n"
;
