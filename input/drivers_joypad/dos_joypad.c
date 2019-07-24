/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#include "../../config.def.h"

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../../tasks/tasks_internal.h"
#include "../drivers_keyboard/keyboard_event_dos.h"

#define END_FUNC(x) static void x##_End() { }

#define LOCK_VAR(x) LockData((void*)&x, sizeof(x))
#define LOCK_FUNC(x) LockCode(x, (int)x##_End - (int)x)

static uint16_t normal_keys[LAST_KEYCODE + 1];

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
   static unsigned char buffer = 0;
   unsigned char rawcode       = inp(0x60);
   /* read scancode from keyboard controller */
   unsigned char make_break    = !(rawcode & 0x80);
   /* bit 7: 0 = make, 1 = break */
   int scancode                = rawcode & 0x7F;

   if (buffer == 0xE0)
   {
      /* second byte of an extended key */
      if (scancode < 0x60)
         normal_keys[scancode | (1 << 8)] = make_break;

      buffer = 0;
   }
   else if (buffer >= 0xE1 && buffer <= 0xE2)
      buffer = 0; /* ignore these extended keys */
   else if (rawcode >= 0xE0 && rawcode <= 0xE2)
      buffer = rawcode; /* first byte of an extended key */
   else if (scancode < 0x60)
      normal_keys[scancode] = make_break;

   outp(0x20, 0x20); /* must send EOI to finish interrupt */
}
END_FUNC(keyb_int)

static void hook_keyb_int(void)
{
   _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd_int);

   memset(&kbd_int, 0, sizeof(kbd_int));
   memset(normal_keys, 0, sizeof(normal_keys));

   LOCK_FUNC(keyb_int);
   LOCK_VAR(normal_keys);

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
   input_autoconfigure_connect(
         dos_joypad_name(autoconf_pad),
         NULL,
         dos_joypad.ident,
         autoconf_pad,
         0,
         0
         );
}

static bool dos_joypad_init(void *data)
{
   hook_keyb_int();

   dos_joypad_autodetect_add(0);

   (void)data;

   return true;
}

static bool dos_joypad_button(unsigned port_num, uint16_t key)
{
   uint16_t *buf = dos_keyboard_state_get(port_num);

   if (port_num >= DEFAULT_MAX_PADS)
      return false;

   switch (key)
   {
      case RETRO_DEVICE_ID_JOYPAD_A:
         return buf[DOSKEY_x];
      case RETRO_DEVICE_ID_JOYPAD_B:
         return buf[DOSKEY_z];
      case RETRO_DEVICE_ID_JOYPAD_X:
         return buf[DOSKEY_s];
      case RETRO_DEVICE_ID_JOYPAD_Y:
         return buf[DOSKEY_a];
      case RETRO_DEVICE_ID_JOYPAD_SELECT:
         return buf[DOSKEY_RSHIFT];
      case RETRO_DEVICE_ID_JOYPAD_START:
         return buf[DOSKEY_RETURN];
      case RETRO_DEVICE_ID_JOYPAD_UP:
         return buf[DOSKEY_UP];
      case RETRO_DEVICE_ID_JOYPAD_DOWN:
         return buf[DOSKEY_DOWN];
      case RETRO_DEVICE_ID_JOYPAD_LEFT:
         return buf[DOSKEY_LEFT];
      case RETRO_DEVICE_ID_JOYPAD_RIGHT:
         return buf[DOSKEY_RIGHT];
   }

   return false;
}

static void dos_joypad_poll(void)
{
   uint32_t i;

   for (i = 0; i <= DEFAULT_MAX_PADS; i++)
   {
      uint32_t key;
      uint16_t *cur_state = dos_keyboard_state_get(i);

      for (key = 0; key < LAST_KEYCODE; key++)
      {
         if (cur_state[key] != normal_keys[key])
         {
            unsigned code = input_keymaps_translate_keysym_to_rk(key);

            input_keyboard_event(normal_keys[key], code, code, 0, RETRO_DEVICE_KEYBOARD);
         }
      }

      memcpy(cur_state, normal_keys, sizeof(normal_keys));
   }
}

static bool dos_joypad_query_pad(unsigned pad)
{
   return (pad < MAX_USERS);
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
   NULL,
   dos_joypad_axis,
   dos_joypad_poll,
   NULL,
   dos_joypad_name,
   "dos",
};
