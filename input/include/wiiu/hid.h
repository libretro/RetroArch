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

#include "hid_types.h"
#include "input.h"
#include "../../common/hid/hid_device_driver.h"

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

/* Adapter has been detected and needs to be initialized */
#define ADAPTER_STATE_NEW     0
/* Adapter has been initialized successfully */
#define ADAPTER_STATE_READY   1
/* The read loop has been started */
#define ADAPTER_STATE_READING 2
/* The read loop is shutting down */
#define ADAPTER_STATE_DONE    3
/* The read loop has fully stopped and the adapter can be freed */
#define ADAPTER_STATE_GC      4

struct wiiu_hid {
   /* used to register for HID notifications */
   HIDClient *client;
   /* thread state data for the HID input polling thread */
   OSThread *polling_thread;
   /* stack space for polling thread */
   void *polling_thread_stack;
   /* watch variable for telling the polling thread to terminate */
   volatile bool polling_thread_quit;
};

/**
 * Each HID device attached to the WiiU gets its own adapter, which
 * connects the HID subsystem with the HID device driver.
 */
struct wiiu_adapter {
   wiiu_adapter_t *next;
   hid_device_t *driver;
   void *driver_handle;
   wiiu_hid_t *hid;
   uint8_t state;
   uint8_t *rx_buffer;
   int32_t rx_size;
   uint8_t *tx_buffer;
   int32_t tx_size;
   uint32_t handle;
   uint8_t interface_index;
   bool connected;
};

/**
 * When a HID device is connected, the OS generates an attach
 * event; the attach event handler translate them into these
 * structures.
 */
struct wiiu_attach {
   wiiu_attach_event *next;
   hid_device_t *driver;
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

struct _wiiu_event_list {
   OSFastMutex lock;
   wiiu_attach_event *list;
};

struct _wiiu_adapter_list {
   OSFastMutex lock;
   wiiu_adapter_t *list;
};

extern wiiu_pad_functions_t pad_functions;
extern input_device_driver_t wiiu_joypad;
extern input_device_driver_t wpad_driver;
extern input_device_driver_t kpad_driver;
extern input_device_driver_t hidpad_driver;
extern hid_driver_t wiiu_hid;

static void *alloc_zeroed(size_t alignment, size_t size);
static OSThread *new_thread(void);
static wiiu_hid_t *new_hid(void);
static void delete_hid(wiiu_hid_t *hid);
static void delete_hidclient(HIDClient *client);
static HIDClient *new_hidclient(void);
static wiiu_adapter_t *new_adapter(wiiu_attach_event *event);
static void delete_adapter(wiiu_adapter_t *adapter);
static wiiu_attach_event *new_attach_event(HIDDevice *device);
static void delete_attach_event(wiiu_attach_event *);

static void wiiu_hid_init_lists(void);
static void start_polling_thread(wiiu_hid_t *hid);
static void stop_polling_thread(wiiu_hid_t *hid);
static int wiiu_hid_polling_thread(int argc, const char **argv);
static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);
static wiiu_attach_event *synchronized_get_events_list(void);
static void wiiu_handle_attach_events(wiiu_hid_t *hid, wiiu_attach_event *list);
static void wiiu_hid_attach(wiiu_hid_t *hid, wiiu_attach_event *event);
static void wiiu_hid_detach(wiiu_hid_t *hid, wiiu_attach_event *event);
static void synchronized_process_adapters(wiiu_hid_t *hid);
static void synchronized_add_to_adapters_list(wiiu_adapter_t *adapter);
static wiiu_adapter_t *synchronized_remove_from_adapters_list(uint32_t handle);
static void synchronized_add_event(wiiu_attach_event *event);
static void wiiu_start_read_loop(wiiu_adapter_t *adapter);
static void wiiu_hid_read_loop_callback(uint32_t handle, int32_t error,
               uint8_t *buffer, uint32_t buffer_size, void *userdata);
static void wiiu_hid_polling_thread_cleanup(OSThread *thread, void *stack);

#endif /* __WIIU_HID__H */
