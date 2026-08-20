/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _PLATFORM_UNIX_H
#define _PLATFORM_UNIX_H

#include <stdint.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "../../config.def.h"

#ifndef MAX_AXIS
#define MAX_AXIS 10
#endif

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
#include <retro_atomic.h>

/* struct android_app below embeds retro_atomic_int_t, which is
 * atomic_int under C and std::atomic<int> under C++. If this header were
 * ever pulled into a C++ translation unit the struct layout would differ
 * between that TU and the C ones, silently. Nothing includes it from C++
 * today (griffin.c pulls platform_unix.c and android_input.c into a C
 * TU); fail the build rather than let that change go unnoticed. */
#if defined(__cplusplus)
#error "platform_unix.h is C-only under ANDROID: struct android_app carries retro_atomic_int_t, whose layout differs between C and C++."
#endif

#include "../../config.def.h"

bool test_permissions(const char *path);

char internal_storage_path[PATH_MAX_LENGTH];
char internal_storage_app_path[PATH_MAX_LENGTH];

struct android_app;

struct android_poll_source
{
   /* The identifier of this source.  May be LOOPER_ID_MAIN or
    * LOOPER_ID_INPUT. */
   int32_t id;

   /* The android_app this ident is associated with. */
   struct android_app* app;

   /* Function to call to perform the standard processing of data from
    * this source. */
   void (*process)(struct android_app* app, struct android_poll_source* source);
};

#define PLAT_ANDROID_PERM_RESOLVED (1 << 0)
#define PLAT_ANDROID_PERM_GRANTED  (1 << 1)

struct android_app
{
   /* The application can place a pointer to its own state object
    * here if it likes. */
   void* userData;

   /* Fill this in with the function to process main app commands (APP_CMD_*) */
   void (*onAppCmd)(struct android_app* app, int32_t cmd);

   /* Fill this in with the function to process input events.  At this point
    * the event has already been pre-dispatched, and it will be finished upon
    * return.  Return 1 if you have handled the event, 0 for any default
    * dispatching. */
   int32_t (*onInputEvent)(struct android_app* app, AInputEvent* event);

   /* The ANativeActivity object instance that this app is running in. */
   ANativeActivity* activity;

   /* The current configuration the app is running in. */
   AConfiguration *config;

   /* The last instance's saved state, as provided at creation time, or
    * NULL if there was none. RetroArch never produces a saved state of
    * its own - onSaveInstanceState() returns nothing - so this is only
    * ever the create-time blob, held until it is freed on teardown.
    */
   void* savedState;
   size_t savedStateSize;

   /* The ALooper associated with the app's thread. */
   ALooper* looper;

   /* When non-NULL, this is the input queue from which the app will
    * receive user input events. */
   AInputQueue* inputQueue;

   /* When non-NULL, this is the window surface that the app can draw in. */
   ANativeWindow* window;

   /* Current state of the app's activity.  May be either APP_CMD_START,
    * APP_CMD_RESUME, APP_CMD_PAUSE, or APP_CMD_STOP; see below. */
   int activityState;

   int reinitRequested;

   /* This is non-zero when the application's NativeActivity is being
    * destroyed and waiting for the app thread to complete. */
   int destroyRequested;

   /* Below are "private" implementation of the glue code. */
   slock_t *mutex;
   scond_t *cond;

   int msgread;
   int msgwrite;

   /* Startup storage-permission gate.  Written from the Java UI
    * thread via permissionsResolved() under 'mutex'; read by the
    * native thread in frontend_unix_init before any filesystem-
    * dependent startup work runs. */
   unsigned permission_state;

   /* Completion counters for the lifecycle commands a Java callback
    * blocks on: APP_CMD_INIT_WINDOW, APP_CMD_TERM_WINDOW and
    * APP_CMD_INPUT_CHANGED.  The UI thread takes a ticket from
    * 'cmd_seq' for every such command it posts and waits for
    * 'done_seq' to reach it; the app thread advances 'done_seq' once
    * it has finished acting on one.  Both are written only under
    * 'mutex', and both are compared wrap-safely rather than for
    * equality, so neither needs to be atomic or reset.
    *
    * A waiter must key on these rather than on the window or input
    * queue handle it asked for: the framework reuses those addresses,
    * so a handle comparison can already hold when the command is still
    * queued and lets the callback return while the app thread is about
    * to tear the surface down behind it. */
   unsigned cmd_seq;
   unsigned done_seq;

   sthread_t *thread;

   struct android_poll_source cmdPollSource;
   struct android_poll_source inputPollSource;

   int running;
   int destroyed;

   /* Set by android_app_free() before it asks the app thread to shut
    * down, so android_app_destroy() knows the activity is already being
    * torn down by the framework and must not call finish() on it. */
   int destroy_from_framework;
   AInputQueue* pendingInputQueue;
   ANativeWindow* pendingWindow;

   /*  Below are "private" implementation of RA code. */
   /* Written by the app thread on APP_CMD_GAINED_FOCUS/LOST_FOCUS, read
    * by the video thread in dispserv_android.c without the mutex. */
   retro_atomic_int_t unfocused;
   unsigned accelerometer_event_rate;
   unsigned gyroscope_event_rate;
   ASensorManager *sensorManager;
   ASensorEventQueue *sensorEventQueue;
   const ASensor* accelerometerSensor;
   const ASensor* gyroscopeSensor;
   uint64_t sensor_state_mask;
   unsigned detected_screen_rotation;
   float    gravity_accum_x;
   float    gravity_accum_y;
   unsigned gravity_sample_count;
   bool     gravity_calibrated;
   char current_ime[NAME_MAX_LENGTH];
   bool input_alive;
   int16_t analog_state[DEFAULT_MAX_PADS][MAX_AXIS];
   int8_t hat_state[DEFAULT_MAX_PADS][2];
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
   jmethodID isAndroidTV;
   jmethodID getRefreshRate;
   jmethodID getDisplayModes;
   jmethodID getCurrentDisplayModeId;
   jmethodID setDisplayModeId;
   jmethodID getPowerstate;
   jmethodID getBatteryLevel;
   jmethodID setSustainedPerformanceMode;
   jmethodID setWindowSettings;
   jmethodID setScreenOrientation;
   jmethodID getUserLanguageString;
   jmethodID doVibrate;
   jmethodID doVibrateJoypad;
   jmethodID doVibrateUSB;
   jmethodID doHapticFeedback;

   jmethodID isPlayStoreBuild;
   jmethodID getAvailableCores;
   jmethodID getInstalledCores;
   jmethodID downloadCore;
   jmethodID deleteCore;

   jmethodID getVolumeCount;
   jmethodID getVolumePath;
   jmethodID inputGrabMouse;

   jmethodID isScreenReaderEnabled;
   jmethodID accessibilitySpeak;

   jmethodID showKeyboard;
   jmethodID hideKeyboard;

   /* Written by the Android UI thread in onContentRectChanged(), read by
    * the video thread in the context drivers, with no lock on either
    * side. Publication is ordered: the dimensions are stored first, then
    * @changed with a release store, and the reader acquires @changed
    * before consuming them.
    *
    * The atomic type makes this struct C-only; see the __cplusplus
    * guard at the top of the ANDROID block. */
   struct
   {
      retro_atomic_int_t width, height;
      retro_atomic_int_t changed;
   } content_rect;
   uint16_t rumble_last_strength_strong[MAX_USERS];
   uint16_t rumble_last_strength_weak[MAX_USERS];
   uint16_t rumble_last_strength[MAX_USERS];
   int id[MAX_USERS];

   bool is_play_store_build;

#ifdef HAVE_SAF
   jmethodID requestOpenDocumentTree;
   jmethodID getPersistedSafTrees;
   bool have_saf;
#endif
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
    * Unused. Upstream glue sends this to ask the app thread to produce a
    * saved state; RetroArch has no such state, so onSaveInstanceState()
    * returns without a round trip and nothing writes this command. Kept
    * so the enumerators below retain their values.
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

   APP_CMD_REINIT_DONE
};

/* Every macro below is wrapped in do/while(0). Without it the trailing
 * JNI_EXCEPTION escapes any unbraced guard at the call site, so
 *
 *    if (env != NULL)
 *       CALL_BOOLEAN_METHOD(env, ...);
 *
 * expanded to a guarded call followed by an *unguarded* exception check
 * that dereferences env regardless - a null dereference on exactly the
 * path the guard existed to protect. */
#define JNI_EXCEPTION(env) \
   do { \
      if ((*env)->ExceptionOccurred(env)) \
      { \
         (*env)->ExceptionDescribe(env); \
         (*env)->ExceptionClear(env); \
      } \
   } while (0)

#define FIND_CLASS(env, var, classname) \
   do { \
      var = (*env)->FindClass(env, classname); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_OBJECT_CLASS(env, var, clazz_obj) \
   do { \
      var = (*env)->GetObjectClass(env, clazz_obj); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_FIELD_ID(env, var, clazz, fieldName, fieldDescriptor) \
   do { \
      var = (*env)->GetFieldID(env, clazz, fieldName, fieldDescriptor); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   do { \
      var = (*env)->GetMethodID(env, clazz, methodName, fieldDescriptor); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_STATIC_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   do { \
      var = (*env)->GetStaticMethodID(env, clazz, methodName, fieldDescriptor); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallObjectMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_STATIC_METHOD(env, var, clazz, methodId) \
   do { \
      var = (*env)->CallStaticObjectMethod(env, clazz, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_STATIC_METHOD_PARAM(env, var, clazz, methodId, ...) \
   do { \
      var = (*env)->CallStaticObjectMethod(env, clazz, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   do { \
      var = (*env)->CallObjectMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_VOID_METHOD(env, clazz_obj, methodId) \
   do { \
      (*env)->CallVoidMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_VOID_METHOD_PARAM(env, clazz_obj, methodId, ...) \
   do { \
      (*env)->CallVoidMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_BOOLEAN_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallBooleanMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_BOOLEAN_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   do { \
      var = (*env)->CallBooleanMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_DOUBLE_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallDoubleMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_INT_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallIntMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_INT_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   do { \
      var = (*env)->CallIntMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_FLOAT_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallFloatMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

extern JNIEnv *jni_thread_getenv(void);

/* Re-assert a chosen display mode and window frame rate after a new
 * ANativeWindow appears.  Both are window state and are lost when the
 * app goes to the background; without this a mode chosen by the user
 * silently reverts on the next resume. */
void android_display_server_reapply_mode(void);

/* Performs the pause-time save of SRAM and config requested by
 * APP_CMD_PAUSE, if one is outstanding. Called from the runloop, which is
 * the nearest point outside the core: the command that requests it is read
 * by the input driver's poll, and a core reaches that poll from inside
 * retro_run(). No-op when nothing is pending. */
void android_input_flush_pending_state(void);

bool android_app_write_cmd(struct android_app *android_app, int8_t cmd);

#ifdef HAVE_ANDROID_LIFECYCLE_HOOKS
/* Runs a named shell script from the app's private data directory, if one
 * is present. Build with -DHAVE_ANDROID_LIFECYCLE_HOOKS to enable; see
 * android_run_lifecycle_hook() for what the hooks may and may not do. */
void android_run_lifecycle_hook(struct android_app *android_app,
      const char *name);
#endif

extern struct android_app *g_android;

void frontend_android_get_name(char *s, size_t len);

void frontend_android_get_manufacturer_model(char *s, size_t len);

void frontend_android_get_version(int32_t *major, int32_t *minor, int32_t *rel);

void frontend_android_get_version_sdk(int32_t *sdk);

bool is_screen_reader_enabled(void);

/* Pushes the live values of the window-affecting settings to the
 * activity, so the Java side never reads them from the config file. */
void android_app_set_window_settings(bool notch_write_over,
      bool auto_mouse_grab);

#ifdef HAVE_SAF
struct retro_vfs_authorized_locations;

void android_show_saf_tree_picker(void);
bool android_get_vfs_authorized_locations(
      struct retro_vfs_authorized_locations *locations);
#endif

#endif

#endif
