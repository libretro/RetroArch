/* Lzma2Dec.c -- LZMA2 Decoder
   2009-05-03 : Igor Pavlov : Public domain */

#include <stdint.h>
#include <string.h>

#include "Lzma2Dec.h"

/*
   00000000  -  EOS
   00000001 U U  -  Uncompressed Reset Dic
   00000010 U U  -  Uncompressed No Reset
   100uuuuu U U P P  -  LZMA no reset
   101uuuuu U U P P  -  LZMA reset state
   110uuuuu U U P P S  -  LZMA reset state + new prop
   111uuuuu U U P P S  -  LZMA reset state + new prop + reset dic

   u, U - Unpack Size
   P - Pack Size
   S - Props
   */

#define LZMA2_CONTROL_LZMA (1 << 7)
#define LZMA2_CONTROL_COPY_NO_RESET 2
#define LZMA2_CONTROL_COPY_RESET_DIC 1
#define LZMA2_CONTROL_EOF 0

#define LZMA2_IS_UNCOMPRESSED_STATE(p) (((p)->control & LZMA2_CONTROL_LZMA) == 0)

#define LZMA2_GET_LZMA_MODE(p) (((p)->control >> 5) & 3)
#define LZMA2_IS_THERE_PROP(mode) ((mode) >= 2)

#define LZMA2_LCLP_MAX 4
#define LZMA2_DIC_SIZE_FROM_PROP(p) (((uint32_t)2 | ((p) & 1)) << ((p) / 2 + 11))

#define PRF(x)

typedef enum
{
   LZMA2_STATE_CONTROL,
   LZMA2_STATE_UNPACK0,
   LZMA2_STATE_UNPACK1,
   LZMA2_STATE_PACK0,
   LZMA2_STATE_PACK1,
   LZMA2_STATE_PROP,
   LZMA2_STATE_DATA,
   LZMA2_STATE_DATA_CONT,
   LZMA2_STATE_FINISHED,
   LZMA2_STATE_ERROR
} ELzma2State;

static SRes Lzma2Dec_GetOldProps(uint8_t prop, uint8_t *props)
{
   uint32_t dicSize;
   if (prop > 40)
      return SZ_ERROR_UNSUPPORTED;
   dicSize = (prop == 40) ? 0xFFFFFFFF : LZMA2_DIC_SIZE_FROM_PROP(prop);
   props[0] = (uint8_t)LZMA2_LCLP_MAX;
   props[1] = (uint8_t)(dicSize);
   props[2] = (uint8_t)(dicSize >> 8);
   props[3] = (uint8_t)(dicSize >> 16);
   props[4] = (uint8_t)(dicSize >> 24);
   return SZ_OK;
}

SRes Lzma2Dec_AllocateProbs(CLzma2Dec *p, uint8_t prop, ISzAlloc *alloc)
{
   uint8_t props[LZMA_PROPS_SIZE];
   RINOK(Lzma2Dec_GetOldProps(prop, props));
   return LzmaDec_AllocateProbs(&p->decoder, props, LZMA_PROPS_SIZE, alloc);
}

SRes Lzma2Dec_Allocate(CLzma2Dec *p, uint8_t prop, ISzAlloc *alloc)
{
   uint8_t props[LZMA_PROPS_SIZE];
   RINOK(Lzma2Dec_GetOldProps(prop, props));
   return LzmaDec_Allocate(&p->decoder, props, LZMA_PROPS_SIZE, alloc);
}

void Lzma2Dec_Init(CLzma2Dec *p)
{
   p->state = LZMA2_STATE_CONTROL;
   p->needInitDic = true;
   p->needInitState = true;
   p->needInitProp = true;
   LzmaDec_Init(&p->decoder);
}

static ELzma2State Lzma2Dec_UpdateState(CLzma2Dec *p, uint8_t b)
{
   switch(p->state)
   {
      case LZMA2_STATE_CONTROL:
         p->control = b;
         PRF(printf("\n %4X ", p->decoder.dicPos));
         PRF(printf(" %2X", b));
         if (p->control == 0)
            return LZMA2_STATE_FINISHED;
         if (LZMA2_IS_UNCOMPRESSED_STATE(p))
         {
            if ((p->control & 0x7F) > 2)
               return LZMA2_STATE_ERROR;
            p->unpackSize = 0;
         }
         else
            p->unpackSize = (uint32_t)(p->control & 0x1F) << 16;
         return LZMA2_STATE_UNPACK0;

      case LZMA2_STATE_UNPACK0:
         p->unpackSize |= (uint32_t)b << 8;
         return LZMA2_STATE_UNPACK1;

      case LZMA2_STATE_UNPACK1:
         p->unpackSize |= (uint32_t)b;
         p->unpackSize++;
         PRF(printf(" %8d", p->unpackSize));
         return (LZMA2_IS_UNCOMPRESSED_STATE(p)) ? LZMA2_STATE_DATA : LZMA2_STATE_PACK0;

      case LZMA2_STATE_PACK0:
         p->packSize = (uint32_t)b << 8;
         return LZMA2_STATE_PACK1;

      case LZMA2_STATE_PACK1:
         p->packSize |= (uint32_t)b;
         p->packSize++;
         PRF(printf(" %8d", p->packSize));
         return LZMA2_IS_THERE_PROP(LZMA2_GET_LZMA_MODE(p)) ? LZMA2_STATE_PROP:
            (p->needInitProp ? LZMA2_STATE_ERROR : LZMA2_STATE_DATA);

      case LZMA2_STATE_PROP:
         {
            int lc, lp;
            if (b >= (9 * 5 * 5))
               return LZMA2_STATE_ERROR;
            lc = b % 9;
            b /= 9;
            p->decoder.prop.pb = b / 5;
            lp = b % 5;
            if (lc + lp > LZMA2_LCLP_MAX)
               return LZMA2_STATE_ERROR;
            p->decoder.prop.lc = lc;
            p->decoder.prop.lp = lp;
            p->needInitProp = false;
            return LZMA2_STATE_DATA;
         }
   }
   return LZMA2_STATE_ERROR;
}

static void LzmaDec_UpdateWithUncompressed(CLzmaDec *p, const uint8_t *src, size_t size)
{
   memcpy(p->dic + p->dicPos, src, size);
   p->dicPos += size;
   if (p->checkDicSize == 0 && p->prop.dicSize - p->processedPos <= size)
      p->checkDicSize = p->prop.dicSize;
   p->processedPos += (uint32_t)size;
}

void LzmaDec_InitDicAndState(CLzmaDec *p, bool initDic, bool initState);

SRes Lzma2Dec_DecodeToDic(CLzma2Dec *p, size_t dicLimit,
      const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
   size_t inSize = *srcLen;
   *srcLen = 0;
   *status = LZMA_STATUS_NOT_SPECIFIED;

   while (p->state != LZMA2_STATE_FINISHED)
   {
      size_t dicPos = p->decoder.dicPos;
      if (p->state == LZMA2_STATE_ERROR)
         return SZ_ERROR_DATA;
      if (dicPos == dicLimit && finishMode == LZMA_FINISH_ANY)
      {
         *status = LZMA_STATUS_NOT_FINISHED;
         return SZ_OK;
      }
      if (p->state != LZMA2_STATE_DATA && p->state != LZMA2_STATE_DATA_CONT)
      {
         if (*srcLen == inSize)
         {
            *status = LZMA_STATUS_NEEDS_MORE_INPUT;
            return SZ_OK;
         }
         (*srcLen)++;
         p->state = Lzma2Dec_UpdateState(p, *src++);
         continue;
      }
      {
         size_t destSizeCur = dicLimit - dicPos;
         size_t srcSizeCur = inSize - *srcLen;
         ELzmaFinishMode curFinishMode = LZMA_FINISH_ANY;

         if (p->unpackSize <= destSizeCur)
         {
            destSizeCur = (size_t)p->unpackSize;
            curFinishMode = LZMA_FINISH_END;
         }

         if (LZMA2_IS_UNCOMPRESSED_STATE(p))
         {
            if (*srcLen == inSize)
            {
               *status = LZMA_STATUS_NEEDS_MORE_INPUT;
               return SZ_OK;
            }

            if (p->state == LZMA2_STATE_DATA)
            {
               bool initDic = (p->control == LZMA2_CONTROL_COPY_RESET_DIC);
               if (initDic)
                  p->needInitProp = p->needInitState = true;
               else if (p->needInitDic)
                  return SZ_ERROR_DATA;
               p->needInitDic = false;
               LzmaDec_InitDicAndState(&p->decoder, initDic, false);
            }

            if (srcSizeCur > destSizeCur)
               srcSizeCur = destSizeCur;

            if (srcSizeCur == 0)
               return SZ_ERROR_DATA;

            LzmaDec_UpdateWithUncompressed(&p->decoder, src, srcSizeCur);

            src += srcSizeCur;
            *srcLen += srcSizeCur;
            p->unpackSize -= (uint32_t)srcSizeCur;
            p->state = (p->unpackSize == 0) ? LZMA2_STATE_CONTROL : LZMA2_STATE_DATA_CONT;
         }
         else
         {
            size_t outSizeProcessed;
            SRes res;

            if (p->state == LZMA2_STATE_DATA)
            {
               int mode = LZMA2_GET_LZMA_MODE(p);
               bool initDic = (mode == 3);
               bool initState = (mode > 0);
               if ((!initDic && p->needInitDic) || (!initState && p->needInitState))
                  return SZ_ERROR_DATA;

               LzmaDec_InitDicAndState(&p->decoder, initDic, initState);
               p->needInitDic = false;
               p->needInitState = false;
               p->state = LZMA2_STATE_DATA_CONT;
            }
            if (srcSizeCur > p->packSize)
               srcSizeCur = (size_t)p->packSize;

            res = LzmaDec_DecodeToDic(&p->decoder, dicPos + destSizeCur, src, &srcSizeCur, curFinishMode, status);

            src += srcSizeCur;
            *srcLen += srcSizeCur;
            p->packSize -= (uint32_t)srcSizeCur;

            outSizeProcessed = p->decoder.dicPos - dicPos;
            p->unpackSize -= (uint32_t)outSizeProcessed;

            RINOK(res);
            if (*status == LZMA_STATUS_NEEDS_MORE_INPUT)
               return res;

            if (srcSizeCur == 0 && outSizeProcessed == 0)
            {
               if (*status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK ||
                     p->unpackSize != 0 || p->packSize != 0)
                  return SZ_ERROR_DATA;
               p->state = LZMA2_STATE_CONTROL;
            }
            if (*status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
               *status = LZMA_STATUS_NOT_FINISHED;
         }
      }
   }
   *status = LZMA_STATUS_FINISHED_WITH_MARK;
   return SZ_OK;
}

SRes Lzma2Dec_DecodeToBuf(CLzma2Dec *p, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
   size_t outSize = *destLen, inSize = *srcLen;
   *srcLen = *destLen = 0;
   for (;;)
   {
      size_t srcSizeCur = inSize, outSizeCur, dicPos;
      ELzmaFinishMode curFinishMode;
      SRes res;
      if (p->decoder.dicPos == p->decoder.dicBufSize)
         p->decoder.dicPos = 0;
      dicPos = p->decoder.dicPos;
      if (outSize > p->decoder.dicBufSize - dicPos)
      {
         outSizeCur = p->decoder.dicBufSize;
         curFinishMode = LZMA_FINISH_ANY;
      }
      else
      {
         outSizeCur = dicPos + outSize;
         curFinishMode = finishMode;
      }

      res = Lzma2Dec_DecodeToDic(p, outSizeCur, src, &srcSizeCur, curFinishMode, status);
      src += srcSizeCur;
      inSize -= srcSizeCur;
      *srcLen += srcSizeCur;
      outSizeCur = p->decoder.dicPos - dicPos;
      memcpy(dest, p->decoder.dic + dicPos, outSizeCur);
      dest += outSizeCur;
      outSize -= outSizeCur;
      *destLen += outSizeCur;
      if (res != 0)
         return res;
      if (outSizeCur == 0 || outSize == 0)
         return SZ_OK;
   }
}

SRes Lzma2Decode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
      uint8_t prop, ELzmaFinishMode finishMode, ELzmaStatus *status, ISzAlloc *alloc)
{
   CLzma2Dec decoder;
   SRes res;
   size_t outSize = *destLen, inSize = *srcLen;
   uint8_t props[LZMA_PROPS_SIZE];

   Lzma2Dec_Construct(&decoder);

   *destLen = *srcLen = 0;
   *status = LZMA_STATUS_NOT_SPECIFIED;
   decoder.decoder.dic = dest;
   decoder.decoder.dicBufSize = outSize;

   RINOK(Lzma2Dec_GetOldProps(prop, props));
   RINOK(LzmaDec_AllocateProbs(&decoder.decoder, props, LZMA_PROPS_SIZE, alloc));

   *srcLen = inSize;
   res = Lzma2Dec_DecodeToDic(&decoder, outSize, src, srcLen, finishMode, status);
   *destLen = decoder.decoder.dicPos;
   if (res == SZ_OK && *status == LZMA_STATUS_NEEDS_MORE_INPUT)
      res = SZ_ERROR_INPUT_EOF;

   LzmaDec_FreeProbs(&decoder.decoder, alloc);
   return res;
}
