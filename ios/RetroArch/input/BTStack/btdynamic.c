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
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include "btdynamic.h"

static bool bt_is_loaded;

static struct
{
   const char* name;
   void** target;
}  grabbers[] =
{
   {"bt_open", (void**)&bt_open_ptr},
   {"bt_flip_addr", (void**)&bt_flip_addr_ptr},
   {"bt_register_packet_handler", (void**)&bt_register_packet_handler_ptr},
   {"bt_send_cmd", (void**)&bt_send_cmd_ptr},
   {"bt_send_l2cap", (void**)&bt_send_l2cap_ptr},
   {"run_loop_init", (void**)&run_loop_init_ptr},
   {"btstack_get_system_bluetooth_enabled", (void**)&btstack_get_system_bluetooth_enabled_ptr},
   {"btstack_set_power_mode", (void**)&btstack_set_power_mode_ptr},
   {"btstack_set_system_bluetooth_enabled", (void**)&btstack_set_system_bluetooth_enabled_ptr},
   {"hci_delete_stored_link_key", (void**)&hci_delete_stored_link_key_ptr},
   {"hci_inquiry", (void**)&hci_inquiry_ptr},
   {"hci_inquiry_cancel", (void**)&hci_inquiry_cancel_ptr},
   {"hci_pin_code_request_reply", (void**)&hci_pin_code_request_reply_ptr},
   {"hci_remote_name_request", (void**)&hci_remote_name_request_ptr},
   {"hci_remote_name_request_cancel", (void**)&hci_remote_name_request_cancel_ptr},
   {"hci_write_authentication_enable", (void**)&hci_write_authentication_enable_ptr},
   {"hci_write_inquiry_mode", (void**)&hci_write_inquiry_mode_ptr},
   {"l2cap_create_channel", (void**)&l2cap_create_channel_ptr},
   {0, 0}
};

bool load_btstack()
{
   assert(sizeof(void**) == sizeof(void(*)));

   if (bt_is_loaded)
      return true;

   void* btstack = dlopen("/usr/lib/libBTstack.dylib", RTLD_LAZY);

   if (!btstack)
      return false;

   for (int i = 0; grabbers[i].name; i ++)
   {
      *grabbers[i].target = dlsym(btstack, grabbers[i].name);

      if (!*grabbers[i].target)
      {
         dlclose(btstack);
         return false;
      }
   }

   return true;
}
