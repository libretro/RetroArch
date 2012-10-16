#ifndef _ANDROID_GENERAL_H
#define _ANDROID_GENERAL_H

#include <android/sensor.h>
#include <android_native_app_glue.h>
#include "../../../boolean.h"

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

   bool window_inited;
   unsigned animating;
   unsigned width;
   unsigned height;
   struct saved_state state;
};

extern struct droid g_android;

#endif
