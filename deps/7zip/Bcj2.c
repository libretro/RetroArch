/* Bcj2.c -- Converter for x86 code (BCJ2)
   2008-10-04 : Igor Pavlov : Public domain */

#include <stdint.h>
#include "Bcj2.h"

#define IsJcc(b0, b1) ((b0) == 0x0F && ((b1) & 0xF0) == 0x80)
#define IsJ(b0, b1) ((b1 & 0xFE) == 0xE8 || IsJcc(b0, b1))

#define kNumTopBits 24
#define kTopValue ((uint32_t)1 << kNumTopBits)

#define kNumBitModelTotalBits 11
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5

#define RC_READ_BYTE (*buffer++)
#define RC_TEST { if (buffer == bufferLim) return SZ_ERROR_DATA; }
#define RC_INIT2 code = 0; range = 0xFFFFFFFF; \
{ int i; for (i = 0; i < 5; i++) { RC_TEST; code = (code << 8) | RC_READ_BYTE; }}

#define BCJ2_NORMALIZE if (range < kTopValue) { RC_TEST; range <<= 8; code = (code << 8) | RC_READ_BYTE; }

#define BCJ2_IF_BIT_0(p) ttt = *(p); bound = (range >> kNumBitModelTotalBits) * ttt; if (code < bound)
#define BCJ2_UPDATE_0(p) range = bound; *(p) = (uint16_t)(ttt + ((kBitModelTotal - ttt) >> kNumMoveBits)); BCJ2_NORMALIZE;
#define BCJ2_UPDATE_1(p) range -= bound; code -= bound; *(p) = (uint16_t)(ttt - (ttt >> kNumMoveBits)); BCJ2_NORMALIZE;

int Bcj2_Decode(
      const uint8_t *buf0, size_t size0,
      const uint8_t *buf1, size_t size1,
      const uint8_t *buf2, size_t size2,
      const uint8_t *buf3, size_t size3,
      uint8_t *outBuf, size_t outSize)
{
   uint16_t p[256 + 2];
   size_t inPos = 0, outPos = 0;

   const uint8_t *buffer, *bufferLim;
   uint32_t range, code;
   uint8_t prevuint8_t = 0;

   unsigned int i;
   for (i = 0; i < sizeof(p) / sizeof(p[0]); i++)
      p[i] = kBitModelTotal >> 1;

   buffer = buf3;
   bufferLim = buffer + size3;
   RC_INIT2

      if (outSize == 0)
         return SZ_OK;

   for (;;)
   {
      uint8_t b;
      uint16_t *prob;
      uint32_t bound;
      uint32_t ttt;

      size_t limit = size0 - inPos;
      if (outSize - outPos < limit)
         limit = outSize - outPos;
      while (limit != 0)
      {
         uint8_t b = buf0[inPos];
         outBuf[outPos++] = b;
         if (IsJ(prevuint8_t, b))
            break;
         inPos++;
         prevuint8_t = b;
         limit--;
      }

      if (limit == 0 || outPos == outSize)
         break;

      b = buf0[inPos++];

      if (b == 0xE8)
         prob = p + prevuint8_t;
      else if (b == 0xE9)
         prob = p + 256;
      else
         prob = p + 257;

      BCJ2_IF_BIT_0(prob)
      {
         BCJ2_UPDATE_0(prob)
            prevuint8_t = b;
      }
      else
      {
         uint32_t dest;
         const uint8_t *v;
         BCJ2_UPDATE_1(prob)
            if (b == 0xE8)
            {
               v = buf1;
               if (size1 < 4)
                  return SZ_ERROR_DATA;
               buf1 += 4;
               size1 -= 4;
            }
            else
            {
               v = buf2;
               if (size2 < 4)
                  return SZ_ERROR_DATA;
               buf2 += 4;
               size2 -= 4;
            }
         dest = (((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) |
               ((uint32_t)v[2] << 8) | ((uint32_t)v[3])) - ((uint32_t)outPos + 4);
         outBuf[outPos++] = (uint8_t)dest;
         if (outPos == outSize)
            break;
         outBuf[outPos++] = (uint8_t)(dest >> 8);
         if (outPos == outSize)
            break;
         outBuf[outPos++] = (uint8_t)(dest >> 16);
         if (outPos == outSize)
            break;
         outBuf[outPos++] = prevuint8_t = (uint8_t)(dest >> 24);
      }
   }
   return (outPos == outSize) ? SZ_OK : SZ_ERROR_DATA;
}
