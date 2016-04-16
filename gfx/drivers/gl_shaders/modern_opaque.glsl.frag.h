static const char *stock_fragment_modern =
   "#ifdef GL_ES\n"
   "precision mediump float;\n"
   "#endif\n"
   "uniform sampler2D Texture;\n"
   "varying vec2 tex_coord;\n"
   "void main() {\n"
   "   gl_FragColor = vec4(texture2D(Texture, tex_coord).rgb, 1.0);\n"
   "}";
