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

#ifndef _SONY_PAD_OUTPUT_H
#define _SONY_PAD_OUTPUT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <encodings/crc32.h>

/* The output reports that make a DualShock 4 or a DualSense rumble.
 *
 * For a joypad driver that reads these pads as plain HID - Windows raw
 * input does - and so has their sticks and buttons and nothing else:
 * rumble on them is an output report in the pad's own format, which
 * differs between the two pads and, for each, between USB and
 * Bluetooth. This builds the report; writing it to the device is the
 * driver's.
 *
 * Only the motors are asked for. Every report carries flags that say
 * which of its fields the pad is to act on, and only the rumble flags
 * are set, so the light bar, the player lights, the triggers and the
 * audio stay as they are.
 *
 * No allocation and nothing of any system's, so that
 * samples/input/sony_pad_output can check it on its own; it needs
 * libretro-common's CRC-32 linked in. */

enum sony_pad_model
{
   SONY_PAD_NONE = 0,
   SONY_PAD_DS4,       /* DualShock 4, both revisions, and its USB dongle */
   SONY_PAD_DUALSENSE  /* DualSense and DualSense Edge */
};

#define SONY_PAD_REPORT_MAX 78

/* Which pad a USB vendor and product id is, or SONY_PAD_NONE. */
static INLINE enum sony_pad_model sony_pad_model(uint16_t vid, uint16_t pid)
{
   if (vid != 0x054C)
      return SONY_PAD_NONE;
   switch (pid)
   {
      case 0x05C4: /* DualShock 4 */
      case 0x09CC: /* DualShock 4, second revision */
      case 0x0BA0: /* DualShock 4 USB wireless adaptor */
         return SONY_PAD_DS4;
      case 0x0CE6: /* DualSense */
      case 0x0DF2: /* DualSense Edge */
         return SONY_PAD_DUALSENSE;
   }
   return SONY_PAD_NONE;
}

/* A DualSense Edge has the newer way of driving its motors from the
 * start; a DualSense has it from firmware 2.21 on (the version is in
 * its feature report 0x20) and the older way before that. */
static INLINE bool sony_pad_dualsense_is_edge(uint16_t pid)
{
   return pid == 0x0DF2;
}

/* The CRC a Bluetooth report ends with: CRC-32, libretro-common's,
 * over one byte that is not sent (0xA2, the transport's header for an
 * output report) and then the report up to the CRC. */
static INLINE uint32_t sony_pad_crc32(const uint8_t *data, size_t len)
{
   static const uint8_t header = 0xA2;
   return encoding_crc32(encoding_crc32(0, &header, 1), data, len);
}

/* Builds the report that sets the two motors: @strong the heavy,
 * low-frequency one (in the left grip) and @weak the light one, 0 to
 * 255 each. @vibration_v2 is for a DualSense alone; see above.
 * Returns the report's length - 32 or 48 over USB, 78 over Bluetooth
 * - or 0 for a pad it has no report for or a buffer too small. A
 * driver whose system wants output reports of a fixed length pads
 * with zeros. */
static INLINE size_t sony_pad_rumble_report(uint8_t *buf, size_t cap,
      enum sony_pad_model model, bool bluetooth, bool vibration_v2,
      uint8_t strong, uint8_t weak)
{
   size_t len = 0, at = 0;
   uint32_t crc;

   if (cap < SONY_PAD_REPORT_MAX)
      return 0;
   memset(buf, 0, cap);

   switch (model)
   {
      case SONY_PAD_DS4:
         if (bluetooth)
         {
            buf[0]     = 0x11;
            buf[1]     = 0xC0; /* an HID report, with a CRC */
            buf[3]     = 0x01; /* act on: the motors */
            at         = 6;
            len        = 78;
         }
         else
         {
            buf[0]     = 0x05;
            buf[1]     = 0x01; /* act on: the motors */
            at         = 4;
            len        = 32;
         }
         buf[at]       = weak;   /* right motor */
         buf[at + 1]   = strong; /* left motor */
         break;
      case SONY_PAD_DUALSENSE:
         if (bluetooth)
         {
            buf[0]     = 0x31;
            buf[1]     = 0x02;
            at         = 2;
            len        = 78;
         }
         else
         {
            buf[0]     = 0x02;
            at         = 1;
            len        = 48;
         }
         /* The first flag byte: the motors take the place of the
          * audio-driven haptics, and - the older way - are emulated
          * rumble. The newer way is asked for in the third flag byte,
          * 38 further on. */
         buf[at]       = 0x02;
         if (vibration_v2)
            buf[at + 38] = 0x04;
         else
            buf[at]   |= 0x01;
         buf[at + 2]   = weak;   /* right motor */
         buf[at + 3]   = strong; /* left motor */
         break;
      default:
         return 0;
   }

   if (bluetooth)
   {
      crc          = sony_pad_crc32(buf, len - 4);
      buf[len - 4] = (uint8_t)(crc);
      buf[len - 3] = (uint8_t)(crc >> 8);
      buf[len - 2] = (uint8_t)(crc >> 16);
      buf[len - 1] = (uint8_t)(crc >> 24);
   }
   return len;
}

#endif
