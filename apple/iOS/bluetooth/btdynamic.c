/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <CoreFoundation/CFRunLoop.h>

#include "apple/common/rarch_wrapper.h"

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
   GRAB(bt_close),
   GRAB(bt_flip_addr),
   GRAB(bd_addr_to_str),
   GRAB(bt_register_packet_handler),
   GRAB(bt_send_cmd),
   GRAB(bt_send_l2cap),
   GRAB(run_loop_init),
   GRAB(run_loop_execute),

   GRAB(btstack_set_power_mode),
   GRAB(hci_delete_stored_link_key),
   GRAB(hci_disconnect),
   GRAB(hci_read_bd_addr),
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

static bool btstack_tested;
static bool btstack_loaded;

static pthread_t btstack_thread;
static CFRunLoopSourceRef btstack_quit_source;

bool btstack_try_load()
{
   assert(sizeof(void**) == sizeof(void(*)()));

   if (btstack_tested)
      return btstack_loaded;

   RARCH_LOG("BTstack: Attempting to load\n");
   
   btstack_tested = true;
   btstack_loaded = false;

   void* btstack = dlopen("/usr/lib/libBTstack.dylib", RTLD_LAZY);

   if (!btstack)
   {
      RARCH_LOG("BTstack: /usr/lib/libBTstack.dylib not loadable\n");
      RARCH_LOG("BTstack: Not loaded\n");
      return false;
   }

   for (int i = 0; grabbers[i].name; i ++)
   {
      *grabbers[i].target = dlsym(btstack, grabbers[i].name);

      if (!*grabbers[i].target)
      {
         RARCH_LOG("BTstack: Symbol %s not found in /usr/lib/libBTstack.dylib\n", grabbers[i].name);
         RARCH_LOG("BTstack: Not loaded\n");
      
         dlclose(btstack);
         return false;
      }
   }

   run_loop_init_ptr(RUN_LOOP_COCOA);
   bt_register_packet_handler_ptr(btpad_packet_handler);

   RARCH_LOG("BTstack: Loaded\n");
   btstack_loaded = true;

   return true;
}

void btstack_thread_stop()
{
   bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_OFF);
}

static void* btstack_thread_func(void* data)
{
   RARCH_LOG("BTstack: Thread started");

   if (bt_open_ptr())
   {
      RARCH_LOG("BTstack: bt_open() failed\n");
      return 0;
   }

   CFRunLoopSourceContext ctx = { 0, 0, 0, 0, 0, 0, 0, 0, 0, btstack_thread_stop };
   btstack_quit_source = CFRunLoopSourceCreate(0, 0, &ctx);
   CFRunLoopAddSource(CFRunLoopGetCurrent(), btstack_quit_source, kCFRunLoopCommonModes);

   RARCH_LOG("BTstack: Turning on\n");
   bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_ON);

   RARCH_LOG("BTstack: Running\n");
   CFRunLoopRun();
   
   RARCH_LOG("BTstack: Done\n");

   CFRunLoopSourceInvalidate(btstack_quit_source);
   CFRelease(btstack_quit_source);
   return 0;
}

void btstack_set_poweron(bool on)
{
   if (!btstack_try_load())
      return;

   if (on && !btstack_thread)
      pthread_create(&btstack_thread, 0, btstack_thread_func, 0);
   else if (!on && btstack_thread && btstack_quit_source)
   {
      CFRunLoopSourceSignal(btstack_quit_source);
      pthread_join(btstack_thread, 0);
      btstack_thread = 0;
   }
}

bool btstack_is_running()
{
   return btstack_thread;
}
