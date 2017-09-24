/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2014-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include <wiiu/nsyskbd.h>
#include <wiiu/vpad.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_driver.h"
#include "../input_keymaps.h"

#include "wiiu_dbg.h"

#define MAX_PADS 5

static unsigned char keyboardChannel = 0x00;
static KBDModifier keyboardModifier = 0x00;
static unsigned char keyboardCode = 0x00;
static KEYState keyboardState[256] = { KBD_WIIU_NULL };

typedef struct wiiu_input
{
   bool blocked;
   const input_device_driver_t *joypad;
} wiiu_input_t;

uint64_t lifecycle_state;


void kb_connection_callback(KBDKeyEvent *key)
{
	keyboardChannel = keyboardChannel + (key->channel + 0x01);
}

void kb_disconnection_callback(KBDKeyEvent *key)
{
	keyboardChannel = keyboardChannel - (key->channel + 0x01);
}

void kb_key_callback(KBDKeyEvent *key)
{
   uint16_t mod        = 0;
   unsigned code       = 0;
   bool pressed        = false;

   keyboardModifier    = key->modifier;
   keyboardCode        = key->scancode;

   if (key->state > 0)
      pressed = true;

   code                = input_keymaps_translate_keysym_to_rk(key->scancode);
   keyboardState[code] = key->state;

   if (key->modifier & KBD_WIIU_SHIFT)
      mod |= RETROKMOD_SHIFT;

   if (key->modifier & KBD_WIIU_CTRL)
      mod |= RETROKMOD_CTRL;

   if (key->modifier & KBD_WIIU_ALT)
      mod |= RETROKMOD_ALT;

   if (key->modifier & KBD_WIIU_NUM_LOCK)
      mod |= RETROKMOD_NUMLOCK;

   if (key->modifier & KBD_WIIU_CAPS_LOCK)
      mod |= RETROKMOD_CAPSLOCK;

   if (key->modifier & KBD_WIIU_SCROLL_LOCK)
      mod |= RETROKMOD_SCROLLOCK;

   input_keyboard_event(pressed, code, (char)key->UTF16, mod,
         RETRO_DEVICE_KEYBOARD);
}

/* TODO: emulate a relative mouse. This is suprisingly
   hard to get working nicely.
*/

static int16_t wiiu_pointer_device_state(wiiu_input_t* wiiu, unsigned id)
{
	switch (id)
	{
		case RETRO_DEVICE_ID_POINTER_PRESSED:
			return wiiu->joypad->get_buttons(0) & VPAD_BUTTON_TOUCH;
		case RETRO_DEVICE_ID_POINTER_X:
			return wiiu->joypad->axis(0, 0xFFFF0004UL);
		case RETRO_DEVICE_ID_POINTER_Y:
			return wiiu->joypad->axis(0, 0xFFFF0005UL);
	}

	return 0;
}

static void wiiu_input_poll(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;

   if (wiiu && wiiu->joypad)
      wiiu->joypad->poll();
}

static bool wiiu_key_pressed(int key)
{
   bool ret = false;

   if (key >= RETROK_LAST)
      return false;

   if ((keyboardState[key] > 0) && (keyboardChannel > 0))
      ret = true;

   return ret;
}

static int16_t wiiu_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   wiiu_input_t *wiiu         = (wiiu_input_t*)data;

   if(!wiiu || !(port < MAX_PADS) || !binds || !binds[port])
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(wiiu->joypad,
               joypad_info, port, binds[port], id);
      case RETRO_DEVICE_KEYBOARD:
         return wiiu_key_pressed(id);
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(wiiu->joypad,
                  joypad_info, port, idx, id, binds[port]);
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return wiiu_pointer_device_state(wiiu, id);
   }

   return 0;
}

static void wiiu_input_free_input(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;

   if (wiiu && wiiu->joypad)
      wiiu->joypad->destroy();

   KBDTeardown();

   free(data);
}

static void* wiiu_input_init(const char *joypad_driver)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)calloc(1, sizeof(*wiiu));
   if (!wiiu)
      return NULL;

   DEBUG_STR(joypad_driver);
   wiiu->joypad = input_joypad_init_driver(joypad_driver, wiiu);

   KBDSetup(&kb_connection_callback,
         &kb_disconnection_callback,&kb_key_callback);

   input_keymaps_init_keyboard_lut(rarch_key_map_wiiu);

   return wiiu;
}

static bool wiiu_input_meta_key_pressed(void *data, int key)
{
   if (BIT64_GET(lifecycle_state, key))
      return true;

   return false;
}

static uint64_t wiiu_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |
          (1 << RETRO_DEVICE_ANALOG) |
          (1 << RETRO_DEVICE_KEYBOARD) |
          (1 << RETRO_DEVICE_POINTER);
}

static const input_device_driver_t *wiiu_input_get_joypad_driver(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;
   if (wiiu)
      return wiiu->joypad;
   return NULL;
}

static void wiiu_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool wiiu_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

static bool wiiu_input_keyboard_mapping_is_blocked(void *data)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;
   if (!wiiu)
      return false;
   return wiiu->blocked;
}

static void wiiu_input_keyboard_mapping_set_block(void *data, bool value)
{
   wiiu_input_t *wiiu = (wiiu_input_t*)data;
   if (!wiiu)
      return;
   wiiu->blocked = value;
}

input_driver_t input_wiiu = {
   wiiu_input_init,
   wiiu_input_poll,
   wiiu_input_state,
   wiiu_input_meta_key_pressed,
   wiiu_input_free_input,
   NULL,
   NULL,
   wiiu_input_get_capabilities,
   "wiiu",
   wiiu_input_grab_mouse,
   NULL,
   wiiu_input_set_rumble,
   wiiu_input_get_joypad_driver,
   NULL,
   wiiu_input_keyboard_mapping_is_blocked,
   wiiu_input_keyboard_mapping_set_block,
};
