#ifndef _SHADERS_COMMON
#define _SHADERS_COMMON

#if defined(HAVE_OPENGLES)
#define CG(src)   "" #src
#define GLSL(src) "#extension GL_OES_standard_derivatives : enable\n" \
                  "#ifdef GL_ES\n" \
                  "  #ifdef GL_FRAGMENT_PRECISION_HIGH\n" \
                  "    precision highp float;\n" \
                  "  #else\n" \
                  "    precision mediump float;\n" \
                  "  #endif\n" \
                  "#else\n" \
                  "  precision mediump float;\n" \
                  "#endif\n" #src
#define GLSL_330(src) "#version 330 es\n" \
                  "#ifdef GL_ES\n" \
                  "  #ifdef GL_FRAGMENT_PRECISION_HIGH\n" \
                  "    precision highp float;\n" \
                  "  #else\n" \
                  "    precision mediump float;\n" \
                  "  #endif\n" \
                  "#else\n" \
                  "  precision mediump float;\n" \
                  "#endif\n" #src
#else
#define CG(src)   "" #src
#define GLSL(src) "" #src
#define GLSL_300(src)   "#version 300 es\n"   #src
#define GLSL_330(src)   "#version 330 core\n"   #src
#endif

#endif
