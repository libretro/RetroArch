#ifndef _ANDROID_GENERAL_H
#define _ANDROID_GENERAL_H

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android_native_app_glue.h>

struct saved_state
{
   float angle;
   int32_t x;
   int32_t y;
};

struct droid
{
   struct android_app* app;

   ASensorManager* sensorManager;
   const ASensor* accelerometerSensor;
   ASensorEventQueue* sensorEventQueue;

   int animating;
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
   int32_t width;
   int32_t height;
   struct saved_state state;
};

extern struct droid g_android;

#endif
