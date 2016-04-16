static const char *stock_fragment_legacy =
   "uniform sampler2D Texture;\n"
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_FragColor = color * texture2D(Texture, gl_TexCoord[0].xy);\n"
   "}";
