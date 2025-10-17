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
#define GLSL_STANDARD_DERIVATIVES(src) "#version 130\n" \
                  "#extension GL_OES_standard_derivatives : enable\n" \
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
#define GLSL_STANDARD_DERIVATIVES(src) "" #src
#endif

#endif
