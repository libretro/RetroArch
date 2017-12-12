/* Bra.c -- Converters for RISC code
   2010-04-16 : Igor Pavlov : Public domain */

#include <stdint.h>
#include "Bra.h"

size_t ARM_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding)
{
   size_t i;
   if (size < 4)
      return 0;
   size -= 4;
   ip += 8;
   for (i = 0; i <= size; i += 4)
   {
      if (data[i + 3] == 0xEB)
      {
         uint32_t dest;
         uint32_t src = ((uint32_t)data[i + 2] << 16) | ((uint32_t)data[i + 1] << 8) | (data[i + 0]);
         src <<= 2;
         if (encoding)
            dest = ip + (uint32_t)i + src;
         else
            dest = src - (ip + (uint32_t)i);
         dest >>= 2;
         data[i + 2] = (uint8_t)(dest >> 16);
         data[i + 1] = (uint8_t)(dest >> 8);
         data[i + 0] = (uint8_t)dest;
      }
   }
   return i;
}

size_t ARMT_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding)
{
   size_t i;
   if (size < 4)
      return 0;
   size -= 4;
   ip += 4;
   for (i = 0; i <= size; i += 2)
   {
      if ((data[i + 1] & 0xF8) == 0xF0 &&
            (data[i + 3] & 0xF8) == 0xF8)
      {
         uint32_t dest;
         uint32_t src =
            (((uint32_t)data[i + 1] & 0x7) << 19) |
            ((uint32_t)data[i + 0] << 11) |
            (((uint32_t)data[i + 3] & 0x7) << 8) |
            (data[i + 2]);

         src <<= 1;
         if (encoding)
            dest = ip + (uint32_t)i + src;
         else
            dest = src - (ip + (uint32_t)i);
         dest >>= 1;

         data[i + 1] = (uint8_t)(0xF0 | ((dest >> 19) & 0x7));
         data[i + 0] = (uint8_t)(dest >> 11);
         data[i + 3] = (uint8_t)(0xF8 | ((dest >> 8) & 0x7));
         data[i + 2] = (uint8_t)dest;
         i += 2;
      }
   }
   return i;
}

size_t PPC_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding)
{
   size_t i;
   if (size < 4)
      return 0;
   size -= 4;
   for (i = 0; i <= size; i += 4)
   {
      if ((data[i] >> 2) == 0x12 && (data[i + 3] & 3) == 1)
      {
         uint32_t src = ((uint32_t)(data[i + 0] & 3) << 24) |
            ((uint32_t)data[i + 1] << 16) |
            ((uint32_t)data[i + 2] << 8) |
            ((uint32_t)data[i + 3] & (~3));

         uint32_t dest;
         if (encoding)
            dest = ip + (uint32_t)i + src;
         else
            dest = src - (ip + (uint32_t)i);
         data[i + 0] = (uint8_t)(0x48 | ((dest >> 24) &  0x3));
         data[i + 1] = (uint8_t)(dest >> 16);
         data[i + 2] = (uint8_t)(dest >> 8);
         data[i + 3] &= 0x3;
         data[i + 3] |= dest;
      }
   }
   return i;
}

size_t SPARC_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding)
{
   uint32_t i;
   if (size < 4)
      return 0;
   size -= 4;
   for (i = 0; i <= size; i += 4)
   {
      if ((data[i] == 0x40 && (data[i + 1] & 0xC0) == 0x00) ||
            (data[i] == 0x7F && (data[i + 1] & 0xC0) == 0xC0))
      {
         uint32_t src =
            ((uint32_t)data[i + 0] << 24) |
            ((uint32_t)data[i + 1] << 16) |
            ((uint32_t)data[i + 2] << 8) |
            ((uint32_t)data[i + 3]);
         uint32_t dest;

         src <<= 2;
         if (encoding)
            dest = ip + i + src;
         else
            dest = src - (ip + i);
         dest >>= 2;

         dest = (((0 - ((dest >> 22) & 1)) << 22) & 0x3FFFFFFF) | (dest & 0x3FFFFF) | 0x40000000;

         data[i + 0] = (uint8_t)(dest >> 24);
         data[i + 1] = (uint8_t)(dest >> 16);
         data[i + 2] = (uint8_t)(dest >> 8);
         data[i + 3] = (uint8_t)dest;
      }
   }
   return i;
}
