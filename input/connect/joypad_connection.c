/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <string.h>
#include "joypad_connection.h"

static int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      joypad_connection_t *conn = joyconn ? (joypad_connection_t*)&joyconn[i] : NULL;

      if (conn && !conn->connected)
         return i;
   }

   return -1;
}

void *pad_connection_init(unsigned pads)
{
   int i;
   joypad_connection_t *joyconn = (joypad_connection_t*)
      calloc(pads, sizeof(*joyconn));

   if (!joyconn)
      return NULL;

   for (i = 0; i < pads; i++)
   {
      joypad_connection_t *conn = (joypad_connection_t*)&joyconn[i];

      if (!conn)
         continue;

      conn->connected = false;
      conn->iface     = NULL;
      conn->is_gcapi  = false;
      conn->data      = NULL;
   }

   return joyconn;
}

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char* name, uint16_t vid, uint16_t pid, 
   void *data, send_control_t ptr)
{
   int pad = pad_connection_find_vacant_pad(joyconn);

   if (pad != -1)
   {
      unsigned i;
      joypad_connection_t* s = (joypad_connection_t*)&joyconn[pad];
      static const struct
      {
         const char* name;
         uint16_t vid;
         uint16_t pid;
         pad_connection_interface_t *iface;
      } pad_map[] = 
      {
         { "Nintendo RVL-CNT-01",         1406,  816,    &pad_connection_wii },
#if 0
         { "Nintendo RVL-CNT-01-UC",      0,       0,    &pad_connection_wii_u },
#endif
         { "Wireless Controller",         1356, 1476,    &pad_connection_ps4 },
         { "PLAYSTATION(R)3 Controller",  1356,  616,    &pad_connection_ps3 },
         { 0, 0}
      };

      if (s)
      {
         for (i = 0; name && pad_map[i].name; i++)
         {
            char *name_match = strstr(name, pad_map[i].name);

            if (name_match || (pad_map[i].vid == vid && pad_map[i].pid == pid))
            {
               s->iface      = pad_map[i].iface;
               s->data       = s->iface->init(data, pad, ptr);
               s->connected  = true;

               return pad;
            }
         }
      }
   }

   return pad;
}

void pad_connection_pad_deinit(joypad_connection_t *s, uint32_t pad)
{
   if (!s || !s->connected)
       return;
    
   if (s->iface)
   {
      s->iface->set_rumble(s->data, RETRO_RUMBLE_STRONG, 0);
      s->iface->set_rumble(s->data, RETRO_RUMBLE_WEAK, 0);

      if (s->iface->deinit)
         s->iface->deinit(s->data);
   }

   s->iface     = NULL;
   s->connected = false;
   s->is_gcapi  = false;
}

void pad_connection_packet(joypad_connection_t *s, uint32_t pad,
      uint8_t* data, uint32_t length)
{
   if (!s->connected)
       return;
   if (s->iface && s->data && s->iface->packet_handler)
      s->iface->packet_handler(s->data, data, length);
}

uint64_t pad_connection_get_buttons(joypad_connection_t *s, unsigned pad)
{
   if (!s->iface)
      return 0;
   return s->iface->get_buttons(s->data);
}

int16_t pad_connection_get_axis(joypad_connection_t *s,
   unsigned idx, unsigned i)
{
   if (!s->iface)
      return 0;
   return s->iface->get_axis(s->data, i);
}

bool pad_connection_has_interface(joypad_connection_t *s, unsigned pad)
{
   if (s && s->connected && s->iface)
      return true;
   return false;
}

void pad_connection_destroy(joypad_connection_t *joyconn)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i ++)
   {
      joypad_connection_t *s = (joypad_connection_t*)&joyconn[i];
      pad_connection_pad_deinit(s, i);
   }
}

bool pad_connection_rumble(joypad_connection_t *s,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength)
{
   if (!s->connected)
      return false;
   if (!s->iface)
      return false;
   if (!s->iface->set_rumble)
      return false;

   s->iface->set_rumble(s->data, effect, strength);
   return true;
}
