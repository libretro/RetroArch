/*  RetroArch - A frontend for libretro.
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

#ifndef _INPUT_KEYBOARD_DEVICES_H
#define _INPUT_KEYBOARD_DEVICES_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* Which of the devices an operating system reports are the keyboards
 * on the desk, and which are the mice.
 *
 * What a system calls a keyboard is anything that can send keys. One
 * keyboard is often two or three such devices - its media keys, its
 * extra-key interface - and a mouse or a headset whose buttons send
 * keys is one, and so is the power button. Listed as the system has
 * them, a desk with one keyboard shows half a dozen, and a port given
 * "a keyboard" gets one part of one.
 *
 * An input driver that can tell its devices apart describes each
 * device that sends keys, and each pointer, with an input_kbdev_t and
 * calls input_kbdev_group(). What comes back in .group is the
 * keyboard, numbered from 0 in the order the devices were given, that
 * the device is part of - or INPUT_KBDEV_NONE:
 *
 * - devices with the same .key are one device on the desk, and their
 *   key-sending parts are one keyboard;
 * - it is listed if one of its parts is a keyboard in its own right
 *   (.keyboard) - a volume knob or a power button alone is not;
 * - unless it is also a pointer and none of its parts is a USB boot
 *   keyboard: that is a mouse whose buttons can send keys. Where it
 *   is not known whether a part is a boot keyboard (.boot < 0) the
 *   benefit of the doubt is the device's, and it is listed;
 * - a .remote device, one with nothing on the desk behind it, is part
 *   of nothing.
 *
 * Keys from a device that is part of no keyboard are still keys: the
 * driver goes on feeding them to the one state every keyboard feeds.
 *
 * The mice are the other side of the same coin: what a system calls a
 * mouse is anything that can send pointer events, and a keyboard
 * whose macros can move the pointer is one. input_kbdev_mice() says
 * of each pointer whether it is a mouse on the desk (.mouse):
 *
 * - not if nothing on the desk is behind it (.remote);
 * - not if it is part of a device that has a USB boot keyboard and no
 *   part of which is, or may be, a boot mouse: that is a keyboard
 *   that can send pointer events;
 * - otherwise it is. A keyboard and a mouse on one receiver are both
 *   what they are, and so is a keyboard with a pointer built in of
 *   which it is not known what its parts are.
 *
 * A driver numbers the mice on the desk first, so that the first is a
 * mouse, and leaves the rest out of the menu.
 *
 * No allocation, and nothing of any driver's or system's in here, so
 * that samples/input/keyboard_devices can run it on its own. */

#define INPUT_KBDEV_NONE 0xFF

typedef struct
{
   /* What tells one device on the desk from another, the same for
    * every part of it: where it is plugged in, the system's own id
    * for the physical device, its address. "" if not known, and then
    * the device is taken to be one on its own. */
   char    key[64];
   bool    keyboard; /* a keyboard in its own right: it has the keys of one */
   bool    pointer;  /* a mouse or other pointer, not something that sends keys */
   bool    remote;   /* nothing on the desk is behind it */
   /* its USB interface says "boot keyboard": 1 yes, 0 no, -1 not known */
   int8_t  boot;
   /* and "boot mouse", the same way */
   int8_t  boot_mouse;
   /* out: the keyboard it is part of, or INPUT_KBDEV_NONE */
   uint8_t group;
   /* out, of a pointer: it is a mouse on the desk */
   bool    mouse;
} input_kbdev_t;

/* Groups @devs; returns how many keyboards there are, at most @max. */
static INLINE unsigned input_kbdev_group(input_kbdev_t *devs, unsigned n,
      unsigned max)
{
   unsigned i, j, groups = 0;

   for (i = 0; i < n; i++)
      devs[i].group = INPUT_KBDEV_NONE;

   for (i = 0; i < n; i++)
   {
      bool keyboard  = false;
      bool pointer   = false;
      bool none_boot = true;

      /* the first key-sending part of a device decides for all of it */
      if (devs[i].pointer || devs[i].remote)
         continue;
      for (j = 0; j < i; j++)
         if (     !devs[j].pointer && !devs[j].remote
               && devs[i].key[0] && !strcmp(devs[i].key, devs[j].key))
            break;
      if (j < i)
      {
         devs[i].group = devs[j].group;
         continue;
      }

      for (j = 0; j < n; j++)
      {
         if (devs[j].remote)
            continue;
         if (j != i && !(devs[i].key[0] && !strcmp(devs[i].key, devs[j].key)))
            continue;
         if (devs[j].pointer)
            pointer = true;
         else
         {
            if (devs[j].keyboard)
               keyboard = true;
            if (devs[j].boot != 0)
               none_boot = false;
         }
      }

      if (!keyboard || (pointer && none_boot) || groups >= max)
         continue;
      devs[i].group = (uint8_t)groups++;
   }
   return groups;
}

/* Says of each pointer in @devs whether it is a mouse on the desk
 * (.mouse); returns how many are. */
static INLINE unsigned input_kbdev_mice(input_kbdev_t *devs, unsigned n)
{
   unsigned i, j, mice = 0;

   for (i = 0; i < n; i++)
   {
      bool boot_keyboard = false;
      bool maybe_mouse   = false;

      devs[i].mouse = false;
      if (!devs[i].pointer || devs[i].remote)
         continue;

      /* the device it is part of: has it a boot keyboard, and is any
       * pointer part of it a boot mouse, or not known not to be? */
      for (j = 0; j < n; j++)
      {
         if (devs[j].remote)
            continue;
         if (j != i && !(devs[i].key[0] && !strcmp(devs[i].key, devs[j].key)))
            continue;
         if (devs[j].pointer)
         {
            if (devs[j].boot_mouse != 0)
               maybe_mouse = true;
         }
         else if (devs[j].boot == 1)
            boot_keyboard = true;
      }

      if (boot_keyboard && !maybe_mouse)
         continue;
      devs[i].mouse = true;
      mice++;
   }
   return mice;
}

#endif
