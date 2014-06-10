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
#define PRESSED_UP(x, y)    ((y <= (-g_settings.input.axis_threshold)))
#define PRESSED_DOWN(x, y)  ((y >= (g_settings.input.axis_threshold)))
#define PRESSED_LEFT(x, y)  ((x <= (-g_settings.input.axis_threshold)))
#define PRESSED_RIGHT(x, y) ((x >= (g_settings.input.axis_threshold)))

#define MAX_PADS 8

#define AKEY_EVENT_NO_ACTION 255

enum {
   ICADE_PROFILE_RED_SAMURAI = 0,
   ICADE_PROFILE_IPEGA_PG9017,
   ICADE_PROFILE_IPEGA_PG9017_MODE2,
   ICADE_PROFILE_GAMESTOP_WIRELESS,
   ICADE_PROFILE_G910,
   ICADE_PROFILE_MOGA_HERO_POWER,
} icade_profile_enums;

enum {
   AKEYCODE_META_FUNCTION_ON = 8,
   AKEYCODE_NUMPAD_LCK_0    = 96,
   AKEYCODE_NUMPAD_LCK_1    = 97,
   AKEYCODE_NUMPAD_LCK_2    = 98,
   AKEYCODE_NUMPAD_LCK_3    = 99,
   AKEYCODE_NUMPAD_LCK_4    = 100,
   AKEYCODE_NUMPAD_LCK_5    = 101,
   AKEYCODE_NUMPAD_LCK_6    = 102,
   AKEYCODE_NUMPAD_LCK_7    = 103,
   AKEYCODE_NUMPAD_LCK_8    = 104,
   AKEYCODE_NUMPAD_LCK_9    = 105,
   AKEYCODE_OTHR_108        = 108,
   AKEYCODE_NUMPAD_SUB      = 109,
   AKEYCODE_ESCAPE          = 111,
   AKEYCODE_FORWARD_DEL     = 112,
   AKEYCODE_CTRL_LEFT       = 113,
   AKEYCODE_CTRL_RIGHT      = 114,
   AKEYCODE_CAPS_LOCK       = 115,
   AKEYCODE_SCROLL_LOCK     = 116,
   AKEYCODE_SYSRQ           = 120, AKEYCODE_BREAK           = 121,
   AKEYCODE_MOVE_HOME       = 122,
   AKEYCODE_MOVE_END        = 123,
   AKEYCODE_INSERT          = 124,
   AKEYCODE_FORWARD         = 125,
   AKEYCODE_MEDIA_PLAY      = 126,
   AKEYCODE_MEDIA_PAUSE     = 127,
   AKEYCODE_F1              = 131,
   AKEYCODE_F2              = 132,
   AKEYCODE_F3              = 133,
   AKEYCODE_F4              = 134,
   AKEYCODE_F5              = 135,
   AKEYCODE_F6              = 136,
   AKEYCODE_F7              = 137,
   AKEYCODE_F8              = 138,
   AKEYCODE_F9              = 139,
   AKEYCODE_NUMPAD_1        = 145,
   AKEYCODE_NUMPAD_2        = 146,
   AKEYCODE_NUMPAD_3        = 147,
   AKEYCODE_NUMPAD_4        = 148,
   AKEYCODE_NUMPAD_5        = 149,
   AKEYCODE_NUMPAD_6        = 150,
   AKEYCODE_NUMPAD_7        = 151,
   AKEYCODE_NUMPAD_8        = 152,
   AKEYCODE_NUMPAD_9        = 153,
   AKEYCODE_WINDOW          = 171,
   AKEYCODE_BUTTON_1        = 188,
   AKEYCODE_BUTTON_2        = 189,
   AKEYCODE_BUTTON_3        = 190,
   AKEYCODE_BUTTON_4        = 191,
   AKEYCODE_BUTTON_5        = 192,
   AKEYCODE_BUTTON_6        = 193,
   AKEYCODE_BUTTON_7        = 194,
   AKEYCODE_BUTTON_8        = 195,
   AKEYCODE_BUTTON_9        = 196,
   AKEYCODE_BUTTON_10       = 197,
   AKEYCODE_BUTTON_11       = 198,
   AKEYCODE_BUTTON_12       = 199,
   AKEYCODE_BUTTON_13       = 200,
   AKEYCODE_BUTTON_14       = 201,
   AKEYCODE_BUTTON_15       = 202,
   AKEYCODE_BUTTON_16       = 203,
   AKEYCODE_ASSIST          = 219,
};

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

typedef struct android_input
{
   //jmethodID onBackPressed;
   unsigned pads_connected;
   int state_device_ids[MAX_PADS];
   uint8_t pad_state[MAX_PADS][(LAST_KEYCODE + 7) / 8];
   
   uint64_t keycode_lut[LAST_KEYCODE];
   int16_t analog_state[MAX_PADS][MAX_AXIS];
   sensor_t accelerometer_state;
   struct input_pointer pointer[MAX_TOUCH];
   unsigned pointer_count;
   ASensorManager* sensorManager;
   ASensorEventQueue* sensorEventQueue;
   const rarch_joypad_driver_t *joypad;
} android_input_t;


void (*engine_handle_dpad)(void *data, AInputEvent*, int, char*, size_t, int, bool);
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

static void engine_handle_dpad_default(void *data, AInputEvent *event,
      int port, char *msg, size_t msg_sizeof,
      int source, bool debug_enable)
{
   size_t motion_pointer = AMotionEvent_getAction(event) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   android_input_t *android = (android_input_t*)data;
   float x = AMotionEvent_getX(event, motion_pointer);
   float y = AMotionEvent_getY(event, motion_pointer);

   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);

   if (debug_enable)
      snprintf(msg, msg_sizeof, "Pad %d : x = %.2f, y = %.2f, src %d.\n",
            port, x, y, source);
}

static void engine_handle_dpad_getaxisvalue(void *data, AInputEvent *event,
      int port, char *msg, size_t msg_sizeof, int source,
      bool debug_enable)
{
   size_t motion_pointer = AMotionEvent_getAction(event) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   android_input_t *android = (android_input_t*)data;
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

   // XXX: this could be a loop instead, but do we really want to loop through every axis?
   android->analog_state[port][0] = (int16_t)(x * 32767.0f);
   android->analog_state[port][1] = (int16_t)(y * 32767.0f);
   android->analog_state[port][2] = (int16_t)(z * 32767.0f);
   android->analog_state[port][3] = (int16_t)(rz * 32767.0f);
   android->analog_state[port][4] = (int16_t)(hatx * 32767.0f);
   android->analog_state[port][5] = (int16_t)(haty * 32767.0f);
   android->analog_state[port][6] = (int16_t)(ltrig * 32767.0f);
   android->analog_state[port][7] = (int16_t)(rtrig * 32767.0f);
   android->analog_state[port][8] = (int16_t)(brake * 32767.0f);
   android->analog_state[port][9] = (int16_t)(gas * 32767.0f);

   if (debug_enable)
      snprintf(msg, msg_sizeof, "Pad %d : x %.2f, y %.2f, z %.2f, rz %.2f, src %d.\n",
            port, x, y, z, rz, source);
}

static void *android_input_init(void)
{
   android_input_t *android = (android_input_t*)calloc(1, sizeof(*android));
   if (!android)
      return NULL;

   android->pads_connected = 0;

   android->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);

#if 0
   JNIEnv *env;
   jclass class;

   env = jni_thread_getenv();
   if (!env)
      goto retobj;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   if (!class)
      goto retobj;

   GET_METHOD_ID(env, android->onBackPressed, class, "onBackPressed", "()V");
   if (!android->onBackPressed)
   {
      RARCH_ERR("Could not set onBackPressed JNI function pointer.\n");
      goto retobj;
   }
#endif

   return android;
}

static int zeus_id = -1;
static int zeus_second_id = -1;
static unsigned zeus_port;

static void android_input_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   android_input_t *android = (android_input_t*)data;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      int i;

      /* eight 8-bit values are packed into one uint64_t
       * one for each of the 8 pads */
      unsigned shift = 8 + (port * 8);

      // NOTE - we have to add '1' to the bit mask because
      // RETRO_DEVICE_ID_JOYPAD_B is 0

      RARCH_LOG("Detecting keybinds. Device %u port %u id %u keybind_action %u\n", device, port, id, keybind_action);

      switch (device)
      {
         case DEVICE_LOGITECH_RUMBLEPAD2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Rumblepad 2",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)       << shift);
            break;
         case DEVICE_LOGITECH_DUAL_ACTION:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Dual Action",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL]  |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR]  |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            android->keycode_lut[AKEYCODE_BACK]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)           << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_LOGITECH_PRECISION_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Precision",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);

            android->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);

            android->keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)           << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_GAMEMID:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "GameMID",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)        << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)   << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)       << shift);

            /* left analog stick - TODO */
            /* right analog stick - TODO */
            /* menu button? */
            break;
         case DEVICE_ICONTROLPAD_HID_JOYSTICK:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "iControlPad HID Joystick profile",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)   << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)        << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            break;
         case DEVICE_ICONTROLPAD_BLUEZ_IME:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "iControlPad SPP profile (using Bluez IME)",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)        << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)        << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)        << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)        << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)        << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)   << shift);

            /* left analog stick */
            android->keycode_lut[AKEYCODE_W] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)              << shift);
            android->keycode_lut[AKEYCODE_S] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)            << shift);
            android->keycode_lut[AKEYCODE_A] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)            << shift);
            android->keycode_lut[AKEYCODE_D] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)           << shift);

            /* right analog stick - TODO */
            // Left - 11
            // Right - 13
            // Down - 12
            // Up - 15
            break;
         case DEVICE_TTT_THT_ARCADE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "TTT THT Arcade",
                  sizeof(g_settings.input.device_names[port]));

            /* same as Rumblepad 2 - merge? */
            //android->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);

            android->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);

            /* TODO - unsure about pad 2 still */
#if 0
            android->keycode_lut[AKEYCODE_BUTTON_Z]  |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_11]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_13]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_16]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
#endif
            break;
         case DEVICE_TOMMO_NEOGEOX_ARCADE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "TOMMO Neogeo X Arcade",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_MADCATZ_PC_USB_STICK:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Madcatz PC USB Stick",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift); /* request hack */
            android->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift); /* request hack */
            android->keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_MODE] |= ((RARCH_QUIT_KEY+1) << shift);
            break;
         case DEVICE_LOGICOOL_RUMBLEPAD2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logicool Rumblepad 2",
                  sizeof(g_settings.input.device_names[port]));
            
            // Rumblepad 2 DInput */
            /* TODO: Add L3/R3 */
            android->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_L2]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);

            android->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            break;
         case DEVICE_IDROID_X360:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "iDroid x360",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_INSERT] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_MINUS] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_SLASH] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_MOVE_END] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_LEFT_BRACKET] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_FORWARD_DEL] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_MEDIA_PLAY] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_EQUALS] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            // Left Analog Up: 152
            // Left Analog Down: 146
            // Left Analog Right: 150
            // Left Analog Left: 148
            // Right Analog Up: 92 (AKEYCODE_PAGE_UP)
            // Right Analog Down: 93 (AKEYCODE_PAGE_DOWN)
            // Right Analog Right: 113 (AKEYCODE_CTRL_LEFT)
            // Right Analog Left: 72 (AKEYCODE_RIGHT_BRACKET)
            android->keycode_lut[AKEYCODE_NUMPAD_8] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_NUMPAD_2] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_NUMPAD_6] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_NUMPAD_4] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            break;
         case DEVICE_ZEEMOTE_STEELSERIES:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Zeemote Steelseries",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_MODE]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_HUIJIA_USB_SNES:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Huijia USB SNES",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_SUPER_SMARTJOY:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Super Smartjoy",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_SAITEK_RUMBLE_P480:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Saitek Rumble P480",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_MS_SIDEWINDER_DUAL_STRIKE:
            /* TODO - unfinished */
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "MS Sidewinder Dual Strike",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_4] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_L2) << shift);
            break;
         case DEVICE_MS_SIDEWINDER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "MS Sidewinder",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_MS_XBOX:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Xbox",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_MODE] |= ((RARCH_MENU_TOGGLE + 1) << shift);
            android->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR]  |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_WISEGROUP_PLAYSTATION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "WiseGroup PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_JCPS102_PLAYSTATION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JCPS102 PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_GENERIC_PLAYSTATION2_CONVERTER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Generic PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_KEYBOARD_RETROPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Generic Keyboard",
                  sizeof(g_settings.input.device_names[port]));

            // Keyboard
            // TODO: Map L2/R2/L3/R3
            android->keycode_lut[AKEYCODE_Z] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_SHIFT_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            android->keycode_lut[AKEYCODE_X] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_W] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);

            /* Misc control scheme */
            android->keycode_lut[AKEYCODE_F1] |= ((RARCH_MENU_TOGGLE + 1) << shift);
            android->keycode_lut[AKEYCODE_F2] |= ((RARCH_SAVE_STATE_KEY+1) << shift);
            android->keycode_lut[AKEYCODE_F4] |= ((RARCH_LOAD_STATE_KEY+1) << shift);
            android->keycode_lut[AKEYCODE_F7] |= ((RARCH_STATE_SLOT_PLUS+1) << shift);
            android->keycode_lut[AKEYCODE_F6] |= ((RARCH_STATE_SLOT_MINUS+1) << shift);
            android->keycode_lut[AKEYCODE_SPACE] |= ((RARCH_FAST_FORWARD_KEY+1) << shift);
            android->keycode_lut[AKEYCODE_L] |= ((RARCH_FAST_FORWARD_HOLD_KEY+1) << shift);
            android->keycode_lut[AKEYCODE_BREAK] |= ((RARCH_PAUSE_TOGGLE+1) << shift);
            android->keycode_lut[AKEYCODE_K] |= ((RARCH_FRAMEADVANCE+1) << shift);
            android->keycode_lut[AKEYCODE_H] |= ((RARCH_RESET+1) << shift);
            android->keycode_lut[AKEYCODE_R] |= ((RARCH_REWIND+1) << shift);
            android->keycode_lut[AKEYCODE_F9] |= ((RARCH_MUTE+1) << shift);

            android->keycode_lut[AKEYCODE_ESCAPE] |= ((RARCH_QUIT_KEY+1) << shift);
            break;
         case DEVICE_PLAYSTATION3_VERSION1:
         case DEVICE_PLAYSTATION3_VERSION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "PlayStation3",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |= ((RARCH_MENU_TOGGLE+1)     << shift);
            break;
         case DEVICE_SEGA_VIRTUA_STICK_HIGH_GRADE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Sega Virtua Stick",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_MODE] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            break;
         case DEVICE_PSMOVE_NAVI:
            /* TODO - unfinished */
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "PS Move Navi",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);

            android->keycode_lut[AKEYCODE_BUTTON_11]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            android->keycode_lut[AKEYCODE_UNKNOWN]   |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_JXD_S7300B:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JXD S7300B",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);

            android->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);

            android->keycode_lut[AKEYCODE_ENTER]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_SPACE]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);

            //android->keycode_lut[AKEYCODE_VOLUME_UP]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            //android->keycode_lut[AKEYCODE_VOLUME_DOWN]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_JXD_S7800B:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JXD S7800B",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);

            android->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);

            android->keycode_lut[AKEYCODE_BUTTON_START]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_IDROID_CON:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "i.droid",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12]  |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            break;
         case DEVICE_NYKO_PLAYPAD_PRO:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Nyko Playpad Pro",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_ONLIVE_WIRELESS_CONTROLLER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Onlive Wireless",
                  sizeof(g_settings.input.device_names[port]));

            /* TODO - Does D-pad work as D-pad? */

            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);

            /* TODO - 
             * LT - find out value
             * RT - find out value
             */
            break;
         case DEVICE_GENIUS_MAXFIRE_G08XU:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Genius Maxfire G08XU",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "USB 2 Axis 8 button",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_BUFFALO_BGC_FC801:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Buffalo BGC FC801",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_RETROUSB_RETROPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "RetroUSB NES",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_RETROUSB_SNES_RETROPORT:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "RetroUSB SNES",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_CYPRESS_USB:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Cypress USB",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_MAYFLASH_WII_CLASSIC:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Mayflash Wii Classic",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_SZMY_POWER_DUAL_BOX_WII:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "SZMy Power Dual Box Wii",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            android->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_TOODLES_2008_CHIMP:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Toodles 2008 Chimp",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_ARCHOS_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Archos Gamepad",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_JXD_S5110:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JXD S5110",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_SPACE] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_JXD_S5110_SKELROM:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JXD S5110 Skelrom",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_OUYA:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "OUYA",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            break;
         case DEVICE_HOLTEK_JC_U912F:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Elecom JC-U912F",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_XPERIA_PLAY:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Xperia Play",
                  sizeof(g_settings.input.device_names[port]));

            if ((zeus_second_id != -1 && (zeus_second_id == id)))
            {
               port = zeus_port;
               shift = 8 + (port * 8);
            }

            g_extern.lifecycle_state |= (1ULL << MODE_INPUT_XPERIA_PLAY_HACK);

            android->keycode_lut[AKEYCODE_DPAD_CENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_BROADCOM_BLUETOOTH_HID:
            RARCH_LOG("Bluetooth HID\n");
            if ((g_settings.input.icade_count +1) < 4)
            {
               g_settings.input.device[port] = device;
               g_settings.input.icade_count++;
               RARCH_LOG("Using icade profile %u\n", g_settings.input.icade_count - 1);

               switch(g_settings.input.icade_profile[g_settings.input.icade_count - 1])  /* was just incremented... */
               {
                  case ICADE_PROFILE_RED_SAMURAI:
                     /* TODO: unsure about Select button here */
                     /* TODO: hookup right stick 
                      * RStick Up: 37
                      * RStick Down: 39
                      * RStick Left:38
                      * RStick Right: 40 */

                     /* Red Samurai */
                     strlcpy(g_settings.input.device_names[port], "Red Samurai",
                        sizeof(g_settings.input.device_names[port]));
                     android->keycode_lut[AKEYCODE_W]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
                     android->keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
                     android->keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
                     android->keycode_lut[AKEYCODE_D]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
                     android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
                     android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
                     android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
                     android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
                     android->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
                     android->keycode_lut[AKEYCODE_ENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
                     android->keycode_lut[AKEYCODE_9] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
                     android->keycode_lut[AKEYCODE_0] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
                     android->keycode_lut[AKEYCODE_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     android->keycode_lut[AKEYCODE_6] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
                     android->keycode_lut[AKEYCODE_7] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     android->keycode_lut[AKEYCODE_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);

                     /* unsure if the person meant the SNES-mapping here or whether it's the pad */ 
                     android->keycode_lut[AKEYCODE_1] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     android->keycode_lut[AKEYCODE_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     android->keycode_lut[AKEYCODE_3] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     android->keycode_lut[AKEYCODE_4] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     break;
                  case ICADE_PROFILE_IPEGA_PG9017:
                     strlcpy(g_settings.input.device_names[port], "iPega PG-9017",
                        sizeof(g_settings.input.device_names[port]));
                     /* This maps to SNES layout, not button labels on gamepad -- SNES layout has A to the right of B */
                     android->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift); /* Button labeled X on gamepad */
                     android->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift); /* Button labeled A on gamepad */
                     android->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift); /* Button labeled B on gamepad */
                     android->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift); /* Button labeled Y on gamepad */
                     android->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     android->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     android->keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
                     android->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
                     /* These don't work, in gamepad mode the dpad sends AXIS_HAT_X and AXIS_HAT_Y motion events
                        instead of button events, so they get processed by engine_handle_dpad_getaxisvalue()  */
                     android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)     << shift);
                     android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)   << shift);
                     android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)   << shift);
                     android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
                     break;
                  case ICADE_PROFILE_IPEGA_PG9017_MODE2:
                     strlcpy(g_settings.input.device_names[port], "iPega PG-9017 (Mode2)",
                        sizeof(g_settings.input.device_names[port]));
                     android->keycode_lut[AKEYCODE_M]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     android->keycode_lut[AKEYCODE_J]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     android->keycode_lut[AKEYCODE_K]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     android->keycode_lut[AKEYCODE_I]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     android->keycode_lut[AKEYCODE_Q]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     android->keycode_lut[AKEYCODE_P]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     android->keycode_lut[AKEYCODE_R]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
                     android->keycode_lut[AKEYCODE_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
                     android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)     << shift);
                     android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)   << shift);
                     android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)   << shift);
                     android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
                     break;
                  case ICADE_PROFILE_G910:
                     strlcpy(g_settings.input.device_names[port], "G910",
                        sizeof(g_settings.input.device_names[port]));
                     /* Face buttons map to SNES layout, not button labels on gamepad -- SNES layout has A to the right of B */
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_3]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift); /* Button labeled X on gamepad */
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_0]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift); /* Button labeled A on gamepad */
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_1]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift); /* Button labeled B on gamepad */
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_4]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift); /* Button labeled Y on gamepad */
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_6]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_8]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_2]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_7]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_9]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_5]  |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_SUB]    |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
                     android->keycode_lut[AKEYCODE_OTHR_108]      |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
                     /* These don't work, in gamepad mode the dpad sends AXIS_HAT_X and AXIS_HAT_Y motion events
                        instead of button events, so they get processed by engine_handle_dpad_getaxisvalue()  */
                     android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)     << shift);
                     android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)   << shift);
                     android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)   << shift);
                     android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
                     break;   
                  case ICADE_PROFILE_GAMESTOP_WIRELESS:
                     strlcpy(g_settings.input.device_names[port], "Gamestop Wireless",
                        sizeof(g_settings.input.device_names[port]));
                     android->keycode_lut[AKEYCODE_W] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
                     android->keycode_lut[AKEYCODE_S] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
                     android->keycode_lut[AKEYCODE_A] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
                     android->keycode_lut[AKEYCODE_D] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
                     android->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
                     android->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
                     android->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
                     android->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
                     android->keycode_lut[AKEYCODE_3] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     android->keycode_lut[AKEYCODE_4] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     android->keycode_lut[AKEYCODE_1] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     android->keycode_lut[AKEYCODE_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     android->keycode_lut[AKEYCODE_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     android->keycode_lut[AKEYCODE_7] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     android->keycode_lut[AKEYCODE_ESCAPE] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
                     android->keycode_lut[AKEYCODE_ENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
                     android->keycode_lut[AKEYCODE_6] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
                     android->keycode_lut[AKEYCODE_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
                     android->keycode_lut[AKEYCODE_9] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
                     android->keycode_lut[AKEYCODE_0] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
                     break;
                  case ICADE_PROFILE_MOGA_HERO_POWER:
                     strlcpy(g_settings.input.device_names[port], "Moga Hero Power",
                        sizeof(g_settings.input.device_names[port]));
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_0] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_1] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_3] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_4] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_6] |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     android->keycode_lut[AKEYCODE_NUMPAD_LCK_7] |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     android->keycode_lut[AKEYCODE_BACK] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
                     android->keycode_lut[AKEYCODE_OTHR_108] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
                     android->keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
                     android->keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
               }
            }
            break;
         case DEVICE_THRUST_PREDATOR:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Trust Predator",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO: L3/R3 */

            android->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            break;
         case DEVICE_DRAGONRISE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "DragonRise",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO: L3/R3 */
	
            android->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            break;

         case DEVICE_SAMSUNG_GAMEPAD_EIGP20:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Samsung Game Pad EI-GP20",
                  sizeof(g_settings.input.device_names[port]));

	//B: "Pad 0: 96, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
	//A: "Pad 0: 97, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
	//X: "Pad 0: 99, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
	//Y: "Pad 0: 100, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
	//Left Trigger: "Pad 0: 102, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
	//Right Trigger: "Pad 0: 103, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
	//Play Button: "Pad 0: 0, ac=1, src = 1281"
	//Start: "Pad 0: 108, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
	//Select: "Pad 0: 109, ac=1, src = 1281"
            android->keycode_lut[AKEYCODE_BUTTON_SELECT]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
	    break;



         case DEVICE_TOMEE_NES_USB:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Tomee NES USB",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            break;
         case DEVICE_THRUSTMASTER_T_MINI:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Thrustmaster T Mini",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);
            break;
         case DEVICE_DEFENDER_GAME_RACER_CLASSIC:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Defender Game Racer Classic",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_BUTTON_10]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_1]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_2]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_3]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_4]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_5]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_6]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_7]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_8]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_9]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            break;
         case DEVICE_MOGA_IME:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Moga IME",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO:
             * right stick up: 188
             * right stick down: 189 
             * right stick left: 190
             * right stick right: 191
             */
            android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            break;
         case DEVICE_NVIDIA_SHIELD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "NVIDIA Shield",
                  sizeof(g_settings.input.device_names[port]));
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey       = (AKEYCODE_BUTTON_A);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey       = (AKEYCODE_BUTTON_X);

            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey  = (AKEYCODE_BUTTON_THUMBL);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey   = (AKEYCODE_BUTTON_THUMBR);


            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey       = (AKEYCODE_BUTTON_B);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey       = (AKEYCODE_BUTTON_Y);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey       = (AKEYCODE_BUTTON_L1);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey       = (AKEYCODE_BUTTON_R1);


            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_L3);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_R3);

            g_settings.input.binds[port][RARCH_MENU_TOGGLE].def_joykey              = (AKEYCODE_BUTTON_START);

            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joykey       = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joykey      = NO_BTN;
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joykey     = NO_BTN;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joyaxis = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joyaxis  = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joyaxis     = AXIS_NEG(5);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joyaxis   = AXIS_POS(5);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joyaxis   = AXIS_NEG(4);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joyaxis  = AXIS_POS(4);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joyaxis      = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joyaxis     = AXIS_POS(6);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joyaxis     = AXIS_POS(7);
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joyaxis     = AXIS_NONE;
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joyaxis      = AXIS_POS(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joyaxis     = AXIS_NEG(0);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joyaxis      = AXIS_POS(1);
            g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joyaxis     = AXIS_NEG(1);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joyaxis     = AXIS_POS(2);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joyaxis    = AXIS_NEG(2);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joyaxis     = AXIS_POS(3);
            g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joyaxis    = AXIS_NEG(3);
            break;
         case DEVICE_MUCH_IREADGO_I5:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "MUCH iReadyGo i5",
                  sizeof(g_settings.input.device_names[port]));

            android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);

            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_DPAD_CENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);

            break;
         case DEVICE_WIKIPAD:
            /* untested : D-pad/analogs */
            /* untested: L2/R2 */
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Wikipad",
                  sizeof(g_settings.input.device_names[port]));
            
            android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);

            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);

            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            android->keycode_lut[AKEYCODE_BACK] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_FC30_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "FC30 Gamepad",
                  sizeof(g_settings.input.device_names[port]));
            
            android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);

            android->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_CCPCREATIONS_WIIUSE_IME:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "ccpCreations WiiUse IME",
                  sizeof(g_settings.input.device_names[port]));
            

            /* Player 1 */
            android->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_1]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_2]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_3]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_4]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_5]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_6]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)   << shift);
            android->keycode_lut[AKEYCODE_M]         |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            android->keycode_lut[AKEYCODE_P]         |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            android->keycode_lut[AKEYCODE_E]         |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            android->keycode_lut[AKEYCODE_B]         |= ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            android->keycode_lut[AKEYCODE_F]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            android->keycode_lut[AKEYCODE_G]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            android->keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            android->keycode_lut[AKEYCODE_LEFT_BRACKET]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
            android->keycode_lut[AKEYCODE_RIGHT_BRACKET] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);
            android->keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            android->keycode_lut[AKEYCODE_H]         |= ((RARCH_RESET+1)                    << shift);
            android->keycode_lut[AKEYCODE_W]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_S]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_A]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_D]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);

            /* Player 2 */
            shift += 8;
            android->keycode_lut[AKEYCODE_I]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_K] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_J] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_O]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_COMMA] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_PERIOD] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_VOLUME_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            android->keycode_lut[AKEYCODE_VOLUME_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            android->keycode_lut[AKEYCODE_MEDIA_PREVIOUS] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            android->keycode_lut[AKEYCODE_MEDIA_NEXT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            android->keycode_lut[AKEYCODE_MEDIA_PLAY] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            android->keycode_lut[AKEYCODE_MEDIA_STOP] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            android->keycode_lut[AKEYCODE_ENDCALL] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            android->keycode_lut[AKEYCODE_CALL] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            android->keycode_lut[AKEYCODE_PLUS] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            android->keycode_lut[AKEYCODE_MINUS] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            android->keycode_lut[AKEYCODE_BACKSLASH] |= ((RARCH_RESET+1)      << shift);
            android->keycode_lut[AKEYCODE_L] |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            android->keycode_lut[AKEYCODE_R] |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            android->keycode_lut[AKEYCODE_SEARCH] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            android->keycode_lut[AKEYCODE_TAB] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);

            /* Player 3 */
            shift += 8;
            android->keycode_lut[AKEYCODE_PAGE_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_PAGE_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_MEDIA_REWIND] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_MEDIA_FAST_FORWARD]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_SOFT_LEFT]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            android->keycode_lut[AKEYCODE_SOFT_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBR]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_THUMBL]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            android->keycode_lut[AKEYCODE_SPACE]|= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_SYM]|= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_EXPLORER]|= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_ENVELOPE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L1]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);

            /* Player 4 */
            shift += 8;
            android->keycode_lut[AKEYCODE_N]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            android->keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            android->keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            android->keycode_lut[AKEYCODE_NOTIFICATION]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            android->keycode_lut[AKEYCODE_MUTE]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_START]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            android->keycode_lut[AKEYCODE_BUTTON_SELECT]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            android->keycode_lut[AKEYCODE_CLEAR]|= ((RARCH_RESET+1)    << shift);
            android->keycode_lut[AKEYCODE_CAPS_LOCK]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            android->keycode_lut[AKEYCODE_SCROLL_LOCK] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            //android->keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift); -- Left meta
            //android->keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift); -- right meta
            android->keycode_lut[AKEYCODE_META_FUNCTION_ON] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            android->keycode_lut[AKEYCODE_SYSRQ] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            android->keycode_lut[AKEYCODE_BREAK] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            android->keycode_lut[AKEYCODE_MOVE_HOME] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            android->keycode_lut[AKEYCODE_BUTTON_Z] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            android->keycode_lut[AKEYCODE_GRAVE] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            android->keycode_lut[AKEYCODE_MEDIA_PAUSE] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            break;
         case DEVICE_NONE:
         default:
            g_settings.input.device[port] = 0;
            strlcpy(g_settings.input.device_names[port], "Unknown",
                  sizeof(g_settings.input.device_names[port]));
            break;
      }

      for (i = 0; i < RARCH_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
         g_settings.input.binds[port][i].joyaxis = g_settings.input.binds[port][i].def_joyaxis;
      }

      android->keycode_lut[AKEYCODE_MENU] |= ((RARCH_MENU_TOGGLE + 1) << shift);
   }
}

static int android_input_poll_event_type_motion(android_input_t *android, AInputEvent *event,
      float *x, float *y, int port, int source)
{
   if (source & ~(AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE))
   {
      return 1;
   }
   else
   {
      int getaction = AMotionEvent_getAction(event);
      int action = getaction & AMOTION_EVENT_ACTION_MASK;
      size_t motion_pointer = getaction >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
      bool keyup = (action == AMOTION_EVENT_ACTION_UP ||
            action == AMOTION_EVENT_ACTION_CANCEL || action == AMOTION_EVENT_ACTION_POINTER_UP) ||
         (source == AINPUT_SOURCE_MOUSE && action != AMOTION_EVENT_ACTION_DOWN);

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
         int pointer_max = min(AMotionEvent_getPointerCount(event), MAX_TOUCH);
         for (motion_pointer = 0; motion_pointer < pointer_max; motion_pointer++)
         {
            *x = AMotionEvent_getX(event, motion_pointer);
            *y = AMotionEvent_getY(event, motion_pointer);

            input_translate_coord_viewport(*x, *y,
                  &android->pointer[motion_pointer].x, &android->pointer[motion_pointer].y,
                  &android->pointer[motion_pointer].full_x, &android->pointer[motion_pointer].full_y);

            android->pointer_count = max(android->pointer_count, motion_pointer + 1);
         }
      }
   }

   return 0;
}

static void android_input_poll_event_type_key(android_input_t *android, struct android_app *android_app,
      AInputEvent *event, int port, int keycode, int source, int type_event, int *handled)
{
   int action  = AKeyEvent_getAction(event);

   // some controllers send both the up and down events at once when the button is released for "special" buttons, like menu buttons
   // work around that by only using down events for meta keys (which get cleared every poll anyway)
   if (action == AKEY_EVENT_ACTION_UP)
      clear_bit(android->pad_state[port], keycode);
   else if (action == AKEY_EVENT_ACTION_DOWN)
      set_bit(android->pad_state[port], keycode);

   if ((keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN))// && android->keycode_lut[keycode] == 0)
      *handled = 0;
}

static void input_autodetect_get_device_name(void *data, char *buf, size_t size, int id)
{
   jclass class;
   jmethodID method, getName;
   jobject device, name;
   JNIEnv *env = (JNIEnv*)jni_thread_getenv();

   if (!env)
      return;

   buf[0] = '\0';

   class = NULL;
   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      return;

   method = NULL;
   GET_STATIC_METHOD_ID(env, method, class, "getDevice", "(I)Landroid/view/InputDevice;");
   if (!method)
      return;

   device = NULL;
   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
   {
      RARCH_ERR("Failed to find device for ID: %d\n", id);
      return;
   }

   getName = NULL;
   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      return;

   name = NULL;
   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
   {
      RARCH_ERR("Failed to find name for device ID: %d\n", id);
      return;
   }

   const char *str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);
}

static void handle_hotplug(void *data, char *msg,
      size_t sizeof_msg, unsigned port, unsigned id,
      int source, bool *primary)
{
   struct android_app *android_app = (struct android_app*)data;

   unsigned device;
   char name_buf[256], *current_ime;
   name_buf[0] = 0;

   if (port > MAX_PADS)
   {
      snprintf(msg, sizeof_msg, "Max number of pads reached.\n");
      return;
   }

   current_ime = (char*)android_app->current_ime;

   input_autodetect_get_device_name(android_app, name_buf, sizeof(name_buf), id);

   RARCH_LOG("device name: %s\n", name_buf);

   /* Shitty hack put back in again */
   if (strstr(name_buf, "keypad-game-zeus") || strstr(name_buf, "keypad-zeus"))
   {
      if (zeus_id < 0)
      {
         RARCH_LOG("zeus_pad 1 detected: %d\n", id);
         zeus_id = id;
         zeus_port = port;
      }
      else
      {
         RARCH_LOG("zeus_pad 2 detected: %d\n", id);
         zeus_second_id = id;
      }
   }

   device = 0;

   if (strstr(name_buf,"Logitech") && strstr(name_buf, "RumblePad 2"))
      device = DEVICE_LOGITECH_RUMBLEPAD2;
   else if (strstr(name_buf, "Logitech") && strstr(name_buf, "Dual Action"))
      device = DEVICE_LOGITECH_DUAL_ACTION;
   else if (strstr(name_buf, "Logitech") && strstr(name_buf, "Precision"))
      device = DEVICE_LOGITECH_PRECISION_GAMEPAD;
   else if (strstr(name_buf, "iControlPad-")) // followed by a 4 (hex) char HW id
      device = DEVICE_ICONTROLPAD_HID_JOYSTICK;
   else if (strstr(name_buf, "SEGA VIRTUA STICK High Grade"))
      device = DEVICE_SEGA_VIRTUA_STICK_HIGH_GRADE;
   else if (strstr(name_buf, "TTT THT Arcade console 2P USB Play"))
      device = DEVICE_TTT_THT_ARCADE;
   else if (strstr(name_buf, "TOMMO NEOGEOX Arcade Stick"))
      device = DEVICE_TOMMO_NEOGEOX_ARCADE;
   else if (strstr(name_buf, "Onlive Wireless Controller"))
      device = DEVICE_ONLIVE_WIRELESS_CONTROLLER;
   else if (strstr(name_buf, "MadCatz") && strstr(name_buf, "PC USB Wired Stick"))
      device = DEVICE_MADCATZ_PC_USB_STICK;
   else if (strstr(name_buf, "Logicool") && strstr(name_buf, "RumblePad 2"))
      device = DEVICE_LOGICOOL_RUMBLEPAD2;
   else if (strstr(name_buf, "Sun4i-keypad"))
      device = DEVICE_IDROID_X360;
   else if (strstr(name_buf, "Zeemote") && strstr(name_buf, "Steelseries free"))
      device = DEVICE_ZEEMOTE_STEELSERIES;
   else if (strstr(name_buf, "HuiJia  USB GamePad"))
      device = DEVICE_HUIJIA_USB_SNES;
   else if (strstr(name_buf, "Smartjoy Family Super Smartjoy 2"))
      device = DEVICE_SUPER_SMARTJOY;
   else if (strstr(name_buf, "Jess Tech Dual Analog Rumble Pad"))
      device = DEVICE_SAITEK_RUMBLE_P480;
   else if (strstr(name_buf, "mtk-kpd"))
      device = DEVICE_MUCH_IREADGO_I5;
   else if (strstr(name_buf, "Wikipad"))
      device = DEVICE_WIKIPAD;
   else if (strstr(name_buf, "Microsoft"))
   {
      if (strstr(name_buf, "Dual Strike"))
         device = DEVICE_MS_SIDEWINDER_DUAL_STRIKE;
      else if (strstr(name_buf, "SideWinder"))
         device = DEVICE_MS_SIDEWINDER;
      else if (strstr(name_buf, "X-Box 360") || strstr(name_buf, "X-Box")
            || strstr(name_buf, "Xbox 360 Wireless Receiver"))
         device = DEVICE_MS_XBOX;
   }
   else if (strstr(name_buf, "WiseGroup"))
   {
      if (strstr(name_buf, "TigerGame") || strstr(name_buf, "Game Controller Adapter")
            || strstr(name_buf, "JC-PS102U") || strstr(name_buf, "Dual USB Joypad"))
      {
         if (strstr(name_buf, "WiseGroup"))
            device = DEVICE_WISEGROUP_PLAYSTATION2;
         else if (strstr(name_buf, "JC-PS102U"))
            device = DEVICE_JCPS102_PLAYSTATION2;
         else
            device = DEVICE_GENERIC_PLAYSTATION2_CONVERTER;
      }
   }
   else if (strstr(name_buf, "PLAYSTATION(R)3") || strstr(name_buf, "Dualshock3")
         || strstr(name_buf,"Sixaxis") || strstr(name_buf, "Gasia,Co") ||
         (strstr(name_buf, "Gamepad 0") || strstr(name_buf, "Gamepad 1") || 
          strstr(name_buf, "Gamepad 2") || strstr(name_buf, "Gamepad 3")))
   {
      if (strstr(name_buf, "Gamepad 0") || strstr(name_buf, "Gamepad 1") || 
            strstr(name_buf, "Gamepad 2") || strstr(name_buf, "Gamepad 3"))
         device = DEVICE_PLAYSTATION3_VERSION1;
      else
         device = DEVICE_PLAYSTATION3_VERSION2;
   }
   else if (strstr(name_buf, "MOGA"))
      device = DEVICE_MOGA;
   else if (strstr(name_buf, "Sony Navigation Controller"))
      device = DEVICE_PSMOVE_NAVI;
   else if (strstr(name_buf, "OUYA Game Controller"))
      device = DEVICE_OUYA;
   else if (strstr(name_buf, "adc joystick"))
      device = DEVICE_JXD_S7300B;
   else if (strstr(name_buf, "idroid:con"))
      device = DEVICE_IDROID_CON;
   else if (strstr(name_buf, "NYKO PLAYPAD PRO"))
      device = DEVICE_NYKO_PLAYPAD_PRO;
   else if (strstr(name_buf, "2-Axis, 8-Button"))
      device = DEVICE_GENIUS_MAXFIRE_G08XU;
   else if (strstr(name_buf, "USB,2-axis 8-button gamepad"))
      device = DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD;
   else if (strstr(name_buf, "BUFFALO BGC-FC801"))
      device = DEVICE_BUFFALO_BGC_FC801;
   else if (strstr(name_buf, "8Bitdo FC30"))
      device = DEVICE_FC30_GAMEPAD;
   else if (strstr(name_buf, "RetroUSB.com RetroPad"))
      device = DEVICE_RETROUSB_RETROPAD;
   else if (strstr(name_buf, "RetroUSB.com SNES RetroPort"))
      device = DEVICE_RETROUSB_SNES_RETROPORT;
   else if (strstr(name_buf, "CYPRESS USB"))
      device = DEVICE_CYPRESS_USB;
   else if (strstr(name_buf, "Mayflash Wii Classic"))
      device = DEVICE_MAYFLASH_WII_CLASSIC;
   else if (strstr(name_buf, "SZMy-power LTD CO.  Dual Box WII"))
      device = DEVICE_SZMY_POWER_DUAL_BOX_WII;
   else if (strstr(name_buf, "Toodles 2008 ChImp"))
      device = DEVICE_TOODLES_2008_CHIMP;
   else if (strstr(name_buf, "joy_key"))
      device = DEVICE_ARCHOS_GAMEPAD;
   else if (strstr(name_buf, "matrix_keyboard"))
      device = DEVICE_JXD_S5110;
   else if (strstr(name_buf, "tincore_adc_joystick"))
      device = DEVICE_JXD_S5110_SKELROM;
   else if (strstr(name_buf, "keypad-zeus") || (strstr(name_buf, "keypad-game-zeus")))
      device = DEVICE_XPERIA_PLAY;
   else if (strstr(name_buf, "Broadcom Bluetooth HID"))
      device = DEVICE_BROADCOM_BLUETOOTH_HID;
   else if (strstr(name_buf, "USB Gamepad"))
      device = DEVICE_THRUST_PREDATOR;
   else if (strstr(name_buf, "ADC joystick"))
      device = DEVICE_JXD_S7800B;
   else if (strstr(name_buf, "DragonRise"))
      device = DEVICE_DRAGONRISE;
   else if (strstr(name_buf, "Thrustmaster T Mini"))
      device = DEVICE_THRUSTMASTER_T_MINI;
   else if (strstr(name_buf, "2Axes 11Keys Game  Pad"))
      device = DEVICE_TOMEE_NES_USB;
   else if (strstr(name_buf, "rk29-keypad") || strstr(name_buf, "GAMEMID"))
      device = DEVICE_GAMEMID;
   else if (strstr(name_buf, "USB Gamepad"))
      device = DEVICE_DEFENDER_GAME_RACER_CLASSIC;
   else if (strstr(name_buf, "HOLTEK JC - U912F vibration game"))
      device = DEVICE_HOLTEK_JC_U912F;
   else if (strstr(name_buf, "NVIDIA Controller"))
   {
      device = DEVICE_NVIDIA_SHIELD;
      port = 0; // Shield is always player 1.
      *primary = true;
   }
   else if (strstr(name_buf, "Samsung Game Pad EI-GP20"))
      device = DEVICE_SAMSUNG_GAMEPAD_EIGP20;

   if (strstr(current_ime, "net.obsidianx.android.mogaime"))
   {
      device = DEVICE_MOGA_IME;
      snprintf(name_buf, sizeof(name_buf), "MOGA IME");
   }
   else if (strstr(current_ime, "com.ccpcreations.android.WiiUseAndroid"))
   {
      device = DEVICE_CCPCREATIONS_WIIUSE_IME;
      snprintf(name_buf, sizeof(name_buf), "ccpcreations WiiUse");
   }
   else if (strstr(current_ime, "com.hexad.bluezime"))
   {
      device = DEVICE_ICONTROLPAD_BLUEZ_IME;
      snprintf(name_buf, sizeof(name_buf), "iControlpad SPP mode (using Bluez IME)");
   }

   if (source == AINPUT_SOURCE_KEYBOARD && device != DEVICE_XPERIA_PLAY)
      device = DEVICE_KEYBOARD_RETROPAD;

   if (driver.input->set_keybinds)
      driver.input->set_keybinds(driver.input_data, device, port, id,
            (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));

   if (name_buf[0] != 0)
      snprintf(msg, sizeof_msg, "Port %d: %s.\n", port, name_buf);
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
         bool debug_enable = g_settings.input.debug_enable;
         AInputEvent* event = NULL;

         // Read all pending events.
         do
         {
            while (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
            {
               bool long_msg_enable = false;
               int32_t handled = 1;
               char msg[128];
               int source, id, keycode, type_event, port;
               int predispatched;

               msg[0] = 0;
               predispatched = AInputQueue_preDispatchEvent(android_app->inputQueue,event);

               if (predispatched)
                  continue;

               source = AInputEvent_getSource(event);
               id = AInputEvent_getDeviceId(event);
               if (id == zeus_second_id)
                  id = zeus_id;

               type_event = AInputEvent_getType(event);
               port = -1;

               if (source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE | AINPUT_SOURCE_TOUCHPAD))
                  port = 0; // touch overlay is always player 1
               else
               {
                  unsigned i;
                  for (i = 0; i < android->pads_connected; i++)
                     if (android->state_device_ids[i] == id)
                        port = i;
               }

               if (port < 0)
               {
                  port = android->pads_connected;
                  if (g_settings.input.autodetect_enable)
                  {
                     bool primary = false;
                     handle_hotplug(android_app, msg, sizeof(msg), port, id, source, &primary);

                     if (primary)
                     {
                        RARCH_LOG("Found primary input device.\n");
                        memmove(android->state_device_ids + 1, android->state_device_ids,
                              android->pads_connected * sizeof(android->state_device_ids[0]));
                        port = 0;
                        android->state_device_ids[0] = id;
                        android->pads_connected++;
                     }
                     else
                        android->state_device_ids[android->pads_connected++] = id;
                  }
                  else
                     android->state_device_ids[android->pads_connected++] = id;

                  long_msg_enable = true;
               }

               if (type_event == AINPUT_EVENT_TYPE_MOTION)
               {
                  float x = 0.0f;
                  float y = 0.0f;

                  if (android_input_poll_event_type_motion(android, event, &x, &y, port, source))
                     engine_handle_dpad(android, event, port, msg, sizeof(msg), source, debug_enable);
                  else
                  {
                     if (debug_enable)
                        snprintf(msg, sizeof(msg), "Pad %d : x = %.2f, y = %.2f, src %d.\n", port, x, y, source);
                  }
               }
               else if (type_event == AINPUT_EVENT_TYPE_KEY)
               {
                  keycode = AKeyEvent_getKeyCode(event);
                  android_input_poll_event_type_key(android, android_app, event, port, keycode, source, type_event, &handled);
                  if (debug_enable)
                     snprintf(msg, sizeof(msg), "Pad %d : %d, src = %d.\n", port, keycode, source);
               }

               if (msg[0] != 0)
               {
                  msg_queue_clear(g_extern.msg_queue);
                  msg_queue_push(g_extern.msg_queue, msg, 0, long_msg_enable ? 180 : 30);
                  RARCH_LOG("Input debug: %s\n", msg);
               }

               AInputQueue_finishEvent(android_app->inputQueue, event, handled);
            }
         }while (AInputQueue_hasEvents(android_app->inputQueue));
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

unsigned android_input_devices_size(void *data)
{
   return DEVICE_LAST;
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
   android_input_set_keybinds,
   android_input_set_sensor_state,
   android_input_get_sensor_input,
   android_input_get_capabilities,
   android_input_devices_size,
   "android_input",

   NULL,
   NULL,
   android_input_get_joypad_driver,
};

static bool android_joypad_init(void)
{
   int i;

   for (i = 0; i < MAX_PLAYERS; i++)
      strlcpy(g_settings.input.device_names[i], "Custom", sizeof(g_settings.input.device_names[i]));

   if ((dlopen("/system/lib/libandroid.so", RTLD_LOCAL | RTLD_LAZY)) != 0)
   {
      engine_handle_dpad = engine_handle_dpad_default;
      p_AMotionEvent_getAxisValue = dlsym(RTLD_DEFAULT, "AMotionEvent_getAxisValue");

      if (p_AMotionEvent_getAxisValue)
      {
         RARCH_LOG("Set engine_handle_dpad to 'Get Axis Value' (for reading extra analog sticks)");
         engine_handle_dpad = engine_handle_dpad_getaxisvalue;
      }
   }
   else
   {
      RARCH_WARN("Unable to open libandroid.so\n");
   }

   return true;
}

static bool android_joypad_button(unsigned port_num, uint16_t joykey)
{
   android_input_t *android = (android_input_t*)driver.input_data;

   if (!android || port_num >= MAX_PADS || joykey >= LAST_KEYCODE)
      return false;

   return get_bit(android->pad_state[port_num], joykey);
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

static const char *android_joypad_name(unsigned pad)
{
   return NULL;
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
