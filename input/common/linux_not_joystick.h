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

#ifndef _LINUX_NOT_JOYSTICK_H
#define _LINUX_NOT_JOYSTICK_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_inline.h>
#include <string/stdstring.h>

/* Linux (udev) tags as joysticks some devices that are not: keyboards, mice,
 * tablets and their wireless receivers whose HID descriptor declares
 * joystick-like axes. Taken at its word, such a device is given a port
 * of its own - the first free one, so often port 1, where it pushes
 * the real controller to port 2 and leaves the menu, which listens to
 * port 1 by default, without a controller.
 *
 * Two things identify them.
 *
 * The node's name. The kernel names each HID application collection
 * of a device after what it is, and "System Control" and "Consumer
 * Control" are the power/sleep and media keys of a keyboard or
 * receiver. No controller is either.
 *
 * The device's ids, for those whose main node is mistagged. The list
 * is the one SDL uses (initial_blacklist_devices in SDL_joystick.c),
 * which comes from the udev-joystick-blacklist project.
 *
 * Kept free of libudev and evdev types, so that every Linux joypad
 * driver can share the one rule set and samples/input/linux_not_joystick
 * can build it on its own. */
static const uint16_t linux_not_joystick_ids[][2] = {
   {0x045e, 0x009d}, {0x045e, 0x00b0}, {0x045e, 0x00b4}, {0x045e, 0x0730},
   {0x045e, 0x0745}, {0x045e, 0x0748}, {0x045e, 0x0750}, {0x045e, 0x0768},
   {0x045e, 0x0773}, {0x045e, 0x07a5}, {0x045e, 0x07b2}, {0x045e, 0x0800},
   {0x046d, 0xc30a}, {0x04d9, 0x8008}, {0x04d9, 0x8009}, {0x04d9, 0xa0df},
   {0x04d9, 0xa292}, {0x04d9, 0xa293}, {0x04f2, 0xa13c}, {0x056a, 0x0010},
   {0x056a, 0x0011}, {0x056a, 0x0012}, {0x056a, 0x0013}, {0x056a, 0x0014},
   {0x056a, 0x0015}, {0x056a, 0x0016}, {0x056a, 0x0017}, {0x056a, 0x0018},
   {0x056a, 0x0019}, {0x056a, 0x00d1}, {0x056a, 0x030e}, {0x09da, 0x054f},
   {0x09da, 0x1410}, {0x09da, 0x3043}, {0x09da, 0x31b5}, {0x09da, 0x3997},
   {0x09da, 0x3f8b}, {0x09da, 0x51f4}, {0x09da, 0x5589}, {0x09da, 0x7b22},
   {0x09da, 0x7f2d}, {0x09da, 0x8090}, {0x09da, 0x9033}, {0x09da, 0x9066},
   {0x09da, 0x9090}, {0x09da, 0x90c0}, {0x09da, 0xf012}, {0x09da, 0xf32a},
   {0x09da, 0xf613}, {0x09da, 0xf624}, {0x0e6f, 0x018a}, {0x1532, 0x0266},
   {0x1532, 0x0282}, {0x1b1c, 0x1b3c}, {0x1d57, 0xad03}, {0x1e7d, 0x2e4a},
   {0x20a0, 0x422d}, {0x20d6, 0x0002}, {0x2516, 0x001f}, {0x2516, 0x0028},
   {0x256c, 0x006d}, {0x256c, 0x006e}, {0x26ce, 0x01a2}, {0x3297, 0x1969},
   {0x3434, 0x0121}, {0x3434, 0x0163}, {0x3434, 0x0211}, {0x3434, 0x02a0},
   {0x3434, 0x0353}, {0x3434, 0xd030}
};

static INLINE bool linux_input_is_not_joystick(const char *name,
      uint16_t vid, uint16_t pid)
{
   size_t i;

   if (     string_ends_with(name, " System Control")
         || string_ends_with(name, " Consumer Control"))
      return true;

   for (i = 0; i < sizeof(linux_not_joystick_ids)
         / sizeof(linux_not_joystick_ids[0]); i++)
   {
      if (     linux_not_joystick_ids[i][0] == vid
            && linux_not_joystick_ids[i][1] == pid)
         return true;
   }

   return false;
}

#endif
