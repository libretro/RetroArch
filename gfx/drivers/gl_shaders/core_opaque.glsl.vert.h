static const char *stock_vertex_core =
   "in vec2 TexCoord;\n"
   "in vec2 VertexCoord;\n"
   "in vec4 Color;\n"
   "uniform mat4 MVPMatrix;\n"
   "out vec2 tex_coord;\n"
   "void main() {\n"
   "   gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);\n"
   "   tex_coord = TexCoord;\n"
   "}";
