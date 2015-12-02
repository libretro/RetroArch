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

#ifndef _PLATFORM_LINUX_H
#define _PLATFORM_LINUX_H

#include <stdint.h>
#include <sys/cdefs.h>

#include <boolean.h>

typedef enum
{
   CPU_FAMILY_UNKNOWN = 0,
   CPU_FAMILY_ARM,
   CPU_FAMILY_X86,
   CPU_FAMILY_MIPS,

   CPU_FAMILY_MAX  /* do not remove */
} cpu_family;

enum
{
   CPU_ARM_FEATURE_ARMv7       = (1 << 0),
   CPU_ARM_FEATURE_VFPv3       = (1 << 1),
   CPU_ARM_FEATURE_NEON        = (1 << 2),
   CPU_ARM_FEATURE_LDREX_STREX = (1 << 3)
};

enum
{
   CPU_X86_FEATURE_SSSE3       = (1 << 0),
   CPU_X86_FEATURE_POPCNT      = (1 << 1),
   CPU_X86_FEATURE_MOVBE       = (1 << 2)
};

cpu_family   linux_get_cpu_family(void);

uint64_t    linux_get_cpu_features(void);

int         linux_get_cpu_count(void);

#ifdef ANDROID
#include <jni.h>
#include <poll.h>
#include <sched.h>

#include <android/looper.h>
#include <android/configuration.h>
#include <android/native_activity.h>
#include <android/window.h>
#include <android/sensor.h>

#include <rthreads/rthreads.h>

bool test_permissions(const char *path);

char sdcard_dir[PATH_MAX_LENGTH];

struct android_app
{
   /* The ANativeActivity object instance that this app is running in. */
   ANativeActivity* activity;

   /* The current configuration the app is running in. */
   AConfiguration *config;

   /* This is the last instance's saved state, as provided at creation time.
    * It is NULL if there was no state.  You can use this as you need; the
    * memory will remain around until you call android_app_exec_cmd() for
    * APP_CMD_RESUME, at which point it will be freed and savedState set to NULL.
    * These variables should only be changed when processing a APP_CMD_SAVE_STATE,
    * at which point they will be initialized to NULL and you can malloc your
    * state and place the information here.  In that case the memory will be
    * freed for you later.
    */
   void* savedState;
   size_t savedStateSize;

   /* The ALooper associated with the app's thread. */
   ALooper* looper;

   /* When non-NULL, this is the input queue from which the app will
    * receive user input events. */
   AInputQueue* inputQueue;

   AInputQueue* pendingInputQueue;

   /* When non-NULL, this is the window surface that the app can draw in. */
   ANativeWindow* window;

   ANativeWindow* pendingWindow;

   /* This is non-zero when the application's NativeActivity is being
    * destroyed and waiting for the app thread to complete. */
   int destroyRequested;

   slock_t *mutex;
   scond_t *cond;

   /* Current state of the app's activity.  May be either APP_CMD_START,
    * APP_CMD_RESUME, APP_CMD_PAUSE, or APP_CMD_STOP; see below. */
   int activityState;

   int msgread;
   int msgwrite;

   sthread_t *thread;

   int running;
   int stateSaved;
   int destroyed;

   bool unfocused;
   unsigned accelerometer_event_rate;
   const ASensor* accelerometerSensor;
   uint64_t sensor_state_mask;
   char current_ime[PATH_MAX_LENGTH];
   jmethodID getIntent;
   jmethodID onRetroArchExit;
   jmethodID getStringExtra;
   jmethodID clearPendingIntent;
   jmethodID hasPendingIntent;
   jmethodID getPendingIntentConfigPath;
   jmethodID getPendingIntentLibretroPath;
   jmethodID getPendingIntentFullPath;
   jmethodID getPendingIntentIME;
   jmethodID getPendingIntentStorageLocation;
   jmethodID getPendingIntentDownloadsLocation;
   jmethodID getPendingIntentScreenshotsLocation;
};

enum
{
   LOOPER_ID_MAIN = 1,
   LOOPER_ID_INPUT,
   LOOPER_ID_USER,
   LOOPER_ID_INPUT_MSG
};

enum
{
   APP_CMD_INPUT_CHANGED,
   /**
    * Command from main thread: a new ANativeWindow is ready for use.  Upon
    * receiving this command, android_app->window will contain the new window
    * surface.
    */
   APP_CMD_INIT_WINDOW,

   /**
    * Command from main thread: the existing ANativeWindow needs to be
    * terminated.  Upon receiving this command, android_app->window still
    * contains the existing window; after calling android_app_exec_cmd
    * it will be set to NULL.
    */
   APP_CMD_TERM_WINDOW,

   /**
    * Command from main thread: the current ANativeWindow has been resized.
    * Please redraw with its new size.
    */
   APP_CMD_WINDOW_RESIZED,

   /**
    * Command from main thread: the system needs that the current ANativeWindow
    * be redrawn.  You should redraw the window before handing this to
    * android_app_exec_cmd() in order to avoid transient drawing glitches.
    */
   APP_CMD_WINDOW_REDRAW_NEEDED,

   /**
    * Command from main thread: the content area of the window has changed,
    * such as from the soft input window being shown or hidden.  You can
    * find the new content rect in android_app::contentRect.
    */
   APP_CMD_CONTENT_RECT_CHANGED,

   /**
    * Command from main thread: the app's activity window has gained
    * input focus.
    */
   APP_CMD_GAINED_FOCUS,

   /**
    * Command from main thread: the app's activity window has lost
    * input focus.
    */
   APP_CMD_LOST_FOCUS,

   /**
    * Command from main thread: the current device configuration has changed.
    */
   APP_CMD_CONFIG_CHANGED,

   /**
    * Command from main thread: the system is running low on memory.
    * Try to reduce your memory use.
    */
   APP_CMD_LOW_MEMORY,

   /**
    * Command from main thread: the app's activity has been started.
    */
   APP_CMD_START,

   /**
    * Command from main thread: the app's activity has been resumed.
    */
   APP_CMD_RESUME,

   /**
    * Command from main thread: the app should generate a new saved state
    * for itself, to restore from later if needed.  
    */
   APP_CMD_SAVE_STATE,

   /**
    * Command from main thread: the app's activity has been paused.
    */
   APP_CMD_PAUSE,

   /**
    * Command from main thread: the app's activity has been stopped.
    */
   APP_CMD_STOP,

   /**
    * Command from main thread: the app's activity is being destroyed,
    * and waiting for the app thread to clean up and exit before proceeding.
    */
   APP_CMD_DESTROY,

   // Set by thread when it will no longer reply to commands.
   APP_CMD_DEAD
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

extern JNIEnv *jni_thread_getenv(void);

extern struct android_app *g_android;
#else
#endif

#endif
