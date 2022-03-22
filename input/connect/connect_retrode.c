/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include "joypad_connection.h"

#include "../input_defines.h"

#define RETRODE_MAX_PAD 4

#define RETRODE_TYPE_DEVICE 0x00
#define RETRODE_TYPE_PAD    0x01

typedef struct hidpad_retrode_pad_data retrode_pad_data_t;
typedef struct hidpad_retrode_data retrode_device_data_t;

struct hidpad_retrode_pad_data
{
   uint8_t datatype;
   retrode_device_data_t *device_data;
   joypad_connection_t *joypad;
   int pad_index;
   uint32_t buttons;
   uint8_t data[64];
};

struct hidpad_retrode_data
{
   uint8_t datatype;
   void *handle;
   hid_driver_t *driver;
   retrode_pad_data_t pad_data[RETRODE_MAX_PAD];
   uint8_t data[64];
};

const char *RETRODE_PAD         = "Retrode pad";
const char *RETRODE_DEVICE_NAME = "Retrode adapter";

static void* hidpad_retrode_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   int i;
   retrode_device_data_t * device  = (retrode_device_data_t *)calloc(1, sizeof(retrode_device_data_t));

   if (!device)
      return NULL;

   if (!data)
   {
      free(device);
      return NULL;
   }

   device->handle                     = data;

   for (i = 0; i < RETRODE_MAX_PAD; i++)
   {
      device->pad_data[i].datatype    = RETRODE_TYPE_PAD;
      device->pad_data[i].device_data = device;
      device->pad_data[i].joypad      = NULL;
      device->pad_data[i].pad_index   = i;
   }

   device->driver                     = driver;

   return device;
}

static void hidpad_retrode_deinit(void *data)
{
   retrode_device_data_t *device = (retrode_device_data_t *)data;

   if (device)
      free(device);
}

static void hidpad_retrode_get_buttons(void *pad_data, input_bits_t *state)
{
    retrode_pad_data_t *pad = (retrode_pad_data_t *)pad_data;
    if (pad)
    {
       if(pad->datatype == RETRODE_TYPE_PAD)
       {
          BITS_COPY16_PTR(state, pad->buttons);
       }
       else
       {
          retrode_device_data_t *device = (retrode_device_data_t *)pad_data;
          BITS_COPY16_PTR(state, device->pad_data[0].buttons);
       }
    }
    else
    {
       BIT256_CLEAR_ALL_PTR(state);
    }
}

static int16_t hidpad_retrode_get_axis(void *pad_data, unsigned axis)
{
   int val;
   retrode_pad_data_t *pad = (retrode_pad_data_t *)pad_data;
   retrode_device_data_t *device = (retrode_device_data_t *)pad_data;

   if (!pad || axis >= 2)
      return 0;

   if(pad->datatype == RETRODE_TYPE_PAD)
      val = pad->data[2 + axis];
   else
      val = device->pad_data[0].data[2 + axis];

   /* map Retrode values to a known gamepad (VID=0x0079, PID=0x0011) */
   if (val == 0x9C)
      val = 0x00; /* axis=0 left, axis=1 up */
   else if (val == 0x64)
      val = 0xFF; /* axis=0 right, axis=1 down */
   else
      val = 0x7F; /* no button pressed */

   val = (val << 8) - 0x8000;

   if (abs(val) > 0x1000)
      return val;
   return 0;
}

static void retrode_update_button_state(retrode_pad_data_t *pad)
{
   uint32_t i, pressed_keys;

   static const uint32_t button_mapping[8] =
   {
           RETRO_DEVICE_ID_JOYPAD_B,
           RETRO_DEVICE_ID_JOYPAD_Y,
           RETRO_DEVICE_ID_JOYPAD_SELECT,
           RETRO_DEVICE_ID_JOYPAD_START,
           RETRO_DEVICE_ID_JOYPAD_A,
           RETRO_DEVICE_ID_JOYPAD_X,
           RETRO_DEVICE_ID_JOYPAD_L,
           RETRO_DEVICE_ID_JOYPAD_R
   };

   if (!pad)
      return;

   pressed_keys = pad->data[4];
   pad->buttons = 0;

   for (i = 0; i < 8; i ++)
      if (button_mapping[i] != NO_BTN)
          pad->buttons |= (pressed_keys & (1 << i)) ? (1 << button_mapping[i]) : 0;
}

static void hidpad_retrode_pad_packet_handler(retrode_pad_data_t *pad, uint8_t *packet, size_t size)
{
   memcpy(pad->data, packet, size);
   retrode_update_button_state(pad);
}

static void hidpad_retrode_packet_handler(void *device_data, uint8_t *packet, uint16_t size)
{
   retrode_device_data_t *device = (retrode_device_data_t *)device_data;

   if (!device)
      return;

   memcpy(device->data, packet, size);

   /*
    * packet[1] contains Retrode port number
    * 1 = left SNES
    * 2 = right SNES
    * 3 = left Genesis/MD
    * 4 = right Genesis/MD
    */

   hidpad_retrode_pad_packet_handler(&device->pad_data[packet[1] - 1], &device->data[0], size);
}

static void hidpad_retrode_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
    (void)data;
    (void)effect;
    (void)strength;
}

const char * hidpad_retrode_get_name(void *pad_data)
{
   /* this could be improved by marking it as pad/mouse */
   retrode_pad_data_t *pad = (retrode_pad_data_t *)pad_data;
   if(!pad || pad->datatype != RETRODE_TYPE_PAD)
      return RETRODE_DEVICE_NAME;

   return RETRODE_PAD;
}

static int32_t hidpad_retrode_button(void *pad_data, uint16_t joykey)
{
   retrode_pad_data_t *pad = (retrode_pad_data_t *)pad_data;

   if (!pad)
      return 0;

   if (pad->datatype != RETRODE_TYPE_PAD)
   {
      retrode_device_data_t *device = (retrode_device_data_t *)pad_data;
      pad = &device->pad_data[0];
  }

  if (!pad->device_data || joykey > 31 || !pad->joypad)
     return 0;

  return pad->buttons & (1 << joykey);
}

static void *hidpad_retrode_pad_init(void *device_data, int pad_index, joypad_connection_t *joypad)
{
   retrode_device_data_t *device = (retrode_device_data_t *)device_data;

   if(!device || pad_index < 0 || pad_index >= RETRODE_MAX_PAD || !joypad || device->pad_data[pad_index].joypad)
      return NULL;

   device->pad_data[pad_index].joypad = joypad;
   return &device->pad_data[pad_index];
}

static void hidpad_retrode_pad_deinit(void *pad_data)
{
   retrode_pad_data_t *pad = (retrode_pad_data_t *)pad_data;

   if(!pad)
      return;

   pad->joypad = NULL;
}

static int8_t hidpad_retrode_status(void *device_data, int pad_index)
{
   retrode_device_data_t *device = (retrode_device_data_t *)device_data;
   int8_t result = 0;

   if(!device || pad_index < 0 || pad_index >= RETRODE_MAX_PAD)
      return 0;

  result |= PAD_CONNECT_READY;

   if (device->pad_data[pad_index].joypad)
      result |= PAD_CONNECT_BOUND;

   return result;
}

static joypad_connection_t *hidpad_retrode_joypad(void *device_data, int pad_index)
{
   retrode_device_data_t *device = (retrode_device_data_t *)device_data;

   if(!device || pad_index < 0 || pad_index >= RETRODE_MAX_PAD)
      return 0;
   return device->pad_data[pad_index].joypad;
}

pad_connection_interface_t pad_connection_retrode = {
   hidpad_retrode_init,
   hidpad_retrode_deinit,
   hidpad_retrode_packet_handler,
   hidpad_retrode_set_rumble,
   hidpad_retrode_get_buttons,
   hidpad_retrode_get_axis,
   hidpad_retrode_get_name,
   hidpad_retrode_button,
   true,
   RETRODE_MAX_PAD,
   hidpad_retrode_pad_init,
   hidpad_retrode_pad_deinit,
   hidpad_retrode_status,
   hidpad_retrode_joypad,
};
