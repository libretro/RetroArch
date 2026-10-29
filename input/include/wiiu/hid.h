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

#include <retro_atomic.h>
#include <queues/mpsc_stack.h>

#include "hid_types.h"
#include "input.h"

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

/* An adapter's state, and which thread moves it on from there:
 * detected, for the frontend to initialise */
#define ADAPTER_STATE_NEW     0
/* initialised, for the polling thread to read from or wind down */
#define ADAPTER_STATE_READY   1
/* a read in flight, for its completion to return to READY */
#define ADAPTER_STATE_READING 2
/* no more reads, for the polling thread to hand back */
#define ADAPTER_STATE_DONE    3
/* handed back, for the frontend to deregister and retire */
#define ADAPTER_STATE_GC      4

struct wiiu_hid
{
   /* used to register for HID notifications */
   HIDClient *client;
   /* thread state data for the HID input polling thread */
   OSThread *polling_thread;
   /* stack space for polling thread */
   void *polling_thread_stack;
   /* Tells the polling thread to terminate: set by the thread tearing
    * the driver down, read by the polling thread's loop. An atomic and
    * not a volatile bool: volatile orders nothing between two threads. */
   retro_atomic_int_t polling_thread_quit;
};

/**
 * Each HID device attached to the WiiU gets its own adapter, which
 * connects the HID subsystem with the HID device driver.
 *
 * The polling thread and the frontend each keep their own list of
 * adapters and pass them to each other through lock-free stacks; an
 * adapter is in at most one of those at a time, by its node.
 */
struct wiiu_adapter
{
   mpsc_stack_node_t node;          /* first: the stacks hand this out */
   wiiu_adapter_t *next;            /* the polling thread's list */
   wiiu_adapter_t *next_frontend;   /* the frontend's list */
   pad_connection_interface_t *pad_driver;
   void *pad_driver_data;
   wiiu_hid_t *hid;
   uint8_t *rx_buffer;
   uint8_t *tx_buffer;
   retro_atomic_int_t state;        /* ADAPTER_STATE_* */
   int32_t rx_size;
   int32_t tx_size;
   uint32_t handle;
   uint16_t vendor_id;
   uint16_t product_id;
   char device_name[32];
   uint8_t interface_index;
   bool connected;                  /* the polling thread's alone */
};

/**
 * When a HID device is connected, the OS generates an attach
 * event; the attach event handler translate them into these
 * structures.
 */
struct wiiu_attach
{
   mpsc_stack_node_t node;          /* first: the event stack hands this out */
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

#endif /* __WIIU_HID__H */
