#ifndef _ANDROID_GENERAL_H
#define _ANDROID_GENERAL_H

#include <android_native_app_glue.h>
#include "../../../boolean.h"

struct droid
{
   struct android_app* app;
   bool init_quit;
   bool window_inited;
};

extern struct droid g_android;

#endif
