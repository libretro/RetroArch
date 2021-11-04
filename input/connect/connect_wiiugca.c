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

#define GCA_MAX_PAD 4

#define GCA_TYPE_DEVICE 0x00
#define GCA_TYPE_PAD    0x01

#define GCA_PORT_INITIALIZING  0x00
#define GCA_PORT_POWERED       0x04
#define GCA_PORT_CONNECTED     0x10
#define GCA_WAVEBIRD_CONNECTED 0x22

typedef struct hidpad_wiiugca_pad_data gca_pad_data_t;
typedef struct hidpad_wiiugca_data gca_device_data_t;

struct hidpad_wiiugca_pad_data
{
   uint8_t datatype;
   gca_device_data_t *device_data;
   joypad_connection_t *joypad;
   int pad_index;
   uint32_t buttons;
   int16_t analog[3][2];
   uint8_t data[9];
};

struct hidpad_wiiugca_data
{
   uint8_t datatype;
   void *handle;
   hid_driver_t *driver;
   char connected[GCA_MAX_PAD];
   gca_pad_data_t pad_data[GCA_MAX_PAD];
   uint8_t data[64];
};

const char *GAMECUBE_PAD = "GameCube controller";
const char *WAVEBIRD_PAD = "WaveBird controller";
const char *DEVICE_NAME  = "Wii U GC Controller Adapter";

static void* hidpad_wiiugca_init(void *data, uint32_t slot, hid_driver_t *driver)
{
#ifdef WIIU
   static uint8_t magic_data[] = {0x13      }; /* Special command to enable reading */
#else
   static uint8_t magic_data[] = {0x01, 0x13}; /* Special command to enable reading */
#endif
   int i;
   gca_device_data_t * device  = (gca_device_data_t *)calloc(1, sizeof(gca_device_data_t));

   if (!device)
      return NULL;

   if (!data)
   {
      free(device);
      return NULL;
   }

   device->handle                     = data;
   for (i = 0; i < GCA_MAX_PAD; i++)
   {
      device->pad_data[i].datatype    = GCA_TYPE_PAD;
      device->pad_data[i].device_data = device;
      device->pad_data[i].joypad      = NULL;
      device->pad_data[i].pad_index   = i;
   }
   
   device->driver                     = driver;

   device->driver->send_control(device->handle, magic_data, sizeof(magic_data));

   return device;
}

static void hidpad_wiiugca_deinit(void *device_data)
{
   gca_device_data_t *device = (gca_device_data_t *)device_data;

   if (device)
      free(device);
}

static void hidpad_wiiugca_get_buttons(void *pad_data, input_bits_t *state)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;
   if (pad)
   {
      if(pad->datatype == GCA_TYPE_PAD)
      {
         BITS_COPY16_PTR(state, pad->buttons);
      }
      else
      {
         gca_device_data_t *device = (gca_device_data_t *)pad_data;
         BITS_COPY16_PTR(state, device->pad_data[0].buttons);
      }
   }
   else
   {
      BIT256_CLEAR_ALL_PTR(state);
   }
}

static int16_t hidpad_wiiugca_get_axis(void *pad_data, unsigned axis)
{
   axis_data axis_data;
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;
   gca_device_data_t *device = (gca_device_data_t *)pad_data;

   gamepad_read_axis_data(axis, &axis_data);

   if (!pad || axis_data.axis >= 4)
      return 0;

   if(pad->datatype == GCA_TYPE_PAD)
      return gamepad_get_axis_value(pad->analog, &axis_data);

   return gamepad_get_axis_value(device->pad_data[0].analog, &axis_data);
}

static void update_button_state(gca_pad_data_t *pad)
{
   uint32_t i, pressed_keys;

   static const uint32_t button_mapping[12] =
   {
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_LEFT,
      RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L,
   };

   if (!pad)
      return;

   pressed_keys     = pad->data[1] | (pad->data[2] << 8);
   pad->buttons     = 0;

   for (i = 0; i < 12; i++)
      pad->buttons |= (pressed_keys & (1 << i)) ?
         (1 << button_mapping[i]) : 0;
}

static void update_analog_state(gca_pad_data_t *pad)
{
   int pad_axis;
   int16_t interpolated;
   unsigned stick, axis;

   /* GameCube analog axis are 8-bit unsigned, where 128/128 is center.
    * So, we subtract 128 to get a signed, 0-based value and then mulitply
    * by 256 to get the 16-bit range RetroArch expects. */
   for (pad_axis = 0; pad_axis < 4; pad_axis++)
   {
      axis         = (pad_axis % 2) ? 0 : 1;
      stick        = pad_axis / 2;
      interpolated = pad->data[3 + pad_axis];
      /* libretro requires "up" to be negative, so we invert the y axis */
      interpolated = (axis) ?
         ((interpolated - 128) * 256) :
         ((interpolated - 128) * -256);

      pad->analog[stick][axis] = interpolated;
   }
}

static void hidpad_wiiugca_pad_packet_handler(gca_pad_data_t *pad, uint8_t *packet, size_t size)
{
   if (size > 9)
      return;

   memcpy(pad->data, packet, size);
   update_button_state(pad);
   update_analog_state(pad);
}

static void hidpad_wiiugca_packet_handler(void *device_data, uint8_t *packet, uint16_t size)
{
   uint32_t i;
   int port;
   unsigned char port_connected;

   gca_device_data_t *device = (gca_device_data_t *)device_data;

   if (!device)
      return;

/* Mac OSX reads a 39-byte packet which has both a leading and trailing byte from
 * the actual packet data.
 */
#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
   packet++;
   size = 37;
#endif

   memcpy(device->data, packet, size);

   for (i = 1; i < 37; i += 9)
   {
      port           = i / 9;
      port_connected = device->data[i];

      if (port_connected > GCA_PORT_POWERED)
      {
         device->connected[port] = port_connected;
         hidpad_wiiugca_pad_packet_handler(&device->pad_data[port], &device->data[i], 9);
      }
      else
         device->connected[port] = 0;
   }
}

static void hidpad_wiiugca_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
  (void)data;
  (void)effect;
  (void)strength;
}

const char *hidpad_wiiugca_get_name(void *pad_data)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;
   if(!pad || pad->datatype != GCA_TYPE_PAD)
      return DEVICE_NAME;

   switch(pad->device_data->connected[pad->pad_index])
   {
      case 0:
         return DEVICE_NAME;
      case GCA_WAVEBIRD_CONNECTED:
         return WAVEBIRD_PAD;
      default:
         break;
   }

   /* For now we return a single static name */
   return GAMECUBE_PAD;
}

static int32_t hidpad_wiiugca_button(void *pad_data, uint16_t joykey)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;

   if (!pad)
      return 0;

   if (pad->datatype != GCA_TYPE_PAD)
   {
      gca_device_data_t *device = (gca_device_data_t *)pad_data;
      pad = &device->pad_data[0];
   }

   if (!pad->device_data || joykey > 31 || !pad->joypad)
      return 0;

   return pad->buttons & (1 << joykey);
}

static void *hidpad_wiiugca_pad_init(void *device_data, int pad_index, joypad_connection_t *joypad)
{
   gca_device_data_t *device = (gca_device_data_t *)device_data;

   if(!device || pad_index < 0 || pad_index >= GCA_MAX_PAD || !joypad || device->pad_data[pad_index].joypad || !device->connected[pad_index])
      return NULL;

   device->pad_data[pad_index].joypad = joypad;
   return &device->pad_data[pad_index];
}

static void hidpad_wiiugca_pad_deinit(void *pad_data)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;

   if(!pad)
      return;

   pad->joypad = NULL;
}

static int8_t hidpad_wiiugca_status(void *device_data, int pad_index)
{
   gca_device_data_t *device = (gca_device_data_t *)device_data;
   int8_t result = 0;

   if(!device || pad_index < 0 || pad_index >= GCA_MAX_PAD)
      return 0;

   if (device->connected[pad_index])
      result |= PAD_CONNECT_READY;

   if (device->pad_data[pad_index].joypad)
      result |= PAD_CONNECT_BOUND;

   return result;
}

static joypad_connection_t *hidpad_wiiugca_joypad(void *device_data, int pad_index)
{
   gca_device_data_t *device = (gca_device_data_t *)device_data;

   if(!device || pad_index < 0 || pad_index >= GCA_MAX_PAD)
      return 0;

   return device->pad_data[pad_index].joypad;
}

pad_connection_interface_t pad_connection_wiiugca = {
   hidpad_wiiugca_init,
   hidpad_wiiugca_deinit,
   hidpad_wiiugca_packet_handler,
   hidpad_wiiugca_set_rumble,
   hidpad_wiiugca_get_buttons,
   hidpad_wiiugca_get_axis,
   hidpad_wiiugca_get_name,
   hidpad_wiiugca_button,
   true,
   GCA_MAX_PAD,
   hidpad_wiiugca_pad_init,
   hidpad_wiiugca_pad_deinit,
   hidpad_wiiugca_status,
   hidpad_wiiugca_joypad,
};
