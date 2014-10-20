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

static int find_vacant_pad(joypad_connection_t *joyconn)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      joypad_connection_t *conn = (joypad_connection_t*)&joyconn[i];
      if (conn && !conn->used)
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
        conn->used     = false;
        conn->iface    = NULL;
        conn->is_gcapi = false;
        conn->data     = NULL;
    }
    
    return joyconn;
}

int32_t pad_connection_connect(joypad_connection_t *joyconn,
   const char* name, void *data, send_control_t ptr)
{
   int pad = find_vacant_pad(joyconn);

   if (pad != -1)
   {
      unsigned i;
      joypad_connection_t* s = (joypad_connection_t*)&joyconn[pad];

      s->used = true;

      static const struct
      {
         const char* name;
         pad_connection_interface_t *iface;
      } pad_map[] = 
      {
         { "Nintendo RVL-CNT-01",         &pad_connection_wii },
#if 0
         { "Nintendo RVL-CNT-01-UC",   &pad_connection_wii_u },
         { "Wireless Controller",         &pad_connection_ps4 },
#endif
         { "PLAYSTATION(R)3 Controller",  &pad_connection_ps3 },
         { 0, 0}
      };

      for (i = 0; name && pad_map[i].name; i++)
         if (strstr(name, pad_map[i].name))
         {
            s->iface = pad_map[i].iface;
            s->data = s->iface->connect(data, pad, ptr);
         }
   }

   return pad;
}

int32_t apple_joypad_connect_gcapi(joypad_connection_t *joyconn)
{
   int pad = find_vacant_pad(joyconn);

   if (pad >= 0 && pad < MAX_PLAYERS)
   {
      joypad_connection_t *s = (joypad_connection_t*)&joyconn[pad];

      if (s)
      {
          s->used = true;
          s->is_gcapi = true;
      }
   }

   return pad;
}

void pad_connection_disconnect(joypad_connection_t *s, uint32_t pad)
{
   if (!s || !s->used)
       return;
    
      if (s->iface)
      {
          s->iface->set_rumble(s->data, RETRO_RUMBLE_STRONG, 0);
          s->iface->set_rumble(s->data, RETRO_RUMBLE_WEAK, 0);
          if (s->iface->disconnect)
              s->iface->disconnect(s->data);
      }
       
       s->iface    = NULL;
       s->used     = false;
       s->is_gcapi = false;
}

void pad_connection_packet(joypad_connection_t *s, uint32_t pad,
      uint8_t* data, uint32_t length)
{
   if (!s->used)
       return;
    
   if (s->iface && s->data && s->iface->packet_handler)
      s->iface->packet_handler(s->data, data, length);
}

uint32_t pad_connection_get_buttons(joypad_connection_t *s, unsigned pad)
{
   if (s->iface)
      return s->iface->get_buttons(s->data);
   return 0;
}

int16_t pad_connection_get_axis(joypad_connection_t *s,
   unsigned idx, unsigned i)
{
   if (s->iface)
      return s->iface->get_axis(s->data, i);
   return 0;
}

bool pad_connection_has_interface(joypad_connection_t *s, unsigned pad)
{
   if (s->used && s->iface)
      return true;
   return false;
}

void pad_connection_destroy(joypad_connection_t *joyconn)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i ++)
   {
      joypad_connection_t *s = (joypad_connection_t*)&joyconn[i];
      pad_connection_disconnect(s, i);
   }
}

bool pad_connection_rumble(joypad_connection_t *s,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength)
{
   if (s->used && s->iface && s->iface->set_rumble)
   {
      s->iface->set_rumble(s->data, effect, strength);
      return true;
   }

   return false;
}
