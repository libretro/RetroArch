/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include "android_general.h"
#include "../../../input/input_common.h"
#include "../../../performance.h"
#include "../../../general.h"
#include "../../../driver.h"

#define AKEY_EVENT_NO_ACTION 255
#define MAX_PADS 8
#define MAX_TOUCH 8

enum
{
   DPAD_EMULATION_NONE = 0,
   DPAD_EMULATION_LSTICK,
   DPAD_EMULATION_RSTICK,
   DPAD_EMULATION_LAST
};

enum {
   AKEYCODE_ESCAPE          = 111,
   AKEYCODE_BREAK           = 121,
   AKEYCODE_F2              = 132,
   AKEYCODE_F3              = 133,
   AKEYCODE_F4              = 134,
   AKEYCODE_F5              = 135,
   AKEYCODE_F6              = 136,
   AKEYCODE_F7              = 137,
   AKEYCODE_F8              = 138,
   AKEYCODE_F9              = 139,
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

#define PRESSED_UP(x, y)   ((-0.80f > y) && (x >= -1.00f))
#define PRESSED_DOWN(x, y) ((0.80f  < y) && (y <= 1.00f))
#define PRESSED_LEFT(x, y) ((-0.80f > x) && (x >= -1.00f))
#define PRESSED_RIGHT(x, y) ((0.80f  < x) && (x <= 1.00f))

#define MAX_DEVICE_IDS 50

static unsigned pads_connected;
static uint64_t state[MAX_PADS];
static int8_t state_device_ids[MAX_DEVICE_IDS];
static uint64_t keycode_lut[LAST_KEYCODE];

struct input_pointer
{
   int16_t x, y;
};

static struct input_pointer pointer[MAX_TOUCH];
static unsigned pointer_count;

static void get_device_name(char *buf, size_t size, int id)
{
   JavaVM *vm = g_android.app->activity->vm;
   JNIEnv *env = NULL;
   (*vm)->AttachCurrentThread(vm, &env, 0);

   jclass input_device_class = NULL;
   FIND_CLASS(env, input_device_class, "android/view/InputDevice");

   jmethodID method = NULL;
   GET_STATIC_METHOD_ID(env, method, input_device_class, "getDevice", "(I)Landroid/view/InputDevice;");

   jobject device = NULL;
   CALL_OBJ_STATIC_METHOD_PARAM(env, device, input_device_class, method, (jint)id);

   jmethodID getName = NULL;
   GET_METHOD_ID(env, getName, input_device_class, "getName", "()Ljava/lang/String;");

   jobject name = NULL;
   CALL_OBJ_METHOD(env, name, device, getName);

   const char *str = (*env)->GetStringUTFChars(env, name, 0);
   strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);

   (*vm)->DetachCurrentThread(vm);
}

static void setup_keycode_lut(unsigned port, unsigned id)
{
   char msg[128];
   msg[0] = 0;

   if (port > MAX_PADS)
   {
      snprintf(msg, sizeof(msg), "Max number of pads reached.\n");
      msg_queue_push(g_extern.msg_queue, msg, 0, 30);
      return;
   }

   char name_buf[256];

   g_settings.input.dpad_emulation[port] = DPAD_EMULATION_LSTICK;

   get_device_name(name_buf, sizeof(name_buf), id);
   RARCH_LOG("Device %d: %s, port: %d.\n", id, name_buf, port);

   /* eight 8-bit values are packed into one uint64_t
    * one for each of the 8 pads */
   uint8_t shift = 8 + (port * 8);

   // Hack - we have to add '1' to the bit mask here because
   // RETRO_DEVICE_ID_JOYPAD_B is 0

   if (strstr(name_buf, "Logitech"))
   {
      if(strstr(name_buf, "RumblePad 2"))
      {
         snprintf(msg, sizeof(msg), "RetroPad #%d is: RumblePad 2.\n", port);
         keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
         keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
         keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
         keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
      }
   }

#if 0
   // com.ccpcreations.android.WiiUseAndroid IME driver

   // Player 1

   keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
   keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
   keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
   keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
   keycode_lut[AKEYCODE_1]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
   keycode_lut[AKEYCODE_2]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
   keycode_lut[AKEYCODE_3]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
   keycode_lut[AKEYCODE_4]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
   keycode_lut[AKEYCODE_5]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
   keycode_lut[AKEYCODE_6]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)   << shift);
   keycode_lut[AKEYCODE_M]         |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
   keycode_lut[AKEYCODE_P]         |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
   keycode_lut[AKEYCODE_E]         |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
   keycode_lut[AKEYCODE_B]         |= ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
   keycode_lut[AKEYCODE_F]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
   keycode_lut[AKEYCODE_G]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
   keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
   keycode_lut[AKEYCODE_LEFT_BRACKET]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
   keycode_lut[AKEYCODE_RIGHT_BRACKET] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);
   keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
   keycode_lut[AKEYCODE_H]         |= ((RARCH_RESET+1)                    << shift);
   keycode_lut[AKEYCODE_W]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
   keycode_lut[AKEYCODE_S]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
   keycode_lut[AKEYCODE_A]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
   keycode_lut[AKEYCODE_D]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
   keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
   keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
#endif

   else if (strstr(name_buf, "HuiJia  USB GamePad"))
   {
      snprintf(msg, sizeof(msg), "RetroPad #%d is: HuiJia USB Gamepad.\n", port);
      keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
   }
   else if (strstr(name_buf, "Microsoft"))
   {
      if (strstr(name_buf, "Dual Strike"))
      {
         snprintf(msg, sizeof(msg), "RetroPad #%d is: Sidewinder Dual Strike.\n", port);
         keycode_lut[AKEYCODE_BUTTON_4] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_2] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_3] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X) << shift);
         keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L) << shift);
         keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R) << shift);
         keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_L2) << shift);
      }
      else if (strstr(name_buf, "SideWinder"))
      {
         snprintf(msg, sizeof(msg), "RetroPad #%d is: Sidewinder.\n", port);
         keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
      }
   }
   else if (strstr(name_buf, "WiseGroup") && strstr(name_buf, "Dual USB Joypad"))
   {
      snprintf(msg, sizeof(msg), "RetroPad #%d is: WiseGroup PS2 to USB.\n", port);
      keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
   }
   else if (strstr(name_buf, "PLAYSTATION(R)3"))
   {
      snprintf(msg, sizeof(msg), "RetroPad #%d is: DualShock3/Sixaxis.\n", port);
      g_settings.input.dpad_emulation[port] = DPAD_EMULATION_NONE;
      keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
      keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
      keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
      keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
      keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
      keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
      keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
      keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
      keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
      keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
      keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
      keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
      keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
      keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
      keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
      keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
   }

   // Keyboard
   // TODO: Map L2/R2/L3/R3

   keycode_lut[AKEYCODE_Z] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
   keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
   keycode_lut[AKEYCODE_SHIFT_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
   keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
   keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
   keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
   keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
   keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
   keycode_lut[AKEYCODE_X] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
   keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
   keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
   keycode_lut[AKEYCODE_W] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);

   /* Misc control scheme */
   keycode_lut[AKEYCODE_F2] |= ((RARCH_SAVE_STATE_KEY+1) << shift);
   keycode_lut[AKEYCODE_F4] |= ((RARCH_LOAD_STATE_KEY+1) << shift);
   keycode_lut[AKEYCODE_F7] |= ((RARCH_STATE_SLOT_PLUS+1) << shift);
   keycode_lut[AKEYCODE_F6] |= ((RARCH_STATE_SLOT_MINUS+1) << shift);
   keycode_lut[AKEYCODE_SPACE] |= ((RARCH_FAST_FORWARD_KEY+1) << shift);
   keycode_lut[AKEYCODE_L] |= ((RARCH_FAST_FORWARD_HOLD_KEY+1) << shift);
   keycode_lut[AKEYCODE_BREAK] |= ((RARCH_PAUSE_TOGGLE+1) << shift);
   keycode_lut[AKEYCODE_K] |= ((RARCH_FRAMEADVANCE+1) << shift);
   keycode_lut[AKEYCODE_H] |= ((RARCH_RESET+1) << shift);
   keycode_lut[AKEYCODE_R] |= ((RARCH_REWIND+1) << shift);
   keycode_lut[AKEYCODE_F9] |= ((RARCH_MUTE+1) << shift);

   keycode_lut[AKEYCODE_ESCAPE] |= ((RARCH_QUIT_KEY+1) << shift);
   keycode_lut[AKEYCODE_BACK] |= ((RARCH_QUIT_KEY+1) << shift);

   if (msg[0] != 0)
      msg_queue_push(g_extern.msg_queue, msg, 0, 30);
}

static void *android_input_init(void)
{
   pads_connected = 0;

   for(unsigned player = 0; player < 4; player++)
      for(unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         g_settings.input.binds[player][i].id = i;
         g_settings.input.binds[player][i].joykey = 0;
      }

   for(int player = 0; player < 4; player++)
   {
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_B].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_Y].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_Y);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_SELECT].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_START].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_START);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_UP].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_UP);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_A].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_A);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_X].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_X);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_L].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_L);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_R].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_R);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_L2].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_L2);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_R2].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_R2);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_L3].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_L3);
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_R3].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_R3);
   }

   for(int i = 0; i < MAX_DEVICE_IDS; i++)
      state_device_ids[i] = -1;

   for(int j = 0; j < LAST_KEYCODE; j++)
      keycode_lut[j] = 0;

   return (void*)-1;
}


static void android_input_poll(void *data)
{
   (void)data;

   RARCH_PERFORMANCE_INIT(input_poll);
   RARCH_PERFORMANCE_START(input_poll);

   struct android_app* android_app = g_android.app;

   g_extern.lifecycle_state &= ~((1ULL << RARCH_RESET) | (1ULL << RARCH_REWIND) | (1ULL << RARCH_FAST_FORWARD_KEY) | (1ULL << RARCH_FAST_FORWARD_HOLD_KEY) | (1ULL << RARCH_MUTE) | (1ULL << RARCH_SAVE_STATE_KEY) | (1ULL << RARCH_LOAD_STATE_KEY) | (1ULL << RARCH_STATE_SLOT_PLUS) | (1ULL << RARCH_STATE_SLOT_MINUS));

   // Read all pending events.
   while(AInputQueue_hasEvents(android_app->inputQueue))
   {
      AInputEvent* event = NULL;
      AInputQueue_getEvent(android_app->inputQueue, &event);

      if (AInputQueue_preDispatchEvent(android_app->inputQueue, event))
         continue;

      int32_t handled = 1;

      int source = AInputEvent_getSource(event);
      int id = AInputEvent_getDeviceId(event);


      int type_event = AInputEvent_getType(event);
      int state_id = state_device_ids[id];

      if(state_id == -1)
      {
         state_id = state_device_ids[id] = pads_connected++;
         setup_keycode_lut(state_id, id);
      }

      int action = 0;
#ifdef RARCH_INPUT_DEBUG
      char msg[128];
#endif

      if(type_event == AINPUT_EVENT_TYPE_MOTION && (g_settings.input.dpad_emulation[state_id] != DPAD_EMULATION_NONE))
      {
         action = AMotionEvent_getAction(event);
         size_t motion_pointer = action >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
         action &= AMOTION_EVENT_ACTION_MASK;

         if(source & ~(AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE))
         {
            float x = AMotionEvent_getX(event, motion_pointer);
            float y = AMotionEvent_getY(event, motion_pointer);
            state[state_id] &= ~((1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) |
                  (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN));
            state[state_id] |= PRESSED_LEFT(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
            state[state_id] |= PRESSED_RIGHT(x, y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
            state[state_id] |= PRESSED_UP(x, y)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
            state[state_id] |= PRESSED_DOWN(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
         }
         else
         {
            bool keyup = (action == AMOTION_EVENT_ACTION_UP ||
                  action == AMOTION_EVENT_ACTION_CANCEL || action == AMOTION_EVENT_ACTION_POINTER_UP) ||
               (source == AINPUT_SOURCE_MOUSE && action != AMOTION_EVENT_ACTION_DOWN);

            if (!keyup)
            {
               float x = AMotionEvent_getX(event, motion_pointer);
               float y = AMotionEvent_getY(event, motion_pointer);

               input_translate_coord_viewport(x, y,
                     &pointer[motion_pointer].x, &pointer[motion_pointer].y);

               pointer_count = max(pointer_count, motion_pointer + 1);
            }
            else
            {
               memmove(pointer + motion_pointer, pointer + motion_pointer + 1, (MAX_TOUCH - motion_pointer - 1) * sizeof(struct input_pointer));
               pointer_count--;
            }
         }
#ifdef RARCH_INPUT_DEBUG
         snprintf(msg, sizeof(msg), "Pad %d : x = %.2f, y = %.2f, src %d.\n", state_id, x, y, source);
#endif
      }
      else if (type_event == AINPUT_EVENT_TYPE_KEY)
      {
         int keycode = AKeyEvent_getKeyCode(event);

         /* Hack - we have to decrease the unpacked value by 1
          * because we 'added' 1 to each entry in the LUT -
          * RETRO_DEVICE_ID_JOYPAD_B is 0
          */
         uint8_t unpacked = (keycode_lut[keycode] >> ((state_id+1) << 3)) - 1;
         uint64_t input_state = (1ULL << unpacked);
         int action  = AKeyEvent_getAction(event);
#ifdef RARCH_INPUT_DEBUG
         snprintf(msg, sizeof(msg), "Pad %d : %d, ac = %d, src = %d.\n", state_id, keycode, action, source);
#endif
         uint64_t *key = NULL;

         if(input_state < (1ULL << RARCH_FIRST_META_KEY))
            key = &state[state_id];
         else if(input_state)
            key = &g_extern.lifecycle_state;

         if(key != NULL)
         {
            if (action == AKEY_EVENT_ACTION_UP)
               *key &= ~(input_state);
            else if (action == AKEY_EVENT_ACTION_DOWN)
               *key |= input_state;
         }

         if(keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN)
            handled = 0;
      }
#ifdef RARCH_INPUT_DEBUG
      msg_queue_push(g_extern.msg_queue, msg, 0, 30);
#endif
      AInputQueue_finishEvent(android_app->inputQueue, event, handled);
   }

#ifdef RARCH_INPUT_DEBUG
   {
      char msg[64];
      snprintf(msg, sizeof(msg), "Pointers: %u", pointer_count);
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 0, 30);
   }
#endif

   RARCH_PERFORMANCE_STOP(input_poll);
}

static int16_t android_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return ((state[port] & binds[port][id].joykey) && (port < pads_connected));
      case RETRO_DEVICE_POINTER:
         switch(id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return pointer[index].x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return pointer[index].y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return index < pointer_count;
            default:
               return 0;
         }
      default:
         return 0;
   }
}

static bool android_input_key_pressed(void *data, int key)
{
   return ((g_extern.lifecycle_state | driver.overlay_state) & (1ULL << key));
}

static void android_input_free_input(void *data)
{
   (void)data;
}

const input_driver_t input_android = {
   android_input_init,
   android_input_poll,
   android_input_state,
   android_input_key_pressed,
   android_input_free_input,
   "android_input",
};

