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
#include <pthread.h>
#include <CoreFoundation/CFRunLoop.h>

#include "../../rarch_wrapper.h"

#define BUILDING_BTDYNAMIC
#include "btdynamic.h"

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
   GRAB(run_loop_execute),
   GRAB(btstack_get_system_bluetooth_enabled),
   GRAB(btstack_set_power_mode),
   GRAB(btstack_set_system_bluetooth_enabled),
   GRAB(hci_delete_stored_link_key),
   GRAB(hci_disconnect),
   GRAB(hci_inquiry),
   GRAB(hci_inquiry_cancel),
   GRAB(hci_pin_code_request_reply),
   GRAB(hci_pin_code_request_negative_reply),
   GRAB(hci_remote_name_request),
   GRAB(hci_remote_name_request_cancel),
   GRAB(hci_write_authentication_enable),
   GRAB(hci_write_inquiry_mode),
   GRAB(l2cap_create_channel),
   GRAB(l2cap_register_service),
   GRAB(l2cap_accept_connection),
   GRAB(l2cap_decline_connection),
   {0, 0}
};

extern void btpad_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static pthread_t btstack_thread;
static bool btstack_tested;
static bool btstack_loaded;

// TODO: This may need to be synchronized
static volatile bool btstack_poweron;

static void* btstack_thread_function(void* data)
{
   ios_add_log_message("BTstack: Thread Initializing");

   run_loop_init_ptr(RUN_LOOP_COCOA);
   bt_register_packet_handler_ptr(btpad_packet_handler);

   if (bt_open_ptr())
   {
      ios_add_log_message("BTstack: Failed to open, exiting thread.");
      return 0;
   }

   while (1)
   {
      static bool poweron = false;
      
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
      
      if (poweron != btstack_poweron)
      {
         poweron = btstack_poweron;
         bt_send_cmd_ptr(btstack_set_power_mode_ptr, poweron ? HCI_POWER_ON : HCI_POWER_OFF);
      
         ios_add_log_message("BTstack: Responding to power switch (now %s)", poweron ? "ON" : "OFF");
      }
   }

   return 0;
}


bool btstack_load()
{
   assert(sizeof(void**) == sizeof(void(*)()));

   if (btstack_tested)
      return btstack_loaded;

   ios_add_log_message("BTstack: Attempting to load");
   
   btstack_tested = true;
   btstack_loaded = false;

   void* btstack = dlopen("/usr/lib/libBTstack.dylib", RTLD_LAZY);

   if (!btstack)
   {
      ios_add_log_message("BTstack: /usr/lib/libBTstack.dylib not loadable");
      ios_add_log_message("BTstack: Not loaded");
      return false;
   }

   for (int i = 0; grabbers[i].name; i ++)
   {
      *grabbers[i].target = dlsym(btstack, grabbers[i].name);

      if (!*grabbers[i].target)
      {
         ios_add_log_message("BTstack: Symbol %s not found in /usr/lib/libBTstack.dylib", grabbers[i].name);
         ios_add_log_message("BTstack: Not loaded");
      
         dlclose(btstack);
         return false;
      }
   }

   ios_add_log_message("BTstack: Loaded");
   btstack_loaded = true;

   return true;
}

void btstack_start()
{
   if (!btstack_load())
      return;

   static bool thread_started = false;
   if (!thread_started)
   {
      ios_add_log_message("BTstack: Starting thread");
      pthread_create(&btstack_thread, NULL, btstack_thread_function, 0);
      thread_started = true;
   }
   
   if (!btstack_poweron)
   {
      ios_add_log_message("BTstack: Setting poweron flag");
      btstack_poweron = true;
   }
}

void btstack_stop()
{
   if (!btstack_load())
      return;

   if (btstack_poweron)
   {
      ios_add_log_message("BTstack: Clearing poweron flag");
      btstack_poweron = false;
   }
}

bool btstack_is_loaded()
{
   return btstack_load();
}

bool btstack_is_running()
{
   return btstack_poweron;
}

