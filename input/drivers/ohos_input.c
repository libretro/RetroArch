/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2019 - Daniel De Matteis
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

#include <stdlib.h>
#include <unistd.h>
#include <GameControllerKit/game_controller_type.h>
#include <GameControllerKit/game_device.h>
#include <GameControllerKit/game_device_event.h>
#include <GameControllerKit/game_pad.h>
#include <GameControllerKit/game_pad_event.h>
#include <multimodalinput/oh_key_code.h>

#include "../../frontend/drivers/platform_unix.h"
#include "../../retroarch.h"
#include "../input_keymaps.h"
#include "../input_driver.h"

#define MAX_TOUCH 10
#define MAX_NUM_KEYBOARDS 3
#define DEFAULT_ASENSOR_EVENT_RATE 60


/* Use this to enable/disable using the touch screen as mouse */
#define ENABLE_TOUCH_SCREEN_MOUSE 0
// TODO fix
#define KEYCODE_ASSIST 219
#define KEYCODE_BACK 220

#define LAST_KEYCODE KEYCODE_ASSIST

#define MAX_KEYS ((LAST_KEYCODE + 7) / 8)

/* First ports are used to keep track of gamepad states.
 * Last port is used for keyboard state */
static uint8_t ohos_key_state[DEFAULT_MAX_PADS + 1][MAX_KEYS];

#define OHOS_KEYBOARD_PORT_INPUT_PRESSED(binds, id) (BIT_GET(ohos_key_state[OHOS_KEYBOARD_PORT], rarch_keysym_lut[(binds)[(id)].key]))

#define OHOS_KEYBOARD_INPUT_PRESSED(key) (BIT_GET(ohos_key_state[0], (key)))

uint8_t *ohos_keyboard_state_get(unsigned port)
{
   return ohos_key_state[port];
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
   int screen_x, screen_y;
   int16_t confined_x, confined_y;
   int16_t full_x, full_y;
};

typedef struct state_device
{
   char id[NAME_MAX];
   int port;
   char name[NAME_MAX];
} state_device_t;

typedef struct ohos_input
{
   int64_t downTime;
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
} ohos_input_t;

void ohos_input_poll_touch_event(
   void *ohos_input, OH_NativeXComponent_TouchEvent event)
{
   video_driver_state_t *video_st   = video_state_get_ptr();
   if(ohos_input == NULL || video_st->data == NULL)
      return;
   ohos_input_t* ohos = (ohos_input_t*)ohos_input;
   int motion_ptr = event.id;
   int action        = event.type;
   bool keyup        = (
         action == OH_NATIVEXCOMPONENT_UP
         || action == OH_NATIVEXCOMPONENT_CANCEL
         || action == OH_NATIVEXCOMPONENT_UNKNOWN
    );
   if (keyup && motion_ptr < MAX_TOUCH)
   {
      memmove(ohos->pointer + motion_ptr,
            ohos->pointer + motion_ptr + 1,
            (MAX_TOUCH - motion_ptr - 1) * sizeof(struct input_pointer));
      if (ohos->pointer_count > 0)
         ohos->pointer_count--;
   }
   else
   {
      int pointer_max  = MIN(event.numPoints, MAX_TOUCH);
      for (motion_ptr = 0; motion_ptr < pointer_max; motion_ptr++)
      {
         float x = event.touchPoints[motion_ptr].x;
         float y = event.touchPoints[motion_ptr].y;
         ohos->pointer[motion_ptr].screen_x = x;
         ohos->pointer[motion_ptr].screen_y = y;
         ohos->pointer_count = MAX(
               ohos->pointer_count,
               motion_ptr + 1);
      }
   }

}

void ohos_input_poll_key_event(
    void  *ohos_input, KeyEvent *event)
{
   int action   = event->type;
   int keycode = event->keyCode;
   int keydown           = (action == KEY_EVENT_ACTION_DOWN);
   unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(keycode);
   /* Set keyboard modifier based on shift,ctrl and alt state */
   uint16_t mod          = 0;
//   int meta              = AKeyEvent_getMetaState(event);
//   if (meta & AMETA_ALT_ON)
//      mod |= RETROKMOD_ALT;
//   if (meta & AMETA_CTRL_ON)
//      mod |= RETROKMOD_CTRL;
//   if (meta & AMETA_SHIFT_ON)
//      mod |= RETROKMOD_SHIFT;
//   if (meta & AMETA_CAPS_LOCK_ON)
//      mod |= RETROKMOD_CAPSLOCK;
//   if (meta & AMETA_NUM_LOCK_ON)
//      mod |= RETROKMOD_NUMLOCK;
//   if (meta & AMETA_SCROLL_LOCK_ON)
//      mod |= RETROKMOD_SCROLLOCK;
//   if (meta & AMETA_META_ON)
//      mod |= RETROKMOD_META;
   input_keyboard_event(keydown, keyboardcode,
         keyboardcode, mod, RETRO_DEVICE_KEYBOARD);
   uint8_t *buf = ohos_key_state[OHOS_KEYBOARD_PORT];

   int keysym  = keycode;
   /* Handle 'duplicate' inputs that correspond
    * to the same RETROK_* key */
   switch (keycode)
   {
      case KEYCODE_DPAD_CENTER:
         keysym = KEYCODE_ENTER;
      default:
         break;
   }
   /* some controllers send both the up and down events at once
    * when the button is released for "special" buttons, like menu buttons
    * work around that by only using down events for meta keys (which get
    * cleared every poll anyway)
    */
   switch (action)
   {
      case KEY_EVENT_ACTION_UP:
         BIT_CLEAR(buf, keysym);
         break;
      case KEY_EVENT_ACTION_DOWN:
         BIT_SET(buf, keysym);
         break;
   }
}
static int ohos_input_get_id_port(ohos_input_t *ohos, char* id,
      int source)
{
   unsigned i;
   int ret = -1;
   if (source)
      ret = 0; /* touch overlay is always user 1 */
   for (i = 0; i < ohos->pads_connected; i++)
   {
      if (strcmp(ohos->pad_states[i].id, id))
      {
         ret = i;
         break;
      }
   }

   return ret;
}

static void engine_handle_dpad_getaxisvalue(struct ohos_app *ohos, const struct GamePad_AxisEvent *axisEvent, int port)
{
   double x           = 0;
   double y           = 0;
   double z           = 0;
   double rz          = 0;
   double hatx        = 0;
   double haty        = 0;
   double ltrig       = 0;
   double rtrig       = 0;
   double brake       = 0;
   double gas         = 0;
   OH_GamePad_AxisEvent_GetXAxisValue(axisEvent, &x);
   OH_GamePad_AxisEvent_GetYAxisValue(axisEvent, &y);
   OH_GamePad_AxisEvent_GetZAxisValue(axisEvent, &z);
   OH_GamePad_AxisEvent_GetRZAxisValue(axisEvent, &rz);
   OH_GamePad_AxisEvent_GetHatXAxisValue(axisEvent, &hatx);
   OH_GamePad_AxisEvent_GetHatYAxisValue(axisEvent, &haty);
   OH_GamePad_AxisEvent_GetBrakeAxisValue(axisEvent, &brake);
   OH_GamePad_AxisEvent_GetGasAxisValue(axisEvent, &gas);
//   OH_GamePad_AxisEvent_GetBrakeAxisValue(axisEvent, &ltrig);
//   OH_GamePad_AxisEvent_GetGasAxisValue(axisEvent, &rtrig);
    
   ohos->hat_state[port][0]    = (int)hatx;
   ohos->hat_state[port][1]    = (int)haty;

   ohos->analog_state[port][0] = (int16_t)(x * 32767.0f);
   ohos->analog_state[port][1] = (int16_t)(y * 32767.0f);
   ohos->analog_state[port][2] = (int16_t)(z * 32767.0f);
   ohos->analog_state[port][3] = (int16_t)(rz * 32767.0f);
   ohos->analog_state[port][6] = (int16_t)(ltrig * 32767.0f);
   ohos->analog_state[port][7] = (int16_t)(rtrig * 32767.0f);
//   ohos->analog_state[port][8] = (int16_t)(brake * 32767.0f);
//   ohos->analog_state[port][9] = (int16_t)(gas * 32767.0f);
}
static void OnAxisEvent(const struct GamePad_AxisEvent *axisEvent){
   char *deviceId = NULL;
   OH_GameDevice_DeviceInfo_GetDeviceId(axisEvent, &deviceId);
   free(deviceId);
   int port = ohos_input_get_id_port(g_ohos->ohos_input, deviceId, 0);
   engine_handle_dpad_getaxisvalue(g_ohos, axisEvent, port);
}
static void OnDeviceChanged(const struct GameDevice_DeviceEvent *deviceEvent){
    GameDevice_StatusChangedType type;
    GameController_ErrorCode err = OH_GameDevice_DeviceEvent_GetChangedType(deviceEvent, &type);
    if (err != GAME_CONTROLLER_SUCCESS) {
        return;
    }
    GameDevice_DeviceInfo *deviceInfo = NULL;
    err = OH_GameDevice_DeviceEvent_GetDeviceInfo(deviceEvent, &deviceInfo);
    if (err != GAME_CONTROLLER_SUCCESS || !deviceInfo) {
        return;
    }
    char *deviceId = NULL;
    err = OH_GameDevice_DeviceInfo_GetDeviceId(deviceInfo, &deviceId);
    if (err != GAME_CONTROLLER_SUCCESS) {
        OH_GameDevice_DestroyDeviceInfo(&deviceInfo);
        return;
    }
   char *name = NULL;
   OH_GameDevice_DeviceInfo_GetName(deviceInfo, &name);
   ohos_input_t* ohos = g_ohos->ohos_input;
   int port = ohos->pads_connected;
   strlcpy(ohos->pad_states[port].id, deviceId, sizeof(ohos->pad_states[port].id));
   ohos->pad_states[ohos->pads_connected].port = port;
   strlcpy(ohos->pad_states[port].name, name, sizeof(ohos->pad_states[port].name));
   ohos->pads_connected ++;
}

static int ohos_check_quick_tap(ohos_input_t *ohos)
{
   /* Check if the touch screen has been been quick tapped
    * and then not touched again for 200ms
    * If so then return true and deactivate quick tap timer */
   retro_time_t now = cpu_features_get_time_usec();
   if (ohos->quick_tap_time &&
         (now / 1000 - ohos->quick_tap_time / 1000000) >= 200)
   {
      ohos->quick_tap_time = 0;
      return 1;
   }

   return 0;
}


static void *ohos_input_init(const char *joypad_driver)
{
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   ohos_input_t *ohos  = NULL;
   if (!(ohos = (ohos_input_t*)calloc(1, sizeof(*ohos))))
      return NULL;
   ohos_app->ohos_input = ohos;
    
   ohos->mouse_activated = false;
   ohos->pads_connected = 0;
   ohos->quick_tap_time = 0;
   // to handle fullscreen
   settings_t *settings = config_get_ptr();
   ohos_send_native_event(EVENT_NATIVE_VIDEO_NOTCH_WRITE_OVER_ENABLE, settings->bools.video_notch_write_over_enable ? 1: 0);
    
   input_keymaps_init_keyboard_lut(rarch_key_map_ohos);
   // TODO sensors
   {
      settings_t *settings = config_get_ptr();
      bool enable_sensors = !settings || settings->bools.input_sensors_enable;
      if (enable_sensors && !ohos_app->sensor_state_mask)
      {
         unsigned rate = g_ohos->accelerometer_event_rate;
         if (rate == 0)
            rate = DEFAULT_ASENSOR_EVENT_RATE;
         rate = g_ohos->gyroscope_event_rate;
         if (rate == 0)
            rate = DEFAULT_ASENSOR_EVENT_RATE;
      }
   }
//   OH_GameDevice_RegisterDeviceMonitor(OnDeviceChanged);
//   OH_GamePad_LeftThumbstick_RegisterAxisInputMonitor(OnAxisEvent);
//   OH_GamePad_RightThumbstick_RegisterAxisInputMonitor(OnAxisEvent);
//   OH_GamePad_LeftTrigger_RegisterAxisInputMonitor(OnAxisEvent);
//   OH_GamePad_RightTrigger_RegisterAxisInputMonitor(OnAxisEvent);
//   OH_GamePad_Dpad_RegisterAxisInputMonitor(OnAxisEvent);
   return ohos;
}

static int16_t ohos_input_state(
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
   ohos_input_t *ohos = (ohos_input_t*)data;

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
                           && OHOS_KEYBOARD_PORT_INPUT_PRESSED(binds[port], i))
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
                     && OHOS_KEYBOARD_PORT_INPUT_PRESSED(binds[port], id)
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                     )
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && BIT_GET(ohos_key_state[OHOS_KEYBOARD_PORT], rarch_keysym_lut[id]);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            /* Same mouse state is reported for all ports. */
            int val = 0;
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return ohos->mouse_l || ohos_check_quick_tap(ohos);
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return ohos->mouse_r;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return ohos->mouse_m;
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return ohos->mouse_x_viewport_screen;

                  val = ohos->mouse_x_delta;
                  ohos->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return ohos->mouse_y_viewport_screen;

                  val = ohos->mouse_y_delta;
                  ohos->mouse_y_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  val = ohos->mouse_wu;
                  ohos->mouse_wu = 0;
                  return val;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  val = ohos->mouse_wd;
                  ohos->mouse_wd = 0;
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
                  if (ohos->mouse_activated)
                     return ohos->mouse_x_viewport_screen;
                  else
                     return ohos->pointer[idx].x;
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                  if (ohos->mouse_activated)
                     return ohos->mouse_y_viewport_screen;
                  else
                     return ohos->pointer[idx].y;
               /* Deprecated relative lightgun. */
               case RETRO_DEVICE_ID_LIGHTGUN_X:
                  val                    = ohos->mouse_x_delta;
                  ohos->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_LIGHTGUN_Y:
                  val                    = ohos->mouse_y_delta;
                  ohos->mouse_y_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
               case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
                  return ohos->mouse_m || ohos->pointer_count == 3;
               case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                  return ohos->mouse_r && ohos->mouse_l;
               case RETRO_DEVICE_ID_LIGHTGUN_START:
               case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
                  return ohos->mouse_r || ohos->pointer_count == 2;
               case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
                  return ohos->mouse_l || ohos_check_quick_tap(ohos) || ohos->pointer_count == 1;
               case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                  return input_driver_pointer_is_offscreen(ohos->pointer[idx].x, ohos->pointer[idx].y);
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         /* Same pointer state is reported for all ports. */
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return ohos->pointer[idx].full_x;
               return ohos->pointer[idx].confined_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return ohos->pointer[idx].full_y;
               return ohos->pointer[idx].confined_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               /* On mobile platforms, touches outside screen / core viewport are not reported. */
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return (idx < ohos->pointer_count) &&
                     (ohos->pointer[idx].full_x != -0x8000) &&
                     (ohos->pointer[idx].full_y != -0x8000);
               return (idx < ohos->pointer_count) &&
                  (ohos->pointer[idx].x != -0x8000) &&
                  (ohos->pointer[idx].y != -0x8000);
            case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
               return input_driver_pointer_is_offscreen(ohos->pointer[idx].x, ohos->pointer[idx].y);
            case RETRO_DEVICE_ID_POINTER_COUNT:
               return ohos->pointer_count;
            case RARCH_DEVICE_ID_POINTER_BACK:
            {
               const struct retro_keybind *keyptr =
                  &input_autoconf_binds[0][RARCH_MENU_TOGGLE];
               if (keyptr->joykey == 0)
                  return OHOS_KEYBOARD_INPUT_PRESSED(KEYCODE_BACK);
            }
         }
         break;
   }

   return 0;
}

static void ohos_input_free(void *data)
{
   ohos_input_t *ohos = (ohos_input_t*)data;

   if (!ohos)
      return;
   free(data);
}

static bool ohos_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned rate)
{
   ohos_input_t *ohos = (ohos_input_t*)data;

   if (!ohos)
      return false;
   return false;
}

static float ohos_input_get_sensor_input(void *data, unsigned port, unsigned id)
{
   ohos_input_t *ohos = (ohos_input_t*)data;

   if (!ohos)
      return 0.0f;

   switch (id)
   {
//      case RETRO_SENSOR_ILLUMINANCE:
//         if (ohos->illuminance_sensor)
//            return linux_get_illuminance_reading(ohos->illuminance_sensor);
      default:
         break;
   }

   return 0.0f;
}

static void ohos_input_poll(void *data)
{
   ohos_input_t *ohos = (ohos_input_t*)data;
   uint32_t i;
    
   for (i = 0; i < ohos->pointer_count || i == 0; i++)
   {
         struct video_viewport vp = {0};
         memset(&vp, 0, sizeof(vp));
         video_driver_translate_coord_viewport_confined_wrap(
               &vp,
               ohos->pointer[i].screen_x, 
               ohos->pointer[i].screen_y,
               &ohos->pointer[i].confined_x,
               &ohos->pointer[i].confined_y,
               &ohos->pointer[i].full_x,
               &ohos->pointer[i].full_y);

         video_driver_translate_coord_viewport_wrap(
               &vp,
               ohos->pointer[i].screen_x, 
               ohos->pointer[i].screen_y,
               &ohos->pointer[i].x,
               &ohos->pointer[i].y,
               &ohos->pointer[i].full_x,
               &ohos->pointer[i].full_y);
      }
}

static uint64_t ohos_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD)
        | (1 << RETRO_DEVICE_ANALOG)
        | (1 << RETRO_DEVICE_KEYBOARD);
}
input_driver_t input_ohos = {
   ohos_input_init,
   ohos_input_poll,
   ohos_input_state,
   ohos_input_free,
   ohos_input_set_sensor_state,
   ohos_input_get_sensor_input,
   ohos_get_capabilities,
   "ohos",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
