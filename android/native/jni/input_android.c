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
#include "../../../performance.h"
#include "../../../general.h"
#include "../../../driver.h"

#define AKEY_EVENT_NO_ACTION 255
#define MAX_PADS 8

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
static int64_t keycode_lut[LAST_KEYCODE];

static void setup_keycode_lut(void)
{
   for(int i = 0; i < LAST_KEYCODE; i++)
      keycode_lut[i] = -1;


   /* Control scheme 1
    * fd=196
    * path='/dev/input/event4'
    * name='Logitech Logitech RumblePad 2 USB'
    * classes=0x80000141
    * configuration=''
    * keyLayout='/system/usr/keylayout/Generic.kl'
    * keyCharacterMap='/system/usr/keychars/Generic.kcm'
    * builtinKeyboard=false
    */

   keycode_lut[AKEYCODE_BUTTON_2] = (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
   keycode_lut[AKEYCODE_BUTTON_1] = (1ULL << RETRO_DEVICE_ID_JOYPAD_Y);
   keycode_lut[AKEYCODE_BUTTON_9] = (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT);
   keycode_lut[AKEYCODE_BUTTON_10] = (1ULL << RETRO_DEVICE_ID_JOYPAD_START);
   keycode_lut[AKEYCODE_BUTTON_3] = (1ULL << RETRO_DEVICE_ID_JOYPAD_A);
   keycode_lut[AKEYCODE_BUTTON_4] = (1ULL << RETRO_DEVICE_ID_JOYPAD_X);
   keycode_lut[AKEYCODE_BUTTON_5] = (1ULL << RETRO_DEVICE_ID_JOYPAD_L);
   keycode_lut[AKEYCODE_BUTTON_6] = (1ULL << RETRO_DEVICE_ID_JOYPAD_R);
   keycode_lut[AKEYCODE_BUTTON_7] = (1ULL << RETRO_DEVICE_ID_JOYPAD_L2);
   keycode_lut[AKEYCODE_BUTTON_8] = (1ULL << RETRO_DEVICE_ID_JOYPAD_R2);
   keycode_lut[AKEYCODE_BUTTON_11] = (1ULL << RETRO_DEVICE_ID_JOYPAD_L3);
   keycode_lut[AKEYCODE_BUTTON_12] = (1ULL << RETRO_DEVICE_ID_JOYPAD_R3);

   /* Control scheme 2
    * Tested with: SNES Pad USB converter
    * fd=196
    * path='/dev/input/event4'
    * name='HuiJia  USB GamePad'
    * classes=0x80000141
    * configuration=''
    * keyLayout='/system/usr/keylayout/Generic.kl'
    * keyCharacterMap='/system/usr/keychars/Generic.kcm'
    * builtinKeyboard=false
    */

   keycode_lut[AKEYCODE_BUTTON_C] = (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
   keycode_lut[AKEYCODE_BUTTON_X] = (1ULL << RETRO_DEVICE_ID_JOYPAD_Y);
   keycode_lut[AKEYCODE_BUTTON_L2] = (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT);
   keycode_lut[AKEYCODE_BUTTON_R2] = (1ULL << RETRO_DEVICE_ID_JOYPAD_START);
   keycode_lut[AKEYCODE_BUTTON_B] = (1ULL << RETRO_DEVICE_ID_JOYPAD_A);
   keycode_lut[AKEYCODE_BUTTON_A] = (1ULL << RETRO_DEVICE_ID_JOYPAD_X);
   keycode_lut[AKEYCODE_BUTTON_L1] = (1ULL << RETRO_DEVICE_ID_JOYPAD_L);
   keycode_lut[AKEYCODE_BUTTON_R1] = (1ULL << RETRO_DEVICE_ID_JOYPAD_R);

   /* Control scheme 3
    * fd=196
    * path='/dev/input/event4'
    * name='Microsoft® Microsoft® SideWinder® Game Pad USB'
    * classes=0x80000141
    * configuration=''
    * keyLayout='/system/usr/keylayout/Generic.kl'
    * keyCharacterMap='/system/usr/keychars/Generic.kcm'
    * builtinKeyboard=false
    */

   /*
      keycode_lut[AKEYCODE_BUTTON_A] = ANDROID_GAMEPAD_CROSS;
      keycode_lut[AKEYCODE_BUTTON_X] = ANDROID_GAMPAD_SQUARE:
      keycode_lut[AKEYCODE_BUTTON_R2] = ANDROID_GAMEPAD_SELECT;
      keycode_lut[AKEYCODE_BUTTON_L2] = ANDROID_GAMEPAD_START;
      keycode_lut[AKEYCODE_BUTTON_B] = ANDROID_GAMEPAD_CIRCLE;
      keycode_lut[AKEYCODE_BUTTON_Y] = ANDROID_GAMEPAD_TRIANGLE;
      keycode_lut[AKEYCODE_BUTTON_L1] = ANDROID_GAMEPAD_L1;
      keycode_lut[AKEYCODE_BUTTON_R1] = ANDROID_GAMEPAD_R1;
      keycode_lut[AKEYCODE_BUTTON_Z] = ANDROID_GAMEPAD_L2;
      keycode_lut[AKEYCODE_BUTTON_C] = ANDROID_GAMEPAD_R2;
      keycode_lut[AKEYCODE_BUTTON_11] = ANDROID_GAMEPAD_L3;
      keycode_lut[AKEYCODE_BUTTON_12] = ANDROID_GAMEPAD_R3;
      */

   /* Control scheme 4
    * Tested with: Sidewinder Dual Strike
    * fd=196
    * path='/dev/input/event4'
    * name='Microsoft SideWinder Dual Strike USB version 1.0'
    * classes=0x80000141
    * configuration=''
    * keyLayout='/system/usr/keylayout/Generic.kl'
    * keyCharacterMap='/system/usr/keychars/Generic.kcm'
    * builtinKeyboard=false
    */

   /*
      keycode_lut[AKEYCODE_BUTTON_4] = ANDROID_GAMEPAD_CROSS;
      keycode_lut[AKEYCODE_BUTTON_2] = ANDROID_GAMPAD_SQUARE:
      keycode_lut[AKEYCODE_BUTTON_6] = ANDROID_GAMEPAD_SELECT;
      keycode_lut[AKEYCODE_BUTTON_5] = ANDROID_GAMEPAD_START;
      keycode_lut[AKEYCODE_BUTTON_3] = ANDROID_GAMEPAD_CIRCLE;
      keycode_lut[AKEYCODE_BUTTON_1] = ANDROID_GAMEPAD_TRIANGLE;
      keycode_lut[AKEYCODE_BUTTON_7] = ANDROID_GAMEPAD_L1;
      keycode_lut[AKEYCODE_BUTTON_8] = ANDROID_GAMEPAD_R1;
      keycode_lut[AKEYCODE_BUTTON_9] = ANDROID_GAMEPAD_L2;
      */

   /* Control scheme 5
    * fd=196
    * path='/dev/input/event4'
    * name='WiseGroup.,Ltd MP-8866 Dual USB Joypad'
    * classes=0x80000141
    * configuration=''
    * keyLayout='/system/usr/keylayout/Generic.kl'
    * keyCharacterMap='/system/usr/keychars/Generic.kcm'
    * builtinKeyboard=false
    */

   /*
      keycode_lut[AKEYCODE_BUTTON_3] = ANDROID_GAMEPAD_CROSS;
      keycode_lut[AKEYCODE_BUTTON_4] = ANDROID_GAMPAD_SQUARE:
      keycode_lut[AKEYCODE_BUTTON_10] = ANDROID_GAMEPAD_SELECT;
      keycode_lut[AKEYCODE_BUTTON_9] = ANDROID_GAMEPAD_START;
      keycode_lut[AKEYCODE_BUTTON_2] = ANDROID_GAMEPAD_CIRCLE;
      keycode_lut[AKEYCODE_BUTTON_1] = ANDROID_GAMEPAD_TRIANGLE;
      keycode_lut[AKEYCODE_BUTTON_7] = ANDROID_GAMEPAD_L1;
      keycode_lut[AKEYCODE_BUTTON_8] = ANDROID_GAMEPAD_R1;
      keycode_lut[AKEYCODE_BUTTON_5] = ANDROID_GAMEPAD_L2;
      keycode_lut[AKEYCODE_BUTTON_6] = ANDROID_GAMEPAD_R2;
      keycode_lut[AKEYCODE_BUTTON_11] = ANDROID_GAMEPAD_L3;
      keycode_lut[AKEYCODE_BUTTON_12] = ANDROID_GAMEPAD_R3;
      */

   /* Control scheme 6
    * Keyboard
    * TODO: Map L2/R2/L3/R3
    * */

   keycode_lut[AKEYCODE_Z] = (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
   keycode_lut[AKEYCODE_A] = (1ULL << RETRO_DEVICE_ID_JOYPAD_Y);
   keycode_lut[AKEYCODE_SHIFT_RIGHT] = (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT);
   keycode_lut[AKEYCODE_ENTER] = (1ULL << RETRO_DEVICE_ID_JOYPAD_START);
   keycode_lut[AKEYCODE_DPAD_UP] = (1ULL << RETRO_DEVICE_ID_JOYPAD_UP);
   keycode_lut[AKEYCODE_DPAD_DOWN] = (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN);
   keycode_lut[AKEYCODE_DPAD_LEFT] = (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT);
   keycode_lut[AKEYCODE_DPAD_RIGHT] = (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   keycode_lut[AKEYCODE_X] = (1ULL << RETRO_DEVICE_ID_JOYPAD_A);
   keycode_lut[AKEYCODE_S] = (1ULL << RETRO_DEVICE_ID_JOYPAD_X);
   keycode_lut[AKEYCODE_Q] = (1ULL << RETRO_DEVICE_ID_JOYPAD_L);
   keycode_lut[AKEYCODE_W] = (1ULL << RETRO_DEVICE_ID_JOYPAD_R);

   /* Misc control scheme */
   keycode_lut[AKEYCODE_BACK] = (1ULL << RARCH_QUIT_KEY);
   keycode_lut[AKEYCODE_F2] = (1ULL << RARCH_SAVE_STATE_KEY);
   keycode_lut[AKEYCODE_F4] = (1ULL << RARCH_LOAD_STATE_KEY);
   keycode_lut[AKEYCODE_F7] = (1ULL << RARCH_STATE_SLOT_PLUS);
   keycode_lut[AKEYCODE_F6] = (1ULL << RARCH_STATE_SLOT_MINUS);
   keycode_lut[AKEYCODE_SPACE] = (1ULL << RARCH_FAST_FORWARD_KEY);
   keycode_lut[AKEYCODE_L] = (1ULL << RARCH_FAST_FORWARD_HOLD_KEY);
   keycode_lut[AKEYCODE_ESCAPE] = (1ULL << RARCH_QUIT_KEY);
   keycode_lut[AKEYCODE_BREAK] = (1ULL << RARCH_PAUSE_TOGGLE);
   keycode_lut[AKEYCODE_K] = (1ULL << RARCH_FRAMEADVANCE);
   keycode_lut[AKEYCODE_H] = (1ULL << RARCH_RESET);
   keycode_lut[AKEYCODE_R] = (1ULL << RARCH_REWIND);
   keycode_lut[AKEYCODE_F9] = (1ULL << RARCH_MUTE);
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

   setup_keycode_lut();

   return (void*)-1;
}

static void android_input_poll(void *data)
{
   (void)data;

   struct android_app* android_app = g_android.app;

   g_extern.lifecycle_state &= ~((1ULL << RARCH_RESET) | (1ULL << RARCH_REWIND) | (1ULL << RARCH_FAST_FORWARD_KEY) | (1ULL << RARCH_FAST_FORWARD_HOLD_KEY) | (1ULL << RARCH_MUTE) | (1ULL << RARCH_SAVE_STATE_KEY) | (1ULL << RARCH_LOAD_STATE_KEY) | (1ULL << RARCH_STATE_SLOT_PLUS) | (1ULL << RARCH_STATE_SLOT_MINUS));

   // Read all pending events.
   while(AInputQueue_hasEvents(android_app->inputQueue))
   {
      AInputEvent* event = NULL;
      AInputQueue_getEvent(android_app->inputQueue, &event);

      int32_t handled = 1;

      int id = AInputEvent_getDeviceId(event);
      int type = AInputEvent_getType(event);
      int i = state_device_ids[id];

      if(i == -1)
         i = state_device_ids[id] = pads_connected++;

      int motion_action = AMotionEvent_getAction(event);
      int pointer_count = AMotionEvent_getPointerCount(event);
      bool motion_do = ((motion_action == AMOTION_EVENT_ACTION_DOWN) || (motion_action ==
               AMOTION_EVENT_ACTION_POINTER_DOWN) || (motion_action == AMOTION_EVENT_ACTION_MOVE)
            && pointer_count);

      if(type == AINPUT_EVENT_TYPE_MOTION && motion_do)
      {
         float x = AMotionEvent_getX(event, 0);
         float y = AMotionEvent_getY(event, 0);
#ifdef RARCH_INPUT_DEBUG
         char msg[128];
         snprintf(msg, sizeof(msg), "RetroPad %d : x = %f, y = %f.\n", i, x, y);
         msg_queue_push(g_extern.msg_queue, msg, 0, 30);
#endif
         state[i] &= ~((1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) |
               (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN));
         state[i] |= PRESSED_LEFT(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
         state[i] |= PRESSED_RIGHT(x, y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         state[i] |= PRESSED_UP(x, y)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
         state[i] |= PRESSED_DOWN(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
      }
      else
      {
         int keycode = AKeyEvent_getKeyCode(event);
         uint64_t input_state = keycode_lut[keycode];
#ifdef RARCH_INPUT_DEBUG
         char msg[128];
         snprintf(msg, sizeof(msg), "Keycode RetroPad %d : %d.\n", i, keycode);
         msg_queue_push(g_extern.msg_queue, msg, 0, 30);
#endif
         int action  = AKeyEvent_getAction(event);
         uint64_t *key = NULL;

         if(input_state < (1ULL << RARCH_FIRST_META_KEY))
            key = &state[i];
         else if(input_state != -1)
            key = &g_extern.lifecycle_state;

         if(key != NULL)
         {
            if(action == AKEY_EVENT_ACTION_DOWN)
               *key |= input_state;
            else if(action == AKEY_EVENT_ACTION_UP)
               *key &= ~(input_state);
         }

         if(keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN || input_state != -1)
            handled = 0;
      }
      AInputQueue_finishEvent(android_app->inputQueue, event, handled);
   }
}

static int16_t android_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   unsigned player = port; 
   uint64_t button = binds[player][id].joykey;
   int16_t retval = 0;

   if((player < pads_connected))
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            retval = (state[player] & button) ? 1 : 0;
            break;
      }
    }

   return retval;
}

static bool android_input_key_pressed(void *data, int key)
{
   (void)data;

   if(key == RARCH_QUIT_KEY && (g_extern.lifecycle_state & (1ULL << RARCH_KILL)))
      return true;
   else if(g_extern.lifecycle_state & (1ULL << key))
      return true;

   return false;
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

