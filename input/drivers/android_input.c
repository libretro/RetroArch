/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2013-2014 - Steven Crowe
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

#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>

#include <android/keycodes.h>

#include <dynamic/dylib.h>
#include <retro_inline.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif


#include "../../command.h"
#include "../../frontend/drivers/platform_unix.h"
#include "../drivers_keyboard/keyboard_event_android.h"
#include "../../tasks/tasks_internal.h"
#include "../../performance_counters.h"

#include <compat/strl.h>

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../input_driver.h"

#ifdef HAVE_THREADS
#include "../../gfx/video_thread_wrapper.h"
#endif

#define MAX_TOUCH 16
#define MAX_NUM_KEYBOARDS 3
#define DEFAULT_ASENSOR_EVENT_RATE 60

/* If using an SDK lower than 14 then add missing mouse button codes */
#if __ANDROID_API__ < 14
enum {
    AMOTION_EVENT_BUTTON_PRIMARY = 1 << 0,
    AMOTION_EVENT_BUTTON_SECONDARY = 1 << 1,
    AMOTION_EVENT_BUTTON_TERTIARY = 1 << 2,
    AMOTION_EVENT_BUTTON_BACK = 1 << 3,
    AMOTION_EVENT_BUTTON_FORWARD = 1 << 4,
    AMOTION_EVENT_AXIS_VSCROLL = 9,
    AMOTION_EVENT_ACTION_HOVER_MOVE = 7,
    AINPUT_SOURCE_STYLUS = 0x00004000
};
#endif
/* If using an NDK lower than 16b then add missing definition */
#ifndef __ANDROID_API_O_MR1__
enum {
   AINPUT_SOURCE_MOUSE_RELATIVE = 0x00020000 | AINPUT_SOURCE_CLASS_NAVIGATION
};
#endif

/* If using an SDK lower than 24 then add missing relative axis codes */
#ifndef AMOTION_EVENT_AXIS_RELATIVE_X
#define AMOTION_EVENT_AXIS_RELATIVE_X 27
#endif

#ifndef AMOTION_EVENT_AXIS_RELATIVE_Y
#define AMOTION_EVENT_AXIS_RELATIVE_Y 28
#endif

/* Use this to enable/disable using the touch screen as mouse */
#define ENABLE_TOUCH_SCREEN_MOUSE 0

#define AKEYCODE_ASSIST 219

#define LAST_KEYCODE AKEYCODE_ASSIST

#define MAX_KEYS ((LAST_KEYCODE + 7) / 8)

/* First ports are used to keep track of gamepad states.
 * Last port is used for keyboard state.
 * Every bit index used against a row must be < LAST_KEYCODE;
 * writers bound incoming keycodes, readers bound bind keysyms. */
static uint8_t android_key_state[DEFAULT_MAX_PADS + 1][MAX_KEYS];

#define ANDROID_KEYBOARD_PORT_INPUT_PRESSED(binds, id) (BIT_GET(android_key_state[ANDROID_KEYBOARD_PORT], rarch_keysym_lut[(binds)[(id)].key]))

#define ANDROID_KEYBOARD_INPUT_PRESSED(key) (BIT_GET(android_key_state[0], (key)))

uint8_t *android_keyboard_state_get(unsigned port)
{
   return android_key_state[port];
}

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

struct input_pointer
{
   int16_t x, y;
   int16_t confined_x, confined_y;
   int16_t full_x, full_y;
};

static int pad_id1 = -1;
static int pad_id2 = -1;
static int kbd_id[MAX_NUM_KEYBOARDS];
static int kbd_num = 0;

enum
{
   AXIS_X        = 0,
   AXIS_Y        = 1,
   AXIS_Z        = 11,
   AXIS_RZ       = 14,
   AXIS_HAT_X    = 15,
   AXIS_HAT_Y    = 16,
   AXIS_LTRIGGER = 17,
   AXIS_RTRIGGER = 18,
   AXIS_GAS      = 22,
   AXIS_BRAKE    = 23
};

typedef struct state_device
{
   int id;
   int port;
   char name[256];
} state_device_t;

typedef struct android_input
{
   int64_t quick_tap_time;
   state_device_t pad_states[MAX_USERS];        /* int alignment */
   int mouse_x, mouse_y;
   int16_t mouse_x_viewport_screen, mouse_y_viewport_screen;
   int16_t mouse_x_viewport, mouse_y_viewport;
   int mouse_x_delta, mouse_y_delta;
   int mouse_l, mouse_r, mouse_m, mouse_wu, mouse_wd;
   bool mouse_activated;
   unsigned pads_connected;
   unsigned pointer_count;
   sensor_t accelerometer_state;                /* float alignment */
   sensor_t gyroscope_state;                    /* float alignment */
   float mouse_x_prev, mouse_y_prev;
   struct input_pointer pointer[MAX_TOUCH];     /* int16_t alignment */
   char device_model[256];
} android_input_t;

bool (*engine_lookup_name)(char *buf,
      int *vendorId, int *productId, size_t len, int id);
void (*engine_handle_dpad)(struct android_app *, AInputEvent*, int, int);

static void android_input_poll_input_gingerbread(android_input_t *android);
static void android_input_poll_input_default(android_input_t *android);
static void (*android_input_poll_input)(android_input_t *android);

static bool android_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate);
static void android_keycode_map_free(JNIEnv *env);
static void android_input_enable_sensor_manager(struct android_app *android_app);
static bool android_enable_sensor(struct android_app *android_app,
      const ASensor *sensor, unsigned rate,
      uint64_t enable_bit, uint64_t disable_bit);

extern float AMotionEvent_getAxisValue(const AInputEvent* motion_event,
      int32_t axis, size_t pointer_idx);

static typeof(AMotionEvent_getAxisValue) *p_AMotionEvent_getAxisValue;

#define AMotionEvent_getAxisValue (*p_AMotionEvent_getAxisValue)

extern int32_t AMotionEvent_getButtonState(const AInputEvent* motion_event);

static typeof(AMotionEvent_getButtonState) *p_AMotionEvent_getButtonState;

#define AMotionEvent_getButtonState (*p_AMotionEvent_getButtonState)

#ifdef HAVE_DYLIB
static void *libandroid_handle;
#endif

/* Android system (IME) keyboard support for the menu OSK.
 *
 * Mirrors the iOS ios_keyboard_* hooks (ui/drivers/ui_cocoatouch.m):
 * when the menu wants text input, the keyboard line buffer is repointed
 * to an owned 512-byte buffer and the Java side raises the native soft
 * keyboard (which gives copy/paste and password managers that the
 * custom on-screen keyboard cannot).
 *
 * The Android soft keyboard runs on the UI thread, so committed/pasted
 * text arrives via the onSystemKeyboardInput JNI callback on that
 * thread; it is staged there (under a lock) and applied on the
 * RetroArch input thread in android_keyboard_poll(). */
#define ANDROID_KBD_BUFFER_SIZE 512

static slock_t *android_kbd_lock        = NULL;
static char    *android_kbd_buffer      = NULL; /* == keyboard_line.buffer */
static size_t  *android_kbd_size_ptr    = NULL;
static size_t  *android_kbd_ptr_ptr     = NULL;
static input_keyboard_line_complete_t android_kbd_cb = NULL;
static void    *android_kbd_userdata    = NULL;
static bool     android_kbd_open        = false;

/* Staging: written by the JNI/UI thread, drained by the input thread. */
static char     android_kbd_staging[ANDROID_KBD_BUFFER_SIZE];
static bool     android_kbd_dirty       = false;
static bool     android_kbd_finished    = false;
static bool     android_kbd_cancel      = false;

/* Called by Java (UI thread) on every text change, and once more with
 * finished = true on Done/Enter. A null text means the keyboard was
 * dismissed without confirming, i.e. cancel. */
JNIEXPORT void JNICALL Java_com_retroarch_browser_retroactivity_RetroActivityCommon_onSystemKeyboardInput(
      JNIEnv *env, jobject this_obj, jstring text_obj, jboolean finished)
{
   if (!android_kbd_lock)
      return;

   slock_lock(android_kbd_lock);
   if (text_obj)
   {
      const char *text = (*env)->GetStringUTFChars(env, text_obj, NULL);
      if (text)
      {
         strlcpy(android_kbd_staging, text, sizeof(android_kbd_staging));
         (*env)->ReleaseStringUTFChars(env, text_obj, text);
      }
      android_kbd_cancel = false;
   }
   else
   {
      android_kbd_staging[0] = '\0';
      android_kbd_cancel     = true;
   }
   android_kbd_dirty = true;
   if (finished)
      android_kbd_finished = true;
   slock_unlock(android_kbd_lock);
}

bool android_keyboard_start(char **buffer_ptr, size_t *size_ptr,
      size_t *ptr_ptr, const char *label,
      input_keyboard_line_complete_t cb, void *userdata)
{
   JNIEnv             *env;
   char               *allocated;
   size_t              len;
   struct android_app *android_app = (struct android_app*)g_android;

   if (!android_app || !android_app->showKeyboard || !buffer_ptr || !size_ptr)
      return false;

   if (!android_kbd_lock && !(android_kbd_lock = slock_new()))
      return false;

   if (!(allocated = (char*)malloc(ANDROID_KBD_BUFFER_SIZE)))
      return false;

   /* Seed with any existing content (e.g. when editing a value). */
   if (*buffer_ptr && **buffer_ptr)
      strlcpy(allocated, *buffer_ptr, ANDROID_KBD_BUFFER_SIZE);
   else
      allocated[0] = '\0';

   /* Repoint the keyboard line at our buffer; it is freed later by
    * input_keyboard_line_free(), mirroring the iOS path. */
   *buffer_ptr = allocated;
   len         = strlen(allocated);
   *size_ptr   = len;
   if (ptr_ptr)
      *ptr_ptr = len;

   slock_lock(android_kbd_lock);
   android_kbd_buffer     = allocated;
   android_kbd_size_ptr   = size_ptr;
   android_kbd_ptr_ptr    = ptr_ptr;
   android_kbd_cb         = cb;
   android_kbd_userdata   = userdata;
   android_kbd_staging[0] = '\0';
   android_kbd_dirty      = false;
   android_kbd_finished   = false;
   android_kbd_cancel     = false;
   android_kbd_open       = true;
   slock_unlock(android_kbd_lock);

   if ((env = jni_thread_getenv()))
   {
      jstring jlabel = label ? (*env)->NewStringUTF(env, label) : NULL;
      jstring jinit  = (*env)->NewStringUTF(env, allocated);
      CALL_VOID_METHOD_PARAM(env, android_app->activity->clazz,
            android_app->showKeyboard, jlabel, jinit);
      if (jlabel)
         (*env)->DeleteLocalRef(env, jlabel);
      if (jinit)
         (*env)->DeleteLocalRef(env, jinit);
   }

   return true;
}

bool android_keyboard_active(void)
{
   return android_kbd_open;
}

void android_keyboard_end(void)
{
   JNIEnv             *env         = NULL;
   struct android_app *android_app = (struct android_app*)g_android;

   if (!android_kbd_open || !android_kbd_lock)
      return;

   slock_lock(android_kbd_lock);
   android_kbd_open       = false;
   android_kbd_buffer     = NULL;
   android_kbd_size_ptr   = NULL;
   android_kbd_ptr_ptr    = NULL;
   android_kbd_cb         = NULL;
   android_kbd_userdata   = NULL;
   android_kbd_dirty      = false;
   android_kbd_finished   = false;
   android_kbd_cancel     = false;
   slock_unlock(android_kbd_lock);

   if (android_app && android_app->hideKeyboard && (env = jni_thread_getenv()))
      CALL_VOID_METHOD(env, android_app->activity->clazz,
            android_app->hideKeyboard);
}

/* Drain staged IME text on the RetroArch input thread. */
void android_keyboard_poll(void)
{
   bool                           finished;
   bool                           cancel;
   char                          *buffer;
   void                          *userdata;
   input_keyboard_line_complete_t cb;

   if (!android_kbd_open || !android_kbd_lock)
      return;

   slock_lock(android_kbd_lock);
   if (!android_kbd_dirty)
   {
      slock_unlock(android_kbd_lock);
      return;
   }

   /* Sync staged text into the live keyboard line buffer so the menu
    * displays it (same role as the iOS UITextField delegate). */
   if (!android_kbd_cancel && android_kbd_buffer)
   {
      size_t len;
      strlcpy(android_kbd_buffer, android_kbd_staging, ANDROID_KBD_BUFFER_SIZE);
      len = strlen(android_kbd_buffer);
      if (android_kbd_size_ptr)
         *android_kbd_size_ptr = len;
      if (android_kbd_ptr_ptr)
         *android_kbd_ptr_ptr  = len;
   }

   finished             = android_kbd_finished;
   cancel               = android_kbd_cancel;
   cb                   = android_kbd_cb;
   userdata             = android_kbd_userdata;
   buffer               = android_kbd_buffer;
   android_kbd_dirty    = false;
   android_kbd_finished = false;
   slock_unlock(android_kbd_lock);

   if (finished)
   {
      input_driver_state_t *input_st = input_state_get_ptr();

      /* Mirror the iOS completion block: fire the callback (NULL line
       * on cancel), then release the keyboard line and unblock hotkeys.
       * The callback closes the dialog, which hides the soft keyboard
       * via menu_input_dialog_end() -> android_keyboard_end(). */
      if (cb)
         cb(userdata, cancel ? NULL : buffer);

      if (input_st)
      {
         input_keyboard_line_free(input_st);
         input_st->flags &= ~INP_FLAG_KB_MAPPING_BLOCKED;
      }

      /* The callback normally closes the dialog (-> android_keyboard_end),
       * which clears our state. If it didn't, drop the now-freed buffer
       * pointer so a late JNI callback can't use it after free. */
      slock_lock(android_kbd_lock);
      if (android_kbd_buffer == buffer)
      {
         android_kbd_buffer   = NULL;
         android_kbd_open     = false;
         android_kbd_dirty    = false;
         android_kbd_finished = false;
      }
      slock_unlock(android_kbd_lock);
   }
}

static void android_keyboard_free(void)
{
    unsigned i, j;

    for (i = 0; i < DEFAULT_MAX_PADS; i++)
        for (j = 0; j < MAX_KEYS; j++)
            android_key_state[i][j] = 0;

    for (i = 0; i < (unsigned) kbd_num; i++)
        kbd_id[i] = -1;

    kbd_num = 0;
}

static bool android_input_lookup_name_prekitkat(char *s,
      int *vendorId, int *productId, size_t len, int id)
{
   jobject name      = NULL;
   jmethodID getName = NULL;
   jobject device    = NULL;
   jmethodID method  = NULL;
   jclass    class   = 0;
   const char *str   = NULL;
   JNIEnv     *env   = (JNIEnv*)jni_thread_getenv();

   if (!env)
      return false;

   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      return false;

   GET_STATIC_METHOD_ID(env, method, class, "getDevice",
         "(I)Landroid/view/InputDevice;");
   if (!method)
      return false;

   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
      return false;

   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      return false;

   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
      return false;

   s[0] = '\0';
   str  = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(s, str, len);
   (*env)->ReleaseStringUTFChars(env, name, str);

   return true;
}

static bool android_input_lookup_name(char *s,
      int *vendorId, int *productId, size_t len, int id)
{
   jmethodID getVendorId  = NULL;
   jmethodID getProductId = NULL;
   jmethodID getName      = NULL;
   jobject device         = NULL;
   jobject name           = NULL;
   jmethodID method       = NULL;
   jclass class           = NULL;
   const char *str        = NULL;
   JNIEnv     *env        = (JNIEnv*)jni_thread_getenv();

   if (!env)
      return false;

   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      return false;

   GET_STATIC_METHOD_ID(env, method, class, "getDevice",
         "(I)Landroid/view/InputDevice;");
   if (!method)
      return false;

   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
      return false;

   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      return false;

   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
      return false;

   s[0] = '\0';

   str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(s, str, len);
   (*env)->ReleaseStringUTFChars(env, name, str);

   GET_METHOD_ID(env, getVendorId, class, "getVendorId", "()I");
   if (!getVendorId)
      return false;

   CALL_INT_METHOD(env, *vendorId, device, getVendorId);

   GET_METHOD_ID(env, getProductId, class, "getProductId", "()I");
   if (!getProductId)
      return false;

   *productId = 0;
   CALL_INT_METHOD(env, *productId, device, getProductId);

   return true;
}

static bool android_input_can_be_keyboard_jni(int id)
{
    jmethodID getKeyboardType  = NULL;
    jobject device             = NULL;
    jint keyboard_type         = -1;
    jmethodID method           = NULL;
    jclass class               = NULL;
    const char *str            = NULL;
    JNIEnv     *env            = (JNIEnv*)jni_thread_getenv();

    if (!env)
        return false;

    FIND_CLASS(env, class, "android/view/InputDevice");
    if (!class)
        return false;

    GET_STATIC_METHOD_ID(env, method, class, "getDevice",
                         "(I)Landroid/view/InputDevice;");
    if (!method)
        return false;

    CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
    if (!device)
        return false;

    GET_METHOD_ID(env, getKeyboardType, class, "getKeyboardType", "()I");
    if (!getKeyboardType)
        return false;

    CALL_INT_METHOD(env, keyboard_type, device, getKeyboardType);
    if (keyboard_type < 0)
        return false;

    return keyboard_type == AINPUT_KEYBOARD_TYPE_ALPHABETIC;
}

bool android_input_can_be_keyboard(void *data, int port)
{
    android_input_t *android = (android_input_t *) data;
    if (!android)
        return false;

    if (port < 0 || port >= android->pads_connected)
        return false;

    state_device_t *device = &android->pad_states[port];
    if (!device->id && !*device->name)
        return false;

    return android_input_can_be_keyboard_jni(device->id);
}

#ifdef HAVE_THREADS
/* EGL bindings are per-thread. Under threaded video the context is made
 * current on the video thread, so an eglMakeCurrent() issued from here
 * releases this thread's (empty) binding and leaves the surface current
 * on the worker - and a later create_surface() would bind the same
 * context a second time, on a second thread. Both entry points therefore
 * run where the context lives. */
static uintptr_t android_ctx_create_surface_cb(void *data)
{
   video_driver_state_t *state = video_state_get_ptr();

   if (!state->current_video_context.create_surface)
      return 0;

   return state->current_video_context.create_surface(data) ? 1 : 0;
}

static uintptr_t android_ctx_destroy_surface_cb(void *data)
{
   video_driver_state_t *state = video_state_get_ptr();

   if (state->current_video_context.destroy_surface)
      state->current_video_context.destroy_surface(data);

   return 0;
}
#endif

static void android_input_destroy_surface(video_driver_state_t *state)
{
   if (!state || !state->current_video_context.destroy_surface)
      return;

#ifdef HAVE_THREADS
   /* The video worker may still be recording a frame that references the
    * surface. Drain it before the context driver frees the surface. */
   video_thread_wait_idle();

   /* Dispatches to the video thread, or calls straight through when the
    * wrapper is inactive or this already is the video thread. */
   video_thread_texture_handle(state->context_data,
         android_ctx_destroy_surface_cb);
#else
   state->current_video_context.destroy_surface(state->context_data);
#endif
}

/* Set once the pause-time flush has been performed, cleared again on
 * resume. A pause -> resume -> pause cycle therefore flushes twice, but a
 * duplicate APP_CMD_PAUSE does not rewrite the config a second time. */
static bool android_state_flushed = false;

/* Set by the APP_CMD_PAUSE handler, consumed by
 * android_input_flush_pending_state() at the top of the next runloop
 * iteration. */
static bool android_state_flush_pending = false;

/* Android may reclaim the process at any point after onPause() has
 * returned. onDestroy() is not guaranteed to run at all - in particular,
 * swiping the task away from Recents never delivers it - so onPause() is
 * the last callback that can be relied upon.
 *
 * Everything that would otherwise only be written by retroarch_main_quit()
 * is therefore flushed here instead, so that settings survive the process
 * being killed without the user having to invoke 'Quit RetroArch'.
 *
 * Called from the runloop rather than from the command handler: the
 * command pipe is drained by android_input_poll(), which a core reaches
 * through the input poll callback, so a flush performed there would read
 * core memory and rewrite the config from inside retro_run(). One frame
 * of latency is well inside the window Android allows after onPause(). */
void android_input_flush_pending_state(void)
{
   settings_t *settings        = config_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   if (!android_state_flush_pending)
      return;
   android_state_flush_pending = false;

   if (android_state_flushed)
      return;
   android_state_flushed = true;

   /* Config subsystem is not up yet - nothing to persist. */
   if (!settings)
      return;

   /* SRAM first: it is the more expensive of the two, and the more
    * painful to lose. Non-SRAM cores make this a no-op.
    *
    * Only flush while content is actually loaded. APP_CMD_PAUSE can be
    * delivered while frontend_unix_init() is still pumping events
    * waiting for the window (activity created, then immediately
    * backgrounded / screen locked), long before any core exists - and,
    * worse, while a previous activity instance in the same process may
    * still be mid-teardown, leaving runloop_state.current_core in a
    * transient state. There is nothing to save in either case:
    * CMD_EVENT_SAVE_FILES exists to persist SRAM and game-specific
    * cheats, both of which require loaded content. */
   if (runloop_st->current_core.flags & RETRO_CORE_FLAG_GAME_LOADED)
      command_event(CMD_EVENT_SAVE_FILES, NULL);

   if (settings->bools.config_save_on_exit)
   {
      video_driver_state_t *video_st = video_state_get_ptr();
      char live_driver[32];

      live_driver[0] = '\0';

      /* A core that forces its own renderer overwrites video_driver
       * with the forced name and parks the configured one in
       * cached_driver_id. Writing the config in that state persists the
       * core's choice as the user's, so a driver picked from the menu
       * is silently replaced by whatever the last loaded core wanted.
       * main_exit() restores the cached name before it saves; do the
       * same here.
       *
       * Unlike main_exit(), swap the live value back afterwards: the
       * activity may be resumed, and the renderer actually in use does
       * not change just because the app went to the background. */
      if (video_st->cached_driver_id[0])
      {
         strlcpy(live_driver, settings->arrays.video_driver,
               sizeof(live_driver));
         configuration_set_string(settings,
               settings->arrays.video_driver,
               video_st->cached_driver_id);
      }

      command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

      if (live_driver[0])
         configuration_set_string(settings,
               settings->arrays.video_driver, live_driver);
   }
}

static void android_input_poll_main_cmd(void)
{
   int8_t cmd;
   ssize_t ret;
   struct android_app *android_app = (struct android_app*)g_android;

   /* A command dropped here is never acknowledged, and a lifecycle
    * callback waiting on it would block the Java UI thread until
    * ActivityManager gives up. Retry rather than lose the byte. */
   do
   {
      ret = read(android_app->msgread, &cmd, sizeof(cmd));
   } while (ret < 0 && errno == EINTR);

   if (ret != (ssize_t)sizeof(cmd))
      cmd = -1;

   switch (cmd)
   {
      case APP_CMD_REINIT_DONE:
         slock_lock(android_app->mutex);

         android_app->reinitRequested = 0;

         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_INPUT_CHANGED:
         slock_lock(android_app->mutex);

         if (android_app->inputQueue)
            AInputQueue_detachLooper(android_app->inputQueue);

         android_app->inputQueue = android_app->pendingInputQueue;

         if (android_app->inputQueue)
            AInputQueue_attachLooper(android_app->inputQueue,
                  android_app->looper, LOOPER_ID_INPUT, NULL,
                  NULL);

         android_app->done_seq++;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         /* The set of attached input devices has changed, so a cached
          * KeyCharacterMap may now belong to a device that is gone or
          * has been replaced under the same id. Drop the cache and let
          * it re-resolve on the next key event. */
         android_keycode_map_free((JNIEnv*)jni_thread_getenv());

         break;

      case APP_CMD_INIT_WINDOW:
         slock_lock(android_app->mutex);
         android_app->window = android_app->pendingWindow;
         android_app->reinitRequested = 1;
         android_app->done_seq++;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         /* A resume brings a NEW window, and the display mode and
          * frame rate chosen for the old one do not come with it.
          * Assert them again, or a mode the user picked reverts
          * every time they come back from the Android UI. */
         android_display_server_reapply_mode();

         break;

      case APP_CMD_RESUME:
      case APP_CMD_START:
      case APP_CMD_PAUSE:
      {
         video_driver_state_t *state = video_state_get_ptr();

         slock_lock(android_app->mutex);
         android_app->activityState = cmd;
         /* RESUME/START can arrive before INIT_WINDOW. In that case,
          * wait for INIT_WINDOW rather than falling back to a full
          * video-driver reinitialization without a native window. */
         if (  (cmd == APP_CMD_RESUME || cmd == APP_CMD_START)
             && state->current_video_context.ident
             && string_is_equal(state->current_video_context.ident,
                   "vk_android")
             && android_app->window
             && state->current_video_context.create_surface)
            android_app->reinitRequested = 1;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         if (cmd == APP_CMD_PAUSE)
            android_state_flush_pending = true;
         else
         {
            android_state_flush_pending = false;
            android_state_flushed       = false;
         }

#ifdef HAVE_ANDROID_LIFECYCLE_HOOKS
         /* After the acknowledgement above, so a slow hook delays this
          * thread rather than the UI thread waiting in onStart(). */
         if (cmd == APP_CMD_START)
            android_run_lifecycle_hook(android_app, "switch");
#endif
         break;
      }

      case APP_CMD_STOP:
      {
         video_driver_state_t *state = video_state_get_ptr();

         slock_lock(android_app->mutex);
         android_app->activityState = cmd;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         /* Android may retain the same ANativeWindow while the app is
          * backgrounded. Release Vulkan's acquired buffers anyway so BLAST
          * cannot wedge before APP_CMD_TERM_WINDOW is delivered. */
         if (     state->current_video_context.ident
               && string_is_equal(state->current_video_context.ident,
                     "vk_android"))
            android_input_destroy_surface(state);
         break;
      }

      case APP_CMD_CONFIG_CHANGED:
         AConfiguration_fromAssetManager(android_app->config,
               android_app->activity->assetManager);
         break;
      case APP_CMD_TERM_WINDOW:
      {
         video_driver_state_t *state = video_state_get_ptr();

         android_input_destroy_surface(state);

         slock_lock(android_app->mutex);

         /* The window is being hidden or closed, clean it up. */
         /* terminate display/EGL context here */
         android_app->window = NULL;
         android_app->done_seq++;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;
      }

      case APP_CMD_GAINED_FOCUS:
         {
            runloop_state_t *runloop_st = runloop_state_get_ptr();
            settings_t *settings         = config_get_ptr();
            bool sensors_allowed         = !settings || settings->bools.input_sensors_enable;
            /* Re-enable sensors that were disabled on focus loss */
            bool enable_accelerometer   = (android_app->sensor_state_mask &
                  (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_DISABLE));
            bool enable_gyroscope       = (android_app->sensor_state_mask &
                  (UINT64_C(1) << RETRO_SENSOR_GYROSCOPE_DISABLE));

            /* On first focus (no sensor state yet), enable if setting is on.
             * This handles shader sensor access without going through cores.
             * Default to enabling if settings aren't loaded yet (first launch). */
            if (!enable_accelerometer &&
                !(android_app->sensor_state_mask & (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_ENABLE)))
            {
               if (sensors_allowed)
                  enable_accelerometer = true;
            }
            if (!enable_gyroscope &&
                !(android_app->sensor_state_mask & (UINT64_C(1) << RETRO_SENSOR_GYROSCOPE_ENABLE)))
            {
               if (sensors_allowed)
                  enable_gyroscope = true;
            }

            if (!sensors_allowed)
            {
               enable_accelerometer = false;
               enable_gyroscope     = false;
            }

            runloop_st->flags &= ~(RUNLOOP_FLAG_PAUSED
                                 | RUNLOOP_FLAG_IDLE);
            video_driver_unset_stub_frame();

            /* Try to enable sensors via input driver. If that fails before the
             * input driver has initialized, enable directly via sensor API. */
            if (enable_accelerometer)
            {
               if (!input_set_sensor_state(0,
                     RETRO_SENSOR_ACCELEROMETER_ENABLE,
                     android_app->accelerometer_event_rate) &&
                   !android_app->input_alive)
               {
                  /* Input driver not ready — enable sensor directly */
                  unsigned rate = android_app->accelerometer_event_rate;
                  if (rate == 0)
                     rate = DEFAULT_ASENSOR_EVENT_RATE;
                  android_input_enable_sensor_manager(android_app);
                  if (android_enable_sensor(android_app,
                        android_app->accelerometerSensor, rate,
                        RETRO_SENSOR_ACCELEROMETER_ENABLE,
                        RETRO_SENSOR_ACCELEROMETER_DISABLE))
                     android_app->accelerometer_event_rate = rate;
               }
            }

            if (enable_gyroscope)
            {
               if (!input_set_sensor_state(0,
                     RETRO_SENSOR_GYROSCOPE_ENABLE,
                     android_app->gyroscope_event_rate) &&
                   !android_app->input_alive)
               {
                  /* Input driver not ready — enable sensor directly */
                  unsigned rate = android_app->gyroscope_event_rate;
                  if (rate == 0)
                     rate = DEFAULT_ASENSOR_EVENT_RATE;
                  android_input_enable_sensor_manager(android_app);
                  if (android_enable_sensor(android_app,
                        android_app->gyroscopeSensor, rate,
                        RETRO_SENSOR_GYROSCOPE_ENABLE,
                        RETRO_SENSOR_GYROSCOPE_DISABLE))
                     android_app->gyroscope_event_rate = rate;
               }
            }

            /* Start gravity-based orientation detection (once per app launch only).
             * Samples accelerometer data over ~30 frames to determine which axis
             * gravity is on, detecting both display rotation and sensor IC
             * misalignment automatically. */
            if (!android_app->gravity_calibrated)
            {
               android_app->gravity_accum_x      = 0.0f;
               android_app->gravity_accum_y      = 0.0f;
               android_app->gravity_sample_count = 0;
            }
            else
            {
               /* Gravity already calibrated (not first launch),
                * start rest position capture immediately */
               input_sensor_start_rest_capture();
            }
         }
         /* No waiter: onWindowFocusChanged() posts the command and
          * returns without blocking, so the lock and broadcast only
          * guarded this one scalar. The field is atomic now. */
         retro_atomic_store_release_int(&android_app->unfocused, 0);
         break;
      case APP_CMD_LOST_FOCUS:
         {
            runloop_state_t *runloop_st = runloop_state_get_ptr();
            bool disable_accelerometer  = (android_app->sensor_state_mask &
                  (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_ENABLE)) &&
                        android_app->accelerometerSensor;
            bool disable_gyroscope      = (android_app->sensor_state_mask &
                  (UINT64_C(1) << RETRO_SENSOR_GYROSCOPE_ENABLE)) &&
                        android_app->gyroscopeSensor;

            runloop_st->flags |=  (RUNLOOP_FLAG_PAUSED
                                 | RUNLOOP_FLAG_IDLE);
            video_driver_set_stub_frame();

            /* Avoid draining battery while app is not being used. */
            if (disable_accelerometer)
               input_set_sensor_state(0,
                     RETRO_SENSOR_ACCELEROMETER_DISABLE,
                     android_app->accelerometer_event_rate);

            if (disable_gyroscope)
               input_set_sensor_state(0,
                     RETRO_SENSOR_GYROSCOPE_DISABLE,
                     android_app->gyroscope_event_rate);
         }
         /* No waiter: onWindowFocusChanged() posts the command and
          * returns without blocking, so the lock and broadcast only
          * guarded this one scalar. The field is atomic now. */
         retro_atomic_store_release_int(&android_app->unfocused, 1);
         break;

      case APP_CMD_DESTROY:
         android_app->destroyRequested = 1;
         break;
   }
}

static void engine_handle_dpad_default(struct android_app *android,
      AInputEvent *event, int port, int source)
{
   size_t motion_ptr = AMotionEvent_getAction(event) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x           = AMotionEvent_getX(event, motion_ptr);
   float y           = AMotionEvent_getY(event, motion_ptr);

   if (port < 0 || port >= DEFAULT_MAX_PADS)
      return;

   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);
}

#ifdef HAVE_DYLIB
static void engine_handle_dpad_getaxisvalue(struct android_app *android,
      AInputEvent *event, int port, int source)
{
   size_t motion_ptr = AMotionEvent_getAction(event) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x           = AMotionEvent_getAxisValue(event, AXIS_X, motion_ptr);
   float y           = AMotionEvent_getAxisValue(event, AXIS_Y, motion_ptr);
   float z           = AMotionEvent_getAxisValue(event, AXIS_Z, motion_ptr);
   float rz          = AMotionEvent_getAxisValue(event, AXIS_RZ, motion_ptr);
   float hatx        = AMotionEvent_getAxisValue(event, AXIS_HAT_X, motion_ptr);
   float haty        = AMotionEvent_getAxisValue(event, AXIS_HAT_Y, motion_ptr);
   float ltrig       = AMotionEvent_getAxisValue(event, AXIS_LTRIGGER, motion_ptr);
   float rtrig       = AMotionEvent_getAxisValue(event, AXIS_RTRIGGER, motion_ptr);
   float brake       = AMotionEvent_getAxisValue(event, AXIS_BRAKE, motion_ptr);
   float gas         = AMotionEvent_getAxisValue(event, AXIS_GAS, motion_ptr);

   if (port < 0 || port >= DEFAULT_MAX_PADS)
      return;

   android->hat_state[port][0]    = (int)hatx;
   android->hat_state[port][1]    = (int)haty;

   /* XXX: this could be a loop instead, but do we really want to
    * loop through every axis? */
   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);
   android->analog_state[port][2] = (int16_t)(z * 32767.0f);
   android->analog_state[port][3] = (int16_t)(rz * 32767.0f);
   android->analog_state[port][6] = (int16_t)(ltrig * 32767.0f);
   android->analog_state[port][7] = (int16_t)(rtrig * 32767.0f);
   android->analog_state[port][8] = (int16_t)(brake * 32767.0f);
   android->analog_state[port][9] = (int16_t)(gas * 32767.0f);
}
#endif

static bool android_input_init_handle(void)
{
#ifdef HAVE_DYLIB
   if (libandroid_handle != NULL) /* already initialized */
      return true;
#if defined (ANDROID_AARCH64) || defined(ANDROID_X64)
   if ((libandroid_handle = dlopen("/system/lib64/libandroid.so",
               RTLD_LOCAL | RTLD_LAZY)) == 0)
      return false;
#else
   if ((libandroid_handle = dlopen("/system/lib/libandroid.so",
               RTLD_LOCAL | RTLD_LAZY)) == 0)
      return false;
#endif

   if ((p_AMotionEvent_getAxisValue = dlsym(RTLD_DEFAULT,
               "AMotionEvent_getAxisValue")))
      engine_handle_dpad            = engine_handle_dpad_getaxisvalue;

   p_AMotionEvent_getButtonState    = dlsym(RTLD_DEFAULT,
               "AMotionEvent_getButtonState");
#endif

   pad_id1 = -1;
   pad_id2 = -1;

   return true;
}

static void *android_input_init(const char *joypad_driver)
{
   int32_t sdk;
   struct android_app *android_app = (struct android_app*)g_android;
   android_input_t *android = (android_input_t*)
      calloc(1, sizeof(*android));

   if (!android)
      return NULL;

   android->mouse_activated = false;
   android->pads_connected = 0;
   android->quick_tap_time = 0;

   input_keymaps_init_keyboard_lut(rarch_key_map_android);

   frontend_android_get_version_sdk(&sdk);

   if (sdk >= 19)
      engine_lookup_name       = android_input_lookup_name;
   else
      engine_lookup_name       = android_input_lookup_name_prekitkat;

   engine_handle_dpad          = engine_handle_dpad_default;

   if (sdk > 10)
      android_input_poll_input = android_input_poll_input_default;
   else
      android_input_poll_input = android_input_poll_input_gingerbread;

   if (!android_input_init_handle())
   {
      RARCH_WARN("[Android] Unable to open libandroid.so\n");
   }

   frontend_android_get_name(android->device_model,
         sizeof(android->device_model));

   android_app->input_alive = true;

   /* Enable sensors on init if setting is on (or not yet loaded)
    * and they haven't already been enabled by GAINED_FOCUS.
    * Check sensor_state_mask to avoid double-enabling, since
    * ASensorEventQueue_enableSensor is reference-counted. */
   {
      settings_t *settings = config_get_ptr();
      bool enable_sensors = !settings || settings->bools.input_sensors_enable;

      if (enable_sensors && !android_app->sensor_state_mask)
      {
         unsigned rate = android_app->accelerometer_event_rate;
         if (rate == 0)
            rate = DEFAULT_ASENSOR_EVENT_RATE;
         android_input_enable_sensor_manager(android_app);
         if (android_enable_sensor(android_app,
               android_app->accelerometerSensor, rate,
               RETRO_SENSOR_ACCELEROMETER_ENABLE,
               RETRO_SENSOR_ACCELEROMETER_DISABLE))
            android_app->accelerometer_event_rate = rate;

         rate = android_app->gyroscope_event_rate;
         if (rate == 0)
            rate = DEFAULT_ASENSOR_EVENT_RATE;
         if (android_enable_sensor(android_app,
               android_app->gyroscopeSensor, rate,
               RETRO_SENSOR_GYROSCOPE_ENABLE,
               RETRO_SENSOR_GYROSCOPE_DISABLE))
            android_app->gyroscope_event_rate = rate;
      }
   }

   return android;
}

static int android_check_quick_tap(android_input_t *android)
{
   /* Check if the touch screen has been been quick tapped
    * and then not touched again for 200ms
    * If so then return true and deactivate quick tap timer */
   retro_time_t now = cpu_features_get_time_usec();
   if (android->quick_tap_time &&
         (now / 1000 - android->quick_tap_time / 1000000) >= 200)
   {
      android->quick_tap_time = 0;
      return 1;
   }

   return 0;
}

static INLINE void android_mouse_calculate_deltas(android_input_t *android,
      AInputEvent *event,size_t motion_ptr,int source)
{
   unsigned video_width, video_height;
   video_driver_get_output_size(&video_width, &video_height);

   float x       = 0;
   float x_delta = 0;
   float x_min   = 0;
   float x_max   = (float)video_width;

   float y       = 0;
   float y_delta = 0;
   float y_min   = 0;
   float y_max   = (float)video_height;

   struct video_viewport vp = {0};
   int16_t res_x            = 0;
   int16_t res_y            = 0;
   int16_t res_screen_x     = 0;
   int16_t res_screen_y     = 0;

   /* AINPUT_SOURCE_MOUSE_RELATIVE is available on Oreo (SDK 26) and newer,
    * it passes the relative coordinates in the regular X and Y parts.
    * NOTE: AINPUT_SOURCE_* defines have multiple bits set so do full check */
   if ((source & AINPUT_SOURCE_MOUSE_RELATIVE) == AINPUT_SOURCE_MOUSE_RELATIVE)
   {
      x_delta = AMotionEvent_getX(event, motion_ptr);
      y_delta = AMotionEvent_getY(event, motion_ptr);
   }
   else
   {
      /* This axis is only available on Android Nougat or on
      * Android devices with NVIDIA extensions */
      if (p_AMotionEvent_getAxisValue)
      {
         x_delta = AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_RELATIVE_X,
               motion_ptr);
         y_delta = AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_RELATIVE_Y,
               motion_ptr);
      }

      /* If AXIS_RELATIVE had 0 values it might be because we're not
      * running Android Nougat or on a device
      * with NVIDIA extension, so re-calculate deltas based on
      * AXIS_X and AXIS_Y. This has limitations
      * compared to AXIS_RELATIVE because once the Android mouse cursor
      * hits the edge of the screen it is
      * not possible to move the in-game mouse any further in that direction.
      */
      if (!x_delta && !y_delta)
      {
         x = AMotionEvent_getX(event, motion_ptr);
         y = AMotionEvent_getY(event, motion_ptr);

         x_delta = (x_delta - android->mouse_x_prev);
         y_delta = (y_delta - android->mouse_y_prev);

         android->mouse_x_prev = x;
         android->mouse_y_prev = y;
      }
   }

   android->mouse_x_delta = x_delta;
   android->mouse_y_delta = y_delta;

   if (!x) x = android->mouse_x + android->mouse_x_delta;
   if (!y) y = android->mouse_y + android->mouse_y_delta;

   video_driver_translate_coord_viewport_confined_wrap(&vp,
            (int) x, (int) y,
            &android->mouse_x_viewport, &android->mouse_y_viewport,
            &android->mouse_x_viewport_screen, &android->mouse_y_viewport_screen);

   /* x and y are used for the screen mouse, so we want
    * to avoid values outside of the viewport resolution */
   if (x < x_min) x = x_min;
   else if (x > x_max) x = x_max;
   if (y < y_min) y = y_min;
   else if (y > y_max) y = y_max;

   android->mouse_x = x;
   android->mouse_y = y;
}

static INLINE void android_input_poll_event_type_motion(
      android_input_t *android, AInputEvent *event,
      int port, int source)
{
   int getaction     = AMotionEvent_getAction(event);
   int action        = getaction  & AMOTION_EVENT_ACTION_MASK;
   size_t motion_ptr = getaction >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   bool keyup        = (
            action == AMOTION_EVENT_ACTION_UP
         || action == AMOTION_EVENT_ACTION_CANCEL
         || action == AMOTION_EVENT_ACTION_POINTER_UP);

   /* If source is mouse then calculate button state
    * and mouse deltas and don't process as touchscreen event.
    * NOTE: AINPUT_SOURCE_* defines have multiple bits set so do full check */
   if (    (source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE
        || (source & AINPUT_SOURCE_MOUSE_RELATIVE) == AINPUT_SOURCE_MOUSE_RELATIVE)
   {
      if (!android->mouse_activated)
      {
         RARCH_LOG("[Android Input] Mouse activated.\n");
         android->mouse_activated = true;
      }
      /* getButtonState requires API level 14 */
      if (p_AMotionEvent_getButtonState)
      {
         int btn              = (int)AMotionEvent_getButtonState(event);

         android->mouse_l     = (btn & AMOTION_EVENT_BUTTON_PRIMARY);
         android->mouse_r     = (btn & AMOTION_EVENT_BUTTON_SECONDARY);
         android->mouse_m     = (btn & AMOTION_EVENT_BUTTON_TERTIARY);

         btn                  = (int)AMotionEvent_getAxisValue(event,
               AMOTION_EVENT_AXIS_VSCROLL, motion_ptr);

         if (btn > 0)
            android->mouse_wu = btn;
         else if (btn < 0)
            android->mouse_wd = btn;
      }
      else
      {
         /* If getButtonState is not available
          * then treat all MotionEvent.ACTION_DOWN as left button presses */
         if (action == AMOTION_EVENT_ACTION_DOWN)
            android->mouse_l = 1;
         if (action == AMOTION_EVENT_ACTION_UP)
            android->mouse_l = 0;
      }

      android_mouse_calculate_deltas(android,event,motion_ptr,source);

      return;
   }

   if (keyup && motion_ptr < MAX_TOUCH)
   {
      if (action == AMOTION_EVENT_ACTION_UP && ENABLE_TOUCH_SCREEN_MOUSE)
      {
         /* If touchscreen was pressed for less than 200ms
          * then register time stamp of a quick tap */
         if ((AMotionEvent_getEventTime(event)-AMotionEvent_getDownTime(event))/1000000 < 200)
         {
            /* Prevent the quick tap if a button on the overlay is down */
            input_driver_state_t *input_st = input_state_get_ptr();
            if (!(input_st->flags & INP_FLAG_BLOCK_POINTER_INPUT))
               android->quick_tap_time = AMotionEvent_getEventTime(event);
         }
         android->mouse_l = 0;
      }

      memmove(android->pointer + motion_ptr,
            android->pointer + motion_ptr + 1,
            (MAX_TOUCH - motion_ptr - 1) * sizeof(struct input_pointer));
      if (android->pointer_count > 0)
         android->pointer_count--;
   }
   else
   {
      int      pointer_max     = MIN(
            AMotionEvent_getPointerCount(event), MAX_TOUCH);

      if (action == AMOTION_EVENT_ACTION_DOWN && ENABLE_TOUCH_SCREEN_MOUSE)
      {
         /* When touch screen is pressed, set mouse
          * previous position to current position
          * before starting to calculate mouse movement deltas. */
         android->mouse_x_prev = AMotionEvent_getX(event, motion_ptr);
         android->mouse_y_prev = AMotionEvent_getY(event, motion_ptr);

         /* If another touch happened within 200ms after a quick tap
          * then cancel the quick tap and register left mouse button
          * as being held down */
         if ((AMotionEvent_getEventTime(event) - android->quick_tap_time)/1000000 < 200)
         {
            android->quick_tap_time = 0;
            android->mouse_l        = 1;
         }
      }

      if ((       action == AMOTION_EVENT_ACTION_MOVE
               || action == AMOTION_EVENT_ACTION_HOVER_MOVE)
            && ENABLE_TOUCH_SCREEN_MOUSE)
         android_mouse_calculate_deltas(android,event,motion_ptr,source);

      for (motion_ptr = 0; motion_ptr < pointer_max; motion_ptr++)
      {
         struct video_viewport vp = {0};
         float x = AMotionEvent_getX(event, motion_ptr);
         float y = AMotionEvent_getY(event, motion_ptr);

         /* On other platforms, pointer query uses the confined wrap function, *
          * but some extra functionality is added to Android which needs the   *
          * true offscreen value -0x8000, so both variants are called. */
         video_driver_translate_coord_viewport_confined_wrap(
               &vp,
               x, y,
               &android->pointer[motion_ptr].confined_x,
               &android->pointer[motion_ptr].confined_y,
               &android->pointer[motion_ptr].full_x,
               &android->pointer[motion_ptr].full_y);

         video_driver_translate_coord_viewport_wrap(
               &vp,
               x, y,
               &android->pointer[motion_ptr].x,
               &android->pointer[motion_ptr].y,
               &android->pointer[motion_ptr].full_x,
               &android->pointer[motion_ptr].full_y);

         android->pointer_count = MAX(
               android->pointer_count,
               motion_ptr + 1);
      }
   }

   /* If more than one pointer detected
    * then count it as a mouse right click */
   if (ENABLE_TOUCH_SCREEN_MOUSE)
      android->mouse_r = (android->pointer_count == 2);
}

static bool android_is_keyboard_id(int id)
{
   unsigned i;
   for (i = 0;  i < (unsigned)kbd_num; i++)
      if (id == kbd_id[i])
         return true;

   return false;
}

/* Resolves the printable Unicode codepoint produced by a hardware-key
 * press, honouring the active keyboard layout and modifier (shift, caps
 * lock, ...) state via the device's KeyCharacterMap - this mirrors what
 * android.view.KeyEvent.getUnicodeChar() does.
 *
 * Returns 0 for keys that do not produce a character (modifiers, lock
 * keys, navigation/function keys, ...).  That is exactly what the
 * keyboard line-editor expects: such keys must not emit text, and were
 * previously leaking into menu text fields as '?'.
 *
 * Also returns 0 (rather than failing) whenever JNI is unavailable, so
 * the caller can fall back to its previous keysym-based behaviour. */
/* Resolved once and held for the lifetime of the input driver.
 * @kcm_class is a global reference: the method IDs below stay valid only
 * while the class is reachable, and a local reference would not survive
 * the call that created it. */
#define ANDROID_KCM_CACHE_SIZE 4

static jclass    kcm_class                                   = NULL;
static jmethodID kcm_load                                    = NULL;
static jmethodID kcm_get                                     = NULL;
static jobject   kcm_obj[ANDROID_KCM_CACHE_SIZE];
static int       kcm_obj_device[ANDROID_KCM_CACHE_SIZE];
static int       kcm_obj_next                                = 0;
static bool      kcm_resolve_failed                          = false;

static void android_keycode_map_free(JNIEnv *env)
{
   int i;

   if (!env)
      return;

   for (i = 0; i < ANDROID_KCM_CACHE_SIZE; i++)
   {
      if (kcm_obj[i])
         (*env)->DeleteGlobalRef(env, kcm_obj[i]);
      kcm_obj[i]        = NULL;
      kcm_obj_device[i] = 0;
   }

   if (kcm_class)
      (*env)->DeleteGlobalRef(env, kcm_class);

   kcm_class          = NULL;
   kcm_load           = NULL;
   kcm_get            = NULL;
   kcm_obj_next       = 0;
   kcm_resolve_failed = false;
}

/* Return the KeyCharacterMap for @device_id, resolving and caching the
 * class, its method IDs and the per-device map object on first use.
 *
 * Devices are few and long-lived, so a small round-robin cache covers
 * the realistic case (one or two attached keyboards) without needing
 * invalidation on hotplug: a stale entry is simply evicted in turn, and
 * a reconnected device re-resolves. */
static jobject android_keycode_map_get(JNIEnv *env, int device_id)
{
   int     i;
   jobject local = NULL;

   if (kcm_resolve_failed)
      return NULL;

   if (!kcm_class)
   {
      jclass found = NULL;

      FIND_CLASS(env, found, "android/view/KeyCharacterMap");
      if (!found)
      {
         kcm_resolve_failed = true;
         return NULL;
      }

      kcm_class = (jclass)(*env)->NewGlobalRef(env, found);
      (*env)->DeleteLocalRef(env, found);
      if (!kcm_class)
      {
         kcm_resolve_failed = true;
         return NULL;
      }

      GET_STATIC_METHOD_ID(env, kcm_load, kcm_class, "load",
            "(I)Landroid/view/KeyCharacterMap;");
      GET_METHOD_ID(env, kcm_get, kcm_class, "get", "(II)I");

      if (!kcm_load || !kcm_get)
      {
         android_keycode_map_free(env);
         kcm_resolve_failed = true;
         return NULL;
      }
   }

   for (i = 0; i < ANDROID_KCM_CACHE_SIZE; i++)
      if (kcm_obj[i] && kcm_obj_device[i] == device_id)
         return kcm_obj[i];

   CALL_OBJ_STATIC_METHOD_PARAM(env, local, kcm_class, kcm_load,
         (jint)device_id);
   if (!local)
      return NULL;

   if (kcm_obj[kcm_obj_next])
      (*env)->DeleteGlobalRef(env, kcm_obj[kcm_obj_next]);

   kcm_obj[kcm_obj_next]        = (*env)->NewGlobalRef(env, local);
   kcm_obj_device[kcm_obj_next] = device_id;
   (*env)->DeleteLocalRef(env, local);

   local        = kcm_obj[kcm_obj_next];
   kcm_obj_next = (kcm_obj_next + 1) % ANDROID_KCM_CACHE_SIZE;

   return local;
}

static unsigned android_keycode_to_unicode(int device_id,
      int keycode, int meta_state)
{
   jint      unicode = 0;
   jobject   kcm     = NULL;
   JNIEnv   *env     = (JNIEnv*)jni_thread_getenv();

   if (!env)
      return 0;

   /* No local frame: the cached path creates no local references at all,
    * and the resolve path deletes the two it makes explicitly. */
   if ((kcm = android_keycode_map_get(env, device_id)))
      CALL_INT_METHOD_PARAM(env, unicode, kcm, kcm_get,
            (jint)keycode, (jint)meta_state);

   /* KeyCharacterMap.get() sets the COMBINING_ACCENT (0x80000000) flag
    * for dead keys; the menu line-editor cannot compose those, so strip
    * the flag and keep the base accent character. */
   return ((unsigned)unicode) & 0x7fffffff;
}

static INLINE void android_input_poll_event_type_keyboard(
      AInputEvent *event, int keycode, int *handled)
{
   int keydown           = (AKeyEvent_getAction(event)
         == AKEY_EVENT_ACTION_DOWN);
   unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(keycode);
   /* Set keyboard modifier based on shift,ctrl and alt state */
   uint16_t mod          = 0;
   int meta              = AKeyEvent_getMetaState(event);
   uint32_t character    = 0;

   if (meta & AMETA_ALT_ON)
      mod |= RETROKMOD_ALT;
   if (meta & AMETA_CTRL_ON)
      mod |= RETROKMOD_CTRL;
   if (meta & AMETA_SHIFT_ON)
      mod |= RETROKMOD_SHIFT;
   if (meta & AMETA_CAPS_LOCK_ON)
      mod |= RETROKMOD_CAPSLOCK;
   if (meta & AMETA_NUM_LOCK_ON)
      mod |= RETROKMOD_NUMLOCK;
   if (meta & AMETA_SCROLL_LOCK_ON)
      mod |= RETROKMOD_SCROLLOCK;
   if (meta & AMETA_META_ON)
      mod |= RETROKMOD_META;

   /* Resolve the actual printable character for this key in the current
    * layout + modifier state.  This produces capitals and shifted
    * symbols, and yields 0 for modifier/lock keys so they no longer leak
    * into menu text fields as '?'. */
   character = android_keycode_to_unicode(
         AInputEvent_getDeviceId(event), keycode, meta);

   /* Fall back to the raw keysym for the plain ASCII range when the
    * platform could not resolve a character (e.g. JNI unavailable).
    * This preserves the previous behaviour for lowercase input while
    * still suppressing modifier/lock keys, whose keysyms are >= 0x80,
    * from being emitted as text. */
   if (character == 0 && keyboardcode < 0x80)
      character = keyboardcode;

   input_keyboard_event(keydown, keyboardcode,
         character, mod, RETRO_DEVICE_KEYBOARD);

   if ((keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN))
      *handled = 0;
}

static INLINE void android_input_poll_event_type_key(
      struct android_app *android_app,
      AInputEvent *event, int port, int keycode, int source,
      int type_event, int *handled)
{
   uint8_t *buf;
   int action           = AKeyEvent_getAction(event);
   int keysym           = keycode;
   bool alias_back_as_x =
         (source & AINPUT_SOURCE_GAMEPAD)  != AINPUT_SOURCE_GAMEPAD
      && (source & AINPUT_SOURCE_JOYSTICK) != AINPUT_SOURCE_JOYSTICK;

   /* android_key_state[] has one row per pad slot plus the
    * dedicated keyboard row at ANDROID_KEYBOARD_PORT. */
   if (port < 0 || port > ANDROID_KEYBOARD_PORT)
      return;
   buf           = android_key_state[port];

   /* Handle 'duplicate' inputs that correspond
    * to the same RETROK_* key */
   switch (keycode)
   {
      case AKEYCODE_DPAD_CENTER:
         keysym = AKEYCODE_ENTER;
      default:
         break;
   }
   /* Rows are MAX_KEYS bytes wide and readers bound their lookups at
    * LAST_KEYCODE (android_joypad_button_state). Keycodes arrive
    * straight from the platform and are not confined to that range:
    * public codes run well past AKEYCODE_ASSIST (AKEYCODE_WAKEUP is
    * 224, AKEYCODE_PROFILE_SWITCH 288) and vendor codes are
    * unbounded, so an unguarded BIT_SET writes past the row - and
    * past the array itself for ANDROID_KEYBOARD_PORT, which is the
    * last one - corrupting whatever the linker placed next to it.
    * Nothing above LAST_KEYCODE is bindable, so drop it. */
   if (keysym >= 0 && keysym < LAST_KEYCODE)
   {
      /* some controllers send both the up and down events at once
       * when the button is released for "special" buttons, like menu buttons
       * work around that by only using down events for meta keys (which get
       * cleared every poll anyway)
       */
      switch (action)
      {
         case AKEY_EVENT_ACTION_UP:
            BIT_CLEAR(buf, keysym);
            if (keysym == AKEYCODE_BACK && alias_back_as_x)
            {
               BIT_CLEAR(buf, AKEYCODE_X); /* alias BACK on remote */
               BIT_CLEAR(android_key_state[ANDROID_KEYBOARD_PORT], AKEYCODE_X);
            }
            break;
         case AKEY_EVENT_ACTION_DOWN:
            BIT_SET(buf, keysym);
            if (keysym == AKEYCODE_BACK && alias_back_as_x)
            {
               BIT_SET(buf, AKEYCODE_X);
               BIT_SET(android_key_state[ANDROID_KEYBOARD_PORT], AKEYCODE_X);
            }
            break;
      }
   }

   if ((keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN))
      *handled = 0;
}

static int android_input_get_id_port(android_input_t *android, int id,
      int source)
{
   unsigned i;
   int ret = -1;
   if (source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE |
            AINPUT_SOURCE_MOUSE_RELATIVE | AINPUT_SOURCE_TOUCHPAD))
         ret = 0; /* touch overlay is always user 1 */

   for (i = 0; i < android->pads_connected; i++)
   {
      if (android->pad_states[i].id == id)
      {
         ret = i;
         break;
      }
   }

   return ret;
}

/* Returns the index inside android->pad_state */
static int android_input_get_id_index_from_name(android_input_t *android,
      const char *name)
{
   int i;
   for (i = 0; i < android->pads_connected; i++)
   {
      if (string_is_equal(name, android->pad_states[i].name))
         return i;
   }

   return -1;
}

static int android_input_recover_port(android_input_t *android, int id)
{
   char device_name[256] = { 0 };
   int vendorId          = 0;
   int productId         = 0;
   int ret               = -1;
   settings_t *settings  = config_get_ptr();

   if (!engine_lookup_name(device_name, &vendorId,
			   &productId, sizeof(device_name), id))
       return -1;
   ret = android_input_get_id_index_from_name(android, device_name);
   if (ret < 0)
       return -1;

   if (!settings->bools.android_input_disconnect_workaround)
   {
      char stale_name[256];

      stale_name[0] = '\0';
      /* Even without the user-facing disconnect workaround enabled,
       * rebind the device to its old port when the previously mapped
       * id has verifiably vanished (InputDevice.getDevice() returns
       * NULL for it). Android re-enumerates input devices with fresh
       * ids across suspend/resume, which would otherwise burn one pad
       * slot per wake-up. If the old id still resolves, this is a
       * second identical controller and must get its own port. */
      if (engine_lookup_name(stale_name, &vendorId, &productId,
               sizeof(stale_name), android->pad_states[ret].id))
          return -1;
   }

   android->pad_states[ret].id = id;
   /* Keep the frontend id table in sync so rumble keeps
    * targeting the right device after the rebind. */
   g_android->id[ret]          = id;
   return ret;
}


static bool is_configured_as_physical_keyboard(int vendor_id, int product_id, const char *device_name)
{
    bool is_keyboard;
    bool compare_by_id;
    int keyboard_vendor_id;
    int keyboard_product_id;
    char keyboard_name[256];
    settings_t *settings = config_get_ptr();

    {
        const char *str = settings->arrays.input_android_physical_keyboard;
        char *end       = NULL;
        long vid, pid;

        vid = strtol(str, &end, 16);
        if (end && end != str && *end == ':')
        {
            const char *pid_start = end + 1;
            pid = strtol(pid_start, &end, 16);
            if (end && end != pid_start && (*end == ' ' || *end == '\0'))
            {
                keyboard_vendor_id  = (int)vid;
                keyboard_product_id = (int)pid;
                is_keyboard         = (vendor_id == keyboard_vendor_id && product_id == keyboard_product_id);
                compare_by_id       = true;
            }
            else
            {
                strlcpy(keyboard_name, str, sizeof(keyboard_name));
                is_keyboard   = string_is_equal(device_name, keyboard_name);
                compare_by_id = false;
            }
        }
        else
        {
            strlcpy(keyboard_name, str, sizeof(keyboard_name));
            is_keyboard   = string_is_equal(device_name, keyboard_name);
            compare_by_id = false;
        }
    }

    if (is_keyboard)
    {
       int i;
        /*
         * Check that there is not already a similar physical keyboard attached
         * attached to the system
         */
        for (i = 0; i < kbd_num; i++)
        {
            char kbd_device_name[256] = { 0 };
            int kbd_vendor_id         = 0;
            int kbd_product_id        = 0;

            if (!engine_lookup_name(kbd_device_name, &kbd_vendor_id,
                     &kbd_product_id, sizeof(kbd_device_name), kbd_id[i]))
                return false;

            if (compare_by_id && vendor_id == kbd_vendor_id && product_id == kbd_product_id)
                return false;

            if (!compare_by_id && string_is_equal(device_name, kbd_device_name))
                return false;
        }
        return true;
    }
    return false;
}

static void handle_hotplug(android_input_t *android,
      struct android_app *android_app, int *port, int id,
      int source)
{
   char device_name[256];
   char name_buf[256];
   int vendorId                 = 0;
   int productId                = 0;
   const char *device_model     = android->device_model;

   device_name[0] = name_buf[0] = '\0';

   if (!engine_lookup_name(device_name, &vendorId,
            &productId, sizeof(device_name), id))
      return;

   /* FIXME - per-device hacks for NVidia Shield, Xperia Play and
    * similar devices
    *
    * These hacks depend on autoconf, but can work with user
    * created autoconfs properly
    */

   /* NVIDIA Shield Console
    * This is the most complicated example, the built-in controller
    * has an extra button that can't be used and a remote.
    *
    * We map the remote for navigation and overwrite whenever a
    * real controller is connected.
    * Also group the NVIDIA button on the controller with the
    * main controller inputs so it's usable. It's mapped to
    * menu by default
    *
    * The NVIDIA button is identified as "Virtual" device when first
    * pressed. CEC remote input is also identified as "Virtual" device.
    * If a virtual device is detected before a controller then it will
    * be assigned to port 0 as "SHIELD Virtual Controller". When a real
    * controller is detected it will overwrite the virtual controller
    * and be grouped with the NVIDIA button of the virtual device.
    *
    */
   if (strstr(device_model, "SHIELD Android TV") && (
      strstr(device_name, "Virtual") ||
      strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.0")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("[Android] Special device detected: %s.\n", device_model);
      {
         /* Remove the remote or virtual controller device if it is mapped */
         if (   strstr(android->pad_states[0].name, "SHIELD Remote")
             || strstr(android->pad_states[0].name, "SHIELD Virtual Controller"))
         {
            pad_id1 = -1;
            pad_id2 = -1;
            android->pads_connected = 0;
            *port = 0;
            strlcpy(name_buf, device_name, sizeof(name_buf));
         }

         /* if the actual controller has not been mapped yet,
          * then configure Virtual device for now */
         if (strstr(device_name, "Virtual") && android->pads_connected==0)
            strlcpy_lit(name_buf, "SHIELD Virtual Controller", sizeof(name_buf));
         else
            strlcpy_lit(name_buf, "NVIDIA SHIELD Controller", sizeof(name_buf));

         /* apply the hack only for the first controller
          * store the id for later use
         */
         if (strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.0")
               && android->pads_connected==0)
            pad_id1 = id;
         else if (strstr(device_name, "Virtual") && pad_id1 != -1)
         {
            id = pad_id1;
            return;
         }
      }
   }

   else if (strstr(device_model, "SHIELD") && (
      strstr(device_name, "Virtual") || strstr(device_name, "gpio") ||
      strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.01") ||
      strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.02")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("[Android] Special device detected: %s.\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;
         strlcpy_lit(name_buf, "NVIDIA SHIELD Portable", sizeof(name_buf));
      }
   }

   else if (strstr(device_model, "SHIELD") && (
      strstr(device_name, "Virtual") || strstr(device_name, "gpio") ||
      strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.03")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("[Android] Special device detected: %s.\n", device_model);
      {
         if (strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.03")
             && android->pads_connected==0)
            pad_id1 = id;
         else if (strstr(device_name, "Virtual") || strstr(device_name, "gpio"))
         {
            id = pad_id1;
            return;
         }
         strlcpy_lit(name_buf, "NVIDIA SHIELD Gamepad", sizeof(name_buf));
      }
   }

   /* Other ATV Devices
    * Add other common ATV devices that will follow the Android
    * Gaempad convention as "Android Gamepad"
    */
    /* to-do: add DS4 on Bravia ATV */
   else if (strstr(device_name, "NVIDIA"))
      strlcpy_lit(name_buf, "Android Gamepad", sizeof(name_buf));

   /* GPD XD
    * This is a simple hack, basically groups the "back"
    * button with the rest of the gamepad
    */
   else if (strstr(device_model, "XD") && (
      strstr(device_name, "Virtual") || strstr(device_name, "rk29-keypad") ||
      strstr(device_name,"Playstation3") || strstr(device_name,"XBOX")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("[Android] Special device detected: %s.\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;

         strlcpy_lit(name_buf, "GPD XD", sizeof(name_buf));
         *port = 0;
      }
   }

   /* XPERIA Play
    * This device is composed of two hid devices
    * We make it look like one device
    */
   else if (
            (
               string_starts_with_size(device_model, "R800", STRLEN_CONST("R800")) ||
               strstr(device_model, "Xperia Play") ||
               strstr(device_model, "Play") ||
               strstr(device_model, "SO-01D")
            ) || (
               strstr(device_name, "keypad-game-zeus") ||
               strstr(device_name, "keypad-zeus") ||
               strstr(device_name, "Android Gamepad")
            )
         )
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("[Android] Special device detected: %s.\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;

         strlcpy_lit(name_buf, "XPERIA Play", sizeof(name_buf));
         *port = 0;
      }
   }

   /* ARCHOS Gamepad
    * This device is composed of two hid devices
    * We make it look like one device
    */
   else if (strstr(device_model, "ARCHOS GAMEPAD") && (
      strstr(device_name, "joy_key") || strstr(device_name, "joystick")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("[Android] ARCHOS GAMEPAD Detected: %s.\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;

         strlcpy_lit(name_buf, "ARCHOS GamePad", sizeof(name_buf));
         *port = 0;
      }
   }

   /* Amazon Fire TV & Fire stick */
   else if (
             string_starts_with_size(device_model, "AFT", STRLEN_CONST("AFT")) &&
             (
              strstr(device_model, "AFTB") ||
              strstr(device_model, "AFTT") ||
              strstr(device_model, "AFTS") ||
              strstr(device_model, "AFTM") ||
              strstr(device_model, "AFTRS")
             )
         )
   {
      RARCH_LOG("[Android] Special device detected: %s\n", device_model);
      {
         /* always map remote to port #0 */
         if (strstr(device_name, "Amazon Fire TV Remote"))
         {
            android->pads_connected = 0;
            *port = 0;
            strlcpy(name_buf, device_name, sizeof(name_buf));
         }
         /* remove the remote when a gamepad enters */
         else if (strstr(android->pad_states[0].name,"Amazon Fire TV Remote"))
         {
            android->pads_connected = 0;
            *port = 0;
            strlcpy(name_buf, device_name, sizeof(name_buf));
         }
         else
            strlcpy(name_buf, device_name, sizeof(name_buf));
      }
   }

   /* Other uncommon devices
    * These are mostly remote control type devices, bind them always to port 0
    * And overwrite the binding whenever a controller button is pressed
    */
   else if (strstr(device_name, "Amazon Fire TV Remote")
         || strstr(device_name, "Nexus Remote")
         || strstr(device_name, "SHIELD Remote"))
   {
      android->pads_connected = 0;
      *port = 0;
      strlcpy(name_buf, device_name, sizeof(name_buf));
   }

   else if (strstr(device_name, "iControlPad-"))
      strlcpy_lit(name_buf, "iControlPad HID Joystick profile", sizeof(name_buf));

   else if (strstr(device_name, "TTT THT Arcade console 2P USB Play"))
   {
      if (*port == 0)
         strlcpy_lit(name_buf, "TTT THT Arcade (User 1)", sizeof(name_buf));
      else if (*port == 1)
         strlcpy_lit(name_buf, "TTT THT Arcade (User 2)", sizeof(name_buf));
   }
   else if (strstr(device_name, "MOGA"))
      strlcpy_lit(name_buf, "Moga IME", sizeof(name_buf));

   /* If device is keyboard only and didn't match any of the devices above
    * then assume it is a keyboard, register the id, and return unless the
    * maximum number of keyboards are already registered. */
   else if ((source == AINPUT_SOURCE_KEYBOARD ||
      source == (AINPUT_SOURCE_KEYBOARD | AINPUT_SOURCE_DPAD))
      && kbd_num < MAX_NUM_KEYBOARDS)
   {
      kbd_id[kbd_num] = id;
      kbd_num++;
      return;
   }

   /* If the device is a keyboard, didn't match any of the devices above
    * and is designated as the physical keyboard, then assume it is a keyboard,
    * register the id, and return unless the
    * maximum number of keyboards are already registered. */
   else if ((source & AINPUT_SOURCE_KEYBOARD) && kbd_num < MAX_NUM_KEYBOARDS &&
            is_configured_as_physical_keyboard(vendorId, productId, device_name))
   {
      kbd_id[kbd_num] = id;
      kbd_num++;
      return;
   }

   /* if device was not keyboard only, yet did not match any of the devices
    * then try to autoconfigure as gamepad based on device_name. */
   else if (*device_name)
      strlcpy(name_buf, device_name, sizeof(name_buf));

   if (strstr(android_app->current_ime, "net.obsidianx.android.mogaime"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));
   else if (strstr(android_app->current_ime, "com.ccpcreations.android.WiiUseAndroid"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));
   else if (strstr(android_app->current_ime, "com.hexad.bluezime"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));

   /* All pad slots exhausted: refuse to register the device instead
    * of writing out of bounds. Although pad_states[] holds MAX_USERS
    * entries, every consumer of the port index on the event path
    * (analog_state[], hat_state[], android_key_state[]) is sized
    * DEFAULT_MAX_PADS, so any port at or beyond that limit corrupts
    * adjacent memory. Slots can realistically run out because Android
    * re-enumerates input devices with fresh ids on every
    * suspend/resume cycle. */
   if (android->pads_connected >= DEFAULT_MAX_PADS)
   {
      RARCH_ERR("[Android] Input device \"%s\" ignored: all %d pad slots in use.\n",
            device_name, DEFAULT_MAX_PADS);
      *port = -1;
      return;
   }

   if (*port < 0)
      *port = android->pads_connected;

   input_autoconfigure_connect(
         name_buf,
         NULL, NULL,
         android_joypad.ident,
         *port,
         vendorId,
         productId);

   android->pad_states[android->pads_connected].id   =
      g_android->id[android->pads_connected]         = id;
   android->pad_states[android->pads_connected].port = *port;

   strlcpy(android->pad_states[*port].name, name_buf,
         sizeof(android->pad_states[*port].name));

   android->pads_connected++;
}

static int android_input_get_id(AInputEvent *event)
{
   int id = AInputEvent_getDeviceId(event);
   if (id == pad_id2)
      return pad_id1;
   return id;
}

struct TOUCHSTATE
{
   int down;
   int x;
   int y;
};

static void engine_handle_touchpad(
      struct android_app *android, AInputEvent *event, int port)
{
   size_t n;
   static struct TOUCHSTATE touchstate[64];
   int    raw_action  = AMotionEvent_getAction(event);
   int    action      = AMOTION_EVENT_ACTION_MASK & raw_action;
   size_t ptr_count   = AMotionEvent_getPointerCount(event);
   int    action_id   = -1;

   /* Every other handler on this path bounds the port it is given.
    * This one is in range only because android_input_get_id_port maps
    * a touchpad source to port 0 or to a pad slot below
    * DEFAULT_MAX_PADS, which is an invariant of a different function. */
   if (port < 0 || port >= DEFAULT_MAX_PADS)
      return;

   /* The index of the pointer that went down or up rides in the action
    * word, and AMotionEvent_getPointerId indexes the event's pointer
    * array with it without checking it against the count - an index
    * past the end returns whatever is behind the array, which then
    * indexes touchstate. Nothing in an event that names a pointer it
    * does not carry is worth routing. */
   if (     action == AMOTION_EVENT_ACTION_POINTER_DOWN
         || action == AMOTION_EVENT_ACTION_POINTER_UP)
   {
      size_t action_idx = (size_t)
           ((raw_action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
         >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

      if (action_idx >= ptr_count)
         return;

      action_id = AMotionEvent_getPointerId(event, action_idx);
   }

   for (n = 0; n < ptr_count; ++n)
   {
      int pointer_id = (action_id >= 0)
         ? action_id
         : AMotionEvent_getPointerId(event, n);

      /* Pointer ids run to 31 on every Android release, so this only
       * fires on an id the framework never produces. */
      if (pointer_id < 0 || pointer_id >= (int)ARRAY_SIZE(touchstate))
         continue;

      if (     action  == AMOTION_EVENT_ACTION_DOWN
            || action  == AMOTION_EVENT_ACTION_POINTER_DOWN )
         touchstate[pointer_id].down = 1;
      else if (action  == AMOTION_EVENT_ACTION_UP
            || action  == AMOTION_EVENT_ACTION_POINTER_UP
            || action  == AMOTION_EVENT_ACTION_CANCEL )
         touchstate[pointer_id].down = 0;

      if (touchstate[pointer_id].down)
      {
         int x = touchstate[pointer_id].x  = AMotionEvent_getX(event, n);
         int y = touchstate[pointer_id].y  = AMotionEvent_getY(event, n);
         if (x < 360)
         {
            android->analog_state[port][0] =
               (int16_t)((x / 180.0 - 1.0f) * 32767.0f);
            android->analog_state[port][1] =
               (int16_t)((y / 180.0 - 1.0f) * 32767.0f);
         }
         else if (x >= 606)
         {
            x -= 606;
            android->analog_state[port][2] =
               (int16_t)((x / 180.0 - 1.0f) * 32767.0f);
            android->analog_state[port][3] =
               (int16_t)((y / 180.0 - 1.0f) * 32767.0f);
         }
      }
      else
      {
         if (touchstate[pointer_id].x < 360)
         {
            android->analog_state[port][0] = 0.0f;
            android->analog_state[port][1] = 0.0f;
         }
         else if (touchstate[pointer_id].x >= 606)
         {
            android->analog_state[port][2] = 0.0f;
            android->analog_state[port][3] = 0.0f;
         }
      }
   }
}

static void android_input_poll_input_gingerbread(
      android_input_t *android)
{
   AInputEvent              *event = NULL;
   struct android_app *android_app = (struct android_app*)g_android;

   /* The queue is swapped out from under this poll: a destroyed input
    * queue arrives as a NULL through APP_CMD_INPUT_CHANGED, and a
    * LOOPER_ID_INPUT raised before that command was serviced is still
    * delivered afterwards in the same pollOnce batch.
    * android_input_discard_events guards the same pointer. */
   if (!android_app || !android_app->inputQueue)
      return;

   /* Read all pending events. */
   if (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
   {
      int source, type_event, id, port;
      int32_t   handled = 0;
      if (AInputQueue_preDispatchEvent(android_app->inputQueue, event))
         return;
      source            = AInputEvent_getSource(event);
      type_event        = AInputEvent_getType(event);
      id                = android_input_get_id(event);
      port              = android_input_get_id_port(android, id, source);

      if (port < 0 && !android_is_keyboard_id(id))
         port = android_input_recover_port(android, id);

      if (port < 0 && !android_is_keyboard_id(id))
         handle_hotplug(android, android_app,
         &port, id, source);

      switch (type_event)
      {
         case AINPUT_EVENT_TYPE_MOTION:
            if ((source & AINPUT_SOURCE_TOUCHPAD))
               engine_handle_touchpad(android_app, event, port);
            /* Only handle events from a touchscreen or mouse */
            else if ((source & (AINPUT_SOURCE_TOUCHSCREEN
                        | AINPUT_SOURCE_STYLUS | AINPUT_SOURCE_MOUSE)))
               android_input_poll_event_type_motion(android, event,
                     port, source);
            else
               engine_handle_dpad(android_app, event, port, source);
            handled = 1;
            break;
         case AINPUT_EVENT_TYPE_KEY:
            {
               int keycode = AKeyEvent_getKeyCode(event);

               if (!keycode)
                  break;

               if (android_is_keyboard_id(id))
               {
                  android_input_poll_event_type_keyboard(
                        event, keycode, &handled);
                  android_input_poll_event_type_key(
                        android_app, event, ANDROID_KEYBOARD_PORT,
                        keycode, source, type_event, &handled);
               }
               else
                  android_input_poll_event_type_key(android_app,
                     event, port, keycode, source, type_event, &handled);
            }
            break;
      }

      AInputQueue_finishEvent(android_app->inputQueue, event, handled);
   }
}

static void android_input_poll_input_default(android_input_t *android)
{
   AInputEvent              *event = NULL;
   struct android_app *android_app = (struct android_app*)g_android;

   /* The queue is swapped out from under this poll: a destroyed input
    * queue arrives as a NULL through APP_CMD_INPUT_CHANGED, and a
    * LOOPER_ID_INPUT raised before that command was serviced is still
    * delivered afterwards in the same pollOnce batch.
    * android_input_discard_events guards the same pointer. */
   if (!android_app || !android_app->inputQueue)
      return;

   /* Read all pending events. */
   while (AInputQueue_hasEvents(android_app->inputQueue))
   {
      while (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
      {
         int32_t handled;
         int source, type_event, id, port;

         /* A pre-dispatched event belongs to the IME from here on and
          * reappears in the queue if it goes unconsumed, so it can be
          * neither read from, routed nor finished once this returns
          * non-zero. */
         if (AInputQueue_preDispatchEvent(android_app->inputQueue, event))
            continue;

         handled    = 1;
         source     = AInputEvent_getSource(event);
         type_event = AInputEvent_getType(event);
         id         = android_input_get_id(event);
         port       = android_input_get_id_port(android, id, source);

         if (port < 0 && !android_is_keyboard_id(id))
            port = android_input_recover_port(android, id);

         if (port < 0 && !android_is_keyboard_id(id))
            handle_hotplug(android, android_app,
                  &port, id, source);

         switch (type_event)
         {
            case AINPUT_EVENT_TYPE_MOTION:
               if ((source & AINPUT_SOURCE_TOUCHPAD))
                  engine_handle_touchpad(android_app, event, port);
               /* Only handle events from a touchscreen or mouse */
               else if ((source & (AINPUT_SOURCE_TOUCHSCREEN
                           | AINPUT_SOURCE_MOUSE_RELATIVE
                           | AINPUT_SOURCE_STYLUS | AINPUT_SOURCE_MOUSE)))
                  android_input_poll_event_type_motion(android, event,
                        port, source);
               else
                  engine_handle_dpad(android_app, event, port, source);
               break;
            case AINPUT_EVENT_TYPE_KEY:
               {
                  int keycode = AKeyEvent_getKeyCode(event);

                  if (!keycode)
                     break;

                  if (android_is_keyboard_id(id))
                  {
                     android_input_poll_event_type_keyboard(
                           event, keycode, &handled);
                     android_input_poll_event_type_key(
                           android_app, event, ANDROID_KEYBOARD_PORT,
                           keycode, source, type_event, &handled);
                  }
                  else
                     android_input_poll_event_type_key(android_app,
                           event, port, keycode, source, type_event, &handled);
               }
               break;
         }

         AInputQueue_finishEvent(android_app->inputQueue, event, handled);
      }
   }
}

static void android_input_poll_user(android_input_t *android)
{
   struct android_app *android_app = (struct android_app*)g_android;
   bool poll_accelerometer         = false;
   bool poll_gyroscope             = false;

   if (!android_app->sensorEventQueue)
      return;

   poll_accelerometer = (android_app->sensor_state_mask &
         (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_ENABLE)) &&
               android_app->accelerometerSensor;

   poll_gyroscope     = (android_app->sensor_state_mask &
         (UINT64_C(1) << RETRO_SENSOR_GYROSCOPE_ENABLE)) &&
               android_app->gyroscopeSensor;

   if (poll_accelerometer || poll_gyroscope)
   {
      ASensorEvent event;
      while (ASensorEventQueue_getEvents(
            android_app->sensorEventQueue, &event, 1) > 0)
      {
         switch (event.type)
         {
            case ASENSOR_TYPE_ACCELEROMETER:
               android->accelerometer_state.x = event.acceleration.x;
               android->accelerometer_state.y = event.acceleration.y;
               android->accelerometer_state.z = event.acceleration.z;

               /* Gravity-based orientation auto-detection: accumulate
                * raw accelerometer X/Y over 30 samples, then determine
                * which axis gravity is dominant on to detect sensor IC
                * mounting orientation. */
               if (!android_app->gravity_calibrated)
               {
                  android_app->gravity_accum_x += event.acceleration.x;
                  android_app->gravity_accum_y += event.acceleration.y;
                  android_app->gravity_sample_count++;

                  if (android_app->gravity_sample_count >= 30)
                  {
                     float avg_x = android_app->gravity_accum_x / 30.0f;
                     float avg_y = android_app->gravity_accum_y / 30.0f;
                     float abs_x = (avg_x < 0.0f) ? -avg_x : avg_x;
                     float abs_y = (avg_y < 0.0f) ? -avg_y : avg_y;

                     /* Only commit if device is tilted enough from flat.
                      * 5.0 m/s² threshold = ~30° from horizontal. */
                     if (abs_x > 5.0f || abs_y > 5.0f)
                     {
                        if (abs_y >= abs_x)
                           android_app->detected_screen_rotation =
                                 (avg_y > 0.0f) ? 0 : 2;
                        else
                           android_app->detected_screen_rotation =
                                 (avg_x < 0.0f) ? 1 : 3;

                        android_app->gravity_calibrated = true;

                        /* Now that orientation is known, start rest
                         * position capture with correct rotation */
                        input_sensor_start_rest_capture();
                     }
                     else
                     {
                        /* Device is flat — reset and retry */
                        android_app->gravity_accum_x      = 0.0f;
                        android_app->gravity_accum_y      = 0.0f;
                        android_app->gravity_sample_count = 0;
                     }
                  }
               }
               break;
            case ASENSOR_TYPE_GYROSCOPE:
               /* ASensorEvent struct is mysterious - have to
                * read the raw 'data' field to get rate of
                * rotation... */
               android->gyroscope_state.x = event.data[0];
               android->gyroscope_state.y = event.data[1];
               android->gyroscope_state.z = event.data[2];
               break;
            default:
               break;
         }
      }
   }
}

static void android_input_reinit(void)
{
   struct android_app *android_app = (struct android_app*)g_android;
   uint32_t runloop_flags = runloop_get_flags();

   if (runloop_flags & RUNLOOP_FLAG_PAUSED)
   {
      /* When the app is paused (e.g. by opening the app switcher), Android may
       * destroy the presentation surface while retaining the OpenGL context or
       * Vulkan device. Recreate only the surface when supported before falling
       * back to a full video driver reinitialization. */
      video_driver_state_t *state = video_state_get_ptr();
      bool recreated              = false;

#ifdef HAVE_THREADS
      /* The callback null-checks the hook and reports failure, and the
       * dispatch reports failure when the worker is gone, so either one
       * falls through to the full reinitialization below. */
      recreated = (video_thread_texture_handle(state->context_data,
               android_ctx_create_surface_cb) != 0);
#else
      if (state->current_video_context.create_surface)
         recreated = state->current_video_context.create_surface(
               state->context_data);
#endif

      if (!recreated)
         command_event(CMD_EVENT_REINIT, NULL);
   }

   android_app_write_cmd(android_app, APP_CMD_REINIT_DONE);
}

/* Handle all events.
 */
static void android_input_poll(void *data)
{
   int ident;
   int timeout;
   struct android_app *android_app = (struct android_app*)g_android;
   android_input_t *android        = (android_input_t*)data;
   settings_t            *settings = config_get_ptr();

   /* Apply any text staged by the native (IME) keyboard. */
   android_keyboard_poll();

   /* Backgrounded (APP_CMD_PAUSE/STOP set RUNLOOP_FLAG_IDLE): there is
    * nothing to do until the OS delivers the next command, and the
    * looper will wake us for it. Block on it with -1 instead of the
    * short timeout, the same call android_run_events() makes for the
    * startup pump. Otherwise the runloop's 10 ms idle sleep has this
    * poll returning empty a hundred times a second, which is the
    * opposite of the "avoid draining battery" comment at the flag's
    * set site, and Android's doze accounting penalises exactly that.
    * First iteration blocks; once an event has woken us, drain the rest
    * without blocking so a burst (RESUME then INPUT_CHANGED) is handled
    * in one call. */
   timeout = settings->uints.input_block_timeout;
   if (runloop_state_get_ptr()->flags & RUNLOOP_FLAG_IDLE)
      timeout = -1;

   while ((ident =
            ALooper_pollOnce(timeout, NULL, NULL, NULL)) >= 0)
   {
      timeout = 0;
      switch (ident)
      {
         case LOOPER_ID_INPUT:
            android_input_poll_input(android);
            break;
         case LOOPER_ID_USER:
            android_input_poll_user(android);
            break;
         case LOOPER_ID_MAIN:
            android_input_poll_main_cmd();
            break;
      }

      if (android_app->destroyRequested != 0)
      {
         retroarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
         return;
      }

      if (android_app->reinitRequested != 0)
      {
         android_input_reinit();
         return;
      }
   }
}

/* Dismiss queued input from before the input driver exists. ALooper
 * reports the queue readable until every event has been finished, so
 * leaving them in place spins the caller and holds the window
 * unresponsive until the dispatcher raises an ANR. Events are finished
 * unhandled: there is nowhere to route them yet, and the framework's
 * fallback handling is preferable to dropping them silently. */
static void android_input_discard_events(struct android_app *android_app)
{
   AInputEvent *event = NULL;

   if (!android_app->inputQueue)
      return;

   while (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
   {
      if (AInputQueue_preDispatchEvent(android_app->inputQueue, event))
         continue;
      AInputQueue_finishEvent(android_app->inputQueue, event, 0);
   }
}

bool android_run_events(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;

   /* LOOPER_ID_USER needs no arm here: the sensor queue is attached by
    * the input driver, which does not exist while this pump runs. */
   switch (ALooper_pollOnce(-1, NULL, NULL, NULL))
   {
      case LOOPER_ID_MAIN:
         android_input_poll_main_cmd();
         break;
      case LOOPER_ID_INPUT:
         android_input_discard_events(android_app);
         break;
      default:
         break;
   }

   /* Check if we are exiting. */
   if (android_app->destroyRequested != 0)
   {
      retroarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
      return false;
   }

   if (android_app->reinitRequested != 0)
      android_input_reinit();

   return true;
}

static int16_t android_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   android_input_t *android           = (android_input_t*)data;

   /* The MOUSE/LIGHTGUN/POINTER paths below dereference the driver
    * data unconditionally. current_data can be NULL in the window
    * around driver teardown/reinit (surface loss on sleep/wake),
    * so fail closed rather than fault. */
   if (!android)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (     (binds[port][i].key && binds[port][i].key < RETROK_LAST)
                           && ANDROID_KEYBOARD_PORT_INPUT_PRESSED(binds[port], i))
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if (     (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                     && ANDROID_KEYBOARD_PORT_INPUT_PRESSED(binds[port], id)
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                     )
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && BIT_GET(android_key_state[ANDROID_KEYBOARD_PORT], rarch_keysym_lut[id]);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            /* Same mouse state is reported for all ports. */
            int val = 0;
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return android->mouse_l || android_check_quick_tap(android);
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return android->mouse_r;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return android->mouse_m;
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return android->mouse_x_viewport_screen;

                  val = android->mouse_x_delta;
                  android->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return android->mouse_y_viewport_screen;

                  val = android->mouse_y_delta;
                  android->mouse_y_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  val = android->mouse_wu;
                  android->mouse_wu = 0;
                  return val;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  val = android->mouse_wd;
                  android->mouse_wd = 0;
                  return val;
            }
         }
         break;
      case RETRO_DEVICE_LIGHTGUN:
         {
            /* Same lightgun state is reported for all ports. */
            int val = 0;
            switch (id)
            {
               /* Favor mouse for lightgun control. */
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                  if (android->mouse_activated)
                     return android->mouse_x_viewport_screen;
                  else if (idx < MAX_TOUCH)
                     return android->pointer[idx].x;
                  return 0;
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                  if (android->mouse_activated)
                     return android->mouse_y_viewport_screen;
                  else if (idx < MAX_TOUCH)
                     return android->pointer[idx].y;
                  return 0;
               /* Deprecated relative lightgun. */
               case RETRO_DEVICE_ID_LIGHTGUN_X:
                  val                    = android->mouse_x_delta;
                  android->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_LIGHTGUN_Y:
                  val                    = android->mouse_y_delta;
                  android->mouse_y_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
               case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
                  return android->mouse_m || android->pointer_count == 3;
               case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                  return android->mouse_r && android->mouse_l;
               case RETRO_DEVICE_ID_LIGHTGUN_START:
               case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
                  return android->mouse_r || android->pointer_count == 2;
               case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
                  return android->mouse_l || android_check_quick_tap(android) || android->pointer_count == 1;
               case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                  if (idx >= MAX_TOUCH)
                     return 0;
                  return input_driver_pointer_is_offscreen(android->pointer[idx].x, android->pointer[idx].y);
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         /* Same pointer state is reported for all ports.
          * idx is core/frontend-controlled: bounds-check it before
          * any pointer[] access. PRESSED already range-checks via
          * pointer_count (<= MAX_TOUCH). */
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               if (idx >= MAX_TOUCH)
                  return 0;
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return android->pointer[idx].full_x;
               return android->pointer[idx].confined_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               if (idx >= MAX_TOUCH)
                  return 0;
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return android->pointer[idx].full_y;
               return android->pointer[idx].confined_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               /* On mobile platforms, touches outside screen / core viewport are not reported. */
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return (idx < android->pointer_count) &&
                     (android->pointer[idx].full_x != -0x8000) &&
                     (android->pointer[idx].full_y != -0x8000);
               return (idx < android->pointer_count) &&
                  (android->pointer[idx].x != -0x8000) &&
                  (android->pointer[idx].y != -0x8000);
            case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
               if (idx >= MAX_TOUCH)
                  return 0;
               return input_driver_pointer_is_offscreen(android->pointer[idx].x, android->pointer[idx].y);
            case RETRO_DEVICE_ID_POINTER_COUNT:
               return android->pointer_count;
            case RARCH_DEVICE_ID_POINTER_BACK:
            {
               const struct retro_keybind *keyptr =
                  &input_autoconf_binds[0][RARCH_MENU_TOGGLE];
               if (keyptr->joykey == 0)
                  return ANDROID_KEYBOARD_INPUT_PRESSED(AKEYCODE_BACK);
            }
         }
         break;
   }

   return 0;
}

static void android_input_free_input(void *data)
{
   android_input_t *android = (android_input_t*)data;
   struct android_app *android_app = (struct android_app*)g_android;
   if (!android)
      return;

   if (android_app->sensorManager &&
       android_app->sensorEventQueue)
      ASensorManager_destroyEventQueue(android_app->sensorManager,
            android_app->sensorEventQueue);

   android_app->sensorEventQueue    = NULL;
   android_app->accelerometerSensor = NULL;
   android_app->gyroscopeSensor     = NULL;
   android_app->sensorManager       = NULL;
   android_app->sensor_state_mask   = 0;

   android_app->input_alive         = false;

#ifdef HAVE_DYLIB
   dylib_close((dylib_t)libandroid_handle);
   libandroid_handle = NULL;
#endif

   android_keycode_map_free((JNIEnv*)jni_thread_getenv());

   android_keyboard_free();
   free(data);
}

static uint64_t android_input_get_capabilities(void *data)
{
   return
        (1 << RETRO_DEVICE_JOYPAD)
      | (1 << RETRO_DEVICE_POINTER)
      | (1 << RETRO_DEVICE_MOUSE)
      | (1 << RETRO_DEVICE_KEYBOARD)
      | (1 << RETRO_DEVICE_LIGHTGUN)
      | (1 << RETRO_DEVICE_ANALOG);
}

static void android_input_enable_sensor_manager(struct android_app *android_app)
{
   if (!android_app->sensorManager)
      android_app->sensorManager = ASensorManager_getInstance();

   if (android_app->sensorManager)
   {
      if (!android_app->accelerometerSensor)
         android_app->accelerometerSensor =
            ASensorManager_getDefaultSensor(android_app->sensorManager,
               ASENSOR_TYPE_ACCELEROMETER);

      if (!android_app->gyroscopeSensor)
         android_app->gyroscopeSensor =
            ASensorManager_getDefaultSensor(android_app->sensorManager,
               ASENSOR_TYPE_GYROSCOPE);

      if (!android_app->sensorEventQueue)
         android_app->sensorEventQueue =
            ASensorManager_createEventQueue(android_app->sensorManager,
               android_app->looper, LOOPER_ID_USER, NULL, NULL);
   }
}

/**
 * Enable a single sensor, set its event rate, and update the bitmask.
 * Returns true on success.
 */
static bool android_enable_sensor(struct android_app *android_app,
      const ASensor *sensor, unsigned rate,
      uint64_t enable_bit, uint64_t disable_bit)
{
   if (!android_app->sensorEventQueue || !sensor)
      return false;

   if (ASensorEventQueue_enableSensor(
            android_app->sensorEventQueue, sensor) >= 0)
   {
      ASensorEventQueue_setEventRate(android_app->sensorEventQueue,
            sensor, (1000L / rate) * 1000);
      BIT64_CLEAR(android_app->sensor_state_mask, disable_bit);
      BIT64_SET(android_app->sensor_state_mask, enable_bit);
      return true;
   }

   return false;
}

static bool android_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   if (port == 0)
   {
      struct android_app *android_app = (struct android_app*)g_android;
      android_input_t *android        = (android_input_t*)data;

      if (!android)
         return false;

      if (event_rate == 0)
         event_rate = DEFAULT_ASENSOR_EVENT_RATE;

      switch (action)
      {
         case RETRO_SENSOR_ACCELEROMETER_ENABLE:
            if (!android_app->accelerometerSensor)
               android_input_enable_sensor_manager(android_app);
            if (android_enable_sensor(android_app,
                  android_app->accelerometerSensor, event_rate,
                  RETRO_SENSOR_ACCELEROMETER_ENABLE,
                  RETRO_SENSOR_ACCELEROMETER_DISABLE))
            {
               android_app->accelerometer_event_rate = event_rate;
               return true;
            }
            return false;

         case RETRO_SENSOR_ACCELEROMETER_DISABLE:
            if (android_app->sensorEventQueue &&
                  android_app->accelerometerSensor)
            {
               /* Note: Always update state even if disable fails, as we want
                * to stop attempting to read from a potentially broken sensor */
               ASensorEventQueue_disableSensor(android_app->sensorEventQueue,
                     android_app->accelerometerSensor);
            }

            android->accelerometer_state.x = 0.0f;
            android->accelerometer_state.y = 0.0f;
            android->accelerometer_state.z = 0.0f;

            BIT64_CLEAR(android_app->sensor_state_mask, RETRO_SENSOR_ACCELEROMETER_ENABLE);
            BIT64_SET(android_app->sensor_state_mask, RETRO_SENSOR_ACCELEROMETER_DISABLE);
            return true;

         case RETRO_SENSOR_GYROSCOPE_ENABLE:
            if (!android_app->gyroscopeSensor)
               android_input_enable_sensor_manager(android_app);
            if (android_enable_sensor(android_app,
                  android_app->gyroscopeSensor, event_rate,
                  RETRO_SENSOR_GYROSCOPE_ENABLE,
                  RETRO_SENSOR_GYROSCOPE_DISABLE))
            {
               android_app->gyroscope_event_rate = event_rate;
               return true;
            }
            return false;

         case RETRO_SENSOR_GYROSCOPE_DISABLE:
            if (android_app->sensorEventQueue &&
                  android_app->gyroscopeSensor)
            {
               /* Note: Always update state even if disable fails, as we want
                * to stop attempting to read from a potentially broken sensor */
               ASensorEventQueue_disableSensor(android_app->sensorEventQueue,
                     android_app->gyroscopeSensor);
            }

            android->gyroscope_state.x = 0.0f;
            android->gyroscope_state.y = 0.0f;
            android->gyroscope_state.z = 0.0f;

            BIT64_CLEAR(android_app->sensor_state_mask, RETRO_SENSOR_GYROSCOPE_ENABLE);
            BIT64_SET(android_app->sensor_state_mask, RETRO_SENSOR_GYROSCOPE_DISABLE);
            return true;

         default:
            break;
      }
   }

   return false;
}

static float android_input_get_sensor_input(void *data,
      unsigned port, unsigned id)
{
   if (port == 0)
   {
      android_input_t      *android      = (android_input_t*)data;

      switch (id)
      {
         case RETRO_SENSOR_ACCELEROMETER_X:
            return android->accelerometer_state.x / 9.80665f;
         case RETRO_SENSOR_ACCELEROMETER_Y:
            return android->accelerometer_state.y / 9.80665f;
         case RETRO_SENSOR_ACCELEROMETER_Z:
            return android->accelerometer_state.z / 9.80665f;
         case RETRO_SENSOR_GYROSCOPE_X:
            return android->gyroscope_state.x;
         case RETRO_SENSOR_GYROSCOPE_Y:
            return android->gyroscope_state.y;
         case RETRO_SENSOR_GYROSCOPE_Z:
            return android->gyroscope_state.z;
      }
   }

   return 0.0f;
}

static void android_input_grab_mouse(void *data, bool state)
{
   JNIEnv *env = jni_thread_getenv();

   if (!env || !g_android)
      return;

   if (g_android->inputGrabMouse)
      CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
            g_android->inputGrabMouse, state);
}

static void android_input_keypress_vibrate()
{
   static const int keyboard_press = 3;
   JNIEnv *env = (JNIEnv*)jni_thread_getenv();

   if (!env)
      return;

   CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
         g_android->doHapticFeedback, (jint)keyboard_press);
}

input_driver_t input_android = {
   android_input_init,
   android_input_poll,
   android_input_state,
   android_input_free_input,
   android_input_set_sensor_state,
   android_input_get_sensor_input,
   android_input_get_capabilities,
   "android",
   android_input_grab_mouse,
   NULL,
   android_input_keypress_vibrate
};
