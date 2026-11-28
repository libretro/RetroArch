/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memdesc.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <memory/memdesc.h>

static size_t memdesc_add_bits_down(size_t n)
{
   n |= n >>  1;
   n |= n >>  2;
   n |= n >>  4;
   n |= n >>  8;
   n |= n >> 16;
   /* Only compiled on 64-bit targets; avoids a runtime branch per call */
#if SIZE_MAX > 0xFFFFFFFFUL
   n |= n >> 32;
#endif
   return n;
}

static size_t memdesc_inflate(size_t addr, size_t mask)
{
   while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;

      /* to put in an 1 bit instead, OR in tmp+1 */
      addr       = ((addr & ~tmp) << 1) | (addr & tmp);
      mask       = mask & (mask - 1);
   }

   return addr;
}

static size_t memdesc_reduce(size_t addr, size_t mask)
{
   while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;
      addr       = (addr & tmp) | ((addr >> 1) & ~tmp);
      mask       = (mask & (mask - 1)) >> 1;
   }

   return addr;
}

static size_t memdesc_highest_bit(size_t n)
{
   n = memdesc_add_bits_down(n);
   return n ^ (n >> 1);
}

bool memdesc_preprocess(
      struct retro_memory_descriptor *first, unsigned count)
{
   size_t                           top_addr = 1;
   struct retro_memory_descriptor *desc      = NULL;
   const struct retro_memory_descriptor *end = NULL;
   size_t                  highest_reachable = 0;

   if (!count)
      return true;

   end = first + count;

   for (desc = first; desc < end; desc++)
   {
      if (desc->select != 0)
         top_addr |= desc->select;
      else
         top_addr |= desc->start + desc->len - 1;
   }

   top_addr = memdesc_add_bits_down(top_addr);

   for (desc = first; desc < end; desc++)
   {
      if (desc->select == 0)
      {
         if (desc->len == 0)
            return false;

         if ((desc->len & (desc->len - 1)) != 0)
            return false;

         desc->select = top_addr
            & ~memdesc_inflate(memdesc_add_bits_down(desc->len - 1),
               desc->disconnect);
      }

      if (desc->len == 0)
         desc->len = memdesc_add_bits_down(
               memdesc_reduce(top_addr & ~desc->select,
                  desc->disconnect)) + 1;

      if (desc->start & ~desc->select)
         return false;

      highest_reachable = memdesc_inflate(desc->len - 1,
            desc->disconnect);

      /* Disconnect unselected bits that are too high to ever
       * index into the core's buffer. Higher addresses will
       * repeat / mirror the buffer as long as they match select */
      while (memdesc_highest_bit(top_addr
               & ~desc->select
               & ~desc->disconnect) >
                memdesc_highest_bit(highest_reachable))
         desc->disconnect |= memdesc_highest_bit(top_addr
               & ~desc->select
               & ~desc->disconnect);
   }

   return true;
}

const struct retro_memory_descriptor *memdesc_find(
      const struct retro_memory_descriptor *descs, unsigned count,
      size_t address, size_t *offset)
{
   const struct retro_memory_descriptor* desc = descs;
   const struct retro_memory_descriptor* end  = NULL;

   if (!count)
      return NULL;

   end = desc + count;

   for (; desc < end; desc++)
   {
      if (desc->select == 0)
      {
         /* if select is 0, attempt to explicitly match the address */
         if (address >= desc->start && address < desc->start + desc->len)
         {
            *offset = address - desc->start;
            return desc;
         }
      }
      else
      {
         /* otherwise, attempt to match the address by matching the select bits */
         if (((desc->start ^ address) & desc->select) == 0)
         {
            /* adjust the address to the start of the descriptor */
            size_t desc_offset = address - desc->start;

            desc_offset = memdesc_reduce(desc_offset, desc->disconnect);

            /* we've calculated the actual offset of the data within the descriptor */
            *offset = desc_offset;

            /* sanity check - make sure the descriptor is large enough to hold the target address */
            if (desc_offset < desc->len)
               return desc;
         }
      }
   }

   return NULL;
}
