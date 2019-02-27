#ifndef ORBIS_COMMON_H__
#define ORBIS_COMMON_H__

#ifdef HAVE_EGL
#include <piglet.h>
#include "../common/egl_common.h"
#endif

#define ATTR_ORBISGL_WIDTH 1920
#define ATTR_ORBISGL_HEIGHT 1080

typedef struct
{
#ifdef HAVE_EGL
    egl_ctx_data_t egl;
    ScePglConfig pgl_config;
#endif

    SceWindow native_window;
    bool resize;
    unsigned width, height;
    float refresh_rate;
} orbis_ctx_data_t;

#endif
