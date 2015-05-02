/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PLATFORM_ANDROID_H
#define _PLATFORM_ANDROID_H

#include <jni.h>

#include "android_native_app_glue.h"
#include <android/window.h>
#include <android/sensor.h>

#ifndef MAX_AXIS
#define MAX_AXIS 10
#endif

#ifndef MAX_PADS
#define MAX_PADS 8
#endif

#ifndef MAX_TOUCH
#define MAX_TOUCH 16
#endif

#ifndef AKEYCODE_ASSIST
#define AKEYCODE_ASSIST 219
#endif

#define LAST_KEYCODE AKEYCODE_ASSIST

typedef struct state_device
{
   int id;
   int port;
   char name[256];
} state_device_t;

struct input_pointer
{
   int16_t x, y;
   int16_t full_x, full_y;
};

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

typedef struct android_input_state
{
   int16_t analog_state[MAX_PADS][MAX_AXIS];
   int8_t hat_state[MAX_PADS][2];
   uint8_t pad_state[MAX_PADS][(LAST_KEYCODE + 7) / 8];
   unsigned pads_connected;
   state_device_t pad_states[MAX_PADS];
   struct input_pointer pointer[MAX_TOUCH];
   sensor_t accelerometer_state;
   unsigned pointer_count;
} android_input_state_t;

struct android_app_userdata
{
   unsigned accelerometer_event_rate;
   const ASensor* accelerometerSensor;
   uint64_t sensor_state_mask;
   char current_ime[PATH_MAX_LENGTH];
   jmethodID getIntent;
   jmethodID getStringExtra;
   jmethodID clearPendingIntent;
   jmethodID hasPendingIntent;
   jmethodID getPendingIntentConfigPath;
   jmethodID getPendingIntentLibretroPath;
   jmethodID getPendingIntentFullPath;
   jmethodID getPendingIntentIME;
   android_input_state_t thread_state;
   ASensorManager *sensorManager;
   ASensorEventQueue *sensorEventQueue;
};

#define JNI_EXCEPTION(env) \
   if ((*env)->ExceptionOccurred(env)) \
   { \
      (*env)->ExceptionDescribe(env); \
      (*env)->ExceptionClear(env); \
   }

#define FIND_CLASS(env, var, classname) \
   var = (*env)->FindClass(env, classname); \
   JNI_EXCEPTION(env)

#define GET_OBJECT_CLASS(env, var, clazz_obj) \
   var = (*env)->GetObjectClass(env, clazz_obj); \
   JNI_EXCEPTION(env)

#define GET_FIELD_ID(env, var, clazz, fieldName, fieldDescriptor) \
   var = (*env)->GetFieldID(env, clazz, fieldName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define GET_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   var = (*env)->GetMethodID(env, clazz, methodName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define GET_STATIC_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   var = (*env)->GetStaticMethodID(env, clazz, methodName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_STATIC_METHOD(env, var, clazz, methodId) \
   var = (*env)->CallStaticObjectMethod(env, clazz, methodId); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_STATIC_METHOD_PARAM(env, var, clazz, methodId, ...) \
   var = (*env)->CallStaticObjectMethod(env, clazz, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_VOID_METHOD(env, clazz_obj, methodId) \
   (*env)->CallVoidMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_VOID_METHOD_PARAM(env, clazz_obj, methodId, ...) \
   (*env)->CallVoidMethod(env, clazz_obj, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_BOOLEAN_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallBooleanMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_DOUBLE_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallDoubleMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_INT_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallIntMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

bool (*engine_lookup_name)(char *buf,
      int *vendorId, int *productId, size_t size, int id);

JNIEnv *jni_thread_getenv(void);

void android_app_write_cmd(struct android_app *android_app, int8_t cmd);

extern struct android_app *g_android;
extern struct android_app_userdata *g_android_userdata;

void android_main(struct android_app *android_app);

int android_main_poll(void *data);

#endif /* _PLATFORM_ANDROID_H */
