#ifndef _SHADERS_COMMON
#define _SHADERS_COMMON

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
#define CG(src)       "" #src
#define GLSL(src)     GLSL_DERIV_PREAMBLE() GLSL_PREAMBLE() #src
#define GLSL_330(src) "#version 330 es\n" GLSL_PREAMBLE() #src
#else
#define CG(src)         "" #src
#define GLSL(src)       "" GLSL_PREAMBLE() #src
#define GLSL_300(src)   "#version 300 es\n" GLSL_PREAMBLE() #src
#define GLSL_330(src)   "#version 330 core\n" GLSL_PREAMBLE() #src
#endif

#endif
