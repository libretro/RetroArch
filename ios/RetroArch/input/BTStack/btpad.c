/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>

#include "../../rarch_wrapper.h"
#include "btdynamic.h"
#include "btpad.h"
#include "wiimote.h"

static struct btpad_interface* btpad_iface;
static void* btpad_device;
static bool btpad_want_wiimote;
static pthread_mutex_t btpad_lock = PTHREAD_MUTEX_INITIALIZER;

// MAIN THREAD ONLY
uint32_t btpad_get_buttons()
{
   pthread_mutex_lock(&btpad_lock);
   uint32_t result = (btpad_device && btpad_iface) ? btpad_iface->get_buttons(btpad_device) : 0;
   pthread_mutex_unlock(&btpad_lock);

   return result;
}

void btpad_set_pad_type(bool wiimote)
{
   pthread_mutex_lock(&btpad_lock);
   btpad_want_wiimote = wiimote;
   pthread_mutex_unlock(&btpad_lock);
}

// BT THREAD ONLY
static void btpad_connect_pad(bool wiimote)
{
   pthread_mutex_lock(&btpad_lock);

   ios_add_log_message("BTpad: Connecting to %s", wiimote ? "WiiMote" : "PS3");

   btpad_iface = (wiimote) ? &btpad_wii : &btpad_ps3;
   btpad_device = btpad_iface->connect();
   
   pthread_mutex_unlock(&btpad_lock);
}

static void btpad_disconnect_pad()
{
   pthread_mutex_lock(&btpad_lock);

   if (btpad_iface && btpad_device)
   {
      ios_add_log_message("BTpad: Disconnecting");
   
      btpad_iface->disconnect(btpad_device);
      btpad_device = 0;
      btpad_iface = 0;
   }
   
   pthread_mutex_unlock(&btpad_lock);
}

void btpad_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   if (packet_type == HCI_EVENT_PACKET && packet[0] == BTSTACK_EVENT_STATE)
   {
      if (packet[2] == HCI_STATE_WORKING)
         btpad_connect_pad(btpad_want_wiimote);
      else if(packet[2] > HCI_STATE_WORKING && btpad_iface && btpad_device)
         btpad_disconnect_pad();
   }

   if (btpad_device && btpad_iface)
   {
      pthread_mutex_lock(&btpad_lock);
      btpad_iface->packet_handler(btpad_device, packet_type, channel, packet, size);
      pthread_mutex_unlock(&btpad_lock);
   }
}
