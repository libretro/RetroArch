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

#include "../input_driver.h"
#include "../../verbosity.h"

#include "joypad_connection.h"

static bool joypad_is_end_of_list(joypad_connection_t *pad);

int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{
  unsigned i;

  if (!joyconn)
    return -1;

  for (i = 0; !joypad_is_end_of_list(&joyconn[i]); i++)
  {
    if(!joyconn[i].connected)
      return i;
  }

  return -1;
}

static void set_end_of_list(joypad_connection_t *list, unsigned end)
{
  joypad_connection_t *entry = (joypad_connection_t *)&list[end];
  entry->connected = false;
  entry->iface = NULL;
  entry->data = (void *)0xdeadbeef;
}

static bool joypad_is_end_of_list(joypad_connection_t *pad) {
  return pad && !pad->connected && !pad->iface && pad->data == (void *)0xdeadbeef;
}

/**
 * Since the pad_connection_destroy() call needs to iterate through this
 * list, we allocate pads+1 entries and use the extra spot to store a
 * marker.
 */
joypad_connection_t *pad_connection_init(unsigned pads)
{
   unsigned i;
   joypad_connection_t *joyconn;

   if(pads > MAX_USERS)
   {
     RARCH_WARN("[joypad] invalid number of pads requested (%d), using default (%d)\n",
      pads, MAX_USERS);
     pads = MAX_USERS;
   }

   joyconn = (joypad_connection_t*)calloc(pads+1, sizeof(joypad_connection_t));

   if (!joyconn)
      return NULL;

   for (i = 0; i < pads; i++)
   {
      joypad_connection_t *conn = (joypad_connection_t*)&joyconn[i];

      conn->connected           = false;
      conn->iface               = NULL;
      conn->data                = NULL;
   }

   set_end_of_list(joyconn, pads);

   return joyconn;
}

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char *name, uint16_t vid, uint16_t pid,
   void *data, hid_driver_t *driver)
{

   static struct
   {
      const char* name;
      uint16_t vid;
      uint16_t pid;
      pad_connection_interface_t *iface;
   } pad_map[] =
   {
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
      { 0, 0}
   };
   joypad_connection_t *s = NULL;
   int pad                = pad_connection_find_vacant_pad(joyconn);

   if (pad == -1)
      return -1;

   s = &joyconn[pad];

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

   if (s)
   {
      unsigned i;

      for (i = 0; name && pad_map[i].name; i++)
      {
         const char *name_match = strstr(pad_map[i].name, name);

         /* Never change, Nintendo. */
         if(pad_map[i].vid == 1406 && pad_map[i].pid == 816)
         {
            if(!string_is_equal(pad_map[i].name, name))
               continue;
         }

#if 0
         RARCH_LOG("[connect] %s\n", pad_map[i].name);
         RARCH_LOG("[connect] VID: Expected: %04x got: %04x\n",
                   pad_map[i].vid, vid);
         RARCH_LOG("[connect] PID: Expected: %04x got: %04x\n",
                   pad_map[i].pid, pid);
#endif

         if (name_match || (pad_map[i].vid == vid && pad_map[i].pid == pid))
         {
            s->iface      = pad_map[i].iface;
            s->data       = s->iface->init(data, pad, driver);
            s->connected  = true;
#if 0
            RARCH_LOG("%s found \n", pad_map[i].name);
#endif
            break;
         }
#if 0
         else
         {
            RARCH_LOG("%s not found \n", pad_map[i].name);
         }
#endif
      }

      /* We failed to find a matching pad,
       * set up one without an interface */
      if (!s->connected)
      {
         s->iface = NULL;
         s->data = data;
         s->connected = true;
      }
   }

   return pad;
}

void pad_connection_pad_deinit(joypad_connection_t *joyconn, uint32_t pad)
{
   if (!joyconn || !joyconn->connected)
       return;

   if (joyconn->iface)
   {
      joyconn->iface->set_rumble(joyconn->data, RETRO_RUMBLE_STRONG, 0);
      joyconn->iface->set_rumble(joyconn->data, RETRO_RUMBLE_WEAK, 0);

      if (joyconn->iface->deinit)
         joyconn->iface->deinit(joyconn->data);
   }

   joyconn->iface     = NULL;
   joyconn->connected = false;
}

void pad_connection_packet(joypad_connection_t *joyconn, uint32_t pad,
      uint8_t* data, uint32_t length)
{
   if (!joyconn || !joyconn->connected)
       return;
   if (joyconn->iface && joyconn->data && joyconn->iface->packet_handler)
      joyconn->iface->packet_handler(joyconn->data, data, length);
}

void pad_connection_get_buttons(joypad_connection_t *joyconn,
      unsigned pad, input_bits_t *state)
{
	if (joyconn && joyconn->iface)
		joyconn->iface->get_buttons(joyconn->data, state);
   else
		BIT256_CLEAR_ALL_PTR( state );
}

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i)
{
   if (!joyconn || !joyconn->iface)
      return 0;
   return joyconn->iface->get_axis(joyconn->data, i);
}

bool pad_connection_has_interface(joypad_connection_t *joyconn, unsigned pad)
{
   if (     joyconn && pad < MAX_USERS
         && joyconn[pad].connected
         && joyconn[pad].iface)
      return true;
   return false;
}

void pad_connection_destroy(joypad_connection_t *joyconn)
{
   unsigned i;

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

   joyconn->iface->set_rumble(joyconn->data, effect, strength);
   return true;
}

const char* pad_connection_get_name(joypad_connection_t *joyconn, unsigned pad)
{
   if (!joyconn || !joyconn->iface || !joyconn->iface->get_name)
      return NULL;
   return joyconn->iface->get_name(joyconn->data);
}
