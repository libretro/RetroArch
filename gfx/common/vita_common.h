#ifndef VITA_COMMON_H__
#define VITA_COMMON_H__

#include "../deps/Pigs-In-A-Blanket/include/pib.h"

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#define ATTR_VITA_WIDTH 960
#define ATTR_VITA_HEIGHT 544

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   int native_window;
   bool resize;
   unsigned width, height;
   float refresh_rate;
} vita_ctx_data_t;

#endif
