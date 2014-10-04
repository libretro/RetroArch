/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "joypad_connection.h"

typedef struct
{
   bool used;
   struct pad_connection_interface *iface;
   void* data;
   
   bool is_gcapi;
} joypad_slot_t;

static joypad_slot_t slots[MAX_PLAYERS];

static int find_vacant_pad(void)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (slots[i].used)
         continue;

      memset(&slots[i], 0, sizeof(slots[0]));
      return i;
   }
   return -1;
}

int32_t pad_connection_connect(const char* name, void *data)
{
   int pad = find_vacant_pad();

   if (pad >= 0 && pad < MAX_PLAYERS)
   {
      unsigned i;
      joypad_slot_t* s = (joypad_slot_t*)&slots[pad];

      s->used = true;

      static const struct
      {
         const char* name;
         pad_connection_interface_t *iface;
      } pad_map[] = 
      {
         { "Nintendo RVL-CNT-01",         &apple_pad_wii },
         /* { "Nintendo RVL-CNT-01-UC",   &apple_pad_wii }, */ /* WiiU */
         /* { "Wireless Controller",         &apple_pad_ps4 }, */ /* DualShock4 */
         { "PLAYSTATION(R)3 Controller",  &apple_pad_ps3 },
         { 0, 0}
      };

      for (i = 0; name && pad_map[i].name; i++)
         if (strstr(name, pad_map[i].name))
         {
            s->iface = pad_map[i].iface;
            s->data = s->iface->connect(data, pad);
         }
   }

   return pad;
}

int32_t apple_joypad_connect_gcapi(void)
{
   int pad = find_vacant_pad();

   if (pad >= 0 && pad < MAX_PLAYERS)
   {
      joypad_slot_t *s = (joypad_slot_t*)&slots[pad];

      if (s)
      {
         s->used = true;
         s->is_gcapi = true;
      }
   }

   return pad;
}

void pad_connection_disconnect(uint32_t pad)
{
   if (pad < MAX_PLAYERS && slots[pad].used)
   {
      joypad_slot_t* s = (joypad_slot_t*)&slots[pad];

      if (s->iface && s->data && s->iface->disconnect)
         s->iface->disconnect(s->data);

      memset(s, 0, sizeof(joypad_slot_t));
   }
}

void pad_connection_packet(uint32_t pad,
      uint8_t* data, uint32_t length)
{
   if (pad < MAX_PLAYERS && slots[pad].used)
   {
      joypad_slot_t *s = (joypad_slot_t*)&slots[pad];

      if (s->iface && s->data && s->iface->packet_handler)
         s->iface->packet_handler(s->data, data, length);
   }
}

bool pad_connection_has_interface(uint32_t pad)
{
   if (pad < MAX_PLAYERS && slots[pad].used)
      return slots[pad].iface ? true : false;

   return false;
}

void pad_connection_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i ++)
   {
      if (slots[i].used && slots[i].iface
            && slots[i].iface->set_rumble)
      {
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_STRONG, 0);
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_WEAK, 0);
      }
   }
}

bool pad_connection_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   if (pad < MAX_PLAYERS && slots[pad].used && slots[pad].iface
         && slots[pad].iface->set_rumble)
   {
      slots[pad].iface->set_rumble(slots[pad].data, effect, strength);
      return true;
   }

   return false;
}
