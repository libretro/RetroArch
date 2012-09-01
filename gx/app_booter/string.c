// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <stdio.h>

#ifdef HW_DOL
#include <gctypes.h>
#include <ogc/machine/processor.h>

#define DSPCR_DSPDMA        0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINT         0x0008

#define ARAMSTART 0x8000

#define _SHIFTR(v, s, w) \
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static uint8_t buffer[0x800];

static vu16* const _dspReg = (u16*) 0xCC005000;
static __inline__ void __ARClearInterrupt()
{
   u16 cause;

   cause = _dspReg[5]&~(DSPCR_DSPINT|DSPCR_AIINT);
   _dspReg[5] = (cause|DSPCR_ARINT);
}

static __inline__ void __ARWaitDma()
{
   while(_dspReg[5]&DSPCR_DSPDMA);
}

static void __ARReadDMA(u32 memaddr,u32 aramaddr,u32 len)
{
   int level;
   _CPU_ISR_Disable(level);
   // set main memory address
   _dspReg[16] = (_dspReg[16]&~0x03ff)|_SHIFTR(memaddr,16,16);
   _dspReg[17] = (_dspReg[17]&~0xffe0)|_SHIFTR(memaddr, 0,16);

   // set aram address
   _dspReg[18] = (_dspReg[18]&~0x03ff)|_SHIFTR(aramaddr,16,16);
   _dspReg[19] = (_dspReg[19]&~0xffe0)|_SHIFTR(aramaddr, 0,16);

   // set cntrl bits
   _dspReg[20] = (_dspReg[20]&~0x8000)|0x8000;
   _dspReg[20] = (_dspReg[20]&~0x03ff)|_SHIFTR(len,16,16);
   _dspReg[21] = (_dspReg[21]&~0xffe0)|_SHIFTR(len, 0,16);

   __ARWaitDma();
   __ARClearInterrupt();
   _CPU_ISR_Restore(level);
}
#endif

void *memset(void *b, int c, size_t len)
{
   size_t i;

   for (i = 0; i < len; i++)
      ((unsigned char *)b)[i] = c;

   return b;
}

void *memcpy(void *dst, const void *src, size_t len)
{
#ifdef HW_DOL
   if (((unsigned) src & 0x80000000) == 0)
   {
      size_t i;
      u32 _dst = (u32) dst, _src = (u32) src;

      if ((_src & 0x1F) != 0)
      {
         unsigned templen = 32 - (_src & 0x1F);
         unsigned offset = 32 - templen;
         __ARReadDMA((u32) buffer, (u32) (_src & ~0x1F) + ARAMSTART, 32);
         memcpy((void *) _dst, &buffer[offset], templen > len ? len : templen);
         _src += templen;
         _dst += templen;
         if (templen >= len)
            return (void *) _dst;
         len -= templen;
      }

      size_t blocks = len >> 11;
      for (i = 0; i < blocks; i++)
      {
         __ARReadDMA(_dst, _src + ARAMSTART, sizeof(buffer));
         _src += sizeof(buffer);
         _dst += sizeof(buffer);
         len -= sizeof(buffer);
      }

      if (len)
      {
         __ARReadDMA((u32) buffer, _src + ARAMSTART, sizeof(buffer));
         memcpy((void *) _dst, buffer, len);
      }

      return (void *) _dst;
   }
   else
#endif
   {
      size_t i;

      for (i = 0; i < len; i++)
         ((unsigned char *)dst)[i] = ((unsigned char *)src)[i];

      return dst;
   }
}
