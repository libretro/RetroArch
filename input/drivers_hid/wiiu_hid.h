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

#ifndef __WIIU_HID__H
#define __WIIU_HID__H
#include "../connect/joypad_connection.h"
#include "../input_defines.h"
#include "../input_driver.h"
#include "../../verbosity.h"

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

#define MAX_HID_PADS 5

typedef struct wiiu_hid
{
  HIDClient *client;
  joypad_connection_t *connections;
  OSThread *polling_thread;
  void *polling_thread_stack;
  volatile bool polling_thread_quit;
} wiiu_hid_t;

struct wiiu_adapter {
  wiiu_hid_t *hid;
  int32_t slot;
  uint32_t handle;
};

typedef struct wiiu_attach wiiu_attach_event;

struct wiiu_attach {
  wiiu_attach_event *next;
  uint32_t type;
  uint32_t handle;
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t interface_index;
  uint8_t is_keyboard;
  uint8_t is_mouse;
  uint16_t max_packet_size_rx;
  uint16_t max_packet_size_tx;
};

typedef struct _wiiu_event_list wiiu_event_list;

struct _wiiu_event_list {
  OSFastMutex lock;
  wiiu_attach_event *list;
};

static void *alloc_zeroed(size_t alignment, size_t size);
static OSThread *new_thread(void);
static wiiu_hid_t *new_hid(void);
static void delete_hid(wiiu_hid_t *hid);
static void delete_hidclient(HIDClient *client);
static HIDClient *new_hidclient(void);
static struct wiiu_adapter *new_adapter(void);
static void delete_adapter(struct wiiu_adapter *adapter);
static wiiu_attach_event *new_attach_event(HIDDevice *device);
static void delete_attach_event(wiiu_attach_event *);

static void wiiu_hid_init_event_list(void);
static void start_polling_thread(wiiu_hid_t *hid);
static void stop_polling_thread(wiiu_hid_t *hid);
static int wiiu_hid_polling_thread(int argc, const char **argv);
static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);
static wiiu_attach_event *synchronized_get_events_list(void);
static void wiiu_handle_attach_events(wiiu_hid_t *hid, wiiu_attach_event *list);
static void wiiu_hid_attach(wiiu_hid_t *hid, wiiu_attach_event *event);
static void wiiu_hid_detach(wiiu_hid_t *hid, wiiu_attach_event *event);
static void synchronized_add_to_adapters_list(struct wiiu_adapter *adapter);
static void synchronized_add_event(wiiu_attach_event *event);

#endif // __WIIU_HID__H
