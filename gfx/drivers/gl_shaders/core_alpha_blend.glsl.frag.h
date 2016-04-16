static const char *stock_fragment_core_blend =
   "uniform sampler2D Texture;\n"
   "in vec2 tex_coord;\n"
   "in vec4 color;\n"
   "out vec4 FragColor;\n"
   "void main() {\n"
   "   FragColor = color * texture(Texture, tex_coord);\n"
   "}";
