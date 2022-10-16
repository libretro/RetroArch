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

#include "../../config.def.h"

#include "../input_driver.h"
#include "../input_keymaps.h"

#include "wiiu_dbg.h"

/* TODO/FIXME - static global variables */
static uint8_t keyboard_channel         = 0x00;
static bool keyboard_state[RETROK_LAST] = { 0 };

static void kb_connection_callback(KBDKeyEvent *key)
{
   keyboard_channel += (key->channel + 0x01);
}

static void kb_disconnection_callback(KBDKeyEvent *key)
{
	keyboard_channel -= (key->channel + 0x01);
}

static void kb_key_callback(KBDKeyEvent *key)
{
   uint16_t mod            = 0;
   unsigned code           = 0;
   bool pressed            = false;

   if (key->state > 0)
      pressed              = true;

   code                    = input_keymaps_translate_keysym_to_rk(
         key->scancode);
   if (code < RETROK_LAST)
      keyboard_state[code] = pressed;

   if (key->modifier & KBD_WIIU_SHIFT)
      mod                 |= RETROKMOD_SHIFT;

   if (key->modifier & KBD_WIIU_CTRL)
      mod                 |= RETROKMOD_CTRL;

   if (key->modifier & KBD_WIIU_ALT)
      mod                 |= RETROKMOD_ALT;

   if (key->modifier & KBD_WIIU_NUM_LOCK)
      mod                 |= RETROKMOD_NUMLOCK;

   if (key->modifier & KBD_WIIU_CAPS_LOCK)
      mod                 |= RETROKMOD_CAPSLOCK;

   if (key->modifier & KBD_WIIU_SCROLL_LOCK)
      mod                 |= RETROKMOD_SCROLLOCK;

   input_keyboard_event(pressed, code, (char)key->UTF16, mod,
         RETRO_DEVICE_KEYBOARD);
}

static int16_t wiiu_input_state(
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
   if (!(port < DEFAULT_MAX_PADS) || !binds || !binds[port])
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         if (id < RETROK_LAST && keyboard_state[id] && (keyboard_channel > 0))
            return 1;
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         /* TODO: Emulate a relative mouse. 
          * This is suprisingly hard to get working nicely.
            */
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               {
                  input_bits_t state;
                  joypad->get_buttons(0, &state);
                  return BIT256_GET(state, VPAD_BUTTON_TOUCH_BIT) ? 1 : 0;
               }
            case RETRO_DEVICE_ID_POINTER_X:
               return joypad->axis(0, 0xFFFF0004UL);
            case RETRO_DEVICE_ID_POINTER_Y:
               return joypad->axis(0, 0xFFFF0005UL);
         }
         break;
   }

   return 0;
}

static void wiiu_input_free_input(void *data)
{
   KBDTeardown();
}

static void* wiiu_input_init(const char *joypad_driver)
{
   KBDSetup(
         &kb_connection_callback,
         &kb_disconnection_callback,
         &kb_key_callback);

   input_keymaps_init_keyboard_lut(rarch_key_map_wiiu);

   return (void*)-1;
}

static uint64_t wiiu_input_get_capabilities(void *data)
{
   return   (1 << RETRO_DEVICE_JOYPAD)
          | (1 << RETRO_DEVICE_ANALOG)
          | (1 << RETRO_DEVICE_KEYBOARD)
          | (1 << RETRO_DEVICE_POINTER);
}

input_driver_t input_wiiu = {
   wiiu_input_init,
   NULL,                            /* poll */
   wiiu_input_state,
   wiiu_input_free_input,
   NULL,
   NULL,
   wiiu_input_get_capabilities,
   "wiiu",
   NULL,                            /* grab_mouse */
   NULL,
   NULL
};
