/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include "../../driver.h"

enum connect_ps4_dpad_states
{
   DPAD_UP         = 0x0,
   DPAD_UP_RIGHT   = 0x1,
   DPAD_RIGHT      = 0x2,
   DPAD_RIGHT_DOWN = 0x3,
   DPAD_DOWN       = 0x4,
   DPAD_DOWN_LEFT  = 0x5,
   DPAD_LEFT       = 0x6,
   DPAD_LEFT_UP    = 0x7,
   DPAD_OFF        = 0x8
};

struct ps4buttons
{
#ifdef MSB_FIRST
   uint8_t triangle      : 1;
   uint8_t circle        : 1;
   uint8_t cross         : 1;
   uint8_t square        : 1;
   uint8_t dpad          : 4;

   uint8_t r3            : 1;
   uint8_t l3            : 1;
   uint8_t options       : 1;
   uint8_t share         : 1;
   uint8_t r2            : 1;
   uint8_t l2            : 1;
   uint8_t r1            : 1;
   uint8_t l1            : 1;

   uint8_t reportcounter : 6;
   uint8_t touchpad      : 1;
   uint8_t ps            : 1;
#else
   uint8_t dpad          : 4;
   uint8_t square        : 1;
   uint8_t cross         : 1;
   uint8_t circle        : 1;
   uint8_t triangle      : 1;

   uint8_t l1            : 1;
   uint8_t r1            : 1;
   uint8_t l2            : 1;
   uint8_t r2            : 1;
   uint8_t share         : 1;
   uint8_t options       : 1;
   uint8_t l3            : 1;
   uint8_t r3            : 1;

   uint8_t ps            : 1;
   uint8_t touchpad      : 1;
   uint8_t reportcounter : 6;
#endif
}__attribute__((packed));

struct ps4
{
   uint8_t hatvalue[4];
   struct ps4buttons btn;
   uint8_t trigger[2];
};

struct hidpad_ps4_data
{
   struct pad_connection* connection;
   send_control_t send_control;
   struct ps4 data;
   uint32_t slot;
   bool have_led;
   uint16_t motors[2];
};

static void hidpad_ps4_send_control(struct hidpad_ps4_data* device)
{
   /* TODO: Can this be modified to turn off motion tracking? */
   static uint8_t report_buffer[79] = {
      0x52, 0x11, 0xB0, 0x00, 0x0F
   };

#if 0
   uint8_t rgb[4][3] = { { 0xFF, 0, 0 }, { 0, 0xFF, 0 }, { 0, 0, 0xFF }, { 0xFF, 0xFF, 0xFF } };
   report_buffer[ 9] = rgb[(device->slot % 4)][0];
   report_buffer[10] = rgb[(device->slot % 4)][1];
   report_buffer[11] = rgb[(device->slot % 4)][2];
#endif

   device->send_control(device->connection,
         report_buffer, sizeof(report_buffer));
}

static void* hidpad_ps4_init(void *data, uint32_t slot, send_control_t ptr)
{
#if 0
   uint8_t magic_data[0x25];
#endif
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_ps4_data* device    = (struct hidpad_ps4_data*)
      calloc(1, sizeof(struct hidpad_ps4_data));

   if (!device)
      goto error;

   if (!connection)
      goto error;

   device->connection   = connection;
   device->slot         = slot;
   device->send_control = ptr;

#if 0
   /* TODO - unsure of this */
   /* This is needed to get full input packet over bluetooth. */
   device->send_control(device->connection, magic_data, 0x2);
   (void)magic_data;
#endif

   /* Without this, the digital buttons won't be reported. */
   hidpad_ps4_send_control(device);

   return device;

error:
   if (device)
      free(device);
   return NULL;
}

static void hidpad_ps4_deinit(void *data)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;

   if (device)
      free(device);
}

static bool hidpad_ps4_check_dpad(struct ps4 *rpt, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_JOYPAD_UP:
         return (rpt->btn.dpad == DPAD_LEFT_UP)    || (rpt->btn.dpad == DPAD_UP)    || (rpt->btn.dpad == DPAD_UP_RIGHT);
      case RETRO_DEVICE_ID_JOYPAD_RIGHT:
         return (rpt->btn.dpad == DPAD_UP_RIGHT)   || (rpt->btn.dpad == DPAD_RIGHT) || (rpt->btn.dpad == DPAD_RIGHT_DOWN);
      case RETRO_DEVICE_ID_JOYPAD_DOWN:
         return (rpt->btn.dpad == DPAD_RIGHT_DOWN) || (rpt->btn.dpad == DPAD_DOWN)  || (rpt->btn.dpad == DPAD_DOWN_LEFT);
      case RETRO_DEVICE_ID_JOYPAD_LEFT:
         return (rpt->btn.dpad == DPAD_DOWN_LEFT)  || (rpt->btn.dpad == DPAD_LEFT)  || (rpt->btn.dpad == DPAD_LEFT_UP);
   }

   return false;
}

static uint64_t hidpad_ps4_get_buttons(void *data)
{
   uint64_t buttonstate           = 0;
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   struct ps4 *rpt = device ? (struct ps4*)&device->data : NULL;

   if (!device || !rpt)
      return 0;

   buttonstate |= (rpt->btn.r3 ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3)     : 0);
   buttonstate |= (rpt->btn.l3 ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3)     : 0);
   buttonstate |= (rpt->btn.options ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START)  : 0);
   buttonstate |= (rpt->btn.share ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0);
   buttonstate |= (rpt->btn.r2 ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2)     : 0);
   buttonstate |= (rpt->btn.l2 ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2)     : 0);
   buttonstate |= (rpt->btn.r1 ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R)      : 0);
   buttonstate |= (rpt->btn.l1 ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L)      : 0);

   buttonstate |= (rpt->btn.triangle ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X)      : 0);
   buttonstate |= (rpt->btn.circle ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A)      : 0);
   buttonstate |= (rpt->btn.cross  ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B)      : 0);
   buttonstate |= (rpt->btn.square  ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y)      : 0);
   buttonstate |= ((hidpad_ps4_check_dpad(rpt, RETRO_DEVICE_ID_JOYPAD_LEFT))   ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT)   : 0);
   buttonstate |= ((hidpad_ps4_check_dpad(rpt, RETRO_DEVICE_ID_JOYPAD_DOWN))   ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN)   : 0);
   buttonstate |= ((hidpad_ps4_check_dpad(rpt, RETRO_DEVICE_ID_JOYPAD_RIGHT))   ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT)  : 0);
   buttonstate |= ((hidpad_ps4_check_dpad(rpt, RETRO_DEVICE_ID_JOYPAD_UP))   ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP)     : 0);
   buttonstate |= (rpt->btn.ps ? (UINT64_C(1) << RARCH_MENU_TOGGLE)             : 0);

   return buttonstate;
}

static int16_t hidpad_ps4_get_axis(void *data, unsigned axis)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   struct ps4 *rpt = device ? (struct ps4*)&device->data : NULL;

   if (device && (axis < 4))
   {
      int val = rpt ? rpt->hatvalue[axis] : 0;
      val = (val << 8) - 0x8000;
      return (abs(val) > 0x1000) ? val : 0;
   }

   return 0;
}

static void hidpad_ps4_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;

   if (!device)
      return;

#if 0
   if (!device->have_led)
   {
      hidpad_ps4_send_control(device);
      device->have_led = true;
   }
#endif

   memcpy(&device->data, &packet[2], sizeof(struct ps4));
}

static void hidpad_ps4_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   /* TODO */
#if 0
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (device && (device->motors[idx] != strength))
   {
      device->motors[idx] = strength;
      hidpad_ps4_send_control(device);
   }
#endif
}

pad_connection_interface_t pad_connection_ps4 = {
   hidpad_ps4_init,
   hidpad_ps4_deinit,
   hidpad_ps4_packet_handler,
   hidpad_ps4_set_rumble,
   hidpad_ps4_get_buttons,
   hidpad_ps4_get_axis,
   NULL,
};
