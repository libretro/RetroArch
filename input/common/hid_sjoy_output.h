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

#ifndef _HID_SJOY_OUTPUT_H
#define _HID_SJOY_OUTPUT_H

#include <stdint.h>
#include <string.h>

#include <windows.h>
#include <hidsdi.h>

#include <boolean.h>
#include <retro_inline.h>

/* The rumble of WiseGroup's PlayStation-to-USB adapters - the MP-8866
 * Dual USB Joypad, the SmartJoy PLUS, the Super Joy Box 3 - on
 * Windows' own HID driver, which drives none of it.
 *
 * As Linux's hid-sjoy has it: the first three values of the output
 * report that goes with the pad's input are on (1), the small motor
 * (on or off) and the large motor's force (0 to 255). They are one
 * field of three, or three fields; Windows' parser places them, so
 * nothing here knows the report's bytes. A two-port adapter is two
 * devices to Windows, a report each. */

typedef NTSTATUS (WINAPI *hid_sjoy_value_caps_t)(HIDP_REPORT_TYPE,
      PHIDP_VALUE_CAPS, PUSHORT, PHIDP_PREPARSED_DATA);
typedef NTSTATUS (WINAPI *hid_sjoy_set_value_t)(HIDP_REPORT_TYPE, USAGE,
      USHORT, USAGE, ULONG, PHIDP_PREPARSED_DATA, PCHAR, ULONG);
typedef NTSTATUS (WINAPI *hid_sjoy_set_array_t)(HIDP_REPORT_TYPE, USAGE,
      USHORT, USAGE, PCHAR, USHORT, PHIDP_PREPARSED_DATA, PCHAR, ULONG);

typedef struct
{
   USAGE  page;
   USAGE  usage;
   USHORT link;
   USHORT count;     /* values in the field */
   USHORT bits;
} hid_sjoy_field_t;

typedef struct
{
   hid_sjoy_field_t field[3];
   uint8_t fields;   /* 0: no rumble */
} hid_sjoy_output_t;

static INLINE bool hid_sjoy_rumbles(uint16_t vid, uint16_t pid)
{
   return vid == 0x0925 && (pid == 0x8866 || pid == 0x0005 || pid == 0x8888);
}

/* The fields that make the report's first three values, from the
 * @n output value capabilities the device has; false where they do
 * not make three. */
static INLINE bool hid_sjoy_output_setup(hid_sjoy_output_t *o,
      PHIDP_PREPARSED_DATA pp, USHORT n, hid_sjoy_value_caps_t caps)
{
   HIDP_VALUE_CAPS c[8];
   unsigned have = 0;
   USHORT i;

   o->fields = 0;
   if (!caps || !n)
      return false;
   if (n > 8)
      n = 8;
   if (caps(HidP_Output, c, &n, pp) != HIDP_STATUS_SUCCESS)
      return false;
   for (i = 0; i < n && have < 3; i++)
   {
      hid_sjoy_field_t *f = &o->field[o->fields++];
      f->page  = c[i].UsagePage;
      f->usage = c[i].IsRange ? c[i].Range.UsageMin : c[i].NotRange.Usage;
      f->link  = c[i].LinkCollection;
      f->count = c[i].ReportCount ? c[i].ReportCount : 1;
      f->bits  = c[i].BitSize;
      /* a field of several is written as bytes, a value each */
      if (f->count > 1 && (f->bits != 8 || f->count > 8))
         break;
      have    += f->count;
   }
   if (have < 3)
      o->fields = 0;
   return o->fields != 0;
}

/* The report, @len bytes - the longest output report the device has,
 * as Windows takes a write - for the large motor @strong and the small
 * one @weak, 0 to 255; false where Windows' parser would not place
 * them. */
static INLINE bool hid_sjoy_output_report(const hid_sjoy_output_t *o,
      PHIDP_PREPARSED_DATA pp, uint8_t *r, ULONG len,
      uint8_t strong, uint8_t weak,
      hid_sjoy_set_value_t set_value, hid_sjoy_set_array_t set_array)
{
   ULONG v[3];
   unsigned f, at = 0;

   if (!o->fields || !len || !set_value || !set_array)
      return false;
   v[0] = 1;
   v[1] = weak ? 1 : 0;
   v[2] = strong;
   memset(r, 0, len);
   for (f = 0; f < o->fields && at < 3; f++)
   {
      const hid_sjoy_field_t *fd = &o->field[f];
      if (fd->count == 1)
      {
         if (set_value(HidP_Output, fd->page, fd->link, fd->usage,
                  v[at++], pp, (PCHAR)r, len) != HIDP_STATUS_SUCCESS)
            return false;
      }
      else
      {
         char a[8];
         unsigned k;
         memset(a, 0, sizeof(a));
         for (k = 0; k < fd->count && at < 3; k++)
            a[k] = (char)v[at++];
         if (set_array(HidP_Output, fd->page, fd->link, fd->usage,
                  a, fd->count, pp, (PCHAR)r, len) != HIDP_STATUS_SUCCESS)
            return false;
      }
   }
   return true;
}

#endif
