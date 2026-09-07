/* Self-contained CRC-32 (IEEE 802.3, poly 0xEDB88320) for elf2rpl.
 *
 * Provides a drop-in replacement for the single zlib symbol this tool
 * used (crc32), so elf2rpl no longer depends on zlib at all. The
 * seed-and-accumulate contract matches zlib exactly:
 *
 *    crc = crc32(0, NULL, 0);            // seed
 *    crc = crc32(crc, buf, len);         // accumulate
 *
 * Output is bit-identical to zlib's crc32() for these inputs.
 */
#ifndef ELF2RPL_CRC32_H
#define ELF2RPL_CRC32_H

#include <stddef.h>

typedef unsigned long  uLong;
typedef unsigned char  Bytef;
typedef unsigned int   uInt;

#ifndef Z_NULL
#define Z_NULL 0
#endif

static uLong crc32(uLong crc, const Bytef *buf, uInt len)
{
   static uLong table[256];
   static int   have_table = 0;

   if (!have_table)
   {
      uLong c;
      int   n, k;
      for (n = 0; n < 256; n++)
      {
         c = (uLong)n;
         for (k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
         table[n] = c;
      }
      have_table = 1;
   }

   if (!buf)
      return 0UL;

   crc = crc ^ 0xFFFFFFFFUL;
   while (len--)
      crc = table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
   return crc ^ 0xFFFFFFFFUL;
}

#endif /* ELF2RPL_CRC32_H */
