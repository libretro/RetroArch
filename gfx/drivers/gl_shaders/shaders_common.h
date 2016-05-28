#ifndef _SHADERS_COMMON
#define _SHADERS_COMMON

#if defined(HAVE_OPENGLES)
#define GLSL(src) "precision mediump float;\n" #src
#define GLSL_330_ES(src)   "#version 330 es\nprecision mediump float;\n"   #src
#define GLSL_330_CORE(src) "#version 330 core\nprecision mediump float;\n" #src
#else
#define GLSL(src) "" #src
#define GLSL_330_ES(src)   "#version 330 es\n"   #src
#define GLSL_330_CORE(src) "#version 330 core\n" #src
#endif

#endif