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

#include <dlfcn.h>
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
};

enum {
    AKEYSTATE_DONT_PROCESS   = 0,
    AKEYSTATE_PROCESS        = 1,
};

typedef struct {
   int32_t  a_keycode;
   uint16_t r_keycode;
} rarch_android_bind_t;

/* Control scheme 1
 * fd=196
 * path='/dev/input/event4'
 * name='Logitech Logitech RumblePad 2 USB'
 * classes=0x80000141
 * configuration=''
 * keyLayout='/system/usr/keylayout/Generic.kl'
 * keyCharacterMap='/system/usr/keychars/Generic.kcm'
 * builtinKeyboard=false
* 
 */

rarch_android_bind_t android_binds[] = {
   {AKEYCODE_BUTTON_2, ANDROID_GAMEPAD_CROSS},     /* 2     */ 
   {AKEYCODE_BUTTON_1, ANDROID_GAMEPAD_SQUARE},    /* 1     */
   {AKEYCODE_BUTTON_9, ANDROID_GAMEPAD_SELECT},    /* 9     */
   {AKEYCODE_BUTTON_10, ANDROID_GAMEPAD_START},    /* 10    */
   {0, ANDROID_GAMEPAD_DPAD_UP},
   {0, ANDROID_GAMEPAD_DPAD_DOWN},
   {0, ANDROID_GAMEPAD_DPAD_LEFT},
   {0, ANDROID_GAMEPAD_DPAD_RIGHT},
   {AKEYCODE_BUTTON_3, ANDROID_GAMEPAD_CIRCLE},    /* 3     */
   {AKEYCODE_BUTTON_4, ANDROID_GAMEPAD_TRIANGLE},  /* 4     */
   {AKEYCODE_BUTTON_5, ANDROID_GAMEPAD_L1},        /* 5     */
   {AKEYCODE_BUTTON_6, ANDROID_GAMEPAD_R1},        /* 6     */
   {AKEYCODE_BUTTON_7, ANDROID_GAMEPAD_L2},        /* 7     */
   {AKEYCODE_BUTTON_8, ANDROID_GAMEPAD_R2},        /* 8     */
   {AKEYCODE_BUTTON_11, ANDROID_GAMEPAD_L3},       /* ThumbL*/
   {AKEYCODE_BUTTON_12, ANDROID_GAMEPAD_R3},       /* ThumbR*/
};

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

rarch_android_bind_t android_binds_snes[] = {
   {AKEYCODE_BUTTON_C, ANDROID_GAMEPAD_CROSS},     /* B     */
   {AKEYCODE_BUTTON_X, ANDROID_GAMEPAD_SQUARE},    /* Y     */
   {AKEYCODE_BUTTON_L2, ANDROID_GAMEPAD_SELECT},   /* SEL   */
   {AKEYCODE_BUTTON_R2, ANDROID_GAMEPAD_START},    /* START */
   {0, ANDROID_GAMEPAD_DPAD_UP},
   {0, ANDROID_GAMEPAD_DPAD_DOWN},
   {0, ANDROID_GAMEPAD_DPAD_LEFT},
   {0, ANDROID_GAMEPAD_DPAD_RIGHT},
   {AKEYCODE_BUTTON_B, ANDROID_GAMEPAD_CIRCLE},    /* A     */
   {AKEYCODE_BUTTON_A, ANDROID_GAMEPAD_TRIANGLE},  /* X     */
   {AKEYCODE_BUTTON_L1, ANDROID_GAMEPAD_L1},       /* L     */
   {AKEYCODE_BUTTON_R1, ANDROID_GAMEPAD_R1},       /* R     */
   {AKEYCODE_BUTTON_7, ANDROID_GAMEPAD_L2},        /* NA    */
   {AKEYCODE_BUTTON_8, ANDROID_GAMEPAD_R2},        /* NA    */
   {AKEYCODE_BUTTON_11, ANDROID_GAMEPAD_L3},       /* NA    */
   {AKEYCODE_BUTTON_12, ANDROID_GAMEPAD_R3},       /* NA    */
};

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

rarch_android_bind_t android_binds_sidewinder[] = {
   {AKEYCODE_BUTTON_A, ANDROID_GAMEPAD_CROSS},    /* A      */
   {AKEYCODE_BUTTON_X, ANDROID_GAMEPAD_SQUARE},   /* X      */
   {AKEYCODE_BUTTON_R2, ANDROID_GAMEPAD_SELECT},  /* .      */
   {AKEYCODE_BUTTON_L2, ANDROID_GAMEPAD_START},   /* ..     */
   {0, ANDROID_GAMEPAD_DPAD_UP},
   {0, ANDROID_GAMEPAD_DPAD_DOWN},
   {0, ANDROID_GAMEPAD_DPAD_LEFT},
   {0, ANDROID_GAMEPAD_DPAD_RIGHT},
   {AKEYCODE_BUTTON_B, ANDROID_GAMEPAD_CIRCLE},   /* B      */
   {AKEYCODE_BUTTON_Y, ANDROID_GAMEPAD_TRIANGLE}, /* Y      */
   {AKEYCODE_BUTTON_L1, ANDROID_GAMEPAD_L1},      /* L      */
   {AKEYCODE_BUTTON_R1, ANDROID_GAMEPAD_R1},      /* R      */
   {AKEYCODE_BUTTON_Z, ANDROID_GAMEPAD_L2},       /* Z      */
   {AKEYCODE_BUTTON_C, ANDROID_GAMEPAD_R2},       /* C      */
   {AKEYCODE_BUTTON_11, ANDROID_GAMEPAD_L3},      /* NA     */
   {AKEYCODE_BUTTON_12, ANDROID_GAMEPAD_R3},      /* NA     */
};

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

rarch_android_bind_t android_binds_sidewinder_ds[] = {
   {AKEYCODE_BUTTON_4, ANDROID_GAMEPAD_CROSS},    /* D      */
   {AKEYCODE_BUTTON_2, ANDROID_GAMEPAD_SQUARE},   /* B      */
   {AKEYCODE_BUTTON_6, ANDROID_GAMEPAD_SELECT},   /* Y      */
   {AKEYCODE_BUTTON_5, ANDROID_GAMEPAD_START},    /* X      */
   {0, ANDROID_GAMEPAD_DPAD_UP},
   {0, ANDROID_GAMEPAD_DPAD_DOWN},
   {0, ANDROID_GAMEPAD_DPAD_LEFT},
   {0, ANDROID_GAMEPAD_DPAD_RIGHT},
   {AKEYCODE_BUTTON_3, ANDROID_GAMEPAD_CIRCLE},   /* C      */
   {AKEYCODE_BUTTON_1, ANDROID_GAMEPAD_TRIANGLE}, /* A      */
   {AKEYCODE_BUTTON_7, ANDROID_GAMEPAD_L1},       /* L      */
   {AKEYCODE_BUTTON_8, ANDROID_GAMEPAD_R1},       /* R      */
   {AKEYCODE_BUTTON_9, ANDROID_GAMEPAD_L2},       /* ARROW  */
   {AKEYCODE_BUTTON_C, ANDROID_GAMEPAD_R2},       /* NA     */
   {AKEYCODE_BUTTON_11, ANDROID_GAMEPAD_L3},      /* NA     */
   {AKEYCODE_BUTTON_12, ANDROID_GAMEPAD_R3},      /* NA     */
};

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

rarch_android_bind_t android_binds_psx[] = {
   {AKEYCODE_BUTTON_3, ANDROID_GAMEPAD_CROSS},    /* CROSS  */
   {AKEYCODE_BUTTON_4, ANDROID_GAMEPAD_SQUARE},   /* SQUARE */
   {AKEYCODE_BUTTON_10, ANDROID_GAMEPAD_SELECT},  /* SELECT */
   {AKEYCODE_BUTTON_9, ANDROID_GAMEPAD_START},    /* START  */
   {0, ANDROID_GAMEPAD_DPAD_UP},
   {0, ANDROID_GAMEPAD_DPAD_DOWN},
   {0, ANDROID_GAMEPAD_DPAD_LEFT},
   {0, ANDROID_GAMEPAD_DPAD_RIGHT},
   {AKEYCODE_BUTTON_2, ANDROID_GAMEPAD_CIRCLE},   /* CIRCLE */
   {AKEYCODE_BUTTON_1, ANDROID_GAMEPAD_TRIANGLE}, /* TRIANGLE*/
   {AKEYCODE_BUTTON_7, ANDROID_GAMEPAD_L1},       /* L1     */
   {AKEYCODE_BUTTON_8, ANDROID_GAMEPAD_R1},       /* R1     */
   {AKEYCODE_BUTTON_5, ANDROID_GAMEPAD_L2},       /* L2     */
   {AKEYCODE_BUTTON_6, ANDROID_GAMEPAD_R2},       /* R2     */
   {AKEYCODE_BUTTON_11, ANDROID_GAMEPAD_L3},      /* L3     */
   {AKEYCODE_BUTTON_12, ANDROID_GAMEPAD_R3},      /* R3     */
};

/* Control scheme 6
 * Keyboard
 * TODO: Map L2/R2/L3/R3
 */

rarch_android_bind_t android_binds_keyboard[] = {
   {AKEYCODE_Z, ANDROID_GAMEPAD_CROSS},           /* Z      */
   {AKEYCODE_A, ANDROID_GAMEPAD_SQUARE},          /* A      */
   {AKEYCODE_SHIFT_RIGHT, ANDROID_GAMEPAD_SELECT},/* RShift */
   {AKEYCODE_ENTER, ANDROID_GAMEPAD_START},       /* Enter  */
   {AKEYCODE_DPAD_UP, ANDROID_GAMEPAD_DPAD_UP},
   {AKEYCODE_DPAD_DOWN, ANDROID_GAMEPAD_DPAD_DOWN},
   {AKEYCODE_DPAD_LEFT, ANDROID_GAMEPAD_DPAD_LEFT},
   {AKEYCODE_DPAD_RIGHT, ANDROID_GAMEPAD_DPAD_RIGHT},
   {AKEYCODE_X, ANDROID_GAMEPAD_CIRCLE},          /* X      */
   {AKEYCODE_S, ANDROID_GAMEPAD_TRIANGLE},        /* S      */
   {AKEYCODE_Q, ANDROID_GAMEPAD_L1},              /* Q      */
   {AKEYCODE_W, ANDROID_GAMEPAD_R1},              /* W      */
   {0, ANDROID_GAMEPAD_L2},                       /* NA     */
   {0, ANDROID_GAMEPAD_R2},                       /* NA     */
   {0, ANDROID_GAMEPAD_L3},                       /* NA     */
   {0, ANDROID_GAMEPAD_R3},                       /* NA     */
};

//#define RARCH_INPUT_DEBUG

static unsigned pads_connected;
static android_input_state_t state[MAX_PADS];

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
   int id, i;
   bool found_existing_id = false;

   id = AInputEvent_getDeviceId(event);

   for (i = 0; i < pads_connected; i++)
   {
      if (id == state[i].id)
      {
         found_existing_id = true;
         break;
      }
   }

   if(!found_existing_id)
   {
      state[pads_connected++].id = id;
      i = pads_connected;
   }

   {
      bool do_keydown = false;
      bool do_keyrelease = false;
      bool pressed_left, pressed_right, pressed_up, pressed_down;
      float x, y;
      int action, keycode, source, type;
      action = AKEY_EVENT_NO_ACTION;

      type    = AInputEvent_getType(event);
      source  = AInputEvent_getSource(event);
      keycode = AKeyEvent_getKeyCode(event);

#ifdef RARCH_INPUT_DEBUG
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

      action  = AKeyEvent_getAction(event);

      switch(type)
      {
         case AINPUT_EVENT_TYPE_MOTION:
            x = AMotionEvent_getX(event, 0);
            y = AMotionEvent_getY(event, 0);
            pressed_up    = ((-0.80f > y) && (x >= -1.00f));
            pressed_down  = ((0.80f  < y) && (y <= 1.00f));
            pressed_left  = ((-0.80f > x) && (x >= -1.00f));
            pressed_right = ((0.80f  < x) && (x <= 1.00f));
#ifdef RARCH_INPUT_DEBUG
            RARCH_LOG("AINPUT_EVENT_TYPE_MOTION, pad: %d, x: %f, y: %f.\n", i, x, y);
#endif
	    state[i].state &= ~(android_binds[RETRO_DEVICE_ID_JOYPAD_LEFT].r_keycode);
	    state[i].state &= ~(android_binds[RETRO_DEVICE_ID_JOYPAD_RIGHT].r_keycode);
	    state[i].state &= ~(android_binds[RETRO_DEVICE_ID_JOYPAD_UP].r_keycode);
	    state[i].state &= ~(android_binds[RETRO_DEVICE_ID_JOYPAD_DOWN].r_keycode);
	    state[i].state |= pressed_left                    ? ANDROID_GAMEPAD_DPAD_LEFT : 0;
	    state[i].state |= pressed_right                   ? ANDROID_GAMEPAD_DPAD_RIGHT : 0;
	    state[i].state |= pressed_up                      ? ANDROID_GAMEPAD_DPAD_UP : 0;
	    state[i].state |= pressed_down                    ? ANDROID_GAMEPAD_DPAD_DOWN : 0;
            break;
      }

      if(action != AKEY_EVENT_NO_ACTION)
      {
         switch(action)
         {
            case AKEY_EVENT_ACTION_DOWN:
            case AKEY_EVENT_ACTION_MULTIPLE:
#ifdef RARCH_INPUT_DEBUG
               RARCH_LOG("AKEY_EVENT_ACTION_DOWN, pad: %d, keycode: %d.\n", i, keycode);
#endif
	       do_keydown = true;
	       do_keyrelease = false;
               break;
            case AKEY_EVENT_ACTION_UP:
#ifdef RARCH_INPUT_DEBUG
               RARCH_LOG("AKEY_EVENT_ACTION_UP, pad: %d, keycode: %d.\n", i, keycode);
#endif
	       do_keydown = false;
	       do_keyrelease = true;
               break;
         }
      }

      if(do_keydown)
      {
         state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_START].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_START].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_R3].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_R3].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_L3].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_L3].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_SELECT].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_SELECT].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_X].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_X].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_Y].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_Y].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_B].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_B].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_A].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_A].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_R].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_R].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_L].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_L].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_R2].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_R2].r_keycode) : 0;
	 state[i].state |= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_L2].a_keycode) ? (android_binds[RETRO_DEVICE_ID_JOYPAD_L2].r_keycode) : 0;
      }

      if(do_keyrelease)
      {
         state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_START].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_START].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_R3].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_R3].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_L3].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_L3].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_SELECT].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_SELECT].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_X].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_X].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_Y].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_Y].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_B].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_B].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_A].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_A].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_R].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_R].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_L].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_L].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_R2].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_R2].r_keycode) : ~0;
	 state[i].state &= (keycode == android_binds[RETRO_DEVICE_ID_JOYPAD_L2].a_keycode) ? ~(android_binds[RETRO_DEVICE_ID_JOYPAD_L2].r_keycode) : ~0;
      }

   }

   return 1;
}

static void *android_input_init(void)
{
   void *libandroid = 0;

   g_android.app->onInputEvent = engine_handle_input;
   pads_connected = 0;

   return (void*)-1;
}

static void android_input_poll(void *data)
{
   (void)data;
}

static int16_t android_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   unsigned player = port; 
   uint64_t button = (id < 16) ? android_binds[id].r_keycode : 0;
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

