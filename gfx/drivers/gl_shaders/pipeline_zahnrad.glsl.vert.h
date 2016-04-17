static const char *zahnrad_vertex_shader =
"#version 300 es\n"
"uniform mat4 ProjMtx;\n"
"in vec2 Position;\n"
"in vec2 TexCoord;\n"
"in vec4 Color;\n"
"out vec2 Frag_UV;\n"
"out vec4 Frag_Color;\n"
"void main() {\n"
"   Frag_UV = TexCoord;\n"
"   Frag_Color = Color;\n"
"   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
"}\n";
