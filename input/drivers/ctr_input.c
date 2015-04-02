/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
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

#include "3ds.h"

#include "../../general.h"
#include "../../driver.h"


static uint32_t kDown;
static int16_t pad_state;

static void *ctr_input_init(void)
{
   return (void*)-1;
}

static void ctr_input_poll(void *data)
{   
   (void)data;

   hidScanInput();
   kDown = hidKeysHeld();

   pad_state = 0;
   pad_state |= (kDown & KEY_DLEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   pad_state |= (kDown & KEY_DDOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
   pad_state |= (kDown & KEY_DRIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   pad_state |= (kDown & KEY_DUP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   pad_state |= (kDown & KEY_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
   pad_state |= (kDown & KEY_SELECT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
   pad_state |= (kDown & KEY_X) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
   pad_state |= (kDown & KEY_Y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
   pad_state |= (kDown & KEY_B) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
   pad_state |= (kDown & KEY_A) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
   pad_state |= (kDown & KEY_R) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   pad_state |= (kDown & KEY_L) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;
}

static int16_t ctr_input_state(void *data,
      const struct retro_keybind **retro_keybinds, unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   (void)data;
   (void)retro_keybinds;
   (void)port;
   (void)device;
   (void)idx;
   (void)id;

   if (port > 0)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      return pad_state;

      case RETRO_DEVICE_ANALOG:
      return 0;
   }

   return 0;
}

static bool ctr_input_key_pressed(void *data, int key)
{
   (void)data;

   return (pad_state & (1ULL << key));
}

static void ctr_input_free_input(void *data)
{
   (void)data;
}

static uint64_t ctr_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);

   return caps;
}

static bool ctr_input_set_sensor_state(void *data,
      unsigned port, enum retro_sensor_action action, unsigned event_rate)
{
   return false;
}

static void ctr_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool ctr_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

input_driver_t input_ctr = {
   ctr_input_init,
   ctr_input_poll,
   ctr_input_state,
   ctr_input_key_pressed,
   ctr_input_free_input,
   ctr_input_set_sensor_state,
   NULL,
   ctr_input_get_capabilities,
   "ctr",
   ctr_input_grab_mouse,
   ctr_input_set_rumble,
};
