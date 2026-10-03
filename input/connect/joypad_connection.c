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

#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>
#include "../tasks/tasks_internal.h"

#include "../input_driver.h"
#include "../../verbosity.h"

#include "joypad_connection.h"

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

bool pad_connection_acquire(joypad_connection_t *joyconn)
{
   (void)retro_atomic_fetch_add_seq_cst_int(&joyconn->users, 1);
   retro_atomic_thread_fence_seq_cst();
   if (retro_atomic_load_seq_cst_int(&joyconn->live))
      return true;
   (void)retro_atomic_fetch_sub_int(&joyconn->users, 1);
   return false;
}

void pad_connection_release(joypad_connection_t *joyconn)
{
   (void)retro_atomic_fetch_sub_int(&joyconn->users, 1);
}

/* The slot's fields are set: readers may go in. */
static void slot_publish(joypad_connection_t *joyconn)
{
   retro_atomic_store_release_int(&joyconn->live, 1);
}

/* No new reader goes in; returns once none is inside. A reader is in
 * for one call into the pad, so the wait is that long. */
static void slot_retire(joypad_connection_t *joyconn)
{
   (void)retro_atomic_exchange_int(&joyconn->live, 0);
   retro_atomic_thread_fence_seq_cst();
   while (retro_atomic_load_seq_cst_int(&joyconn->users))
   {
#ifdef HAVE_THREADS
      sthread_yield();
#endif
   }
}

/* Clears a retired slot for reuse, keeping its counters. */
static void slot_clear(joypad_connection_t *joyconn)
{
   joyconn->iface        = NULL;
   joyconn->input_driver = NULL;
   joyconn->data         = NULL;
   joyconn->connection   = NULL;
   joyconn->connected    = false;
   pad_connection_release_slot(joyconn);
}

static joypad_connection_entry_t pad_map[] = {
   { "Nintendo RVL-CNT-01",
      VID_NINTENDO,
      PID_NINTENDO_PRO,
      &pad_connection_wii
   },
   { "Nintendo RVL-CNT-01-UC",
      VID_NINTENDO,
      PID_NINTENDO_PRO,
      &pad_connection_wiiupro
   },
   { "Wireless Controller",
      VID_SONY,
      PID_SONY_DS4,
      &pad_connection_ps4
   },
   { "Wireless Controller",
      VID_SONY,
      PID_SONY_DS4_R2,
      &pad_connection_ps4
   },
   { "PLAYSTATION(R)3 Controller",
      VID_SONY,
      PID_SONY_DS3,
      &pad_connection_ps3
   },
   { "PLAYSTATION(R)3 Controller",
      VID_PS3_CLONE,
      PID_DS3_CLONE,
      &pad_connection_ps3
   },
   { "Generic SNES USB Controller",
      VID_SNES_CLONE,
      PID_SNES_CLONE,
      &pad_connection_snesusb
   },
   { "Generic NES USB Controller",
      VID_MICRONTEK,
      PID_MICRONTEK_NES,
      &pad_connection_nesusb
   },
   { "Wii U GC Controller Adapter",
      VID_NINTENDO,
      PID_NINTENDO_GCA,
      &pad_connection_wiiugca
   },
   { "PS2/PSX Controller Adapter",
      VID_PCS,
      PID_PCS_PS2PSX,
      &pad_connection_ps2adapter
   },
   { "PSX to PS3 Controller Adapter",
      VID_PCS,
      PID_PCS_PSX2PS3,
      &pad_connection_psxadapter
   },
   { "Mayflash DolphinBar",
      1406,
      774,
      &pad_connection_wii
   },
   { "Retrode",
      VID_RETRODE,
      PID_RETRODE,
      &pad_connection_retrode
   },
   { "HORI mini wired PS4",
      VID_HORI_1,
      PID_HORI_MINI_WIRED_PS4,
      &pad_connection_ps4_hori_mini
   },
   { "Kade Kick Ass Dynamic Encoder",
      VID_KADE,
      PID_KADE,
      &pad_connection_kade
   },
   { "DragonRise ZeroDelay Encoder",
      VID_DRAGONRISE,
      PID_DRAGONRISE,
      &pad_connection_dragonrise
   },
   { 0, 0}
};

static bool joypad_is_end_of_list(joypad_connection_t *pad)
{
   return pad
      && !pad->connected
      && !pad->iface
      &&  (pad->data == (void *)0xdeadbeef);
}

/* claimed: free, taken, or the list's end marker */
#define SLOT_FREE  0
#define SLOT_TAKEN 1
#define SLOT_END   2

static bool slot_claim(joypad_connection_t *joyconn)
{
#ifdef RETRO_ATOMIC_HAS_CAS
   return retro_atomic_cas_int(&joyconn->claimed, SLOT_FREE, SLOT_TAKEN) != 0;
#else
   if (retro_atomic_load_acquire_int(&joyconn->claimed) != SLOT_FREE)
      return false;
   retro_atomic_store_release_int(&joyconn->claimed, SLOT_TAKEN);
   return true;
#endif
}

void pad_connection_mark_end(joypad_connection_t *joyconn)
{
   joyconn->connected = false;
   joyconn->iface     = NULL;
   joyconn->data      = (void *)0xdeadbeef;
   retro_atomic_int_init(&joyconn->live, 0);
   retro_atomic_int_init(&joyconn->users, 0);
   retro_atomic_int_init(&joyconn->claimed, SLOT_END);
}

void pad_connection_release_slot(joypad_connection_t *joyconn)
{
   if (joyconn)
      retro_atomic_store_release_int(&joyconn->claimed, SLOT_FREE);
}

/* Reads nothing but the claims: the fields of a slot belong to the
 * thread that holds it. */
int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{
   if (joyconn)
   {
      int i;
      for (i = 0; ; i++)
      {
         int state = retro_atomic_load_acquire_int(&joyconn[i].claimed);
         if (state == SLOT_END)
            break;
         if (state == SLOT_FREE && slot_claim(&joyconn[i]))
            return i;
      }
   }
   return -1;
}

/**
 * Since the pad_connection_destroy() call needs to iterate through this
 * list, we allocate pads+1 entries and use the extra spot to store a
 * marker.
 */
joypad_connection_t *pad_connection_init(unsigned pads)
{
   int i;
   joypad_connection_t *joyconn;

   if (pads > MAX_USERS)
   {
      RARCH_WARN("[Joypad] Invalid number of pads requested (%d), using default (%d).\n",
            pads, MAX_USERS);
      pads = MAX_USERS;
   }

   if (!(joyconn = (joypad_connection_t*)calloc(pads+1, sizeof(joypad_connection_t))))
      return NULL;

   for (i = 0; i < (int)pads; i++)
   {
      joypad_connection_t *conn  = (joypad_connection_t*)&joyconn[i];
      retro_atomic_int_init(&conn->live, 0);
      retro_atomic_int_init(&conn->users, 0);
      retro_atomic_int_init(&conn->claimed, 0);

      conn->connected            = false;
      conn->iface                = NULL;
      conn->data                 = NULL;
   }

   pad_connection_mark_end(&joyconn[pads]);

   return joyconn;
}
joypad_connection_entry_t *find_connection_entry(uint16_t vid, uint16_t pid, const char *name)
{
   unsigned i;
   const bool has_name = name && *name;
   size_t name_len     = strlen(name);

   for (i = 0; pad_map[i].name != NULL; i++)
   {
      char *name_match = NULL;
      /* The Wii Pro Controller and WiiU Pro controller have
       * the same VID/PID, so we have to use the
       * descriptor string to differentiate them. */
      if (     pad_map[i].vid == VID_NINTENDO
            && pad_map[i].pid == PID_NINTENDO_PRO
            && pad_map[i].vid == vid
            && pad_map[i].pid == pid)
      {
         name_match = has_name
            ? (char*)strstr(pad_map[i].name, name)
            : NULL;
         if (has_name && name_len < 19)
         {
            /* Wii U: Argument 'name' may be truncated. This is not enough for a reliable name match! */
            RARCH_ERR("[Joypad] find_connection_entry(0x%04x,0x%04x): device name '%s' too short: assuming controller '%s'.\n",
                  SWAP_IF_BIG(vid), SWAP_IF_BIG(pid), name, pad_map[i].name);
         }
         else if (!string_is_equal(pad_map[i].name, name))
            continue;
      }

      if (name_match || (pad_map[i].vid == vid && pad_map[i].pid == pid))
         return &pad_map[i];
   }

   return NULL;
}

static int joypad_to_slot(joypad_connection_t *haystack,
      joypad_connection_t *needle)
{
   int i;

   if (!needle)
      return -1;

   for (i = 0; !joypad_is_end_of_list(&haystack[i]); i++)
   {
      if (&haystack[i] == needle)
         return i;
   }
   return -1;
}

void legacy_pad_connection_pad_deregister(joypad_connection_t *pad_list, pad_connection_interface_t *iface, void *pad_data)
{
   int i;
   for (i = 0; !joypad_is_end_of_list(&pad_list[i]); i++)
   {
      if (pad_list[i].connection == pad_data)
      {
         slot_retire(&pad_list[i]);
         input_autoconfigure_disconnect(i, iface ? iface->get_name(pad_data) : NULL);
         slot_clear(&pad_list[i]);
         return;
      }
   }
}

void pad_connection_pad_deregister(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface, void *pad_data)
{
   int i;

   if (!iface || !iface->multi_pad)
   {
      legacy_pad_connection_pad_deregister(joyconn, iface, pad_data);
      return;
   }

   for (i = 0; i < iface->max_pad; i++)
   {
      int slot = joypad_to_slot(joyconn, iface->joypad(pad_data, i));
      if (slot >= 0)
      {
         slot_retire(&joyconn[slot]);
         input_autoconfigure_disconnect(slot, iface->get_name(joyconn[slot].connection));
         iface->pad_deinit(joyconn[slot].connection);
         slot_clear(&joyconn[slot]);
      }
   }
}



void pad_connection_pad_refresh(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface,
      void *device_data, void *handle,
      input_device_driver_t *input_driver)
{
   int i, slot;
   int8_t state;
   joypad_connection_t *joypad;

   if (!iface->multi_pad || iface->max_pad < 1)
      return;

   for (i = 0; i < iface->max_pad; i++)
   {
      state = iface->status(device_data, i);
      switch(state)
      {
         /* The pad slot is bound to a joypad
            that's no longer connected */
         case PAD_CONNECT_BOUND:
            joypad = iface->joypad(device_data, i);
            slot   = joypad_to_slot(joyconn, joypad);
            slot_retire(joypad);
            input_autoconfigure_disconnect(slot,
                  iface->get_name(joypad->connection));

            iface->pad_deinit(joypad->connection);
            slot_clear(joypad);
            break;
            /* The joypad is connected but has not been bound */
         case PAD_CONNECT_READY:
            if ((slot = pad_connection_find_vacant_pad(joyconn)) >= 0)
            {
               joypad               = &joyconn[slot];
               joypad->connection   = iface->pad_init(device_data,
                                      i, joypad);
               joypad->data         = handle;
               joypad->iface        = iface;
               joypad->input_driver = input_driver;
               joypad->connected    = true;
               slot_publish(joypad);
               input_pad_connect(slot, input_driver);
            }
            break;
         default:
#ifndef NDEBUG
            if (state > 0x03)
               RARCH_LOG("[Joypad] Unrecognized state: 0x%02x.", state);
#endif
            break;
      }
   }
}

void pad_connection_pad_register(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface,
      void *device_data, void *handle,
      input_device_driver_t *input_driver, int slot)
{
   int i;
   int max_pad = 1;

   if (iface->multi_pad)
   {
      if (iface->max_pad <= 1 || !iface->status || !iface->pad_init)
      {
         RARCH_ERR("[Joypad] pad_connection_pad_register: multi-pad driver has incomplete implementation.\n");
         return;
      }
      max_pad = iface->max_pad;
   }

   for (i = 0; i < max_pad; i++)
   {
      int status = iface->multi_pad
         ? iface->status(device_data, i)
         : PAD_CONNECT_READY;
      if (status == PAD_CONNECT_READY)
      {
         void *connection = NULL;
         int found_slot   = (slot == SLOT_AUTO)
            ? pad_connection_find_vacant_pad(joyconn)
            : slot;
         if (found_slot < 0)
            continue;
         if (iface->multi_pad)
            connection = iface->pad_init(device_data, i,
                  &joyconn[found_slot]);
         else
            connection = device_data;

         joyconn[found_slot].iface        = iface;
         joyconn[found_slot].data         = handle;
         joyconn[found_slot].connection   = connection;
         joyconn[found_slot].input_driver = input_driver;
         joyconn[found_slot].connected    = true;
         slot_publish(&joyconn[found_slot]);

         RARCH_LOG("[Joypad] Connecting pad to slot %d.\n", found_slot);
         input_pad_connect(found_slot, input_driver);
      }
   }
}

int32_t pad_connection_pad_init_entry(joypad_connection_t *joyconn,
      joypad_connection_entry_t *entry,
      void *data, hid_driver_t *driver)
{
   joypad_connection_t *conn = NULL;
   int pad = pad_connection_find_vacant_pad(joyconn);

   if (pad < 0)
      return -1;

   if (!(conn = &joyconn[pad]))
      return -1;

   if (entry)
   {
      conn->iface      = entry->iface;
      conn->connection = conn->iface->init(data, pad, driver);
   }
   else
   {
      /* We failed to find a matching pad.
       * Set up one without an interface */
      RARCH_DBG("[Joypad] Pad was not matched. Setting up without an interface.\n");
      conn->iface      = NULL;
   }
   conn->data          = data;
   conn->connected     = true;
   slot_publish(conn);

   return pad;
}

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char *name, uint16_t vid, uint16_t pid,
   void *data, hid_driver_t *driver)
{
   joypad_connection_entry_t *entry = find_connection_entry(vid, pid, name);
   return pad_connection_pad_init_entry(joyconn, entry, data, driver);
}

void pad_connection_pad_deinit(joypad_connection_t *joyconn,
      uint32_t pad)
{
   if (!joyconn || !joyconn->connected)
       return;

   slot_retire(joyconn);

   if (joyconn->iface)
   {
      joyconn->iface->set_rumble(joyconn->connection,
            RETRO_RUMBLE_STRONG, 0);
      joyconn->iface->set_rumble(joyconn->connection,
            RETRO_RUMBLE_WEAK, 0);

      if (joyconn->iface->deinit)
         joyconn->iface->deinit(joyconn->connection);
   }

   joyconn->iface      = NULL;
   joyconn->connected  = false;
   joyconn->connection = NULL;
   pad_connection_release_slot(joyconn);
}

void pad_connection_packet(joypad_connection_t *joyconn, uint32_t pad,
      uint8_t* data, uint32_t length)
{
   if (!joyconn || !pad_connection_acquire(joyconn))
      return;
   if (     joyconn->connection
         && joyconn->iface
         && joyconn->iface->packet_handler)
      joyconn->iface->packet_handler(joyconn->connection, data, length);
   pad_connection_release(joyconn);
}

void pad_connection_get_buttons(joypad_connection_t *joyconn,
      unsigned pad, input_bits_t *state)
{
   if (joyconn && pad_connection_acquire(joyconn))
   {
      if (joyconn->iface)
         joyconn->iface->get_buttons(joyconn->connection, state);
      else
         BIT256_CLEAR_ALL_PTR( state );
      pad_connection_release(joyconn);
   }
   else
      BIT256_CLEAR_ALL_PTR( state );
}

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i)
{
   int16_t val = 0;
   if (joyconn && pad_connection_acquire(joyconn))
   {
      if (joyconn->iface)
         val = joyconn->iface->get_axis(joyconn->connection, i);
      pad_connection_release(joyconn);
   }
   return val;
}

bool pad_connection_has_interface(joypad_connection_t *joyconn,
      unsigned pad)
{
   bool ret = false;
   if (joyconn && pad < MAX_USERS && pad_connection_acquire(&joyconn[pad]))
   {
      ret = joyconn[pad].iface != NULL;
      pad_connection_release(&joyconn[pad]);
   }
   return ret;
}

void pad_connection_destroy(joypad_connection_t *joyconn)
{
   int i;

   for (i = 0; !joypad_is_end_of_list(&joyconn[i]); i ++)
     pad_connection_pad_deinit(&joyconn[i], i);

   free(joyconn);
}

bool pad_connection_rumble(joypad_connection_t *joyconn,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength)
{
   bool ret = false;
   if (!pad_connection_acquire(joyconn))
      return false;
   if (joyconn->iface && joyconn->iface->set_rumble)
   {
      joyconn->iface->set_rumble(joyconn->connection, effect, strength);
      ret = true;
   }
   pad_connection_release(joyconn);
   return ret;
}

const char* pad_connection_get_name(joypad_connection_t *joyconn,
      unsigned pad)
{
   const char *name = NULL;
   if (joyconn && pad_connection_acquire(joyconn))
   {
      if (joyconn->iface && joyconn->iface->get_name)
         name = joyconn->iface->get_name(joyconn->connection);
      pad_connection_release(joyconn);
   }
   return name;
}
