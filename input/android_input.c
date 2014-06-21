/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#include <android/keycodes.h>
#include <unistd.h>
#include <dlfcn.h>
#include "../frontend/platform/platform_android.h"
#include "input_common.h"
#include "../performance.h"
#include "../general.h"
#include "../driver.h"

#define MAX_TOUCH 16
#define MAX_PADS 8

#define AKEY_EVENT_NO_ACTION 255

#ifndef AKEYCODE_ASSIST
#define AKEYCODE_ASSIST 219
#endif

#define LAST_KEYCODE AKEYCODE_ASSIST

enum input_devices
{
   DEVICE_NONE = 0,
   DEVICE_LOGITECH_RUMBLEPAD2,
   DEVICE_LOGITECH_DUAL_ACTION,
   DEVICE_LOGITECH_PRECISION_GAMEPAD,
   DEVICE_ICONTROLPAD_HID_JOYSTICK,
   DEVICE_ICONTROLPAD_BLUEZ_IME,
   DEVICE_TTT_THT_ARCADE,
   DEVICE_TOMMO_NEOGEOX_ARCADE,
   DEVICE_MADCATZ_PC_USB_STICK,
   DEVICE_LOGICOOL_RUMBLEPAD2,
   DEVICE_IDROID_X360,
   DEVICE_ZEEMOTE_STEELSERIES,
   DEVICE_HUIJIA_USB_SNES,
   DEVICE_SUPER_SMARTJOY,
   DEVICE_SAITEK_RUMBLE_P480,
   DEVICE_MS_SIDEWINDER_DUAL_STRIKE,
   DEVICE_MS_SIDEWINDER,
   DEVICE_MS_XBOX,
   DEVICE_WISEGROUP_PLAYSTATION2,
   DEVICE_JCPS102_PLAYSTATION2,
   DEVICE_GENERIC_PLAYSTATION2_CONVERTER,
   DEVICE_PSMOVE_NAVI,
   DEVICE_JXD_S7300B,
   DEVICE_JXD_S7800B,
   DEVICE_IDROID_CON,
   DEVICE_GENIUS_MAXFIRE_G08XU,
   DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD,
   DEVICE_BUFFALO_BGC_FC801,
   DEVICE_RETROUSB_RETROPAD,
   DEVICE_RETROUSB_SNES_RETROPORT,
   DEVICE_CYPRESS_USB,
   DEVICE_MAYFLASH_WII_CLASSIC,
   DEVICE_SZMY_POWER_DUAL_BOX_WII,
   DEVICE_ARCHOS_GAMEPAD,
   DEVICE_JXD_S5110,
   DEVICE_JXD_S5110_SKELROM,
   DEVICE_XPERIA_PLAY,
   DEVICE_BROADCOM_BLUETOOTH_HID,
   DEVICE_THRUST_PREDATOR,
   DEVICE_DRAGONRISE,
   DEVICE_PLAYSTATION3_VERSION1,
   DEVICE_PLAYSTATION3_VERSION2,
   DEVICE_MOGA_IME,
   DEVICE_NYKO_PLAYPAD_PRO,
   DEVICE_TOODLES_2008_CHIMP,
   DEVICE_MOGA,
   DEVICE_SEGA_VIRTUA_STICK_HIGH_GRADE,
   DEVICE_CCPCREATIONS_WIIUSE_IME,
   DEVICE_KEYBOARD_RETROPAD,
   DEVICE_OUYA,
   DEVICE_ONLIVE_WIRELESS_CONTROLLER,
   DEVICE_TOMEE_NES_USB,
   DEVICE_THRUSTMASTER_T_MINI,
   DEVICE_GAMEMID,
   DEVICE_DEFENDER_GAME_RACER_CLASSIC,
   DEVICE_HOLTEK_JC_U912F,
   DEVICE_NVIDIA_SHIELD,
   DEVICE_MUCH_IREADGO_I5,
   DEVICE_WIKIPAD,
   DEVICE_FC30_GAMEPAD,
   DEVICE_SAMSUNG_GAMEPAD_EIGP20,
   DEVICE_LAST
};

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
   AXIS_BRAKE = 23,
};

#define MAX_AXIS 10

typedef struct state_device
{
   int id;
   int port;
   char name[256];
} state_device_t;

typedef struct android_input
{
   unsigned pads_connected;
   state_device_t pad_states[MAX_PADS];
   uint8_t pad_state[MAX_PADS][(LAST_KEYCODE + 7) / 8];
   int8_t hat_state[MAX_PADS][2];
   
   int16_t analog_state[MAX_PADS][MAX_AXIS];
   sensor_t accelerometer_state;
   struct input_pointer pointer[MAX_TOUCH];
   unsigned pointer_count;
   ASensorManager *sensorManager;
   ASensorEventQueue *sensorEventQueue;
   const rarch_joypad_driver_t *joypad;
} android_input_t;


void (*engine_handle_dpad)(android_input_t *android, AInputEvent*, int, int);
static bool android_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned event_rate);

extern float AMotionEvent_getAxisValue(const AInputEvent* motion_event,
      int32_t axis, size_t pointer_index);

static typeof(AMotionEvent_getAxisValue) *p_AMotionEvent_getAxisValue;

#define AMotionEvent_getAxisValue (*p_AMotionEvent_getAxisValue)

static inline bool get_bit(const uint8_t *buf, unsigned bit)
{
   return buf[bit >> 3] & (1 << (bit & 7));
}

static inline void clear_bit(uint8_t *buf, unsigned bit)
{
   buf[bit >> 3] &= ~(1 << (bit & 7));
}

static inline void set_bit(uint8_t *buf, unsigned bit)
{
   buf[bit >> 3] |= 1 << (bit & 7);
}

static void engine_handle_dpad_default(android_input_t *android, AInputEvent *event, int port, int source)
{
   size_t motion_pointer = AMotionEvent_getAction(event) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x = AMotionEvent_getX(event, motion_pointer);
   float y = AMotionEvent_getY(event, motion_pointer);

   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);
}

static void engine_handle_dpad_getaxisvalue(android_input_t *android, AInputEvent *event, int port, int source)
{
   size_t motion_pointer = AMotionEvent_getAction(event) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x = AMotionEvent_getAxisValue(event, AXIS_X, motion_pointer);
   float y = AMotionEvent_getAxisValue(event, AXIS_Y, motion_pointer);
   float z = AMotionEvent_getAxisValue(event, AXIS_Z, motion_pointer);
   float rz = AMotionEvent_getAxisValue(event, AXIS_RZ, motion_pointer);
   float hatx = AMotionEvent_getAxisValue(event, AXIS_HAT_X, motion_pointer);
   float haty = AMotionEvent_getAxisValue(event, AXIS_HAT_Y, motion_pointer);
   float ltrig = AMotionEvent_getAxisValue(event, AXIS_LTRIGGER, motion_pointer);
   float rtrig = AMotionEvent_getAxisValue(event, AXIS_RTRIGGER, motion_pointer);
   float brake = AMotionEvent_getAxisValue(event, AXIS_BRAKE, motion_pointer);
   float gas = AMotionEvent_getAxisValue(event, AXIS_GAS, motion_pointer);

   android->hat_state[port][0] = (int)hatx;
   android->hat_state[port][1] = (int)haty;

   // XXX: this could be a loop instead, but do we really want to loop through every axis?
   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);
   android->analog_state[port][2] = (int16_t)(z * 32767.0f);
   android->analog_state[port][3] = (int16_t)(rz * 32767.0f);
   //android->analog_state[port][4] = (int16_t)(hatx * 32767.0f);
   //android->analog_state[port][5] = (int16_t)(haty * 32767.0f);
   android->analog_state[port][6] = (int16_t)(ltrig * 32767.0f);
   android->analog_state[port][7] = (int16_t)(rtrig * 32767.0f);
   android->analog_state[port][8] = (int16_t)(brake * 32767.0f);
   android->analog_state[port][9] = (int16_t)(gas * 32767.0f);
}

static void *android_input_init(void)
{
   android_input_t *android = (android_input_t*)calloc(1, sizeof(*android));
   if (!android)
      return NULL;

   android->pads_connected = 0;
   android->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);

   return android;
}

static int zeus_id = -1;
static int zeus_second_id = -1;
static unsigned zeus_port;

static inline int android_input_poll_event_type_motion(android_input_t *android, AInputEvent *event,
      int port, int source)
{
   int getaction, action;
   size_t motion_pointer;
   bool keyup;

   if (source & ~(AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE))
      return 1;

   getaction = AMotionEvent_getAction(event);
   action = getaction & AMOTION_EVENT_ACTION_MASK;
   motion_pointer = getaction >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   keyup = (
         action == AMOTION_EVENT_ACTION_UP ||
         action == AMOTION_EVENT_ACTION_CANCEL ||
         action == AMOTION_EVENT_ACTION_POINTER_UP) ||
      (source == AINPUT_SOURCE_MOUSE &&
       action != AMOTION_EVENT_ACTION_DOWN);

   if (keyup && motion_pointer < MAX_TOUCH)
   {
      memmove(android->pointer + motion_pointer, 
            android->pointer + motion_pointer + 1,
            (MAX_TOUCH - motion_pointer - 1) * sizeof(struct input_pointer));
      if (android->pointer_count > 0)
         android->pointer_count--;
   }
   else
   {
      float x, y;
      int pointer_max = min(AMotionEvent_getPointerCount(event), MAX_TOUCH);
      for (motion_pointer = 0; motion_pointer < pointer_max; motion_pointer++)
      {
         x = AMotionEvent_getX(event, motion_pointer);
         y = AMotionEvent_getY(event, motion_pointer);

         input_translate_coord_viewport(x, y,
               &android->pointer[motion_pointer].x, &android->pointer[motion_pointer].y,
               &android->pointer[motion_pointer].full_x, &android->pointer[motion_pointer].full_y);

         android->pointer_count = max(android->pointer_count, motion_pointer + 1);
      }
   }

   return 0;
}

static inline void android_input_poll_event_type_key(android_input_t *android, struct android_app *android_app,
      AInputEvent *event, int port, int keycode, int source, int type_event, int *handled)
{
   int action  = AKeyEvent_getAction(event);

   // some controllers send both the up and down events at once when the button is released for "special" buttons, like menu buttons
   // work around that by only using down events for meta keys (which get cleared every poll anyway)
   if (action == AKEY_EVENT_ACTION_UP)
      clear_bit(android->pad_state[port], keycode);
   else if (action == AKEY_EVENT_ACTION_DOWN)
      set_bit(android->pad_state[port], keycode);

   if ((keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN))
      *handled = 0;
}

static int android_input_get_id_port(android_input_t *android, int id, int source)
{
   unsigned i;
   if (source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE | AINPUT_SOURCE_TOUCHPAD))
      return 0; // touch overlay is always player 1

   for (i = 0; i < android->pads_connected; i++)
      if (android->pad_states[i].id == id)
         return i;

   return -1;
}

static bool android_input_lookup_name(char *buf, size_t size, int id)
{
   jclass class;
   jmethodID method, getName;
   jobject device, name;
   JNIEnv *env = (JNIEnv*)jni_thread_getenv();

   if (!env)
      goto error;

   class = NULL;
   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      goto error;

   method = NULL;
   GET_STATIC_METHOD_ID(env, method, class, "getDevice", "(I)Landroid/view/InputDevice;");
   if (!method)
      goto error;

   device = NULL;
   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
   {
      RARCH_ERR("Failed to find device for ID: %d\n", id);
      goto error;
   }

   getName = NULL;
   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      goto error;

   name = NULL;
   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
   {
      RARCH_ERR("Failed to find name for device ID: %d\n", id);
      goto error;
   }

   buf[0] = '\0';

   const char *str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);

   RARCH_LOG("device name: %s\n", buf);
   return true;
error:
   return false;
}

/*
 * Returns the index inside android->pad_states
 */
static int android_input_get_id_index_from_name(android_input_t *android, const char *name)
{
   int i;
   for (i = 0; i < android->pads_connected; i++)
   {
      if (strcmp(name, android->pad_states[i].name) == 0)
         return i;
   }

   return -1;
}

static void handle_hotplug(android_input_t *android, struct android_app *android_app, unsigned *port, unsigned id,
      int source)
{
   unsigned device;
   char device_name[256], name_buf[256];
   name_buf[0] = device_name[0] = 0;

   if (!g_settings.input.autodetect_enable)
      return;

   if (*port > MAX_PADS)
   {
      RARCH_ERR("Max number of pads reached.\n");
      return;
   }

   android_input_lookup_name(device_name, sizeof(device_name), id);

   //FIXME: Ugly hack, see other FIXME note below.
   if (strstr(device_name, "keypad-game-zeus") || strstr(device_name, "keypad-zeus"))
   {
      if (zeus_id < 0)
      {
         RARCH_LOG("zeus_pad 1 detected: %d\n", id);
         zeus_id = id;
         zeus_port = *port;
      }
      else
      {
         RARCH_LOG("zeus_pad 2 detected: %d\n", id);
         zeus_second_id = id;
      }
      strlcpy(name_buf, "Xperia Play", sizeof(name_buf));
   }

   device = 0;

   if (strstr(device_name,"Logitech") && strstr(device_name, "RumblePad 2"))
      strlcpy(name_buf, "RumblePad 2", sizeof(name_buf));
   else if (strstr(device_name, "Logitech") && strstr(device_name, "Dual Action"))
      strlcpy(name_buf, "Logitech Dual Action", sizeof(name_buf));
   else if (strstr(device_name, "Logitech") && strstr(device_name, "Precision"))
      strlcpy(name_buf, "Logitech Precision", sizeof(name_buf));
   else if (strstr(device_name, "iControlPad-")) // followed by a 4 (hex) char HW id
      device = DEVICE_ICONTROLPAD_HID_JOYSTICK;
   else if (strstr(device_name, "SEGA VIRTUA STICK High Grade"))
      strlcpy(name_buf, "Sega Virtua Stick", sizeof(name_buf));
   else if (strstr(device_name, "TTT THT Arcade console 2P USB Play"))
   {
      //FIXME - need to do a similar thing here as we did for nVidia Shield
      //and Xperia Play. We need to keep 'count' of the amount of similar (grouped)
      //devices.
      //
      //For Xperia Play - count similar devices and bind them to the same 'player'
      //port
      //
      //For nVidia Shield - see above
      //
      //For TTT HT - keep track of how many of these 'pads' are already
      //connected, and based on that, assign one of them to be Player 1 and
      //the other to be Player 2.
      //
      //If this is finally implemented right, then these port conditionals can go.
      if (*port == 0)
         strlcpy(name_buf, "TTT THT Arcade (Player 1)", sizeof(name_buf));
      else if (*port == 1)
         strlcpy(name_buf, "TTT THT Arcade (Player 2)", sizeof(name_buf));
   }      
   else if (strstr(device_name, "TOMMO NEOGEOX Arcade Stick"))
      strlcpy(name_buf, "TOMMO Neo-Geo X", sizeof(name_buf));
   else if (strstr(device_name, "Onlive Wireless Controller"))
      device = DEVICE_ONLIVE_WIRELESS_CONTROLLER;
   else if (strstr(device_name, "MadCatz") && strstr(device_name, "PC USB Wired Stick"))
      device = DEVICE_MADCATZ_PC_USB_STICK;
   else if (strstr(device_name, "Logicool") && strstr(device_name, "RumblePad 2"))
      strlcpy(name_buf, "Logicool RumblePad 2", sizeof(name_buf));
   else if (strstr(device_name, "Sun4i-keypad"))
      strlcpy(name_buf, "iDroid x360", sizeof(name_buf));
   else if (strstr(device_name, "Zeemote") && strstr(device_name, "Steelseries free"))
      device = DEVICE_ZEEMOTE_STEELSERIES;
   else if (strstr(device_name, "HuiJia  USB GamePad"))
      strlcpy(name_buf, "HuiJia", sizeof(name_buf));
   else if (strstr(device_name, "Smartjoy Family Super Smartjoy 2"))
      strlcpy(name_buf, "Super Smartjoy 2", sizeof(name_buf));
   else if (strstr(device_name, "Jess Tech Dual Analog Rumble Pad"))
      device = DEVICE_SAITEK_RUMBLE_P480;
   else if (strstr(device_name, "mtk-kpd"))
      strlcpy(name_buf, "MUCH iReadyGo i5", sizeof(name_buf));
   else if (strstr(device_name, "Wikipad"))
      strlcpy(name_buf, "Wikipad", sizeof(name_buf));
   else if (strstr(device_name, "360 Wireless"))
      strlcpy(name_buf, "XBox 360 Wireless", sizeof(name_buf));
   else if (strstr(device_name, "Microsoft"))
   {
      if (strstr(device_name, "Dual Strike"))
         strlcpy(device_name, "SideWinder Dual Strike", sizeof(device_name));
      else if (strstr(device_name, "SideWinder"))
         strlcpy(name_buf, "SideWinder Classic", sizeof(name_buf));
      else if (strstr(device_name, "X-Box 360")
            || strstr(device_name, "X-Box"))
         strlcpy(name_buf, "XBox 360", sizeof(name_buf));
   }
   else if (strstr(device_name, "WiseGroup"))
   {
      if (strstr(device_name, "TigerGame") || strstr(device_name, "Game Controller Adapter")
            || strstr(device_name, "JC-PS102U") || strstr(device_name, "Dual USB Joypad"))
      {
         if (strstr(device_name, "WiseGroup"))
            strlcpy(name_buf, "PlayStation2 WiseGroup", sizeof(name_buf));
         else if (strstr(device_name, "JC-PS102U"))
            strlcpy(name_buf, "PlayStation2 JCPS102", sizeof(name_buf));
         else
            strlcpy(name_buf, "PlayStation2 Generic", sizeof(name_buf));
      }
   }
   else if (strstr(device_name, "PLAYSTATION(R)3") || strstr(device_name, "Dualshock3")
         || strstr(device_name, "Sixaxis") || strstr(device_name, "Gasia,Co") ||
         (strstr(device_name, "Gamepad 0") || strstr(device_name, "Gamepad 1") || 
          strstr(device_name, "Gamepad 2") || strstr(device_name, "Gamepad 3")))
      strlcpy(name_buf, "PlayStation3", sizeof(name_buf));
   else if (strstr(device_name, "MOGA"))
      device = DEVICE_MOGA;
   else if (strstr(device_name, "Sony Navigation Controller"))
      device = DEVICE_PSMOVE_NAVI;
   else if (strstr(device_name, "OUYA Game Controller"))
      strlcpy(name_buf, "OUYA", sizeof(name_buf));
   else if (strstr(device_name, "adc joystick"))
      strlcpy(name_buf, "JXD S7300B", sizeof(name_buf));
   else if (strstr(device_name, "idroid:con"))
      device = DEVICE_IDROID_CON;
   else if (strstr(device_name, "NYKO PLAYPAD PRO"))
      strlcpy(name_buf, "Nyko Playpad Pro", sizeof(name_buf));
   else if (strstr(device_name, "2-Axis, 8-Button"))
      device = DEVICE_GENIUS_MAXFIRE_G08XU;
   else if (strstr(device_name, "USB,2-axis 8-button gamepad"))
      device = DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD;
   else if (strstr(device_name, "BUFFALO BGC-FC801"))
      device = DEVICE_BUFFALO_BGC_FC801;
   else if (strstr(device_name, "8Bitdo FC30"))
      strlcpy(name_buf, "FC30 Gamepad", sizeof(name_buf));
   else if (strstr(device_name, "RetroUSB.com RetroPad"))
      device = DEVICE_RETROUSB_RETROPAD;
   else if (strstr(device_name, "RetroUSB.com SNES RetroPort"))
      device = DEVICE_RETROUSB_SNES_RETROPORT;
   else if (strstr(device_name, "CYPRESS USB"))
      device = DEVICE_CYPRESS_USB;
   else if (strstr(device_name, "Mayflash Wii Classic"))
      strlcpy(name_buf, "Mayflash Wii Classic", sizeof(name_buf));
   else if (strstr(device_name, "SZMy-power LTD CO.  Dual Box WII"))
      strlcpy(name_buf, "SZMy Power Dual Box Wii", sizeof(name_buf));
   else if (strstr(device_name, "Toodles 2008 ChImp"))
      device = DEVICE_TOODLES_2008_CHIMP;
   else if (strstr(device_name, "joy_key"))
      strlcpy(name_buf, "Archos Gamepad", sizeof(name_buf));
   else if (strstr(device_name, "matrix_keyboard"))
      strlcpy(name_buf, "JXD S5110B", sizeof(name_buf));
   else if (strstr(device_name, "tincore_adc_joystick"))
      strlcpy(name_buf, "JXD S5110B (Skelrom)", sizeof(name_buf));
   else if (strstr(device_name, "keypad-zeus") || (strstr(device_name, "keypad-game-zeus")))
      strlcpy(name_buf, "Xperia Play", sizeof(name_buf));
   else if (strstr(device_name, "USB Gamepad"))
      device = DEVICE_THRUST_PREDATOR;
   else if (strstr(device_name, "ADC joystick"))
      strlcpy(name_buf, "JXD S7800B", sizeof(name_buf));
   else if (strstr(device_name, "DragonRise"))
      device = DEVICE_DRAGONRISE;
   else if (strstr(device_name, "Thrustmaster T Mini"))
      device = DEVICE_THRUSTMASTER_T_MINI;
   else if (strstr(device_name, "2Axes 11Keys Game  Pad"))
      device = DEVICE_TOMEE_NES_USB;
   else if (strstr(device_name, "rk29-keypad") || strstr(device_name, "GAMEMID"))
      strlcpy(name_buf, "GameMID", sizeof(name_buf));
   else if (strstr(device_name, "USB Gamepad"))
      device = DEVICE_DEFENDER_GAME_RACER_CLASSIC;
   else if (strstr(device_name, "HOLTEK JC - U912F vibration game"))
      device = DEVICE_HOLTEK_JC_U912F;
   else if (strstr(device_name, "NVIDIA Controller"))
   {
      *port = 0; // Shield is always player 1. FIXME: This is kinda ugly. We really need to find a way to detect useless input devices like gpio-keys in a general way.
      strlcpy(name_buf, "NVIDIA Shield", sizeof(name_buf));
   }
   else if (strstr(device_name, "Samsung Game Pad EI-GP20"))
      strlcpy(name_buf, "Samsung Gamepad EI-GP20", sizeof(name_buf));

   if (strstr(android_app->current_ime, "net.obsidianx.android.mogaime"))
   {
      device = DEVICE_MOGA_IME;
      strlcpy(name_buf, "MOGA IME", sizeof(name_buf));
   }
   else if (strstr(android_app->current_ime, "com.ccpcreations.android.WiiUseAndroid"))
   {
      device = DEVICE_CCPCREATIONS_WIIUSE_IME;
      strlcpy(name_buf, "ccpcreations WiiUse", sizeof(name_buf));
   }
   else if (strstr(android_app->current_ime, "com.hexad.bluezime"))
   {
      strlcpy(name_buf, "iControlpad SPP mode (using Bluez IME)", sizeof(name_buf));
   }

   if (source == AINPUT_SOURCE_KEYBOARD && device != DEVICE_XPERIA_PLAY)
      device = DEVICE_KEYBOARD_RETROPAD;

   if (name_buf[0] != '\0')
   {
      strlcpy(g_settings.input.device_names[*port], name_buf, sizeof(g_settings.input.device_names[*port]));
      input_config_autoconfigure_joypad(*port, name_buf, android_joypad.ident);
      RARCH_LOG("Port %d: %s.\n", *port, name_buf);
   }

   *port = android->pads_connected;
   android->pad_states[android->pads_connected].id = id;
   android->pad_states[android->pads_connected].port = *port;
   strlcpy(android->pad_states[*port].name, name_buf,
         sizeof(android->pad_states[*port].name));

   android->pads_connected++;
}


// Handle all events. If our activity is in pause state, block until we're unpaused.
static void android_input_poll(void *data)
{
   int ident;
   struct android_app *android_app = (struct android_app*)g_android;
   android_input_t *android = (android_input_t*)data;

   while ((ident = ALooper_pollAll((input_key_pressed_func(RARCH_PAUSE_TOGGLE)) ? -1 : 0,
               NULL, NULL, NULL)) >= 0)
   {
      if (ident == LOOPER_ID_INPUT)
      {
         AInputEvent *event = NULL;

         // Read all pending events.
         while (AInputQueue_hasEvents(android_app->inputQueue))
         {
            while (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
            {
               int32_t handled = 1;
               int source, id, type_event, port;
               int predispatched;

               predispatched = AInputQueue_preDispatchEvent(android_app->inputQueue, event);

               source = AInputEvent_getSource(event);
               id = AInputEvent_getDeviceId(event);
               if (id == zeus_second_id)
                  id = zeus_id;

               type_event = AInputEvent_getType(event);
               port = android_input_get_id_port(android, id, source);

               if (port < 0)
                  handle_hotplug(android, android_app, &android->pads_connected, id, source);

               if (type_event == AINPUT_EVENT_TYPE_MOTION)
               {
                  if (android_input_poll_event_type_motion(android, event, port, source))
                     engine_handle_dpad(android, event, port, source);
               }
               else if (type_event == AINPUT_EVENT_TYPE_KEY)
               {
                  int keycode = AKeyEvent_getKeyCode(event);
                  android_input_poll_event_type_key(android, android_app, event, port, keycode, source, type_event, &handled);
               }

               if (!predispatched)
                  AInputQueue_finishEvent(android_app->inputQueue, event, handled);
            }
         }
      }
      else if (ident == LOOPER_ID_USER)
      {
         if ((android_app->sensor_state_mask & (1ULL << RETRO_SENSOR_ACCELEROMETER_ENABLE))
               && android_app->accelerometerSensor)
         {
            ASensorEvent event;
            while (ASensorEventQueue_getEvents(android->sensorEventQueue, &event, 1) > 0)
            {
               android->accelerometer_state.x = event.acceleration.x;
               android->accelerometer_state.y = event.acceleration.y;
               android->accelerometer_state.z = event.acceleration.z;
            }
         }
      }
      else if (ident == LOOPER_ID_MAIN)
         engine_handle_cmd(driver.input_data);
   }
}

static int16_t android_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   android_input_t *android = (android_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(android->joypad, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(android->joypad, port, index, id, binds[port]);
      case RETRO_DEVICE_POINTER:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return android->pointer[index].x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return android->pointer[index].y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < android->pointer_count) && (android->pointer[index].x != -0x8000) && (android->pointer[index].y != -0x8000);
            default:
               return 0;
         }
         break;
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return android->pointer[index].full_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return android->pointer[index].full_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < android->pointer_count) && (android->pointer[index].full_x != -0x8000) && (android->pointer[index].full_y != -0x8000);
            default:
               return 0;
         }
         break;
   }

   return 0;
}

static bool android_input_key_pressed(void *data, int key)
{
   android_input_t *android = (android_input_t*)data;
   if (!android)
      return false;
   return ((g_extern.lifecycle_state | driver.overlay_state.buttons) & (1ULL << key))
      || input_joypad_pressed(android->joypad, 0, g_settings.input.binds[0], key);
}

static void android_input_free_input(void *data)
{
   android_input_t *android = (android_input_t*)data;
   if (!android)
      return;

   if (android->sensorManager)
      ASensorManager_destroyEventQueue(android->sensorManager, android->sensorEventQueue);

   free(data);
}

static uint64_t android_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static void android_input_enable_sensor_manager(void *data)
{
   android_input_t *android = (android_input_t*)data;
   struct android_app *android_app = (struct android_app*)g_android;

   android->sensorManager = ASensorManager_getInstance();
   android_app->accelerometerSensor = ASensorManager_getDefaultSensor(android->sensorManager,
         ASENSOR_TYPE_ACCELEROMETER);
   android->sensorEventQueue = ASensorManager_createEventQueue(android->sensorManager,
         android_app->looper, LOOPER_ID_USER, NULL, NULL);
}

static bool android_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned event_rate)
{
   android_input_t *android = (android_input_t*)data;
   struct android_app *android_app = (struct android_app*)g_android;

   if (event_rate == 0)
      event_rate = 60;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         if (!android_app->accelerometerSensor)
            android_input_enable_sensor_manager(android);

         if (android_app->accelerometerSensor)
            ASensorEventQueue_enableSensor(android->sensorEventQueue,
                  android_app->accelerometerSensor);

         // events per second (in us).
         if (android_app->accelerometerSensor)
            ASensorEventQueue_setEventRate(android->sensorEventQueue,
                  android_app->accelerometerSensor, (1000L / event_rate) * 1000);

         android_app->sensor_state_mask &= ~(1ULL << RETRO_SENSOR_ACCELEROMETER_DISABLE);
         android_app->sensor_state_mask |= (1ULL  << RETRO_SENSOR_ACCELEROMETER_ENABLE);
         return true;

      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         if (android_app->accelerometerSensor)
            ASensorEventQueue_disableSensor(android->sensorEventQueue,
                  android_app->accelerometerSensor);
         
         android_app->sensor_state_mask &= ~(1ULL << RETRO_SENSOR_ACCELEROMETER_ENABLE);
         android_app->sensor_state_mask |= (1ULL  << RETRO_SENSOR_ACCELEROMETER_DISABLE);
         return true;
      default:
         return false;
   }

   return false;
}

static float android_input_get_sensor_input(void *data, unsigned port, unsigned id)
{
   android_input_t *android = (android_input_t*)data;

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

static const rarch_joypad_driver_t *android_input_get_joypad_driver(void *data)
{
   android_input_t *android = (android_input_t*)data;
   return android->joypad;
}

const input_driver_t input_android = {
   android_input_init,
   android_input_poll,
   android_input_state,
   android_input_key_pressed,
   android_input_free_input,
   NULL,
   android_input_set_sensor_state,
   android_input_get_sensor_input,
   android_input_get_capabilities,
   NULL,
   "android_input",

   NULL,
   NULL,
   android_input_get_joypad_driver,
};

static const char *android_joypad_name(unsigned pad)
{
   return g_settings.input.device_names[pad];
}

static bool android_joypad_init(void)
{
   unsigned autoconf_pad;

   for (autoconf_pad = 0; autoconf_pad < MAX_PLAYERS; autoconf_pad++)
   {
      strlcpy(g_settings.input.device_names[autoconf_pad], android_joypad_name(autoconf_pad), sizeof(g_settings.input.device_names[autoconf_pad]));
      input_config_autoconfigure_joypad(autoconf_pad, android_joypad_name(autoconf_pad), android_joypad.ident);
   }

   engine_handle_dpad = engine_handle_dpad_default;
   if ((dlopen("/system/lib/libandroid.so", RTLD_LOCAL | RTLD_LAZY)) == 0)
   {
      RARCH_WARN("Unable to open libandroid.so\n");
      return true;
   }

   if ((p_AMotionEvent_getAxisValue = dlsym(RTLD_DEFAULT, "AMotionEvent_getAxisValue")))
   {
      RARCH_LOG("Set engine_handle_dpad to 'Get Axis Value' (for reading extra analog sticks)");
      engine_handle_dpad = engine_handle_dpad_getaxisvalue;
   }

   return true;
}

static bool android_joypad_button(unsigned port_num, uint16_t joykey)
{
   android_input_t *android = (android_input_t*)driver.input_data;

   if (!android || port_num >= MAX_PADS)
      return false;

   if (GET_HAT_DIR(joykey))
   {
      unsigned h = GET_HAT(joykey);
      if (h > 0)
         return false;
      switch (GET_HAT_DIR(joykey))
      {
         case HAT_LEFT_MASK:  return android->hat_state[port_num][0] == -1;
         case HAT_RIGHT_MASK: return android->hat_state[port_num][0] ==  1;
         case HAT_UP_MASK:    return android->hat_state[port_num][1] == -1;
         case HAT_DOWN_MASK:  return android->hat_state[port_num][1] ==  1;
         default: return false;
      }
   }
   else
      return joykey < LAST_KEYCODE && get_bit(android->pad_state[port_num], joykey);
}

static int16_t android_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   android_input_t *android = (android_input_t*)driver.input_data;
   if (!android || joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < MAX_AXIS)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < MAX_AXIS)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   val = android->analog_state[port_num][axis];

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void android_joypad_poll(void)
{
}

static bool android_joypad_query_pad(unsigned pad)
{
   android_input_t *android = (android_input_t*)driver.input_data;
   return (pad < MAX_PLAYERS && pad < android->pads_connected);
}


static void android_joypad_destroy(void)
{
}

const rarch_joypad_driver_t android_joypad = {
   android_joypad_init,
   android_joypad_query_pad,
   android_joypad_destroy,
   android_joypad_button,
   android_joypad_axis,
   android_joypad_poll,
   NULL,
   android_joypad_name,
   "android",
};
