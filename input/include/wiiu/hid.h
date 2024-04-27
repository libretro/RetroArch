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

struct wiiu_hid
{
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
struct wiiu_adapter
{
   wiiu_adapter_t *next;
   pad_connection_interface_t *pad_driver;
   void *pad_driver_data;
   wiiu_hid_t *hid;
   uint16_t vendor_id;
   uint16_t product_id;
   uint8_t state;
   uint8_t *rx_buffer;
   int32_t rx_size;
   uint8_t *tx_buffer;
   int32_t tx_size;
   uint32_t handle;
   uint8_t interface_index;
   char device_name[32];
   bool connected;
};

/**
 * When a HID device is connected, the OS generates an attach
 * event; the attach event handler translate them into these
 * structures.
 */
struct wiiu_attach
{
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
   uint8_t device_name[32];
};

struct _wiiu_event_list
{
   OSFastMutex lock;
   wiiu_attach_event *list;
};

struct _wiiu_adapter_list
{
   OSFastMutex lock;
   wiiu_adapter_t *list;
};

#endif /* __WIIU_HID__H */
