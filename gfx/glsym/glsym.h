#ifndef GLSYM_H__
#define GLSYM_H__

#include "rglgen.h"

#ifndef HAVE_PSGL
#ifdef HAVE_OPENGLES2
#include "glsym_es2.h"
#else
#include "glsym_gl.h"
#endif
#endif

#endif

