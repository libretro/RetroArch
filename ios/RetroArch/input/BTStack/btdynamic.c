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

#define BUILDING_BTDYNAMIC
#include "btdynamic.h"

static bool bt_tested;
static bool bt_is_loaded;

#define GRAB(A) {#A, (void**)&A##_ptr}
static struct
{
   const char* name;
   void** target;
}  grabbers[] =
{
   GRAB(bt_open),
   GRAB(bt_flip_addr),
   GRAB(bt_register_packet_handler),
   GRAB(bt_send_cmd),
   GRAB(bt_send_l2cap),
   GRAB(run_loop_init),
   GRAB(btstack_get_system_bluetooth_enabled),
   GRAB(btstack_set_power_mode),
   GRAB(btstack_set_system_bluetooth_enabled),
   GRAB(hci_delete_stored_link_key),
   GRAB(hci_inquiry),
   GRAB(hci_inquiry_cancel),
   GRAB(hci_pin_code_request_reply),
   GRAB(hci_remote_name_request),
   GRAB(hci_remote_name_request_cancel),
   GRAB(hci_write_authentication_enable),
   GRAB(hci_write_inquiry_mode),
   GRAB(l2cap_create_channel),
   GRAB(l2cap_register_service),
   GRAB(l2cap_accept_connection),
   {0, 0}
};

bool load_btstack()
{
   assert(sizeof(void**) == sizeof(void(*)));

   if (bt_tested)
      return bt_is_loaded;

   bt_tested = true;
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
