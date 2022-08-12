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

/* We init the HID/VID to 0 because we need to do 
   endian magic that we can't do during the declaration */
joypad_connection_entry_t pad_map[] = {
   { "Nintendo RVL-CNT-01",           0,     0,  &pad_connection_wii },
   { "Nintendo RVL-CNT-01-UC",        0,     0,  &pad_connection_wiiupro },
   { "Wireless Controller",           0,     0,  &pad_connection_ps4 },
   { "PLAYSTATION(R)3 Controller",    0,     0,  &pad_connection_ps3 },
   { "PLAYSTATION(R)3 Controller",    0,     0,  &pad_connection_ps3 },
   { "Generic SNES USB Controller",   0,     0,  &pad_connection_snesusb },
   { "Generic NES USB Controller",    0,     0,  &pad_connection_nesusb },
   { "Wii U GC Controller Adapter",   0,     0,  &pad_connection_wiiugca },
   { "PS2/PSX Controller Adapter",    0,     0,  &pad_connection_ps2adapter },
   { "PSX to PS3 Controller Adapter", 0,     0,  &pad_connection_psxadapter },
   { "Mayflash DolphinBar",           0,     0,  &pad_connection_wii },
   { "Retrode",                       0,     0,  &pad_connection_retrode },
   { "HORI mini wired PS4",           0,     0,  &pad_connection_ps4_hori_mini },
   { 0, 0}
};

static bool joypad_is_end_of_list(joypad_connection_t *pad)
{
   return pad
      && !pad->connected
      && !pad->iface
      &&  (pad->data == (void *)0xdeadbeef);
}

int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{
   int i;
   if (joyconn)
   {
      for (i = 0; !joypad_is_end_of_list(&joyconn[i]); i++)
      {
         if (!joyconn[i].connected)
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
      RARCH_WARN("[joypad] invalid number of pads requested (%d), using default (%d)\n",
            pads, MAX_USERS);
      pads = MAX_USERS;
   }

   if (!(joyconn = (joypad_connection_t*)calloc(pads+1, sizeof(joypad_connection_t))))
      return NULL;

   for (i = 0; i < pads; i++)
   {
      joypad_connection_t *conn = (joypad_connection_t*)&joyconn[i];

      conn->connected           = false;
      conn->iface               = NULL;
      conn->data                = NULL;
   }

   /* Set end of list */
   {
      joypad_connection_t *entry = (joypad_connection_t *)&joyconn[pads];
      entry->connected           = false;
      entry->iface               = NULL;
      entry->data                = (void*)0xdeadbeef;
   }

   return joyconn;
}

static void init_pad_map(void)
{
   pad_map[0].vid         = VID_NINTENDO;
   pad_map[0].pid         = PID_NINTENDO_PRO;
   pad_map[1].vid         = VID_NINTENDO;
   pad_map[1].pid         = PID_NINTENDO_PRO;
   pad_map[2].vid         = VID_SONY;
   pad_map[2].pid         = PID_SONY_DS4;
   pad_map[3].vid         = VID_SONY;
   pad_map[3].pid         = PID_SONY_DS3;
   pad_map[4].vid         = VID_PS3_CLONE;
   pad_map[4].pid         = PID_DS3_CLONE;
   pad_map[5].vid         = VID_SNES_CLONE;
   pad_map[5].pid         = PID_SNES_CLONE;
   pad_map[6].vid         = VID_MICRONTEK;
   pad_map[6].pid         = PID_MICRONTEK_NES;
   pad_map[7].vid         = VID_NINTENDO;
   pad_map[7].pid         = PID_NINTENDO_GCA;
   pad_map[8].vid         = VID_PCS;
   pad_map[8].pid         = PID_PCS_PS2PSX;
   pad_map[9].vid         = VID_PCS;
   pad_map[9].pid         = PID_PCS_PSX2PS3;
   pad_map[10].vid        = 1406;
   pad_map[10].pid        = 774;
   pad_map[11].vid        = VID_RETRODE;
   pad_map[11].pid        = PID_RETRODE;
   pad_map[12].vid        = VID_HORI_1;
   pad_map[12].pid        = PID_HORI_MINI_WIRED_PS4;
}

joypad_connection_entry_t *find_connection_entry(uint16_t vid, uint16_t pid, const char *name)
{
   unsigned i;
   const bool has_name = !string_is_empty(name);
   size_t name_len     = strlen(name);

   if (pad_map[0].vid == 0)
      init_pad_map();

   for(i = 0; pad_map[i].name != NULL; i++)
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
            RARCH_ERR("find_connection_entry(0x%04x,0x%04x): device name '%s' too short: assuming controller '%s'\n",
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

   for(i = 0; !joypad_is_end_of_list(&haystack[i]); i++)
   {
      if (&haystack[i] == needle)
         return i;
   }
   return -1;
}

void release_joypad(joypad_connection_t *joypad) {

}

void legacy_pad_connection_pad_deregister(joypad_connection_t *pad_list, pad_connection_interface_t *iface, void *pad_data)
{
   int i;
   for(i = 0; !joypad_is_end_of_list(&pad_list[i]); i++)
   {
      if (pad_list[i].connection == pad_data)
      {
         input_autoconfigure_disconnect(i, iface ? iface->get_name(pad_data) : NULL);
         memset(&pad_list[i], 0, sizeof(joypad_connection_t));
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

   for(i = 0; i < iface->max_pad; i++)
   {
      int slot = joypad_to_slot(joyconn, iface->joypad(pad_data, i));
      if (slot >= 0)
      {
         input_autoconfigure_disconnect(slot, iface->get_name(joyconn[slot].connection));
         iface->pad_deinit(joyconn[slot].connection);
         memset(&joyconn[slot], 0, sizeof(joypad_connection_t));
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
            input_autoconfigure_disconnect(slot,
                  iface->get_name(joypad->connection));

            iface->pad_deinit(joypad->connection);
            memset(joypad, 0, sizeof(joypad_connection_t));
            break;
            /* The joypad is connected but has not been bound */
         case PAD_CONNECT_READY:
            slot = pad_connection_find_vacant_pad(joyconn);
            if (slot >= 0)
            {
               joypad = &joyconn[slot];
               joypad->connection   = iface->pad_init(device_data,
                                      i, joypad);
               joypad->data         = handle;
               joypad->iface        = iface;
               joypad->input_driver = input_driver;
               joypad->connected    = true;
               input_pad_connect(slot, input_driver);
            }
            break;
         default:
#ifndef NDEBUG
            if (state > 0x03)
               RARCH_LOG("Unrecognized state: 0x%02x", state);
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
   int max_pad;

   if (     (iface->multi_pad)
         && (iface->max_pad <= 1 || !iface->status || !iface->pad_init))
   {
      RARCH_ERR("pad_connection_pad_register: multi-pad driver has incomplete implementation\n");
      return;
   }

   max_pad = iface->multi_pad ? iface->max_pad : 1;

   for(i = 0; i < max_pad; i++)
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

         RARCH_LOG("Connecting pad to slot %d\n", found_slot);
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
      conn->data       = data;
      conn->connection = conn->iface->init(data, pad, driver);
      conn->connected  = true;
   }
   else
   {
      /* We failed to find a matching pad. 
       * Set up one without an interface */
      RARCH_DBG("Pad was not matched. Setting up without an interface.\n");
      conn->iface      = NULL;
      conn->data       = data;
      conn->connected  = true;
   }

   return pad;
}

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char *name, uint16_t vid, uint16_t pid,
   void *data, hid_driver_t *driver)
{
   joypad_connection_entry_t *entry = NULL;

   if (pad_map[0].vid == 0)
      init_pad_map();

   entry = find_connection_entry(vid, pid, name);

   return pad_connection_pad_init_entry(joyconn, entry, data, driver);
}

void pad_connection_pad_deinit(joypad_connection_t *joyconn,
      uint32_t pad)
{
   if (!joyconn || !joyconn->connected)
       return;

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
}

void pad_connection_packet(joypad_connection_t *joyconn, uint32_t pad,
      uint8_t* data, uint32_t length)
{
   if (     joyconn
         && joyconn->connected
         && joyconn->connection 
         && joyconn->iface
         && joyconn->iface->packet_handler)
      joyconn->iface->packet_handler(joyconn->connection, data, length);
}

void pad_connection_get_buttons(joypad_connection_t *joyconn,
      unsigned pad, input_bits_t *state)
{
   if (joyconn && joyconn->iface)
      joyconn->iface->get_buttons(joyconn->connection, state);
   else
      BIT256_CLEAR_ALL_PTR( state );
}

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i)
{
   if (joyconn && joyconn->iface)
      return joyconn->iface->get_axis(joyconn->connection, i);
   return 0;
}

bool pad_connection_has_interface(joypad_connection_t *joyconn,
      unsigned pad)
{
   return (     joyconn && pad < MAX_USERS
             && joyconn[pad].connected
             && joyconn[pad].iface);
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
   if (!joyconn->connected)
      return false;
   if (!joyconn->iface || !joyconn->iface->set_rumble)
      return false;

   joyconn->iface->set_rumble(joyconn->connection, effect, strength);
   return true;
}

const char* pad_connection_get_name(joypad_connection_t *joyconn,
      unsigned pad)
{
   if (joyconn && joyconn->iface && joyconn->iface->get_name)
      return joyconn->iface->get_name(joyconn->connection);
   return NULL;
}
