static const char *stock_fragment_modern_blend =
   "#ifdef GL_ES\n"
   "precision mediump float;\n"
   "#endif\n"
   "uniform sampler2D Texture;\n"
   "varying vec2 tex_coord;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_FragColor = color * texture2D(Texture, tex_coord);\n"
   "}";
