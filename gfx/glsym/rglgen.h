#ifndef RGLGEN_H__
#define RGLGEN_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#if defined(IOS)
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif defined(HAVE_PSGL)
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <GLES/glext.h>
#elif defined(HAVE_OPENGL_MODERN)
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>
#elif defined(HAVE_OPENGLES2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#elif defined(HAVE_OPENGLES1)
#include <GLES/gl.h>
#include <GLES/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct rglgen_sym_map;

typedef void (*rglgen_func_t)(void);
typedef rglgen_func_t (*rglgen_proc_address_t)(const char*);
void rglgen_resolve_symbols(rglgen_proc_address_t proc);
void rglgen_resolve_symbols_custom(rglgen_proc_address_t proc,
      const struct rglgen_sym_map *map);

#ifdef __cplusplus
}
#endif

#endif

