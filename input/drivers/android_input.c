/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../../frontend/drivers/platform_linux.h"
#include "../input_autodetect.h"
#include "../input_config.h"
#include "../input_joypad_driver.h"
#include "../drivers_keyboard/keyboard_event_android.h"
#include "../../performance.h"
#include "../../general.h"
#include "../../driver.h"

#ifdef HAVE_MENU
#include "../../menu/menu_display.h"
#endif

#define MAX_TOUCH 16

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

static int id_1 = -1;
static int id_2 = -1;
static int id_3 = -1;

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
} state_device_t;

typedef struct android_input_data
{
   state_device_t pad_states[MAX_PADS];
   int16_t analog_state[MAX_PADS][MAX_AXIS];
   int8_t hat_state[MAX_PADS][2];

   unsigned pads_connected;
   sensor_t accelerometer_state;
   struct input_pointer pointer[MAX_TOUCH];
   unsigned pointer_count;
} android_input_data_t;

typedef struct android_input
{
   bool blocked;
   android_input_data_t thread, copy;
   const input_device_driver_t *joypad;
} android_input_t;

static void frontend_android_get_version_sdk(int32_t *sdk);
static void frontend_android_get_name(char *s, size_t len);

bool (*engine_lookup_name)(char *buf,
      int *vendorId, int *productId, size_t size, int id);

void (*engine_handle_dpad)(android_input_data_t *, AInputEvent*, int, int);

static bool android_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate);

extern float AMotionEvent_getAxisValue(const AInputEvent* motion_event,
      int32_t axis, size_t pointer_idx);

static typeof(AMotionEvent_getAxisValue) *p_AMotionEvent_getAxisValue;

#define AMotionEvent_getAxisValue (*p_AMotionEvent_getAxisValue)

static void *libandroid_handle;

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

            runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
            runloop_ctl(RUNLOOP_CTL_SET_IDLE,   &boolean);
#ifdef HAVE_MENU
            menu_display_ctl(MENU_DISPLAY_CTL_UNSET_STUB_DRAW_FRAME, NULL);
            video_driver_ctl(RARCH_DISPLAY_CTL_UNSET_STUB_FRAME, NULL);
#endif

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

            runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
            runloop_ctl(RUNLOOP_CTL_SET_IDLE,   &boolean);
#ifdef HAVE_MENU
            menu_display_ctl(MENU_DISPLAY_CTL_SET_STUB_DRAW_FRAME, NULL);
            video_driver_ctl(RARCH_DISPLAY_CTL_SET_STUB_FRAME, NULL);
#endif

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
   }
}

static void engine_handle_dpad_default(android_input_data_t *android_data,
      AInputEvent *event, int port, int source)
{
   size_t motion_ptr = AMotionEvent_getAction(event) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x           = AMotionEvent_getX(event, motion_ptr);
   float y           = AMotionEvent_getY(event, motion_ptr);

   android_data->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android_data->analog_state[port][1] = (int16_t)(y * 32767.0f);
}

static void engine_handle_dpad_getaxisvalue(android_input_data_t *android_data,
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

   android_data->hat_state[port][0] = (int)hatx;
   android_data->hat_state[port][1] = (int)haty;

   /* XXX: this could be a loop instead, but do we really want to
    * loop through every axis? */
   android_data->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android_data->analog_state[port][1] = (int16_t)(y * 32767.0f);
   android_data->analog_state[port][2] = (int16_t)(z * 32767.0f);
   android_data->analog_state[port][3] = (int16_t)(rz * 32767.0f);
#if 0
   android_data->analog_state[port][4] = (int16_t)(hatx * 32767.0f);
   android_data->analog_state[port][5] = (int16_t)(haty * 32767.0f);
#endif
   android_data->analog_state[port][6] = (int16_t)(ltrig * 32767.0f);
   android_data->analog_state[port][7] = (int16_t)(rtrig * 32767.0f);
   android_data->analog_state[port][8] = (int16_t)(brake * 32767.0f);
   android_data->analog_state[port][9] = (int16_t)(gas * 32767.0f);
}


static bool android_input_init_handle(void)
{
   if (libandroid_handle != NULL) /* already initialized */
      return true;

   if ((libandroid_handle = dlopen("/system/lib/libandroid.so",
               RTLD_LOCAL | RTLD_LAZY)) == 0)
      return false;

   if ((p_AMotionEvent_getAxisValue = dlsym(RTLD_DEFAULT,
               "AMotionEvent_getAxisValue")))
   {
      RARCH_LOG("Set engine_handle_dpad to 'Get Axis Value' (for reading extra analog sticks)");
      engine_handle_dpad = engine_handle_dpad_getaxisvalue;
   }
   id_1 = -1;
   id_2 = -1;

   return true;
}

static void *android_input_init(void)
{
   int32_t sdk;
   settings_t *settings = config_get_ptr();
   struct android_app *android_app = (struct android_app*)g_android;
   android_input_t *android = (android_input_t*)
      calloc(1, sizeof(*android));

   if (!android)
      return NULL;

   android->thread.pads_connected = 0;
   android->copy.pads_connected = 0;
   android->joypad         = input_joypad_init_driver(
         settings->input.joypad_driver, android);
 
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

static INLINE int android_input_poll_event_type_motion(
      android_input_data_t *android_data, AInputEvent *event,
      int port, int source)
{
   int getaction, action;
   size_t motion_ptr;
   bool keyup;

   if (source & ~(AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE))
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

   if (keyup && motion_ptr < MAX_TOUCH)
   {
      memmove(android_data->pointer + motion_ptr,
            android_data->pointer + motion_ptr + 1,
            (MAX_TOUCH - motion_ptr - 1) * sizeof(struct input_pointer));
      if (android_data->pointer_count > 0)
         android_data->pointer_count--;
   }
   else
   {
      float x, y;
      int pointer_max = min(AMotionEvent_getPointerCount(event), MAX_TOUCH);

      for (motion_ptr = 0; motion_ptr < pointer_max; motion_ptr++)
      {
         x = AMotionEvent_getX(event, motion_ptr);
         y = AMotionEvent_getY(event, motion_ptr);

         input_translate_coord_viewport(x, y,
               &android_data->pointer[motion_ptr].x,
               &android_data->pointer[motion_ptr].y,
               &android_data->pointer[motion_ptr].full_x,
               &android_data->pointer[motion_ptr].full_y);

         android_data->pointer_count = max(
               android_data->pointer_count,
               motion_ptr + 1);
      }
   }

   return 0;
}

static INLINE void android_input_poll_event_type_keyboard(
      AInputEvent *event, int keycode, int *handled)
{
   int keydown = (AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN);
   unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(keycode);
                     
   // Set keyboard modifier based on shift,ctrl and alt state
   uint16_t mod = 0;
   int meta = AKeyEvent_getMetaState(event);
   if(meta & AMETA_ALT_ON) mod |= RETROKMOD_ALT;
   if(meta & AMETA_CTRL_ON) mod |= RETROKMOD_CTRL;
   if(meta & AMETA_SHIFT_ON) mod |= RETROKMOD_SHIFT;

   input_keyboard_event(keydown, keyboardcode, keyboardcode, mod, RETRO_DEVICE_KEYBOARD);

   if ((keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN))
      *handled = 0;
}

static INLINE void android_input_poll_event_type_key(
      struct android_app *android_app,
      AInputEvent *event, int port, int keycode, int source,
      int type_event, int *handled)
{
   uint8_t *buf = android_keyboard_state_get(port);
   int action  = AKeyEvent_getAction(event);

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

static int android_input_get_id_port(android_input_data_t *android_data, int id,
      int source)
{
   unsigned i;
   int ret = -1;
   if (source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE |
            AINPUT_SOURCE_TOUCHPAD))
         ret = 0; /* touch overlay is always user 1 */

   for (i = 0; i < android_data->pads_connected; i++)
      if (android_data->pad_states[i].id == id)
         ret = i;

   return ret;
}

/* Returns the index inside android->pad_state */
static int android_input_get_id_index_from_name(android_input_data_t *android_data,
      const char *name)
{
   int i;
   for (i = 0; i < android_data->pads_connected; i++)
   {
      if (!strcmp(name, android_data->pad_states[i].name))
         return i;
   }

   return -1;
}

static void handle_hotplug(android_input_data_t *android_data,
      struct android_app *android_app, int *port, int id,
      int source)
{
   char device_name[256]        = {0};
   char name_buf[256]           = {0};
   int vendorId                 = 0;
   int productId                = 0;
   bool back_mapped             = false;
   settings_t         *settings = config_get_ptr();
   char device_model[256] = {0};
   frontend_android_get_name(device_model, sizeof(device_model));

   RARCH_LOG("Device model: (%s).\n", device_model);

   if (*port > MAX_PADS)
   {
      RARCH_ERR("Max number of pads reached.\n");
      return;
   }

   if (!engine_lookup_name(device_name, &vendorId, &productId, sizeof(device_name), id))
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
    */
   if(strstr(device_model, "SHIELD Android TV") && (
      strstr(device_name, "Virtual") ||
      strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.03")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
#if 0
         RARCH_LOG("- Pads Mapped: %d\n- Device Name: %s\n- IDS: %d, %d, %d", android_data->pads_connected, device_name, id, id_1, id_2);
#endif
         /* remove the remote if it is mapped */
         if (strstr(android_data->pad_states[0].name,"SHIELD Remote"))
         {
            id_1 = -1;
            id_2 = -1;
            android_data->pads_connected = 0;
            *port = 0;
            strlcpy(name_buf, device_name, sizeof(name_buf));
         }
         /* early return, we don't want this to be mapped unless the actual controller has been mapped*/
         if (strstr(device_name, "Virtual") && android_data->pads_connected==0)
            return;

         /* apply the hack only for the first controller
          * store the id for later use
         */
         if (strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.03") && android_data->pads_connected==0)
            id_1 = id;
         else if (strstr(device_name, "Virtual") && id_1 != -1)
         {
            id = id_1;
            return;
         }

         strlcpy (name_buf, "NVIDIA SHIELD Controller", sizeof(name_buf));
      }
   }

   /* NVIDIA Shield Portable
    * This is a simple hack, basically groups the "back"
    * button with the rest of the gamepad
    */
   else if(strstr(device_model, "SHIELD") && (
      strstr(device_name, "Virtual") || strstr(device_name, "gpio") ||
      strstr(device_name, "NVIDIA Corporation NVIDIA Controller v01.01")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
         if ( id_1 < 0 )
            id_1 = id;
         else
            id_2 = id;

         if ( id_2 > 0)
            return;

         strlcpy (name_buf, "NVIDIA SHIELD Portable", sizeof(name_buf));
      }
   }

   /* GPD XD
    * This is a simple hack, basically groups the "back"
    * button with the rest of the gamepad
    */
   else if(strstr(device_model, "XD") && (
      strstr(device_name, "Virtual") || strstr(device_name, "rk29-keypad") ||
      strstr(device_name,"Playstation3") || strstr(device_name,"XBOX")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
         if ( id_1 < 0 )
            id_1 = id;
         else
            id_2 = id;

         if ( id_2 > 0)
            return;

         strlcpy (name_buf, "GPD XD", sizeof(name_buf));
         *port = 0;
      }
   }

   /* XPERIA Play
    * This device is composed of two hid devices
    * We make it look like one device
    */
   else if(strstr(device_model, "R800") && (
       strstr(device_name, "keypad-game-zeus") || strstr(device_name, "keypad-zeus")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("Special Device Detected: %s\n", device_model);
      {
         if ( id_1 < 0 )
            id_1 = id;
         else
            id_2 = id;

         if ( id_2 > 0)
            return;

         strlcpy (name_buf, "XPERIA Play", sizeof(name_buf));
         *port = 0;
      }
   }

   /* ARCHOS Gamepad
    * This device is composed of two hid devices
    * We make it look like one device
    */
   else if(strstr(device_model, "ARCHOS GAMEPAD") && (
      strstr(device_name, "joy_key") || strstr(device_name, "joystick")))
   {
      /* only use the hack if the device is one of the built-in devices */
      RARCH_LOG("ARCHOS GAMEPAD Detected: %s\n", device_model);
      {
         if ( id_1 < 0 )
            id_1 = id;
         else
            id_2 = id;

         if ( id_2 > 0)
            return;

         strlcpy (name_buf, "ARCHOS GamePad", sizeof(name_buf));
         *port = 0;
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
      android_data->pads_connected = 0;
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

   else if (strstr(device_name, "360 Wireless"))
      strlcpy(name_buf, "XBox 360 Wireless", sizeof(name_buf));

   else if (strstr(device_name, "Microsoft"))
   {
      if (strstr(device_name, "Dual Strike"))
         strlcpy(device_name, "SideWinder Dual Strike", sizeof(device_name));
      else if (strstr(device_name, "SideWinder"))
         strlcpy(name_buf, "SideWinder Classic", sizeof(name_buf));
   }

   else if (
         strstr(device_name, "PLAYSTATION(R)3") ||
         strstr(device_name, "Dualshock3") ||
         strstr(device_name, "Sixaxis")
         )
      strlcpy(name_buf, "PlayStation3", sizeof(name_buf));

   else if (strstr(device_name, "MOGA"))
      strlcpy(name_buf, "Moga IME", sizeof(name_buf));

   // if device is keyboard only and didn't match any of the devices above
   // then assume it is a keyboard, register the id, and return unless another
   // keyboard is already registered
   else if(source == AINPUT_SOURCE_KEYBOARD && id_3 == -1)
   {
      id_3 = id;
      return;
   }

   // if device was not keyboard only, yet did not match any of the devices
   // above or another keyboard was already mapped, then try to autoconfigure
   // as gamepad based on device_name
   else if (!string_is_empty(device_name))
      strlcpy(name_buf, device_name, sizeof(name_buf));

   if (strstr(android_app->current_ime, "net.obsidianx.android.mogaime"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));
   else if (strstr(android_app->current_ime, "com.ccpcreations.android.WiiUseAndroid"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));
   else if (strstr(android_app->current_ime, "com.hexad.bluezime"))
      strlcpy(name_buf, android_app->current_ime, sizeof(name_buf));

   if (*port < 0)
      *port = android_data->pads_connected;

   if (settings->input.autodetect_enable)
   {
      bool      autoconfigured;
      autoconfig_params_t params   = {{0}};

      RARCH_LOG("Pads Connected: %d Port: %d\n %s VID/PID: %d/%d\n",android_data->pads_connected, *port, name_buf, params.vid, params.pid);

      strlcpy(params.name, name_buf, sizeof(params.name));
      params.idx = *port;
      params.vid = vendorId;
      params.pid = productId;
      settings->input.pid[*port] = params.pid;
      settings->input.vid[*port] = params.vid;

      strlcpy(params.driver, android_joypad.ident, sizeof(params.driver));
      autoconfigured = input_config_autoconfigure_joypad(&params);

      if (autoconfigured)
      {
         if (settings->input.autoconf_binds[*port][RARCH_MENU_TOGGLE].joykey != 0)
            back_mapped = true;
      }
   }

   if (!string_is_empty(name_buf))
   {
      strlcpy(settings->input.device_names[*port],
            name_buf, sizeof(settings->input.device_names[*port]));
   }

   if (!back_mapped && settings->input.back_as_menu_toggle_enable)
      settings->input.autoconf_binds[*port][RARCH_MENU_TOGGLE].joykey = AKEYCODE_BACK;

   android_data->pad_states[android_data->pads_connected].id = id;
   android_data->pad_states[android_data->pads_connected].port = *port;
   strlcpy(android_data->pad_states[*port].name, name_buf,
         sizeof(android_data->pad_states[*port].name));

   android_data->pads_connected++;
}

static int android_input_get_id(AInputEvent *event)
{
   int id = AInputEvent_getDeviceId(event);

   if (id == id_2)
      id = id_1;

   return id;
}

static void android_input_poll_input(void *data)
{
   AInputEvent *event = NULL;
   struct android_app *android_app = (struct android_app*)g_android;
   android_input_t    *android     = (android_input_t*)data;
   android_input_data_t    *android_data     = (android_input_data_t*)&android->thread;

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
         int          port = android_input_get_id_port(android_data, id, source);

         if (port < 0 && id != id_3)
            handle_hotplug(android_data, android_app,
            &port, id, source);
 
         switch (type_event)
         {
            case AINPUT_EVENT_TYPE_MOTION:
               if (android_input_poll_event_type_motion(android_data, event,
                        port, source))
                  engine_handle_dpad(android_data, event, port, source);
               break;
            case AINPUT_EVENT_TYPE_KEY:
               {
                  int keycode = AKeyEvent_getKeyCode(event);

                  if (id == id_3 && !predispatched)
                     android_input_poll_event_type_keyboard(event, keycode, &handled);
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

static void android_input_poll_user(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   android_input_t    *android     = (android_input_t*)data;
   android_input_data_t *android_data = (android_input_data_t*)&android->thread;

   if ((android_app->sensor_state_mask & (UINT64_C(1) <<
               RETRO_SENSOR_ACCELEROMETER_ENABLE))
         && android_app->accelerometerSensor)
   {
      ASensorEvent event;
      while (ASensorEventQueue_getEvents(android_app->sensorEventQueue, &event, 1) > 0)
      {
         android_data->accelerometer_state.x = event.acceleration.x;
         android_data->accelerometer_state.y = event.acceleration.y;
         android_data->accelerometer_state.z = event.acceleration.z;
      }
   }
}

static void android_input_poll_memcpy(void *data)
{
   unsigned i, j;
   android_input_t    *android     = (android_input_t*)data;
   struct android_app *android_app = (struct android_app*)g_android;
   
   memcpy(&android->copy, &android->thread, sizeof(android->copy));
   
   for (i = 0; i < MAX_PADS; i++)
   {
      for (j = 0; j < 2; j++)
         android_app->hat_state[i][j]    = android->copy.hat_state[i][j];
      for (j = 0; j < MAX_AXIS; j++)
         android_app->analog_state[i][j] = android->copy.analog_state[i][j];
   }
}

/* Handle all events. If our activity is in pause state,
 * block until we're unpaused.
 */
static void android_input_poll(void *data)
{
   int ident;
   unsigned key                    = RARCH_PAUSE_TOGGLE;
   struct android_app *android_app = (struct android_app*)g_android;

   while ((ident =
            ALooper_pollAll((input_driver_ctl(RARCH_INPUT_CTL_KEY_PRESSED, &key))
               ? -1 : 0,
               NULL, NULL, NULL)) >= 0)
   {
      switch (ident)
      {
         case LOOPER_ID_INPUT:
            android_input_poll_input(data);
            break;
         case LOOPER_ID_USER:
            android_input_poll_user(data);
            break;
         case LOOPER_ID_MAIN:
            android_input_poll_main_cmd();
            break;
      }
      
      if (android_app->destroyRequested != 0)
      {
         runloop_ctl(RUNLOOP_CTL_SET_SHUTDOWN, NULL);
         return;
      }

      if (android_app->reinitRequested != 0)
      {
         if (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL))
            event_command(EVENT_CMD_REINIT);
         android_app_write_cmd(android_app, APP_CMD_REINIT_DONE);
         return;
      }
   }

   if (android_app->input_alive)
      android_input_poll_memcpy(data);
}

bool android_run_events(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;

   if (ALooper_pollOnce(-1, NULL, NULL, NULL) == LOOPER_ID_MAIN)
      android_input_poll_main_cmd();

   /* Check if we are exiting. */
   if (android_app->destroyRequested != 0)
   {
      runloop_ctl(RUNLOOP_CTL_SET_SHUTDOWN, NULL);
      return false;
   }

   if (android_app->reinitRequested != 0)
   {
      if (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL))
         event_command(EVENT_CMD_REINIT);
      android_app_write_cmd(android_app, APP_CMD_REINIT_DONE);
   }

   return true;
}

static int16_t android_input_state(void *data,
      const struct retro_keybind **binds, unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   settings_t *settings = config_get_ptr();
   android_input_t *android = (android_input_t*)data;
   android_input_data_t *android_data = (android_input_data_t*)&android->copy;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(android->joypad, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(android->joypad, port, idx, id,
               binds[port]);
      case RETRO_DEVICE_POINTER:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return android_data->pointer[idx].x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return android_data->pointer[idx].y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (idx < android_data->pointer_count) &&
                  (android_data->pointer[idx].x != -0x8000) &&
                  (android_data->pointer[idx].y != -0x8000);
            case RARCH_DEVICE_ID_POINTER_BACK:
               if(settings->input.autoconf_binds[0][RARCH_MENU_TOGGLE].joykey == 0)
                  return android_keyboard_input_pressed(AKEYCODE_BACK);
         }
         break;
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return android_data->pointer[idx].full_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return android_data->pointer[idx].full_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (idx < android_data->pointer_count) &&
                  (android_data->pointer[idx].full_x != -0x8000) &&
                  (android_data->pointer[idx].full_y != -0x8000);
            case RARCH_DEVICE_ID_POINTER_BACK:
               if(settings->input.autoconf_binds[0][RARCH_MENU_TOGGLE].joykey == 0)
                  return android_keyboard_input_pressed(AKEYCODE_BACK);
         }
         break;
   }

   return 0;
}

static bool android_input_key_pressed(void *data, int key)
{
   android_input_t *android = (android_input_t*)data;
   settings_t *settings     = config_get_ptr();

   if (input_joypad_pressed(android->joypad,
         0, settings->input.binds[0], key))
      return true;

   return false;
}

static bool android_input_meta_key_pressed(void *data, int key)
{
   return false;
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

   dylib_close((dylib_t)libandroid_handle);
   libandroid_handle = NULL;

   android_keyboard_free();
   free(data);
}

static uint64_t android_input_get_capabilities(void *data)
{
   (void)data;

   return
      (1 << RETRO_DEVICE_JOYPAD)  |
      (1 << RETRO_DEVICE_POINTER) |
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
   android_input_data_t *android_data = (android_input_data_t*)&android->copy;

   switch (id)
   {
      case RETRO_SENSOR_ACCELEROMETER_X:
         return android_data->accelerometer_state.x;
      case RETRO_SENSOR_ACCELEROMETER_Y:
         return android_data->accelerometer_state.y;
      case RETRO_SENSOR_ACCELEROMETER_Z:
         return android_data->accelerometer_state.z;
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

static bool android_input_keyboard_mapping_is_blocked(void *data)
{
   android_input_t *android = (android_input_t*)data;
   if (!android)
      return false;
   return android->blocked;
}

static void android_input_keyboard_mapping_set_block(void *data, bool value)
{
   android_input_t *android = (android_input_t*)data;
   if (!android)
      return;
   android->blocked = value;
}

static void android_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool android_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

input_driver_t input_android = {
   android_input_init,
   android_input_poll,
   android_input_state,
   android_input_key_pressed,
   android_input_meta_key_pressed,
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
   android_input_keyboard_mapping_is_blocked,
   android_input_keyboard_mapping_set_block,
};
