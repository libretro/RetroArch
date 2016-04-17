static const char *zahnrad_fragment_shader =
"#version 300 es\n"
"precision mediump float;\n"
"uniform sampler2D Texture;\n"
"in vec2 Frag_UV;\n"
"in vec4 Frag_Color;\n"
"out vec4 Out_Color;\n"
"void main(){\n"
"   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
"}\n";
