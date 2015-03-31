/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2014-2015 - Daniel De Matteis
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

struct hidpad_ps4_data
{
   struct pad_connection* connection;
   send_control_t send_control;
   uint8_t data[512];
   uint64_t buttonstate;
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
    
   device->send_control(device->connection, report_buffer, sizeof(report_buffer));
}

static void* hidpad_ps4_init(void *data, uint32_t slot, send_control_t ptr)
{
   uint8_t magic_data[0x25];
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_ps4_data* device = (struct hidpad_ps4_data*)
    calloc(1, sizeof(struct hidpad_ps4_data));

   if (!device)
      return NULL;

   if (!connection)
   {
      free(device);
      return NULL;
   }

   device->connection   = connection;
   device->slot         = slot;
   device->send_control = ptr;
   
#if 0
   /* TODO - unsure of this */
   /* This is needed to get full input packet over bluetooth. */
   device->send_control(device->connection, magic_data, 0x2);
#endif

   /* Without this, the digital buttons won't be reported. */
   hidpad_ps4_send_control(device);
    
   (void)magic_data;

   return device;
}

static void hidpad_ps4_deinit(void *data)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
    
   if (device)
      free(device);
}

static uint64_t hidpad_ps4_get_buttons(void *data)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
    
    if (!device)
        return 0;
   return device->buttonstate;
}

static int16_t hidpad_ps4_get_axis(void *data, unsigned axis)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
    
   if (device && (axis < 4))
   {
      int val = device->data[7 + axis];
      val = (val << 8) - 0x8000;
      return (abs(val) > 0x1000) ? val : 0;
   }

   return 0;
}

static void hidpad_ps4_packet_handler(void *data, uint8_t *packet, uint16_t size)
{
   uint32_t buttons, buttons2, buttons3;
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
    uint8_t *rpt_ptr   = (uint8_t*)&packet[4];
    device->buttonstate = 0;
    
    buttons  = rpt_ptr[2];
    buttons2 = rpt_ptr[3];
    buttons3 = rpt_ptr[4];
    
    //RARCH_LOG("L2 button: %d\n", rpt_ptr[5]);
    //RARCH_LOG("R2 button: %d\n", rpt_ptr[6]);
    //RARCH_LOG("Test: %d\n", rpt_ptr[4] & 0x01);
    //RARCH_LOG("Left  stick X: %d\n", rpt_ptr[-2]);
    //RARCH_LOG("Left  stick Y: %d\n", rpt_ptr[-1]);
    //RARCH_LOG("Right stick X: %d\n", rpt_ptr[0]);
    //RARCH_LOG("Right stick Y: %d\n", rpt_ptr[1]);
    //RARCH_LOG("Digital buttons: %d\n", rpt_ptr[2]);
    //RARCH_LOG("Start/share: %d\n", rpt_ptr[3]);
    device->buttonstate |= ((buttons2 == 128)? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)     : 0);
    device->buttonstate |= ((buttons2 == 64) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)     : 0);
    device->buttonstate |= ((buttons2 == 32) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START)  : 0);
    device->buttonstate |= ((buttons2 == 16) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0);
    device->buttonstate |= ((buttons2 ==  8) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2)     : 0);
    device->buttonstate |= ((buttons2 ==  4) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2)     : 0);
    device->buttonstate |= ((buttons2 ==  2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R)      : 0);
    device->buttonstate |= ((buttons2 ==  1) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L)      : 0);

    device->buttonstate |= ((buttons == 136) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X)      : 0);
    device->buttonstate |= ((buttons == 72)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A)      : 0);
    device->buttonstate |= ((buttons == 40)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B)      : 0);
    device->buttonstate |= ((buttons == 24)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y)      : 0);
    device->buttonstate |= ((buttons == 6)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)   : 0);
    device->buttonstate |= ((buttons == 4)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)   : 0);
    device->buttonstate |= ((buttons == 2)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)  : 0);
    device->buttonstate |= ((buttons == 0)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)     : 0);
    device->buttonstate |= ((buttons3 & 0x01)? (1ULL << RARCH_MENU_TOGGLE)             : 0);
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
};
