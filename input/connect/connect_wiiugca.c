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

/* the sticks' and triggers' reach, as SDL starts it: a stick's from
 * 128 - 88 to 128 + 88, a trigger's from 40, both widened as they go
 * further */
#define GCA_STICK_LOW   (128 - 88)
#define GCA_STICK_HIGH  (128 + 88)
#define GCA_TRIGGER_LOW 40

struct hidpad_wiiugca_pad_data
{
   uint8_t datatype;
   gca_device_data_t *device_data;
   joypad_connection_t *joypad;
   int pad_index;
   /* the sticks as 0 to 3, L and R's travel as 4 and 5 */
   input_pad_view_t view;
   uint8_t low[6];
   uint8_t high[6];
   bool rumble_strong;  /* what the core asked last */
   bool rumble_weak;
};

struct hidpad_wiiugca_data
{
   uint8_t datatype;
   void *handle;
   hid_driver_t *driver;
   char connected[GCA_MAX_PAD];
   gca_pad_data_t pad_data[GCA_MAX_PAD];
   uint8_t motors[GCA_MAX_PAD]; /* as last sent */
   bool motors_due;
};

/* The adapter's report: an ID, then nine bytes for each of the four
 * ports - its state, two bytes of buttons, the sticks' four, the
 * triggers' two. */
#define GCA_REPORT_LEN (1 + 9 * GCA_MAX_PAD)
#define GCA_PORT_RUMBLE_POWER  0x04 /* the second plug is in */

const char *GAMECUBE_PAD = "GameCube controller";
const char *WAVEBIRD_PAD = "WaveBird controller";
const char *DEVICE_NAME  = "Wii U GC Controller Adapter";

static void* hidpad_wiiugca_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   /* Report 0x13, which is its ID alone, starts the reports. The
    * drivers take a report with its ID first; iohidmanager takes the
    * first byte off and sends report 0x01. */
#if defined(__APPLE__) && !defined(WIIU)
   static uint8_t magic_data[] = {0x01, 0x13};
#else
   static uint8_t magic_data[] = {0x13};
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

   /* what SDL sends first, which Nyko's and EVORETRO's adapters need */
   if (device->driver->set_protocol)
      device->driver->set_protocol(device->handle, 1);
   device->driver->send_control(device->handle, magic_data, sizeof(magic_data));

   return device;
}

static void hidpad_wiiugca_deinit(void *device_data)
{
   gca_device_data_t *device = (gca_device_data_t *)device_data;

   if (device)
      free(device);
}

/* the device stands for its first port */
static gca_pad_data_t *hidpad_wiiugca_pad(void *pad_data)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;
   if (pad && pad->datatype != GCA_TYPE_PAD)
      return &((gca_device_data_t *)pad_data)->pad_data[0];
   return pad;
}

static void hidpad_wiiugca_get_buttons(void *pad_data, input_bits_t *state)
{
   gca_pad_data_t *pad = hidpad_wiiugca_pad(pad_data);
   if (pad)
      *state = pad->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_wiiugca_get_axis(void *pad_data, unsigned axis)
{
   axis_data axis_data;
   gca_pad_data_t *pad = hidpad_wiiugca_pad(pad_data);

   if (!pad)
      return 0;
   if (axis < INPUT_PAD_VIEW_AXES)
      return pad->view.axes[axis];
   /* a stick's direction, bound: what wiiu_hid's state() asks */
   gamepad_read_axis_data(axis, &axis_data);
   if (axis_data.axis >= 4)
      return 0;
   return pad->view.axes[axis_data.axis];
}

static void update_button_state(gca_pad_data_t *pad, const uint8_t *data)
{
   uint32_t i, pressed_keys, buttons = 0;

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

   pressed_keys     = data[1] | (data[2] << 8);

   for (i = 0; i < 12; i++)
      buttons |= (pressed_keys & (1 << i)) ?
         (1 << button_mapping[i]) : 0;
   pad_view_set_buttons(&pad->view, buttons, false);
}

static void hidpad_wiiugca_reset_reach(gca_pad_data_t *pad)
{
   memset(pad->low,  GCA_STICK_LOW,  sizeof(pad->low));
   memset(pad->high, GCA_STICK_HIGH, sizeof(pad->high));
   pad->low[4] = GCA_TRIGGER_LOW;
   pad->low[5] = GCA_TRIGGER_LOW;
}

/* @v within the reach seen so far, widened to take it in */
static int32_t hidpad_wiiugca_reach(gca_pad_data_t *pad, unsigned i,
      uint8_t v)
{
   if (v < pad->low[i])
      pad->low[i]  = v;
   if (v > pad->high[i])
      pad->high[i] = v;
   return (int32_t)(v - pad->low[i]) * 65535 / (pad->high[i] - pad->low[i]);
}

static void update_analog_state(gca_pad_data_t *pad, const uint8_t *data)
{
   unsigned i;

   /* the sticks, the reach across the RetroPad's range; libretro's up is
    * negative, the pad's high */
   for (i = 0; i < 4; i++)
   {
      int32_t v = hidpad_wiiugca_reach(pad, i, data[3 + i]) - 32768;
      if (i & 1)
         v = v <= -32768 ? 32767 : -v;
      pad->view.axes[i] = (int16_t)v;
   }
   /* the triggers' travel, 0 to 32767 */
   for (i = 4; i < 6; i++)
      pad->view.axes[i] = (int16_t)(hidpad_wiiugca_reach(pad, i,
               data[3 + i]) >> 1);
}

/* Report 0x11: a byte for each port's motor. A port is only rumbled
 * when its pad has the power for it - the second plug in - and is not
 * a WaveBird, as SDL does. Sent from the packet handler, when changed.
 * Not on macOS, where iohidmanager sends every report as 0x01. */
static void hidpad_wiiugca_send_rumble(gca_device_data_t *device)
{
#if !defined(__APPLE__) || defined(WIIU)
   uint8_t r[1 + GCA_MAX_PAD];
   unsigned i;
   bool changed = false;

   device->motors_due = false;
   r[0] = 0x11;
   for (i = 0; i < GCA_MAX_PAD; i++)
   {
      const gca_pad_data_t *pad = &device->pad_data[i];
      r[1 + i] = (uint8_t)(
               (pad->rumble_strong || pad->rumble_weak)
            && (device->connected[i] & GCA_PORT_RUMBLE_POWER)
            &&  device->connected[i] != GCA_WAVEBIRD_CONNECTED);
      if (r[1 + i] != device->motors[i])
         changed = true;
   }
   if (!changed)
      return;
   memcpy(device->motors, r + 1, GCA_MAX_PAD);
   if (device->driver && device->driver->send_control)
      device->driver->send_control(device->handle, r, sizeof(r));
#else
   device->motors_due = false;
#endif
}

/* A port's nine bytes, read where they lie in the report. */
static void hidpad_wiiugca_pad_packet_handler(gca_pad_data_t *pad,
      const uint8_t *packet)
{
   update_button_state(pad, packet);
   update_analog_state(pad, packet);
}

static void hidpad_wiiugca_packet_handler(void *device_data, uint8_t *packet, uint16_t len)
{
   uint32_t i;
   gca_device_data_t *device = (gca_device_data_t *)device_data;

   /* the whole report or nothing: it is read where it lies, and the
    * copy it replaced took any length into 64 bytes */
   if (!device || len < GCA_REPORT_LEN)
      return;

   for (i = 1; i < GCA_REPORT_LEN; i += 9)
   {
      int port                     = i / 9;
      unsigned char port_connected = packet[i];

      if (port_connected > GCA_PORT_POWERED)
      {
         /* a pad just put in: its sticks' reach is its own */
         if (!device->connected[port])
            hidpad_wiiugca_reset_reach(&device->pad_data[port]);
         device->connected[port] = port_connected;
         hidpad_wiiugca_pad_packet_handler(&device->pad_data[port], &packet[i]);
      }
      else
         device->connected[port] = 0;
   }

   if (device->motors_due)
      hidpad_wiiugca_send_rumble(device);
}

static void hidpad_wiiugca_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)data;
   bool on;

   /* a port's motor is on or off: on while either strength is asked */
   if (!pad || pad->datatype != GCA_TYPE_PAD)
      return;
   on = strength != 0;
   if (effect == RETRO_RUMBLE_STRONG)
      pad->rumble_strong = on;
   else
      pad->rumble_weak   = on;
   pad->device_data->motors_due = true;
}

const char *hidpad_wiiugca_get_name(void *pad_data)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;
   if (!pad || pad->datatype != GCA_TYPE_PAD)
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
   gca_pad_data_t *pad = hidpad_wiiugca_pad(pad_data);

   if (!pad || joykey > 31)
      return 0;
   return BIT256_GET(pad->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_wiiugca_view(void *pad_data)
{
   return &hidpad_wiiugca_pad(pad_data)->view;
}

static void *hidpad_wiiugca_pad_init(void *device_data, int pad_index, joypad_connection_t *joypad)
{
   gca_device_data_t *device = (gca_device_data_t *)device_data;

   if (!device || pad_index < 0 || pad_index >= GCA_MAX_PAD || !joypad || device->pad_data[pad_index].joypad || !device->connected[pad_index])
      return NULL;

   device->pad_data[pad_index].joypad = joypad;
   return &device->pad_data[pad_index];
}

static void hidpad_wiiugca_pad_deinit(void *pad_data)
{
   gca_pad_data_t *pad = (gca_pad_data_t *)pad_data;

   if (pad)
      pad->joypad      = NULL;
}

static int8_t hidpad_wiiugca_status(void *device_data, int pad_index)
{
   int8_t result = 0;
   gca_device_data_t *device = (gca_device_data_t *)device_data;

   if (!device || pad_index < 0 || pad_index >= GCA_MAX_PAD)
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
   if (!device || pad_index < 0 || pad_index >= GCA_MAX_PAD)
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
   NULL, /* set_player */
   hidpad_wiiugca_view
};
