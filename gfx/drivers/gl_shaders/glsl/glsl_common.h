#undef GLSL_DERIV_PREAMBLE
#undef GLSL_PREAMBLE
#undef GLSL
#undef GLSL_300
#undef GLSL_330

#define GLSL_DERIV_PREAMBLE() "#extension GL_OES_standard_derivatives : enable\n"
#define GLSL_PREAMBLE() \
   "#ifdef GL_ES\n" \
   "  #ifdef GL_FRAGMENT_PRECISION_HIGH\n" \
   "    precision highp float;\n" \
   "  #else\n" \
   "    precision mediump float;\n" \
   "  #endif\n" \
   "#else\n" \
   "  precision mediump float;\n" \
   "#endif\n" 

#if defined(HAVE_OPENGLES)
#define GLSL(src)     GLSL_DERIV_PREAMBLE() GLSL_PREAMBLE() #src
#define GLSL_330(src) "#version 330 es\n" GLSL_PREAMBLE() #src
#else
#define GLSL(src)       "" GLSL_PREAMBLE() #src
#define GLSL_300(src)   "#version 300 es\n" GLSL_PREAMBLE() #src
#define GLSL_330(src)   "#version 330 core\n" GLSL_PREAMBLE() #src
#endif
