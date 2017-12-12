/* 7zCrc.c -- CRC32 calculation
   2009-11-23 : Igor Pavlov : Public domain */

#include <stdint.h>
#include "7zCrc.h"
#include "CpuArch.h"

#define kCrcPoly 0xEDB88320

#ifdef MSB_FIRST
#define CRC_NUM_TABLES 1
#else
#define CRC_NUM_TABLES 8
#endif

typedef uint32_t (MY_FAST_CALL *CRC_FUNC)(uint32_t v, const void *data, size_t size, const uint32_t *table);

static CRC_FUNC g_CrcUpdate;
uint32_t g_CrcTable[256 * CRC_NUM_TABLES];

#if CRC_NUM_TABLES == 1

#define CRC_UPDATE_BYTE_2(crc, b) (table[((crc) ^ (b)) & 0xFF] ^ ((crc) >> 8))

static uint32_t MY_FAST_CALL CrcUpdateT1(uint32_t v, const void *data, size_t size, const uint32_t *table)
{
   const uint8_t *p = (const uint8_t *)data;
   for (; size > 0; size--, p++)
      v = CRC_UPDATE_BYTE_2(v, *p);
   return v;
}

#else

uint32_t MY_FAST_CALL CrcUpdateT4(uint32_t v, const void *data, size_t size, const uint32_t *table);
uint32_t MY_FAST_CALL CrcUpdateT8(uint32_t v, const void *data, size_t size, const uint32_t *table);

#endif

uint32_t MY_FAST_CALL CrcUpdate(uint32_t v, const void *data, size_t size)
{
   return g_CrcUpdate(v, data, size, g_CrcTable);
}

uint32_t MY_FAST_CALL CrcCalc(const void *data, size_t size)
{
   return g_CrcUpdate(CRC_INIT_VAL, data, size, g_CrcTable) ^ CRC_INIT_VAL;
}

void MY_FAST_CALL CrcGenerateTable(void)
{
   uint32_t i;
   for (i = 0; i < 256; i++)
   {
      uint32_t r = i;
      unsigned j;
      for (j = 0; j < 8; j++)
         r = (r >> 1) ^ (kCrcPoly & ~((r & 1) - 1));
      g_CrcTable[i] = r;
   }
#if CRC_NUM_TABLES == 1
   g_CrcUpdate = CrcUpdateT1;
#else
   for (; i < 256 * CRC_NUM_TABLES; i++)
   {
      uint32_t r    = g_CrcTable[i - 256];
      g_CrcTable[i] = g_CrcTable[r & 0xFF] ^ (r >> 8);
   }

#ifdef MY_CPU_X86_OR_AMD64
   g_CrcUpdate = CrcUpdateT8;
#else
   g_CrcUpdate = CrcUpdateT4;
#endif
#endif
}
