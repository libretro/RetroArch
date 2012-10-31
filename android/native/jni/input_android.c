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
#include "android-general.h"
#include "../../../general.h"
#include "../../../driver.h"
#include "input_android.h"

enum {
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

enum {
    AKEYSTATE_DONT_PROCESS   = 0,
    AKEYSTATE_PROCESS        = 1,
};


#define PRESSED_UP(x, y)   ((-0.80f > y) && (x >= -1.00f))
#define PRESSED_DOWN(x, y) ((0.80f  < y) && (y <= 1.00f))
#define PRESSED_LEFT(x, y) ((-0.80f > x) && (x >= -1.00f))
#define PRESSED_RIGHT(x, y) ((0.80f  < x) && (x <= 1.00f))

//#define RARCH_INPUT_DEBUG

static unsigned pads_connected;
static android_input_state_t state[MAX_PADS];
static int16_t state_device_ids[50];
static int32_t keycode_lut[LAST_KEYCODE];

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
   int id = AInputEvent_getDeviceId(event);
   int i = state_device_ids[id];

   if(i == -1)
   {
      i = state_device_ids[id] = pads_connected++;
      state[i].id = id;
   }

   int type    = AInputEvent_getType(event);
   int keycode = AKeyEvent_getKeyCode(event);

#ifdef RARCH_INPUT_DEBUG
   int source  = AInputEvent_getSource(event);

   switch(source)
   {
      case AINPUT_SOURCE_DPAD:
         RARCH_LOG("AINPUT_SOURCE_DPAD, pad: %d, keycode: %d.\n", i, keycode);
         break;
      case AINPUT_SOURCE_TOUCHSCREEN:
         RARCH_LOG("AINPUT_SOURCE_TOUCHSCREEN, pad: %d, keycode: %d.\n", i, keycode);
         break; 
      case AINPUT_SOURCE_TOUCHPAD:
         RARCH_LOG("AINPUT_SOURCE_TOUCHPAD, pad: %d, keycode: %d.\n", i, keycode);
         break;
      case AINPUT_SOURCE_ANY:
         RARCH_LOG("AINPUT_SOURCE_ANY, pad: %d, keycode: %d.\n", i, keycode);
         break;
      case 0:
      default:
         RARCH_LOG("AINPUT_SOURCE_DEFAULT, pad: %d, keycode: %d.\n", i, keycode);
         break;
   }
#endif

   int action  = AKeyEvent_getAction(event);

   if(type == AINPUT_EVENT_TYPE_MOTION)
   {
      float x = AMotionEvent_getX(event, 0);
      float y = AMotionEvent_getY(event, 0);
#ifdef RARCH_INPUT_DEBUG
      RARCH_LOG("AINPUT_EVENT_TYPE_MOTION, pad: %d, x: %f, y: %f.\n", i, x, y);
#endif
      state[i].state &= ~(ANDROID_GAMEPAD_DPAD_LEFT | ANDROID_GAMEPAD_DPAD_RIGHT |
            ANDROID_GAMEPAD_DPAD_UP | ANDROID_GAMEPAD_DPAD_DOWN);
      state[i].state |= PRESSED_LEFT(x, y)  ? ANDROID_GAMEPAD_DPAD_LEFT  : 0;
      state[i].state |= PRESSED_RIGHT(x, y) ? ANDROID_GAMEPAD_DPAD_RIGHT : 0;
      state[i].state |= PRESSED_UP(x, y)    ? ANDROID_GAMEPAD_DPAD_UP    : 0;
      state[i].state |= PRESSED_DOWN(x, y)  ? ANDROID_GAMEPAD_DPAD_DOWN  : 0;
   }

   if(action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_MULTIPLE)
      state[i].state |= keycode_lut[keycode];

   if(action == AKEY_EVENT_ACTION_UP)
      state[i].state &= ~(keycode_lut[keycode]);

   if(keycode == AKEYCODE_BACK || keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN)
      return 0;

   return 1;
}

static void setup_keycode_lut(void)
{
   for(int i = 0; i < LAST_KEYCODE; i++)
      keycode_lut[i] = 0;


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

   keycode_lut[AKEYCODE_BUTTON_2] = ANDROID_GAMEPAD_CROSS;
   keycode_lut[AKEYCODE_BUTTON_1] = ANDROID_GAMEPAD_SQUARE;
   keycode_lut[AKEYCODE_BUTTON_9] = ANDROID_GAMEPAD_SELECT;
   keycode_lut[AKEYCODE_BUTTON_10] = ANDROID_GAMEPAD_START;
   keycode_lut[AKEYCODE_BUTTON_3] = ANDROID_GAMEPAD_CIRCLE;
   keycode_lut[AKEYCODE_BUTTON_4] = ANDROID_GAMEPAD_TRIANGLE;
   keycode_lut[AKEYCODE_BUTTON_5] = ANDROID_GAMEPAD_L1;
   keycode_lut[AKEYCODE_BUTTON_6] = ANDROID_GAMEPAD_R1;
   keycode_lut[AKEYCODE_BUTTON_7] = ANDROID_GAMEPAD_L2;
   keycode_lut[AKEYCODE_BUTTON_8] = ANDROID_GAMEPAD_R2;
   keycode_lut[AKEYCODE_BUTTON_11] = ANDROID_GAMEPAD_L3;
   keycode_lut[AKEYCODE_BUTTON_12] = ANDROID_GAMEPAD_R3;

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

   keycode_lut[AKEYCODE_BUTTON_C] = ANDROID_GAMEPAD_CROSS;
   keycode_lut[AKEYCODE_BUTTON_X] = ANDROID_GAMEPAD_SQUARE;
   keycode_lut[AKEYCODE_BUTTON_L2] = ANDROID_GAMEPAD_SELECT;
   keycode_lut[AKEYCODE_BUTTON_R2] = ANDROID_GAMEPAD_START;
   keycode_lut[AKEYCODE_BUTTON_B] = ANDROID_GAMEPAD_CIRCLE;
   keycode_lut[AKEYCODE_BUTTON_A] = ANDROID_GAMEPAD_TRIANGLE;
   keycode_lut[AKEYCODE_BUTTON_L1] = ANDROID_GAMEPAD_L1;
   keycode_lut[AKEYCODE_BUTTON_R1] = ANDROID_GAMEPAD_R1;

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

   keycode_lut[AKEYCODE_Z] = ANDROID_GAMEPAD_CROSS;
   keycode_lut[AKEYCODE_A] = ANDROID_GAMEPAD_SQUARE;
   keycode_lut[AKEYCODE_SHIFT_RIGHT] = ANDROID_GAMEPAD_SELECT;
   keycode_lut[AKEYCODE_ENTER] = ANDROID_GAMEPAD_START;
   keycode_lut[AKEYCODE_DPAD_UP] = ANDROID_GAMEPAD_DPAD_UP;
   keycode_lut[AKEYCODE_DPAD_DOWN] = ANDROID_GAMEPAD_DPAD_DOWN;
   keycode_lut[AKEYCODE_DPAD_LEFT] = ANDROID_GAMEPAD_DPAD_LEFT;
   keycode_lut[AKEYCODE_DPAD_RIGHT] = ANDROID_GAMEPAD_DPAD_RIGHT;
   keycode_lut[AKEYCODE_X] = ANDROID_GAMEPAD_CIRCLE;
   keycode_lut[AKEYCODE_S] = ANDROID_GAMEPAD_TRIANGLE;
   keycode_lut[AKEYCODE_Q] = ANDROID_GAMEPAD_L1;
   keycode_lut[AKEYCODE_W] = ANDROID_GAMEPAD_R1;
}

static void setup_state_ids(void)
{
   for(int i = 0; i < 50; i++)
      state_device_ids[i] = -1;
}

static void *android_input_init(void)
{
   g_android.app->onInputEvent = engine_handle_input;
   pads_connected = 0;


   for(unsigned player = 0; player < 4; player++)
      for(unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         g_settings.input.binds[player][i].id = i;
         g_settings.input.binds[player][i].joykey = 0;
      }

   for(int player = 0; player < 4; player++)
   {
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_B].joykey = ANDROID_GAMEPAD_CROSS;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_Y].joykey = ANDROID_GAMEPAD_SQUARE;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_SELECT].joykey = ANDROID_GAMEPAD_SELECT;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_START].joykey = ANDROID_GAMEPAD_START;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_UP].joykey = ANDROID_GAMEPAD_DPAD_UP;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey = ANDROID_GAMEPAD_DPAD_DOWN;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey = ANDROID_GAMEPAD_DPAD_LEFT;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey = ANDROID_GAMEPAD_DPAD_RIGHT;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_A].joykey = ANDROID_GAMEPAD_CIRCLE;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_X].joykey = ANDROID_GAMEPAD_TRIANGLE;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_L].joykey = ANDROID_GAMEPAD_L1;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_R].joykey = ANDROID_GAMEPAD_R1;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_L2].joykey = ANDROID_GAMEPAD_L2;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_R2].joykey = ANDROID_GAMEPAD_R2;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_L3].joykey = ANDROID_GAMEPAD_L3;
      g_settings.input.binds[player][RETRO_DEVICE_ID_JOYPAD_R3].joykey = ANDROID_GAMEPAD_R3;
   }

   setup_state_ids();
   setup_keycode_lut();

   return (void*)-1;
}

static void android_input_poll(void *data)
{
   (void)data;

    // Read all pending events.
   int ident;
   struct android_poll_source* source;
   struct android_app* state = g_android.app;

   // We loop until all events are read
   ident= ALooper_pollOnce(0, NULL, 0, (void**)&source);

   // Process this event.
   if (ident && source != NULL)
      source->process(state, source);
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
            retval = (state[player].state & button) ? 1 : 0;
#ifdef RARCH_INPUT_DEBUG
            if(retval != 0)
            {
               RARCH_LOG("state: %d, player: %d.\n", retval, player);
            }
#endif
            break;
      }
    }

   return retval;
}

static bool android_input_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;

   switch (key)
   {
      case RARCH_QUIT_KEY:
         if(g_android.init_quit)
            return true;
         else
            return false;
         break;
      default:
         (void)0;
   }

   return false;
}

static void android_input_free(void *data)
{
   (void)data;
}

const input_driver_t input_android = {
   android_input_init,
   android_input_poll,
   android_input_state,
   android_input_key_pressed,
   android_input_free,
   "android_input",
};

