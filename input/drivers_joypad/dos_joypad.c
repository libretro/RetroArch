/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <pc.h>
#include <dos.h>
#include <go32.h>
#include <dpmi.h>
#include <sys/segments.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#include "../input_joypad_driver.h"
#include "../input_driver.h"
#include "../input_config.h"
#include "../../tasks/tasks_internal.h"

#define MAX_PADS 1

#define END_FUNC(x) static void x##_End() { }

#define LOCK_VAR(x) LockData((void*)&x, sizeof(x))
#define LOCK_FUNC(x) LockCode(x, (int)x##_End - (int)x)

static uint64_t dos_key_state[MAX_PADS];

static unsigned char normal_keys[256];
static unsigned char extended_keys[256];

static _go32_dpmi_seginfo old_kbd_int;
static _go32_dpmi_seginfo kbd_int;

int LockData(void *a, int size)
{
   uint32_t baseaddr;
   __dpmi_meminfo region;

   if (__dpmi_get_segment_base_address(_my_ds(), &baseaddr) == -1)
      return -1;

   region.handle = 0;
   region.size = size;
   region.address = baseaddr + (uint32_t)a;

   if (__dpmi_lock_linear_region(&region) == -1)
    return -1;

   return 0;
}

int LockCode(void *a, int size)
{
   uint32_t baseaddr;
   __dpmi_meminfo region;

   if (__dpmi_get_segment_base_address(_my_cs(), &baseaddr) == -1)
      return (-1);

   region.handle = 0;
   region.size = size;
   region.address = baseaddr + (uint32_t)a;

   if (__dpmi_lock_linear_region(&region) == -1)
      return (-1);

   return 0;
}

static void keyb_int(void)
{
   static unsigned char buffer;
   unsigned char rawcode;
   unsigned char make_break;
   int scancode;

   rawcode = inp(0x60); /* read scancode from keyboard controller */
   make_break = !(rawcode & 0x80); /* bit 7: 0 = make, 1 = break */
   scancode = rawcode & 0x7F;

   if (buffer == 0xE0)
   {
      /* second byte of an extended key */
      if (scancode < 0x60)
      {
         extended_keys[scancode] = make_break;
      }

      buffer = 0;
   }
   else if (buffer >= 0xE1 && buffer <= 0xE2)
   {
      buffer = 0; /* ingore these extended keys */
   }
   else if (rawcode >= 0xE0 && rawcode <= 0xE2)
   {
      buffer = rawcode; /* first byte of an extended key */
   }
   else if (scancode < 0x60)
   {
      normal_keys[scancode] = make_break;
   }

   outp(0x20, 0x20); /* must send EOI to finish interrupt */
}
END_FUNC(keyb_int)

static void hook_keyb_int(void)
{
   _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd_int);

   memset(&kbd_int, 0, sizeof(kbd_int));

   LOCK_FUNC(keyb_int);
   LOCK_VAR(normal_keys);
   LOCK_VAR(extended_keys);

   kbd_int.pm_selector = _go32_my_cs();
   kbd_int.pm_offset = (uint32_t)&keyb_int;

   _go32_dpmi_allocate_iret_wrapper(&kbd_int);

   _go32_dpmi_set_protected_mode_interrupt_vector(9, &kbd_int);
}

static void unhook_keyb_int(void)
{
   if (old_kbd_int.pm_offset)
   {
      _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_kbd_int);
      _go32_dpmi_free_iret_wrapper(&kbd_int);

      memset(&old_kbd_int, 0, sizeof(old_kbd_int));
   }
}

static const char *dos_joypad_name(unsigned pad)
{
   return "DOS Controller";
}

static void dos_joypad_autodetect_add(unsigned autoconf_pad)
{
   if (!input_autoconfigure_connect(
         dos_joypad_name(autoconf_pad),
         NULL,
         dos_joypad.ident,
         autoconf_pad,
         0,
         0
         ))
      input_config_set_device_name(autoconf_pad, dos_joypad_name(autoconf_pad));
}

static bool dos_joypad_init(void *data)
{
   memset(dos_key_state, 0, sizeof(dos_key_state));

   hook_keyb_int();

   dos_joypad_autodetect_add(0);

   (void)data;

   return true;
}

static bool dos_joypad_button(unsigned port_num, uint16_t key)
{
   if (port_num >= MAX_PADS)
      return false;

   return (dos_key_state[port_num] & (UINT64_C(1) << key));
}

static uint64_t dos_joypad_get_buttons(unsigned port_num)
{
   if (port_num >= MAX_PADS)
      return 0;
   return dos_key_state[port_num];
}

static void dos_joypad_poll(void)
{
   uint32_t i;

   for (i = 0; i < MAX_PADS; i++)
   {
      uint64_t *cur_state = &dos_key_state[i];

      *cur_state = 0;
      *cur_state |= extended_keys[75] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT : 0;
      *cur_state |= extended_keys[77] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT : 0;
      *cur_state |= extended_keys[72] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP : 0;
      *cur_state |= extended_keys[80] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN : 0;
      *cur_state |= normal_keys[28] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START : 0; /* ENTER */
      *cur_state |= normal_keys[54] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT : 0; /* RSHIFT */
      *cur_state |= normal_keys[44] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B : 0; /* Z */
      *cur_state |= normal_keys[45] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A : 0; /* X */
      *cur_state |= normal_keys[30] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y : 0; /* A */
      *cur_state |= normal_keys[31] ? UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X : 0; /* S */
   }
}

static bool dos_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && dos_key_state[pad];
}

static int16_t dos_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   return 0;
}

static void dos_joypad_destroy(void)
{
   unhook_keyb_int();
}

input_device_driver_t dos_joypad = {
   dos_joypad_init,
   dos_joypad_query_pad,
   dos_joypad_destroy,
   dos_joypad_button,
   dos_joypad_get_buttons,
   dos_joypad_axis,
   dos_joypad_poll,
   NULL,
   dos_joypad_name,
   "dos",
};
