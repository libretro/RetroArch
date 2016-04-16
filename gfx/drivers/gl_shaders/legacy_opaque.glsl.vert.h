static const char *stock_vertex_legacy =
   "varying vec4 color;\n"
   "void main() {\n"
   "   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
   "   gl_TexCoord[0] = gl_MultiTexCoord0;\n"
   "   color = gl_Color;\n"
   "}";
