/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

#ifndef _HID_REPORT_DESC_H
#define _HID_REPORT_DESC_H

/* A gamepad's input reports, read from the layout its HID report
 * descriptor gives, for a HID driver whose system hands over the
 * descriptor but does not parse it (hidraw).
 *
 * The pad is numbered as the Mac's driver numbers such a pad, so a
 * profile made there binds here too: its buttons in usage order, its
 * axes X, Y, Rx, Ry, Z, Rz, then rudder, throttle, steering,
 * accelerator and brake - an axis named twice taking the first free
 * one - and its first hat as the D-pad.
 *
 * Only the gamepad, joystick or multi-axis controller collections are
 * read. The layout is made once, when the pad arrives; a report is
 * decoded into a fixed state with nothing allocated. No system's
 * types, so samples/input/hid_report_desc can check it on its own. */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

#define HID_DESC_FIELDS    64
#define HID_DESC_AXES      11
#define HID_DESC_BUTTONS   32

#define HID_DESC_BUTTON    1
#define HID_DESC_AXIS      2
#define HID_DESC_HAT       3

#define HID_DESC_AXIS_LATER 0xff

typedef struct
{
   uint16_t bit;        /* where it starts, past the report's ID */
   uint8_t  size;       /* its width in bits, 1 to 32 */
   uint8_t  id;         /* the report it is in; 0 where there are none */
   uint8_t  kind;       /* HID_DESC_* */
   uint8_t  index;      /* the button or axis it is */
   int32_t  min;
   int32_t  max;
} hid_desc_field_t;

typedef struct
{
   bool ids;            /* reports start with their ID */
   unsigned fields;
   hid_desc_field_t field[HID_DESC_FIELDS];
} hid_desc_layout_t;

typedef struct
{
   uint32_t buttons;
   int16_t  axes[HID_DESC_AXES];
   int8_t   hat_x;      /* -1 left, 1 right */
   int8_t   hat_y;      /* -1 up, 1 down */
} hid_desc_state_t;

/* The Mac's order of axes: the index, or -1. */
static INLINE int hid_desc_axis_index(unsigned page, unsigned usage)
{
   static const uint16_t gd[6]  = { 0x30, 0x31, 0x33, 0x34, 0x32, 0x35 };
   static const uint16_t sim[5] = { 0xba, 0xbb, 0xc8, 0xc4, 0xc5 };
   int i;
   if (page == 0x01)
   {
      for (i = 0; i < 6; i++)
         if (gd[i] == usage)
            return i;
   }
   else if (page == 0x02)
   {
      for (i = 0; i < 5; i++)
         if (sim[i] == usage)
            return 6 + i;
   }
   return -1;
}

static INLINE int32_t hid_desc_signed(uint32_t v, unsigned bytes)
{
   if (bytes == 1)
      return (int32_t)(int8_t)v;
   if (bytes == 2)
      return (int32_t)(int16_t)v;
   return (int32_t)v;
}

/* Builds @layout from the descriptor. False when it has no gamepad,
 * joystick or multi-axis collection with anything this reads. */
static INLINE bool hid_desc_parse(hid_desc_layout_t *layout,
      const uint8_t *d, size_t len)
{
   /* the global state, with one level of push */
   uint32_t page = 0, rsize = 0, rcount = 0, rid = 0;
   int32_t lmin = 0, lmax = 0;
   uint32_t s_page = 0, s_rsize = 0, s_rcount = 0, s_rid = 0;
   int32_t s_lmin = 0, s_lmax = 0;
   /* the local state */
   uint32_t usage[16];
   unsigned usages = 0;
   uint32_t umin = 0, umax = 0;
   bool range = false;
   /* each report's next bit, by ID */
   uint16_t next_bit[256];
   /* collection depth, and the depth a pad's collection opened at */
   unsigned depth = 0, pad_depth = 0;
   bool in_pad = false, hat = false;
   bool axis_taken[HID_DESC_AXES];
   size_t at = 0;
   unsigned i;

   memset(layout, 0, sizeof(*layout));
   memset(next_bit, 0, sizeof(next_bit));
   for (i = 0; i < HID_DESC_AXES; i++)
      axis_taken[i] = false;

   while (at < len)
   {
      uint8_t prefix = d[at++];
      unsigned bytes, type, tag;
      uint32_t v = 0;

      if (prefix == 0xfe)
      {
         /* a long item: its data length, its tag, then the data */
         if (at + 2 > len)
            break;
         at += 2 + d[at];
         continue;
      }
      bytes = prefix & 3;
      if (bytes == 3)
         bytes = 4;
      type  = (prefix >> 2) & 3;
      tag   = prefix >> 4;
      if (at + bytes > len)
         break;
      for (i = 0; i < bytes; i++)
         v |= (uint32_t)d[at + i] << (8 * i);
      at += bytes;

      if (type == 1) /* global */
      {
         switch (tag)
         {
            case 0:  page   = v; break;
            case 1:  lmin   = hid_desc_signed(v, bytes); break;
            case 2:  lmax   = hid_desc_signed(v, bytes); break;
            case 7:  rsize  = v; break;
            case 8:  rid    = v & 0xff; layout->ids = true; break;
            case 9:  rcount = v; break;
            case 10:
               s_page = page; s_rsize = rsize; s_rcount = rcount;
               s_rid  = rid;  s_lmin  = lmin;  s_lmax   = lmax;
               break;
            case 11:
               page = s_page; rsize = s_rsize; rcount = s_rcount;
               rid  = s_rid;  lmin  = s_lmin;  lmax   = s_lmax;
               break;
         }
         continue;
      }
      if (type == 2) /* local */
      {
         /* a four-byte usage carries its page in the high half */
         if (tag == 0 && usages < 16)
            usage[usages++] = (bytes == 4) ? v : ((page << 16) | v);
         else if (tag == 1)
         {
            umin  = (bytes == 4) ? v : ((page << 16) | v);
            range = true;
         }
         else if (tag == 2)
         {
            umax  = (bytes == 4) ? v : ((page << 16) | v);
            range = true;
         }
         continue;
      }
      if (type != 0) /* reserved */
         continue;

      switch (tag)
      {
         case 10: /* collection */
            depth++;
            if (     !in_pad && v == 0x01 /* application */
                  && usages
                  && (usage[0] >> 16) == 0x01
                  && (   (usage[0] & 0xffff) == 0x04
                      || (usage[0] & 0xffff) == 0x05
                      || (usage[0] & 0xffff) == 0x08))
            {
               in_pad    = true;
               pad_depth = depth;
            }
            break;
         case 12: /* end collection */
            if (in_pad && depth == pad_depth)
               in_pad = false;
            if (depth)
               depth--;
            break;
         case 8: /* input */
         {
            unsigned n;
            uint16_t *bit = &next_bit[rid];
            bool constant = (v & 0x01) != 0;
            bool variable = (v & 0x02) != 0;

            for (n = 0; n < rcount; n++)
            {
               uint32_t u;
               unsigned upage, uid;
               int kind = 0, index = 0;

               if (range && umax >= umin)
                  u = umin + n;
               else if (usages)
                  u = usage[n < usages ? n : usages - 1];
               else
                  u = 0;
               if (range && umax >= umin && u > umax)
                  u = umax;
               upage = u >> 16;
               uid   = u & 0xffff;

               if (     in_pad && !constant && variable
                     && rsize >= 1 && rsize <= 32)
               {
                  if (upage == 0x09 && rsize == 1 && uid >= 1
                        && uid <= HID_DESC_BUTTONS)
                  {
                     kind  = HID_DESC_BUTTON;
                     index = (int)uid - 1;
                  }
                  else if (upage == 0x01 && uid == 0x39 && !hat)
                  {
                     kind  = HID_DESC_HAT;
                     hat   = true;
                  }
                  else if ((index = hid_desc_axis_index(upage, uid)) >= 0)
                  {
                     /* one named again is given a free axis once
                      * every other has its own, as on the Mac */
                     if (axis_taken[index])
                        index = HID_DESC_AXIS_LATER;
                     else
                        axis_taken[index] = true;
                     kind = HID_DESC_AXIS;
                  }
               }
               if (kind && layout->fields < HID_DESC_FIELDS)
               {
                  hid_desc_field_t *f = &layout->field[layout->fields++];
                  f->bit   = *bit;
                  f->size  = (uint8_t)rsize;
                  f->id    = (uint8_t)rid;
                  f->kind  = (uint8_t)kind;
                  f->index = (uint8_t)index;
                  f->min   = lmin;
                  /* a range from nought is unsigned however its bytes
                   * read */
                  f->max   = (lmin >= 0 && lmax < 0 && rsize < 32)
                     ? (int32_t)((1UL << rsize) - 1) : lmax;
               }
               *bit = (uint16_t)(*bit + rsize);
            }
            break;
         }
         default: /* output and feature reports are not read */
            break;
      }
      /* a main item ends the local state */
      usages = 0;
      range  = false;
      umin   = umax = 0;
   }
   for (i = 0; i < layout->fields; i++)
   {
      hid_desc_field_t *f = &layout->field[i];
      unsigned a          = 0;
      if (f->kind != HID_DESC_AXIS || f->index != HID_DESC_AXIS_LATER)
         continue;
      while (a < HID_DESC_AXES && axis_taken[a])
         a++;
      if (a < HID_DESC_AXES)
      {
         axis_taken[a] = true;
         f->index      = (uint8_t)a;
      }
      else
         f->kind       = 0; /* read as nothing */
   }
   return layout->fields > 0;
}

static INLINE uint32_t hid_desc_bits(const uint8_t *data, size_t len,
      unsigned bit, unsigned size)
{
   uint32_t v = 0;
   unsigned i;
   for (i = 0; i < size; i++)
   {
      unsigned b = bit + i;
      if ((b >> 3) >= len)
         break;
      v |= (uint32_t)((data[b >> 3] >> (b & 7)) & 1) << i;
   }
   return v;
}

/* One report into @state. Fields of another report are left as they
 * were. */
static INLINE void hid_desc_decode(const hid_desc_layout_t *layout,
      const uint8_t *report, size_t len, hid_desc_state_t *state)
{
   /* eight ways from the top, clockwise; out of range is rest */
   static const int8_t hx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
   static const int8_t hy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
   const uint8_t *data = report;
   uint8_t id          = 0;
   unsigned i;

   if (layout->ids)
   {
      if (!len)
         return;
      id   = report[0];
      data = report + 1;
      len--;
   }
   for (i = 0; i < layout->fields; i++)
   {
      const hid_desc_field_t *f = &layout->field[i];
      uint32_t raw;
      int32_t v;

      if (f->id != id || ((unsigned)f->bit + f->size + 7) / 8 > len)
         continue;
      raw = hid_desc_bits(data, len, f->bit, f->size);
      v   = (int32_t)raw;
      if (f->min < 0 && f->size < 32 && (raw & (1UL << (f->size - 1))))
         v = (int32_t)(raw | ~((1UL << f->size) - 1));

      switch (f->kind)
      {
         case 0:
            break;
         case HID_DESC_BUTTON:
            if (raw)
               state->buttons |=  (1u << f->index);
            else
               state->buttons &= ~(1u << f->index);
            break;
         case HID_DESC_HAT:
         {
            int32_t way = v - f->min;
            if (f->max - f->min == 3)
               way *= 2;
            if (v < f->min || v > f->max || way < 0 || way > 7)
               state->hat_x = state->hat_y = 0;
            else
            {
               state->hat_x = hx[way];
               state->hat_y = hy[way];
            }
            break;
         }
         case HID_DESC_AXIS:
         {
            int64_t range = (int64_t)f->max - f->min;
            if (range <= 0)
               break;
            if (v < f->min)
               v = f->min;
            else if (v > f->max)
               v = f->max;
            state->axes[f->index] = (int16_t)(
                  (((int64_t)v - f->min) * 65534) / range - 32767);
            break;
         }
      }
   }
}

#endif
