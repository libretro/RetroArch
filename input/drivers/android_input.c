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

#include <unistd.h>
#include <dlfcn.h>

#include <android/keycodes.h>

#include <dynamic/dylib.h>
#include <retro_inline.h>
#include <string/stdstring.h>

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

#include "../../configuration.h"
#include "../../retroarch.h"

#define MAX_TOUCH 16
#define MAX_NUM_KEYBOARDS 3

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

/* If using an SDK lower than 24 then add missing relative axis codes */
#ifndef AMOTION_EVENT_AXIS_RELATIVE_X
#define AMOTION_EVENT_AXIS_RELATIVE_X 27
#endif

#ifndef AMOTION_EVENT_AXIS_RELATIVE_Y
#define AMOTION_EVENT_AXIS_RELATIVE_Y 28
#endif

/* Use this to enable/disable using the touch screen as mouse */
#define ENABLE_TOUCH_SCREEN_MOUSE 1

#define AKEYCODE_ASSIST 219

#define LAST_KEYCODE AKEYCODE_ASSIST

#define MAX_KEYS ((LAST_KEYCODE + 7) / 8)

/* First ports are used to keep track of gamepad states. 
 * Last port is used for keyboard state */
static uint8_t android_key_state[DEFAULT_MAX_PADS + 1][MAX_KEYS];

#define android_keyboard_port_input_pressed(binds, id) (BIT_GET(android_key_state[ANDROID_KEYBOARD_PORT], rarch_keysym_lut[(binds)[(id)].key]))

#define android_keyboard_input_pressed(key) (BIT_GET(android_key_state[0], (key)))

uint8_t *android_keyboard_state_get(unsigned port)
{
   return android_key_state[port];
}

static void android_keyboard_free(void)
{
   unsigned i, j;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
      for (j = 0; j < MAX_KEYS; j++)
         android_key_state[i][j] = 0;
}

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

struct input_pointer
{
   int16_t x, y;
   int16_t full_x, full_y;
};

static int pad_id1 = -1;
static int pad_id2 = -1;
static int kbd_id[MAX_NUM_KEYBOARDS];
static int kbd_num = 0;

enum
{
   AXIS_X = 0,
   AXIS_Y = 1,
   AXIS_Z = 11,
   AXIS_RZ = 14,
   AXIS_HAT_X = 15,
   AXIS_HAT_Y = 16,
   AXIS_LTRIGGER = 17,
   AXIS_RTRIGGER = 18,
   AXIS_GAS = 22,
   AXIS_BRAKE = 23
};

typedef struct state_device
{
   int id;
   int port;
   char name[256];
   uint16_t rumble_last_strength_strong;
   uint16_t rumble_last_strength_weak;
   uint16_t rumble_last_strength;
} state_device_t;

typedef struct android_input
{
   const input_device_driver_t *joypad;

   state_device_t pad_states[MAX_USERS];
   int16_t analog_state[MAX_USERS][MAX_AXIS];
   int8_t hat_state[MAX_USERS][2];

   unsigned pads_connected;
   sensor_t accelerometer_state;
   struct input_pointer pointer[MAX_TOUCH];
   unsigned pointer_count;
   int mouse_x_delta, mouse_y_delta;
   float mouse_x_prev, mouse_y_prev;
   int mouse_l, mouse_r, mouse_m, mouse_wu, mouse_wd;
   int64_t quick_tap_time;
} android_input_t;

static void frontend_android_get_version_sdk(int32_t *sdk);
static void frontend_android_get_name(char *s, size_t len);

bool (*engine_lookup_name)(char *buf,
      int *vendorId, int *productId, size_t size, int id);

void (*engine_handle_dpad)(android_input_t *, AInputEvent*, int, int);

static bool android_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate);

extern float AMotionEvent_getAxisValue(const AInputEvent* motion_event,
      int32_t axis, size_t pointer_idx);

static typeof(AMotionEvent_getAxisValue) *p_AMotionEvent_getAxisValue;

#define AMotionEvent_getAxisValue (*p_AMotionEvent_getAxisValue)

extern int32_t AMotionEvent_getButtonState(const AInputEvent* motion_event);

static typeof(AMotionEvent_getButtonState) *p_AMotionEvent_getButtonState;

#define AMotionEvent_getButtonState (*p_AMotionEvent_getButtonState)

#ifdef HAVE_DYNAMIC
static void *libandroid_handle;
#endif

static bool android_input_lookup_name_prekitkat(char *buf,
      int *vendorId, int *productId, size_t size, int id)
{
   jobject name      = NULL;
   jmethodID getName = NULL;
   jobject device    = NULL;
   jmethodID method  = NULL;
   jclass    class   = 0;
   const char *str   = NULL;
   JNIEnv     *env   = (JNIEnv*)jni_thread_getenv();

   if (!env)
      goto error;

   RARCH_LOG("Using old lookup");

   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      goto error;

   GET_STATIC_METHOD_ID(env, method, class, "getDevice",
         "(I)Landroid/view/InputDevice;");
   if (!method)
      goto error;

   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
   {
      RARCH_ERR("Failed to find device for ID: %d\n", id);
      goto error;
   }

   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      goto error;

   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
   {
      RARCH_ERR("Failed to find name for device ID: %d\n", id);
      goto error;
   }

   buf[0] = '\0';

   str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);

   RARCH_LOG("device name: %s\n", buf);

   return true;
error:
   return false;
}

static bool android_input_lookup_name(char *buf,
      int *vendorId, int *productId, size_t size, int id)
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
      goto error;

   RARCH_LOG("Using new lookup");

   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      goto error;

   GET_STATIC_METHOD_ID(env, method, class, "getDevice",
         "(I)Landroid/view/InputDevice;");
   if (!method)
      goto error;

   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
   {
      RARCH_ERR("Failed to find device for ID: %d\n", id);
      goto error;
   }

   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      goto error;

   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
   {
      RARCH_ERR("Failed to find name for device ID: %d\n", id);
      goto error;
   }

   buf[0] = '\0';

   str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);

   RARCH_LOG("device name: %s\n", buf);

   GET_METHOD_ID(env, getVendorId, class, "getVendorId", "()I");
   if (!getVendorId)
      goto error;

   CALL_INT_METHOD(env, *vendorId, device, getVendorId);

   RARCH_LOG("device vendor id: %d\n", *vendorId);

   GET_METHOD_ID(env, getProductId, class, "getProductId", "()I");
   if (!getProductId)
      goto error;

   *productId = 0;
   CALL_INT_METHOD(env, *productId, device, getProductId);

   RARCH_LOG("device product id: %d\n", *productId);

   return true;
error:
   return false;
}

static void android_input_poll_main_cmd(void)
{
   int8_t cmd;
   struct android_app *android_app = (struct android_app*)g_android;

   if (read(android_app->msgread, &cmd, sizeof(cmd)) != sizeof(cmd))
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
         {
            RARCH_LOG("Attaching input queue to looper");
            AInputQueue_attachLooper(android_app->inputQueue,
                  android_app->looper, LOOPER_ID_INPUT, NULL,
                  NULL);
         }

         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         break;

      case APP_CMD_INIT_WINDOW:
         slock_lock(android_app->mutex);
         android_app->window = android_app->pendingWindow;
         android_app->reinitRequested = 1;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         break;

      case APP_CMD_SAVE_STATE:
         slock_lock(android_app->mutex);
         android_app->stateSaved = 1;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_RESUME:
      case APP_CMD_START:
      case APP_CMD_PAUSE:
      case APP_CMD_STOP:
         slock_lock(android_app->mutex);
         android_app->activityState = cmd;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_CONFIG_CHANGED:
         AConfiguration_fromAssetManager(android_app->config,
               android_app->activity->assetManager);
         break;
      case APP_CMD_TERM_WINDOW:
         slock_lock(android_app->mutex);

         /* The window is being hidden or closed, clean it up. */
         /* terminate display/EGL context here */

         android_app->window = NULL;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_GAINED_FOCUS:
         {
            bool boolean = false;

            rarch_ctl(RARCH_CTL_SET_PAUSED, &boolean);
            rarch_ctl(RARCH_CTL_SET_IDLE,   &boolean);
            video_driver_unset_stub_frame();

            if ((android_app->sensor_state_mask
                     & (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_ENABLE))
                  && android_app->accelerometerSensor == NULL)
               input_sensor_set_state(0,
                     RETRO_SENSOR_ACCELEROMETER_ENABLE,
                     android_app->accelerometer_event_rate);
         }
         slock_lock(android_app->mutex);
         android_app->unfocused = false;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;
      case APP_CMD_LOST_FOCUS:
         {
            bool boolean = true;

            rarch_ctl(RARCH_CTL_SET_PAUSED, &boolean);
            rarch_ctl(RARCH_CTL_SET_IDLE,   &boolean);
            video_driver_set_stub_frame();

            /* Avoid draining battery while app is not being used. */
            if ((android_app->sensor_state_mask
                     & (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_ENABLE))
                  && android_app->accelerometerSensor != NULL
                  )
               input_sensor_set_state(0,
                     RETRO_SENSOR_ACCELEROMETER_DISABLE,
                     android_app->accelerometer_event_rate);
         }
         slock_lock(android_app->mutex);
         android_app->unfocused = true;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_DESTROY:
         RARCH_LOG("APP_CMD_DESTROY\n");
         android_app->destroyRequested = 1;
         break;
      case APP_CMD_VIBRATE_KEYPRESS:
      {
         JNIEnv *env = (JNIEnv*)jni_thread_getenv();

         if (env && g_android && g_android->doVibrate)
         {
            CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
                  g_android->doVibrate, (jint)-1, (jint)RETRO_RUMBLE_STRONG, (jint)255, (jint)1);
         }
         break;
      }
   }
}

static void engine_handle_dpad_default(android_input_t *android,
      AInputEvent *event, int port, int source)
{
   size_t motion_ptr = AMotionEvent_getAction(event) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x           = AMotionEvent_getX(event, motion_ptr);
   float y           = AMotionEvent_getY(event, motion_ptr);

   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);
}

#ifdef HAVE_DYNAMIC
static void engine_handle_dpad_getaxisvalue(android_input_t *android,
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

   android->hat_state[port][0] = (int)hatx;
   android->hat_state[port][1] = (int)haty;

   /* XXX: this could be a loop instead, but do we really want to
    * loop through every axis? */
   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);
   android->analog_state[port][2] = (int16_t)(z * 32767.0f);
   android->analog_state[port][3] = (int16_t)(rz * 32767.0f);
#if 0
   android->analog_state[port][4] = (int16_t)(hatx * 32767.0f);
   android->analog_state[port][5] = (int16_t)(haty * 32767.0f);
#endif
   android->analog_state[port][6] = (int16_t)(ltrig * 32767.0f);
   android->analog_state[port][7] = (int16_t)(rtrig * 32767.0f);
   android->analog_state[port][8] = (int16_t)(brake * 32767.0f);
   android->analog_state[port][9] = (int16_t)(gas * 32767.0f);
}
#endif

static bool android_input_init_handle(void)
{
#ifdef HAVE_DYNAMIC
   if (libandroid_handle != NULL) /* already initialized */
      return true;
#ifdef ANDROID_AARCH64
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
   {
      RARCH_LOG("Set engine_handle_dpad to 'Get Axis Value' (for reading extra analog sticks)");
      engine_handle_dpad = engine_handle_dpad_getaxisvalue;
   }

   p_AMotionEvent_getButtonState = dlsym(RTLD_DEFAULT,"AMotionEvent_getButtonState");
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

   android->pads_connected = 0;
   android->quick_tap_time = 0;
   android->joypad         = input_joypad_init_driver(joypad_driver, android);

   input_keymaps_init_keyboard_lut(rarch_key_map_android);

   frontend_android_get_version_sdk(&sdk);

   RARCH_LOG("sdk version: %d\n", sdk);

   if (sdk >= 19)
      engine_lookup_name = android_input_lookup_name;
   else
      engine_lookup_name = android_input_lookup_name_prekitkat;

   engine_handle_dpad         = engine_handle_dpad_default;

   if (!android_input_init_handle())
   {
      RARCH_WARN("Unable to open libandroid.so\n");
   }

   android_app->input_alive = true;

   return android;
}

static int android_check_quick_tap(android_input_t *android)
{
   /* Check if the touch screen has been been quick tapped
    * and then not touched again for 200ms
    * If so then return true and deactivate quick tap timer */
   retro_time_t now = cpu_features_get_time_usec();
   if (android->quick_tap_time && (now/1000 - android->quick_tap_time/1000000) >= 200)
   {
      android->quick_tap_time = 0;
      return 1;
   }

   return 0;
}

static int16_t android_mouse_state(android_input_t *android, unsigned id)
{
   int val = 0;
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         val = android->mouse_l || android_check_quick_tap(android);
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         val = android->mouse_r;
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         val = android->mouse_m;
         break;
      case RETRO_DEVICE_ID_MOUSE_X:
         val = android->mouse_x_delta;
         android->mouse_x_delta = 0; /* flush delta after it has been read */
         break;
      case RETRO_DEVICE_ID_MOUSE_Y:
         val = android->mouse_y_delta; /* flush delta after it has been read */
         android->mouse_y_delta = 0;
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         val = android->mouse_wu;
         android->mouse_wu = 0;
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         val = android->mouse_wd;
         android->mouse_wd = 0;
         break;
   }

   return val;
}

static int16_t android_lightgun_device_state(android_input_t *android, unsigned id)
{
   int val = 0;
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         val = android->mouse_x_delta;
         android->mouse_x_delta = 0; /* flush delta after it has been read */
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         val = android->mouse_y_delta; /* flush delta after it has been read */
         android->mouse_y_delta = 0;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         val = android->mouse_l || android_check_quick_tap(android);
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         val = android->mouse_m;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         val = android->mouse_r;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         val = android->mouse_m && android->mouse_r;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         val = android->mouse_m && android->mouse_l;
         break;
   }

   return val;
}

static INLINE void android_mouse_calculate_deltas(android_input_t *android,
      AInputEvent *event,size_t motion_ptr)
{
   /* Adjust mouse speed based on ratio
    * between core resolution and system resolution */
   float x, y;
   float                        x_scale = 1;
   float                        y_scale = 1;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   if (av_info)
   {
      video_viewport_t          *custom_vp   = video_viewport_get_custom();
      const struct retro_game_geometry *geom = (const struct retro_game_geometry*)&av_info->geometry;
      x_scale = 2 * (float)geom->base_width / (float)custom_vp->width;
      y_scale = 2 * (float)geom->base_height / (float)custom_vp->height;
   }

   /* This axis is only available on Android Nougat and on Android devices with NVIDIA extensions */
   x = AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_RELATIVE_X, motion_ptr);
   y = AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_RELATIVE_Y, motion_ptr);

   /* If AXIS_RELATIVE had 0 values it might be because we're not running Android Nougat or on a device
    * with NVIDIA extension, so re-calculate deltas based on AXIS_X and AXIS_Y. This has limitations
    * compared to AXIS_RELATIVE because once the Android mouse cursor hits the edge of the screen it is
    * not possible to move the in-game mouse any further in that direction.
    */
   if (!x && !y)
   {
      x = (AMotionEvent_getX(event, motion_ptr) - android->mouse_x_prev);
      y = (AMotionEvent_getY(event, motion_ptr) - android->mouse_y_prev);
      android->mouse_x_prev = AMotionEvent_getX(event, motion_ptr);
      android->mouse_y_prev = AMotionEvent_getY(event, motion_ptr);
   }

   android->mouse_x_delta = ceil(x) * x_scale;
   android->mouse_y_delta = ceil(y) * y_scale;
}

static INLINE int android_input_poll_event_type_motion(
      android_input_t *android, AInputEvent *event,
      int port, int source)
{
   int getaction, action;
   size_t motion_ptr;
   bool keyup;
   int btn;

   /* Only handle events from a touchscreen or mouse */
   if (!(source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_STYLUS | AINPUT_SOURCE_MOUSE)))
      return 1;

   getaction  = AMotionEvent_getAction(event);
   action     = getaction & AMOTION_EVENT_ACTION_MASK;
   motion_ptr = getaction >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   keyup      = (
         action == AMOTION_EVENT_ACTION_UP ||
         action == AMOTION_EVENT_ACTION_CANCEL ||
         action == AMOTION_EVENT_ACTION_POINTER_UP) ||
      (source == AINPUT_SOURCE_MOUSE &&
       action != AMOTION_EVENT_ACTION_DOWN);

   /* If source is mouse then calculate button state
    * and mouse deltas and don't process as touchscreen event */
   if (source == AINPUT_SOURCE_MOUSE)
   {
      /* getButtonState requires API level 14 */
      if (p_AMotionEvent_getButtonState)
      {
         btn = (int)AMotionEvent_getButtonState(event);

         android->mouse_l = (btn & AMOTION_EVENT_BUTTON_PRIMARY);
         android->mouse_r = (btn & AMOTION_EVENT_BUTTON_SECONDARY);
         android->mouse_m = (btn & AMOTION_EVENT_BUTTON_TERTIARY);

         btn = (int)AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, motion_ptr);

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

      android_mouse_calculate_deltas(android,event,motion_ptr);

      return 0;
   }

   if (keyup && motion_ptr < MAX_TOUCH)
   {
      if (action == AMOTION_EVENT_ACTION_UP && ENABLE_TOUCH_SCREEN_MOUSE)
      {
         /* If touchscreen was pressed for less than 200ms
          * then register time stamp of a quick tap */
         if ((AMotionEvent_getEventTime(event)-AMotionEvent_getDownTime(event))/1000000 < 200)
            android->quick_tap_time = AMotionEvent_getEventTime(event);
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
      int pointer_max = MIN(AMotionEvent_getPointerCount(event), MAX_TOUCH);
      settings_t *settings = config_get_ptr();

      if (settings && settings->bools.vibrate_on_keypress && action != AMOTION_EVENT_ACTION_MOVE)
         android_app_write_cmd(g_android, APP_CMD_VIBRATE_KEYPRESS);

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
            android->mouse_l = 1;
         }
      }

      if ((action == AMOTION_EVENT_ACTION_MOVE || action == AMOTION_EVENT_ACTION_HOVER_MOVE) && ENABLE_TOUCH_SCREEN_MOUSE)
         android_mouse_calculate_deltas(android,event,motion_ptr);

      for (motion_ptr = 0; motion_ptr < pointer_max; motion_ptr++)
      {
         struct video_viewport vp;
         float x = AMotionEvent_getX(event, motion_ptr);
         float y = AMotionEvent_getY(event, motion_ptr);

         vp.x                        = 0;
         vp.y                        = 0;
         vp.width                    = 0;
         vp.height                   = 0;
         vp.full_width               = 0;
         vp.full_height              = 0;

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

   return 0;
}

bool is_keyboard_id(int id)
{
   for(int i=0; i<kbd_num; i++)
      if (id == kbd_id[i]) return true;

   return false;
}

static INLINE void android_input_poll_event_type_keyboard(
      AInputEvent *event, int keycode, int *handled)
{
   int keydown           = (AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN);
   unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(keycode);
   /* Set keyboard modifier based on shift,ctrl and alt state */
   uint16_t mod          = 0;
   int meta              = AKeyEvent_getMetaState(event);

   if (meta & AMETA_ALT_ON)
      mod |= RETROKMOD_ALT;
   if (meta & AMETA_CTRL_ON)
      mod |= RETROKMOD_CTRL;
   if (meta & AMETA_SHIFT_ON)
      mod |= RETROKMOD_SHIFT;

   input_keyboard_event(keydown, keyboardcode, keyboardcode, mod, RETRO_DEVICE_KEYBOARD);

   if ((keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN))
      *handled = 0;
}

static INLINE void android_input_poll_event_type_key(
      struct android_app *android_app,
      AInputEvent *event, int port, int keycode, int source,
      int type_event, int *handled)
{
   uint8_t *buf = android_key_state[port];
   int action   = AKeyEvent_getAction(event);

   /* some controllers send both the up and down events at once
    * when the button is released for "special" buttons, like menu buttons
    * work around that by only using down events for meta keys (which get
    * cleared every poll anyway)
    */
   switch (action)
   {
      case AKEY_EVENT_ACTION_UP:
         BIT_CLEAR(buf, keycode);
         break;
      case AKEY_EVENT_ACTION_DOWN:
         BIT_SET(buf, keycode);
         break;
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
            AINPUT_SOURCE_TOUCHPAD))
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

#ifdef HAVE_DYNAMIC
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
#endif

static void handle_hotplug(android_input_t *android,
      struct android_app *android_app, int *port, int id,
      int source)
{
   char device_name[256];
   char device_model[256];
   char name_buf[256];
   int vendorId                 = 0;
   int productId                = 0;

   device_name[0] = device_model[0] = name_buf[0] = '\0';

   frontend_android_get_name(device_model, sizeof(device_model));

   RARCH_LOG("Device model: (%s).\n", device_model);

   if (*port > DEFAULT_MAX_PADS)
   {
      RARCH_ERR("Max number of pads reached.\n");
      return;
   }

   if (!engine_lookup_name(device_name, &vendorId,
            &productId, sizeof(device_name), id))
   {
      RARCH_ERR("Could not look up device name or IDs.\n");
      return;
   }

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
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
#if 0
         RARCH_LOG("- Pads Mapped: %d\n- Device Name: %s\n- IDS: %d, %d, %d",
               android->pads_connected, device_name, id, pad_id1, pad_id2);
#endif
         /* remove the remote or virtual controller device if it is mapped */
         if (strstr(android->pad_states[0].name,"SHIELD Remote") ||
            strstr(android->pad_states[0].name,"SHIELD Virtual Controller"))
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
            strlcpy (name_buf, "SHIELD Virtual Controller", sizeof(name_buf));
         else
            strlcpy (name_buf, "NVIDIA SHIELD Controller", sizeof(name_buf));

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
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;
         strlcpy (name_buf, "NVIDIA SHIELD Portable", sizeof(name_buf));
      }
   }

   else if (strstr(device_model, "SHIELD") && (
      strstr(device_name, "Virtual") || strstr(device_name, "gpio") ||
      strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.03")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
         if (strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.03")
             && android->pads_connected==0)
            pad_id1 = id;
         else if (strstr(device_name, "Virtual") || strstr(device_name, "gpio"))
         {
            id = pad_id1;
            return;
         }
         strlcpy (name_buf, "NVIDIA SHIELD Gamepad", sizeof(name_buf));
      }
   }

   /* Other ATV Devices
    * Add other common ATV devices that will follow the Android
    * Gaempad convention as "Android Gamepad"
    */
    /* to-do: add DS4 on Bravia ATV */
   else if (strstr(device_name, "NVIDIA"))
      strlcpy (name_buf, "Android Gamepad", sizeof(name_buf));

   /* GPD XD
    * This is a simple hack, basically groups the "back"
    * button with the rest of the gamepad
    */
   else if (strstr(device_model, "XD") && (
      strstr(device_name, "Virtual") || strstr(device_name, "rk29-keypad") ||
      strstr(device_name,"Playstation3") || strstr(device_name,"XBOX")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;

         strlcpy (name_buf, "GPD XD", sizeof(name_buf));
         *port = 0;
      }
   }

   /* XPERIA Play
    * This device is composed of two hid devices
    * We make it look like one device
    */
   else if(
            (
               strstr(device_model, "R800x") ||
               strstr(device_model, "R800at") ||
               strstr(device_model, "R800i") ||
               strstr(device_model, "R800a") ||
               strstr(device_model, "R800") ||
               strstr(device_model, "Xperia Play") ||
               strstr(device_model, "Play") ||
               strstr(device_model, "SO-01D")
            ) && (
               strstr(device_name, "keypad-game-zeus") ||
               strstr(device_name, "keypad-zeus") ||
               strstr(device_name, "Android Gamepad")
            )
         )
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;

         strlcpy (name_buf, "XPERIA Play", sizeof(name_buf));
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
      RARCH_LOG("ARCHOS GAMEPAD Detected: %s\n", device_model);
      {
         if ( pad_id1 < 0 )
            pad_id1 = id;
         else
            pad_id2 = id;

         if ( pad_id2 > 0)
            return;

         strlcpy (name_buf, "ARCHOS GamePad", sizeof(name_buf));
         *port = 0;
      }
   }

   /* Amazon Fire TV & Fire stick */
   else if (strstr(device_model, "AFTB") || strstr(device_model, "AFTT") ||
           strstr(device_model, "AFTS") || strstr(device_model, "AFTM") ||
           strstr(device_model, "AFTRS"))
   {
      RARCH_LOG("Special Device Detected: %s\n", device_model);
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
      strlcpy(name_buf, "iControlPad HID Joystick profile", sizeof(name_buf));

   else if (strstr(device_name, "TTT THT Arcade console 2P USB Play"))
   {
      if (*port == 0)
         strlcpy(name_buf, "TTT THT Arcade (User 1)", sizeof(name_buf));
      else if (*port == 1)
         strlcpy(name_buf, "TTT THT Arcade (User 2)", sizeof(name_buf));
   }
   else if (strstr(device_name, "MOGA"))
      strlcpy(name_buf, "Moga IME", sizeof(name_buf));

   /* If device is keyboard only and didn't match any of the devices above
    * then assume it is a keyboard, register the id, and return unless the
    * maximum number of keyboards are already registered. */
   else if (source == AINPUT_SOURCE_KEYBOARD && kbd_num < MAX_NUM_KEYBOARDS)
   {
      kbd_id[kbd_num] = id;
      kbd_num++;
      return;
   }

   /* if device was not keyboard only, yet did not match any of the devices
    * then try to autoconfigure as gamepad based on device_name. */
   else if (!string_is_empty(device_name))
      strlcpy(name_buf, device_name, sizeof(name_buf));

   if (strstr(android_app->current_ime, "net.obsidianx.android.mogaime"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));
   else if (strstr(android_app->current_ime, "com.ccpcreations.android.WiiUseAndroid"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));
   else if (strstr(android_app->current_ime, "com.hexad.bluezime"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));

   if (*port < 0)
      *port = android->pads_connected;

   input_autoconfigure_connect(
         name_buf,
         NULL,
         android_joypad.ident,
         *port,
         vendorId,
         productId);

   android->pad_states[android->pads_connected].id   = id;
   android->pad_states[android->pads_connected].port = *port;

   strlcpy(android->pad_states[*port].name, name_buf,
         sizeof(android->pad_states[*port].name));

   android->pads_connected++;
}

static int android_input_get_id(AInputEvent *event)
{
   int id = AInputEvent_getDeviceId(event);

   if (id == pad_id2)
      id = pad_id1;

   return id;
}

static void android_input_poll_input(android_input_t *android)
{
   AInputEvent              *event = NULL;
   struct android_app *android_app = (struct android_app*)g_android;

   /* Read all pending events. */
   while (AInputQueue_hasEvents(android_app->inputQueue))
   {
      while (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
      {
         int32_t   handled = 1;
         int predispatched = AInputQueue_preDispatchEvent(android_app->inputQueue, event);
         int        source = AInputEvent_getSource(event);
         int    type_event = AInputEvent_getType(event);
         int            id = android_input_get_id(event);
         int          port = android_input_get_id_port(android, id, source);

         if (port < 0 && !is_keyboard_id(id))
            handle_hotplug(android, android_app,
            &port, id, source);

         switch (type_event)
         {
            case AINPUT_EVENT_TYPE_MOTION:
               if (android_input_poll_event_type_motion(android, event,
                        port, source))
                  engine_handle_dpad(android, event, port, source);
               break;
            case AINPUT_EVENT_TYPE_KEY:
               {
                  int keycode = AKeyEvent_getKeyCode(event);

                  if (is_keyboard_id(id))
                  {
                     if (!predispatched)
                     {
                        android_input_poll_event_type_keyboard(event, keycode, &handled);
                        android_input_poll_event_type_key(android_app, event, ANDROID_KEYBOARD_PORT, keycode, source, type_event, &handled);
                     }
                  }
                  else
                     android_input_poll_event_type_key(android_app,
                        event, port, keycode, source, type_event, &handled);
               }
               break;
         }

         if (!predispatched)
            AInputQueue_finishEvent(android_app->inputQueue, event,
                  handled);
      }
   }
}

static void android_input_poll_user(android_input_t *android)
{
   struct android_app *android_app = (struct android_app*)g_android;

   if ((android_app->sensor_state_mask & (UINT64_C(1) <<
               RETRO_SENSOR_ACCELEROMETER_ENABLE))
         && android_app->accelerometerSensor)
   {
      ASensorEvent event;
      while (ASensorEventQueue_getEvents(android_app->sensorEventQueue, &event, 1) > 0)
      {
         android->accelerometer_state.x = event.acceleration.x;
         android->accelerometer_state.y = event.acceleration.y;
         android->accelerometer_state.z = event.acceleration.z;
      }
   }
}

static void android_input_poll_memcpy(android_input_t *android)
{
   unsigned i, j;
   struct android_app *android_app = (struct android_app*)g_android;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      for (j = 0; j < 2; j++)
         android_app->hat_state[i][j]    = android->hat_state[i][j];
      for (j = 0; j < MAX_AXIS; j++)
         android_app->analog_state[i][j] = android->analog_state[i][j];
   }
}

static bool android_input_key_pressed(android_input_t *android, int key)
{
   uint64_t joykey;
   uint32_t joyaxis;
   rarch_joypad_info_t joypad_info;
   joypad_info.joy_idx        = 0;
   joypad_info.auto_binds     = input_autoconf_binds[0];
   joypad_info.axis_threshold = *
      (input_driver_get_float(INPUT_ACTION_AXIS_THRESHOLD));

   if((key < RARCH_BIND_LIST_END)
         && android_keyboard_port_input_pressed(input_config_binds[0],
            key))
      return true;

   joykey                     = 
      (input_config_binds[0][key].joykey != NO_BTN)
      ? input_config_binds[0][key].joykey 
      : joypad_info.auto_binds[key].joykey;
   joyaxis                    = 
      (input_config_binds[0][key].joyaxis != AXIS_NONE)
      ? input_config_binds[0][key].joyaxis 
      : joypad_info.auto_binds[key].joyaxis;

   if ((uint16_t)joykey != NO_BTN && android->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
      return true;
   if (((float)abs(android->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
      return true;
   return false;
}

/* Handle all events. If our activity is in pause state,
 * block until we're unpaused.
 */
static void android_input_poll(void *data)
{
   settings_t *settings = config_get_ptr();
   int ident;
   struct android_app *android_app = (struct android_app*)g_android;
   android_input_t *android        = (android_input_t*)data;

   while ((ident =
            ALooper_pollAll((input_config_binds[0][RARCH_PAUSE_TOGGLE].valid 
               && android_input_key_pressed(android, RARCH_PAUSE_TOGGLE))
               ? -1 : settings->uints.input_block_timeout,
               NULL, NULL, NULL)) >= 0)
   {
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
         rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
         return;
      }

      if (android_app->reinitRequested != 0)
      {
         if (rarch_ctl(RARCH_CTL_IS_PAUSED, NULL))
            command_event(CMD_EVENT_REINIT, NULL);
         android_app_write_cmd(android_app, APP_CMD_REINIT_DONE);
         return;
      }
   }

   if (android_app->input_alive)
      android_input_poll_memcpy(android);
}

bool android_run_events(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;

   if (ALooper_pollOnce(-1, NULL, NULL, NULL) == LOOPER_ID_MAIN)
      android_input_poll_main_cmd();

   /* Check if we are exiting. */
   if (android_app->destroyRequested != 0)
   {
      rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
      return false;
   }

   if (android_app->reinitRequested != 0)
   {
      if (rarch_ctl(RARCH_CTL_IS_PAUSED, NULL))
         command_event(CMD_EVENT_REINIT, NULL);
      android_app_write_cmd(android_app, APP_CMD_REINIT_DONE);
   }

   return true;
}

static int16_t android_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds, unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   android_input_t *android           = (android_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint64_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;
               if ((uint16_t)joykey != NO_BTN && android->joypad->button(
                        joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(android->joypad->axis(
                              joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
               if (android_keyboard_port_input_pressed(binds[port], i))
                  ret |= (1 << i);
            }
            return ret;
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;
            if ((uint16_t)joykey != NO_BTN && android->joypad->button(
                     joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(android->joypad->axis(
                           joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
            if (android_keyboard_port_input_pressed(binds[port], id))
               return true;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(android->joypad, joypad_info,
                  port, idx, id, binds[port]);
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && BIT_GET(android_key_state[ANDROID_KEYBOARD_PORT], rarch_keysym_lut[id]);
      case RETRO_DEVICE_MOUSE:
         return android_mouse_state(android, id);
      case RETRO_DEVICE_LIGHTGUN:
         return android_lightgun_device_state(android, id);
      case RETRO_DEVICE_POINTER:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return android->pointer[idx].x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return android->pointer[idx].y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (idx < android->pointer_count) &&
                  (android->pointer[idx].x != -0x8000) &&
                  (android->pointer[idx].y != -0x8000);
            case RETRO_DEVICE_ID_POINTER_COUNT:
               return android->pointer_count;
            case RARCH_DEVICE_ID_POINTER_BACK:
            {
               const struct retro_keybind *keyptr = &input_autoconf_binds[0][RARCH_MENU_TOGGLE];
               if (keyptr->joykey == 0)
                  return android_keyboard_input_pressed(AKEYCODE_BACK);
            }
         }
         break;
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return android->pointer[idx].full_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return android->pointer[idx].full_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (idx < android->pointer_count) &&
                  (android->pointer[idx].full_x != -0x8000) &&
                  (android->pointer[idx].full_y != -0x8000);
            case RETRO_DEVICE_ID_POINTER_COUNT:
               return android->pointer_count;
            case RARCH_DEVICE_ID_POINTER_BACK:
               {
                  const struct retro_keybind *keyptr = &input_autoconf_binds[0][RARCH_MENU_TOGGLE];
                  if (keyptr->joykey == 0)
                     return android_keyboard_input_pressed(AKEYCODE_BACK);
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

   if (android_app->sensorManager)
      ASensorManager_destroyEventQueue(android_app->sensorManager,
            android_app->sensorEventQueue);

   if (android->joypad)
      android->joypad->destroy();
   android->joypad = NULL;

   android_app->input_alive = false;

#ifdef HAVE_DYNAMIC
   dylib_close((dylib_t)libandroid_handle);
   libandroid_handle = NULL;
#endif

   android_keyboard_free();
   free(data);
}

static uint64_t android_input_get_capabilities(void *data)
{
   (void)data;

   return
      (1 << RETRO_DEVICE_JOYPAD)  |
      (1 << RETRO_DEVICE_POINTER) |
      (1 << RETRO_DEVICE_KEYBOARD)  |
      (1 << RETRO_DEVICE_LIGHTGUN)  |
      (1 << RETRO_DEVICE_ANALOG);
}

static void android_input_enable_sensor_manager(struct android_app *android_app)
{
   android_app->sensorManager = ASensorManager_getInstance();
   android_app->accelerometerSensor =
      ASensorManager_getDefaultSensor(android_app->sensorManager,
         ASENSOR_TYPE_ACCELEROMETER);
   android_app->sensorEventQueue =
      ASensorManager_createEventQueue(android_app->sensorManager,
         android_app->looper, LOOPER_ID_USER, NULL, NULL);
}

static bool android_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   struct android_app *android_app = (struct android_app*)g_android;

   if (event_rate == 0)
      event_rate = 60;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         if (!android_app->accelerometerSensor)
            android_input_enable_sensor_manager(android_app);

         if (android_app->accelerometerSensor)
            ASensorEventQueue_enableSensor(android_app->sensorEventQueue,
                  android_app->accelerometerSensor);

         /* Events per second (in microseconds). */
         if (android_app->accelerometerSensor)
            ASensorEventQueue_setEventRate(android_app->sensorEventQueue,
                  android_app->accelerometerSensor, (1000L / event_rate)
                  * 1000);

         BIT64_CLEAR(android_app->sensor_state_mask, RETRO_SENSOR_ACCELEROMETER_DISABLE);
         BIT64_SET(android_app->sensor_state_mask, RETRO_SENSOR_ACCELEROMETER_ENABLE);
         return true;

      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         if (android_app->accelerometerSensor)
            ASensorEventQueue_disableSensor(android_app->sensorEventQueue,
                  android_app->accelerometerSensor);

         BIT64_CLEAR(android_app->sensor_state_mask, RETRO_SENSOR_ACCELEROMETER_ENABLE);
         BIT64_SET(android_app->sensor_state_mask, RETRO_SENSOR_ACCELEROMETER_DISABLE);
         return true;
      default:
         return false;
   }

   return false;
}

static float android_input_get_sensor_input(void *data,
      unsigned port,unsigned id)
{
   android_input_t      *android      = (android_input_t*)data;

   switch (id)
   {
      case RETRO_SENSOR_ACCELEROMETER_X:
         return android->accelerometer_state.x;
      case RETRO_SENSOR_ACCELEROMETER_Y:
         return android->accelerometer_state.y;
      case RETRO_SENSOR_ACCELEROMETER_Z:
         return android->accelerometer_state.z;
   }

   return 0;
}

static const input_device_driver_t *android_input_get_joypad_driver(void *data)
{
   android_input_t *android = (android_input_t*)data;
   if (!android)
      return NULL;
   return android->joypad;
}

static void android_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static void android_input_set_rumble_internal(
      uint16_t strength,
      uint16_t *last_strength_strong,
      uint16_t *last_strength_weak,
      uint16_t *last_strength,
      int8_t   id,
      enum retro_rumble_effect effect
      )
{
   JNIEnv *env           = (JNIEnv*)jni_thread_getenv();
   uint16_t new_strength = 0;

   if (!env)
      return;

   if (effect == RETRO_RUMBLE_STRONG)
   {
      new_strength          = strength | *last_strength_weak;
      *last_strength_strong = strength;
   }
   else if (effect == RETRO_RUMBLE_WEAK)
   {
      new_strength         = strength | *last_strength_strong;
      *last_strength_weak  = strength;
   }

   if (new_strength != *last_strength)
   {
      /* trying to send this value as a JNI param without 
       * storing it first was causing 0 to be seen on the other side ?? */
      int strength_final   = (255.0f / 65535.0f) * (float)new_strength;

      CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
            g_android->doVibrate, (jint)id, (jint)RETRO_RUMBLE_STRONG, (jint)strength_final, (jint)0);

      *last_strength = new_strength;
   }
}

static bool android_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   settings_t *settings = config_get_ptr();

   if (!g_android || !g_android->doVibrate)
      return false;

   if (settings->bools.enable_device_vibration)
   {
      static uint16_t last_strength_strong = 0;
      static uint16_t last_strength_weak   = 0;
      static uint16_t last_strength        = 0;

      if (port != 0)
         return false;
      
      android_input_set_rumble_internal(
            strength,
            &last_strength_strong,
            &last_strength_weak,
            &last_strength,
            -1,
            effect);

      return true;
   }
   else
   {
      android_input_t *android = (android_input_t*)data;
      state_device_t *state    = android ? &android->pad_states[port] : NULL;

      if (state)
      {
         android_input_set_rumble_internal(
               strength,
               &state->rumble_last_strength_strong,
               &state->rumble_last_strength_weak,
               &state->rumble_last_strength,
               state->id,
               effect);
         return true;
      }
   }

   return false;
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
   android_input_set_rumble,
   android_input_get_joypad_driver,
   NULL,
   false
};
