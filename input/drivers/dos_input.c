/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <retro_miscellaneous.h>

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../drivers_keyboard/keyboard_event_dos.h"

#define MAX_KEYS LAST_KEYCODE + 1

#define END_FUNC(x) static void x##_End() { }

#define LOCK_VAR(x) LockData((void*)&x, sizeof(x))
#define LOCK_FUNC(x) LockCode(x, (int)x##_End - (int)x)

/* static globals for interrupt handler */
static uint16_t normal_keys[MAX_KEYS];
static _go32_dpmi_seginfo old_kbd_int;
static _go32_dpmi_seginfo kbd_int;

static int LockData(void *a, int size)
{
   uint32_t baseaddr;
   if (__dpmi_get_segment_base_address(_my_ds(), &baseaddr) != -1)
   {
      __dpmi_meminfo region;
      region.handle  = 0;
      region.size    = size;
      region.address = baseaddr + (uint32_t)a;
      if (__dpmi_lock_linear_region(&region) != -1)
         return 0;
   }
   return -1;
}

static int LockCode(void *a, int size)
{
   uint32_t baseaddr;
   if (__dpmi_get_segment_base_address(_my_cs(), &baseaddr) != -1)
   {
      __dpmi_meminfo region;
      region.handle  = 0;
      region.size    = size;
      region.address = baseaddr + (uint32_t)a;
      if (__dpmi_lock_linear_region(&region) != -1)
         return 0;
   }
   return -1;
}

static void keyb_int(void)
{
   unsigned char buffer = normal_keys[LAST_KEYCODE];
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
   {
      normal_keys[scancode] = make_break;
      buffer = 0;
   }
   normal_keys[LAST_KEYCODE] = buffer;
   outp(0x20, 0x20); /* must send EOI to finish interrupt */
}
END_FUNC(keyb_int)

/* TODO/FIXME - static globals */
static uint16_t dos_key_state[MAX_KEYS];

uint16_t *dos_keyboard_state_get(void)
{
   return dos_key_state;
}

static void dos_keyboard_free(void)
{
   unsigned j;

   for (j = 0; j < MAX_KEYS; j++)
      dos_key_state[j] = 0;
}

static int16_t dos_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   if (port < MAX_USERS)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            {
               unsigned i;
               int16_t ret = 0;

               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND && !keyboard_mapping_blocked; i++)
               {
                  if (binds[port][i].valid && binds[port][i].key && binds[port][i].key < RETROK_LAST)
                  {
                        if (dos_key_state[rarch_keysym_lut[(enum retro_key)binds[port][i].key]])
                           ret |= (1 << i);
                  }
               }

               return ret;
            }

            if (binds[port][id].valid)
            {
               if (  (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                      && (id < RARCH_BIND_LIST_END
                      && dos_key_state[rarch_keysym_lut[(enum retro_key)binds[port][id].key]])
                      && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                  )
                  return 1;
            }
            break;
         case RETRO_DEVICE_KEYBOARD:

            if (id && id < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id] & LAST_KEYCODE;
               return dos_key_state[sym];
            }
            break;
      }
   }

   return 0;
}

static void dos_input_poll(void *data)
{
   uint32_t key;
   uint16_t *cur_state = dos_keyboard_state_get();

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

static void dos_input_free_input(void *data)
{
   dos_keyboard_free();
   if (old_kbd_int.pm_offset)
   {
      _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_kbd_int);
      _go32_dpmi_free_iret_wrapper(&kbd_int);

      memset(&old_kbd_int, 0, sizeof(old_kbd_int));
   }

}

static void* dos_input_init(const char *joypad_driver)
{
   _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd_int);

   memset(&kbd_int,    0, sizeof(kbd_int));
   memset(normal_keys, 0, sizeof(normal_keys));

   LOCK_FUNC(keyb_int);
   LOCK_VAR(normal_keys);

   kbd_int.pm_selector = _go32_my_cs();
   kbd_int.pm_offset   = (uint32_t)&keyb_int;

   _go32_dpmi_allocate_iret_wrapper(&kbd_int);

   _go32_dpmi_set_protected_mode_interrupt_vector(9, &kbd_int);

   input_keymaps_init_keyboard_lut(rarch_key_map_dos);
   return (void*)-1;
}

static uint64_t dos_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD)
        | (1 << RETRO_DEVICE_KEYBOARD);
}

input_driver_t input_dos = {
   dos_input_init,
   dos_input_poll,
   dos_input_state,
   dos_input_free_input,
   NULL,
   NULL,
   dos_input_get_capabilities,
   "dos",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
