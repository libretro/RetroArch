/* 7zDec.c -- Decoding from 7z folder
2010-11-02 : Igor Pavlov : Public domain */

#include <stdint.h>
#include <string.h>
#include <boolean.h>

#include "7z.h"

#include "Bcj2.h"
#include "Bra.h"
#include "CpuArch.h"
#include "LzmaDec.h"
#include "Lzma2Dec.h"

#define k_Copy 0
#define k_LZMA2 0x21
#define k_LZMA  0x30101
#define k_BCJ   0x03030103
#define k_PPC   0x03030205
#define k_ARM   0x03030501
#define k_ARMT  0x03030701
#define k_SPARC 0x03030805
#define k_BCJ2  0x0303011B

static SRes SzDecodeLzma(CSzCoderInfo *coder,
      uint64_t inSize, ILookInStream *inStream,
      uint8_t *outBuffer, size_t outSize, ISzAlloc *allocMain)
{
  SRes result;
  CLzmaDec state;
  SRes res         = SZ_OK;

  LzmaDec_Construct(&state);
  result           = LzmaDec_AllocateProbs(
        &state, coder->Props.data,
        (unsigned)coder->Props.size, allocMain);
  if (result != 0)
     return result;
  state.dic        = outBuffer;
  state.dicBufSize = outSize;
  LzmaDec_Init(&state);

  for (;;)
  {
    uint8_t *inBuf   = NULL;
    size_t lookahead = (1 << 18);
    if (lookahead > inSize)
      lookahead      = (size_t)inSize;
    res              = inStream->Look(
          (void *)inStream, (const void **)&inBuf, &lookahead);

    if (res != SZ_OK)
      break;

    {
      ELzmaStatus status;
      size_t inProcessed  = (size_t)lookahead, dicPos = state.dicPos;
      res                 = LzmaDec_DecodeToDic(
            &state, outSize, inBuf, &inProcessed, LZMA_FINISH_END, &status);
      lookahead          -= inProcessed;
      inSize             -= inProcessed;

      if (res != SZ_OK)
        break;

      if (
            state.dicPos == state.dicBufSize || 
            (inProcessed == 0 && dicPos == state.dicPos))
      {
        if (
              state.dicBufSize != outSize || 
              lookahead != 0              ||
            (status != LZMA_STATUS_FINISHED_WITH_MARK &&
             status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK))
          res = SZ_ERROR_DATA;
        break;
      }

      res = inStream->Skip((void *)inStream, inProcessed);
      if (res != SZ_OK)
        break;
    }
  }

  LzmaDec_FreeProbs(&state, allocMain);
  return res;
}

static SRes SzDecodeLzma2(CSzCoderInfo *coder,
      uint64_t inSize, ILookInStream *inStream,
      uint8_t *outBuffer, size_t outSize, ISzAlloc *allocMain)
{
   SRes result;
   CLzma2Dec state;
   SRes res = SZ_OK;

   Lzma2Dec_Construct(&state);
   if (coder->Props.size != 1)
      return SZ_ERROR_DATA;

   result = Lzma2Dec_AllocateProbs(&state, coder->Props.data[0], allocMain);
   if (result != 0)
      return result;

   state.decoder.dic        = outBuffer;
   state.decoder.dicBufSize = outSize;
   Lzma2Dec_Init(&state);

   for (;;)
   {
      uint8_t *inBuf   = NULL;
      size_t lookahead = (1 << 18);
      if (lookahead > inSize)
         lookahead     = (size_t)inSize;
      res              = inStream->Look(
            (void *)inStream, (const void **)&inBuf, &lookahead);
      if (res != SZ_OK)
         break;

      {
         ELzmaStatus status;
         size_t inProcessed  = (size_t)lookahead, dicPos = state.decoder.dicPos;
         res                 = Lzma2Dec_DecodeToDic(
               &state, outSize, inBuf, &inProcessed,
               LZMA_FINISH_END, &status);
         lookahead          -= inProcessed;
         inSize             -= inProcessed;
         if (res != SZ_OK)
            break;
         if (
               state.decoder.dicPos == state.decoder.dicBufSize || 
               (inProcessed == 0 && dicPos == state.decoder.dicPos))
         {
            if (state.decoder.dicBufSize != outSize || lookahead != 0 ||
                  (status != LZMA_STATUS_FINISHED_WITH_MARK))
               res = SZ_ERROR_DATA;
            break;
         }
         res = inStream->Skip((void *)inStream, inProcessed);
         if (res != SZ_OK)
            break;
      }
   }

   Lzma2Dec_FreeProbs(&state, allocMain);
   return res;
}

static SRes SzDecodeCopy(uint64_t inSize,
      ILookInStream *inStream, uint8_t *outBuffer)
{
   while (inSize > 0)
   {
      SRes result;
      void *inBuf    = NULL;
      size_t curSize = (1 << 18);
      if (curSize > inSize)
         curSize      = (size_t)inSize;
      result         = inStream->Look(
            (void *)inStream, (const void **)&inBuf, &curSize);

      if (result != 0)
         return result;
      if (curSize == 0)
         return SZ_ERROR_INPUT_EOF;

      memcpy(outBuffer, inBuf, curSize);
      outBuffer += curSize;
      inSize    -= curSize;
      result     = inStream->Skip((void *)inStream, curSize);

      if (result != 0)
         return result;
   }
   return SZ_OK;
}

static bool is_main_method(uint32_t m)
{
   switch(m)
   {
      case k_Copy:
      case k_LZMA:
      case k_LZMA2:
         return true;
      default:
         break;
   }
   return false;
}

static bool is_supported_coder(const CSzCoderInfo *c)
{
  return
      c->NumInStreams == 1 &&
      c->NumOutStreams == 1 &&
      c->MethodID <= (uint32_t)0xFFFFFFFF &&
      is_main_method((uint32_t)c->MethodID);
}

#define IS_BCJ2(c) ((c)->MethodID == k_BCJ2 && (c)->NumInStreams == 4 && (c)->NumOutStreams == 1)

static SRes check_supported_folder(const CSzFolder *f)
{
  if (f->NumCoders < 1 || f->NumCoders > 4)
    return SZ_ERROR_UNSUPPORTED;

  if (!is_supported_coder(&f->Coders[0]))
    return SZ_ERROR_UNSUPPORTED;

  switch (f->NumCoders)
  {
     case 1:
        if (     f->NumPackStreams != 1 
              || f->PackStreams[0] != 0
              || f->NumBindPairs   != 0)
           return SZ_ERROR_UNSUPPORTED;
        return SZ_OK;
     case 2:
        {
           CSzCoderInfo *c = &f->Coders[1];

           if (c->MethodID > (uint32_t)0xFFFFFFFF ||
                 c->NumInStreams   != 1             ||
                 c->NumOutStreams  != 1             ||
                 f->NumPackStreams != 1             ||
                 f->PackStreams[0] != 0             ||
                 f->NumBindPairs   != 1             ||
                 f->BindPairs[0].InIndex  != 1      ||
                 f->BindPairs[0].OutIndex != 0)
              return SZ_ERROR_UNSUPPORTED;

           switch ((uint32_t)c->MethodID)
           {
              case k_BCJ:
              case k_ARM:
                 break;
              default:
                 return SZ_ERROR_UNSUPPORTED;
           }
        }
        return SZ_OK;
     case 4:
        if (!is_supported_coder(&f->Coders[1]) ||
              !is_supported_coder(&f->Coders[2]) ||
              !IS_BCJ2(&f->Coders[3]))
           return SZ_ERROR_UNSUPPORTED;

        if (f->NumPackStreams != 4 ||
              f->PackStreams[0] != 2 ||
              f->PackStreams[1] != 6 ||
              f->PackStreams[2] != 1 ||
              f->PackStreams[3] != 0 ||
              f->NumBindPairs   != 3 ||
              f->BindPairs[0].InIndex  != 5 ||
              f->BindPairs[0].OutIndex != 0 ||
              f->BindPairs[1].InIndex  != 4 ||
              f->BindPairs[1].OutIndex != 1 ||
              f->BindPairs[2].InIndex  != 3 ||
              f->BindPairs[2].OutIndex != 2)
           return SZ_ERROR_UNSUPPORTED;
        return SZ_OK;
  }
  return SZ_ERROR_UNSUPPORTED;
}

static uint64_t get_sum(const uint64_t *values, uint32_t idx)
{
  uint64_t sum = 0;
  uint32_t i;
  for (i = 0; i < idx; i++)
    sum += values[i];
  return sum;
}

static SRes SzFolder_Decode2(const CSzFolder *folder,
      const uint64_t *packSizes,
      ILookInStream *inStream, uint64_t startPos,
      uint8_t *outBuffer, size_t outSize, ISzAlloc *allocMain,
      uint8_t *tempBuf[])
{
   uint32_t ci;
   size_t tempSizes[3] = { 0, 0, 0};
   size_t tempSize3    = 0;
   uint8_t *tempBuf3   = 0;
   SRes result         = check_supported_folder(folder);

   if (result != 0)
      return result;

   for (ci = 0; ci < folder->NumCoders; ci++)
   {
      CSzCoderInfo *coder = &folder->Coders[ci];

      if (is_main_method((uint32_t)coder->MethodID))
      {
         SRes result;
         uint64_t offset    = 0;
         uint64_t inSize    = 0;
         uint32_t si        = 0;
         uint8_t *outBufCur = outBuffer;
         size_t outSizeCur  = outSize;

         if (folder->NumCoders == 4)
         {
            uint32_t indices[]  = { 3, 2, 0 };
            uint64_t unpackSize = folder->UnpackSizes[ci];
            si                  = indices[ci];

            if (ci < 2)
            {
               uint8_t *temp;
               outSizeCur = (size_t)unpackSize;
               if (outSizeCur != unpackSize)
                  return SZ_ERROR_MEM;
               temp = (uint8_t *)IAlloc_Alloc(allocMain, outSizeCur);
               if (temp == 0 && outSizeCur != 0)
                  return SZ_ERROR_MEM;
               outBufCur = tempBuf[1 - ci] = temp;
               tempSizes[1 - ci] = outSizeCur;
            }
            else if (ci == 2)
            {
               if (unpackSize > outSize) /* check it */
                  return SZ_ERROR_PARAM;
               tempBuf3  = outBufCur = outBuffer 
                  + (outSize - (size_t)unpackSize);
               tempSize3 = outSizeCur = (size_t)unpackSize;
            }
            else
               return SZ_ERROR_UNSUPPORTED;
         }

         offset = get_sum(packSizes, si);
         inSize = packSizes[si];
         result = LookInStream_SeekTo(inStream, startPos + offset);

         if (result != 0)
            return result;

         switch (coder->MethodID)
         {
            case k_Copy:
               if (inSize != outSizeCur) /* check it */
                  return SZ_ERROR_DATA;
               result = SzDecodeCopy(inSize, inStream, outBufCur);
               if (result != 0)
                  return result;
               break;
            case k_LZMA:
               result = SzDecodeLzma(
                     coder, inSize, inStream,
                     outBufCur, outSizeCur, allocMain);
               if (result != 0)
                  return result;
               break;
            case k_LZMA2:
               result = SzDecodeLzma2(
                     coder, inSize, inStream,
                     outBufCur, outSizeCur, allocMain);
               if (result != 0)
                  return result;
               break;
            default:
               return SZ_ERROR_UNSUPPORTED;
         }
      }
      else if (coder->MethodID == k_BCJ2)
      {
         SRes res;
         SRes result;
         uint64_t offset = get_sum(packSizes, 1);
         uint64_t s3Size = packSizes[1];

         if (ci != 3)
            return SZ_ERROR_UNSUPPORTED;

         result = LookInStream_SeekTo(inStream, startPos + offset);

         if (result != 0)
            return result;

         tempSizes[2]    = (size_t)s3Size;
         if (tempSizes[2] != s3Size)
            return SZ_ERROR_MEM;

         tempBuf[2]      = (uint8_t *)IAlloc_Alloc(allocMain, tempSizes[2]);
         if (tempBuf[2] == 0 && tempSizes[2] != 0)
            return SZ_ERROR_MEM;

         res    = SzDecodeCopy(s3Size, inStream, tempBuf[2]);
         if (res != 0)
            return res;

         res = Bcj2_Decode(
               tempBuf3, tempSize3,
               tempBuf[0], tempSizes[0],
               tempBuf[1], tempSizes[1],
               tempBuf[2], tempSizes[2],
               outBuffer, outSize);

         if (res != 0)
            return res;
      }
      else
      {
         if (ci != 1)
            return SZ_ERROR_UNSUPPORTED;
         switch(coder->MethodID)
         {
            case k_BCJ:
               {
                  uint32_t state;
                  x86_Convert_Init(state);
                  x86_Convert(outBuffer, outSize, 0, &state, 0);
                  break;
               }
            case k_ARM:
               ARM_Convert(outBuffer, outSize, 0, 0);
               break;
            default:
               return SZ_ERROR_UNSUPPORTED;
         }
      }
   }
   return SZ_OK;
}

SRes SzFolder_Decode(const CSzFolder *folder, const uint64_t *packSizes,
    ILookInStream *inStream, uint64_t startPos,
    uint8_t *outBuffer, size_t outSize, ISzAlloc *allocMain)
{
  int i;
  uint8_t *tempBuf[3] = { 0, 0, 0};
  SRes res            = SzFolder_Decode2(folder,
        packSizes, inStream, startPos,
        outBuffer, (size_t)outSize, allocMain, tempBuf);

  for (i = 0; i < 3; i++)
    IAlloc_Free(allocMain, tempBuf[i]);
  return res;
}
