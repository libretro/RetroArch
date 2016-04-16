static const char *stock_fragment_core =
   "uniform sampler2D Texture;\n"
   "in vec2 tex_coord;\n"
   "out vec4 FragColor;\n"
   "void main() {\n"
   "   FragColor = vec4(texture(Texture, tex_coord).rgb, 1.0);\n"
   "}";
