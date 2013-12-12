/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include "input/input_common.h"
#include "general.h"

#ifdef IOS
#include "apple/iOS/bluetooth/btdynamic.c"
#include "apple/iOS/bluetooth/btpad.c"
#include "apple/iOS/bluetooth/btpad_queue.c"
#elif defined(OSX)
#include "../OSX/hid_pad.c"
#endif

#include "apple/common/hidpad/wiimote.c"
#include "apple/common/hidpad/apple_ps3_pad.c"
#include "apple/common/hidpad/apple_wii_pad.c"

typedef struct
{
   bool used;
   struct apple_pad_interface* iface;
   void* data;
} joypad_slot_t;

static joypad_slot_t slots[MAX_PLAYERS];

static int32_t find_empty_slot()
{
   for (int i = 0; i != MAX_PLAYERS; i ++)
      if (!slots[i].used)
      {
         memset(&slots[i], 0, sizeof(slots[0]));
         return i;
      }
   return -1;
}

int32_t apple_joypad_connect(const char* name, struct apple_pad_connection* connection)
{
   int32_t slot = find_empty_slot();

   if (slot >= 0 && slot < MAX_PLAYERS)
   {
      joypad_slot_t* s = &slots[slot];
      s->used = true;

      static const struct { const char* name; struct apple_pad_interface* iface; } pad_map[] = {
         { "Nintendo RVL-CNT-01",         &apple_pad_wii },
         { "PLAYSTATION(R)3 Controller",  &apple_pad_ps3 },
         { 0, 0} };

      for (int i = 0; name && pad_map[i].name; i ++)
         if (strstr(name, pad_map[i].name))
         {
            s->iface = pad_map[i].iface;
            s->data = s->iface->connect(connection, slot);
         }
   }

   return slot;
}

void apple_joypad_disconnect(uint32_t slot)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
   {
      joypad_slot_t* s = &slots[slot];

      if (s->iface && s->data)
         s->iface->disconnect(s->data);

      s->used = false;
   }
}

void apple_joypad_packet(uint32_t slot, uint8_t* data, uint32_t length)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
   {
      joypad_slot_t* s = &slots[slot];

      if (s->iface && s->data)
         s->iface->packet_handler(s->data, data, length);
   }
}

bool apple_joypad_has_interface(uint32_t slot)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
      return slots[slot].iface ? true : false;
   return false;
}

// RetroArch joypad driver:
static bool apple_joypad_init(void)
{
   return true;
}

static bool apple_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS;
}

static void apple_joypad_destroy(void)
{
   for (int i = 0; i != MAX_PLAYERS; i ++)
   {
      if (slots[i].used && slots[i].iface)
      {
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_STRONG, 0);
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_WEAK, 0);
      }
   }
}

static bool apple_joypad_button(unsigned port, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
      return false;
   else // Check the button
      return (port < MAX_PLAYERS && joykey < 32) ? (g_polled_input_data.pad_buttons[port] & (1 << joykey)) != 0 : false;
}

static int16_t apple_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   int16_t val = 0;
   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val = g_polled_input_data.pad_axis[port][AXIS_NEG_GET(joyaxis)];
      val = (val < 0) ? val : 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val = g_polled_input_data.pad_axis[port][AXIS_POS_GET(joyaxis)];
      val = (val > 0) ? val : 0;
   }

   return val;
}

static void apple_joypad_poll(void)
{
}

static bool apple_joypad_rumble(unsigned pad, enum retro_rumble_effect effect, uint16_t strength)
{
   if (pad < MAX_PLAYERS && slots[pad].used && slots[pad].iface)
   {
      slots[pad].iface->set_rumble(slots[pad].data, effect, strength);
      return true;
   }

   return false;
}


static const char *apple_joypad_name(unsigned joypad)
{
   (void)joypad;
   return NULL;
}

const rarch_joypad_driver_t apple_joypad = {
   apple_joypad_init,
   apple_joypad_query_pad,
   apple_joypad_destroy,
   apple_joypad_button,
   apple_joypad_axis,
   apple_joypad_poll,
   apple_joypad_rumble,
   apple_joypad_name,
   "apple"
};

