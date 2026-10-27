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

#ifndef _LINUX_PAD_MEMBERS_H
#define _LINUX_PAD_MEMBERS_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>

/* A pad's other nodes on Linux. A DualShock 4, a DualSense, a Wii
 * Remote reach the kernel as one HID device and come out as several
 * nodes beside the pad's: its motion sensors, its touchpad, and its
 * raw reports. They are one pad's members (input_group.h): what they
 * share is the HID device, and each has a role.
 *
 * The udev driver lists the HID device's nodes once and this decides,
 * from what each one is, which takes which role:
 *
 * - sensors: the node udev tags ID_INPUT_ACCELEROMETER; failing that,
 *   any other event node of the pad's that is not its touchpad, in the
 *   order listed - hid-wiimote's accelerometer is not tagged. The
 *   driver opens them in that order and keeps the first with axes.
 * - touchpad: the node tagged ID_INPUT_TOUCHPAD.
 * - reports: the hidraw node, for a pad whose evdev node lacks what
 *   its reports have (a DualShock 3's button pressures).
 *
 * No libudev, so samples/input/linux_pad_members builds it alone. */

#define LINUX_PAD_MEMBERS_MAX 8

typedef struct linux_pad_node
{
   uint64_t rdev;       /* the node's device number */
   bool     event;      /* an input event node */
   bool     hidraw;     /* a hidraw node */
   bool     accel;      /* tagged ID_INPUT_ACCELEROMETER */
   bool     touchpad;   /* tagged ID_INPUT_TOUCHPAD */
} linux_pad_node_t;

typedef struct linux_pad_members
{
   /* the nodes to try for the sensors, best first; -1 ends */
   int sensors[LINUX_PAD_MEMBERS_MAX + 1];
   int touchpad;        /* -1 for none */
   int reports;         /* -1 for none */
} linux_pad_members_t;

/* The roles of @count nodes listed under the pad's HID device, the pad
 * being @own (its own node takes none). @want_reports where the pad
 * reads its hidraw node. */
static INLINE void linux_pad_members_choose(const linux_pad_node_t *nodes,
      size_t count, uint64_t own, bool want_reports, linux_pad_members_t *m)
{
   size_t i;
   unsigned n = 0;

   m->touchpad = -1;
   m->reports  = -1;
   if (count > LINUX_PAD_MEMBERS_MAX)
      count = LINUX_PAD_MEMBERS_MAX;
   for (i = 0; i < count; i++)
   {
      const linux_pad_node_t *node = &nodes[i];
      if (node->rdev == own && node->event)
         continue;
      if (node->hidraw)
      {
         if (want_reports && m->reports < 0)
            m->reports = (int)i;
      }
      else if (node->event && node->touchpad)
      {
         if (m->touchpad < 0)
            m->touchpad = (int)i;
      }
      else if (node->event && node->accel)
         m->sensors[n++] = (int)i;
   }
   /* untagged ones after every tagged one */
   for (i = 0; i < count; i++)
   {
      const linux_pad_node_t *node = &nodes[i];
      if (     node->event && !node->hidraw && !node->accel
            && !node->touchpad && node->rdev != own)
         m->sensors[n++] = (int)i;
   }
   m->sensors[n] = -1;
}

#endif
