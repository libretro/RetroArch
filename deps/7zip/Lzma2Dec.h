/* Lzma2Dec.h -- LZMA2 Decoder
2009-05-03 : Igor Pavlov : Public domain */

#ifndef __LZMA2_DEC_H
#define __LZMA2_DEC_H

#include <boolean.h>
#include "LzmaDec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- State Interface ---------- */

typedef struct
{
  CLzmaDec decoder;
  uint32_t packSize;
  uint32_t unpackSize;
  int state;
  uint8_t control;
  bool needInitDic;
  bool needInitState;
  bool needInitProp;
} CLzma2Dec;

#define Lzma2Dec_Construct(p) LzmaDec_Construct(&(p)->decoder)
#define Lzma2Dec_FreeProbs(p, alloc) LzmaDec_FreeProbs(&(p)->decoder, alloc);
#define Lzma2Dec_Free(p, alloc) LzmaDec_Free(&(p)->decoder, alloc);

SRes Lzma2Dec_AllocateProbs(CLzma2Dec *p, uint8_t prop, ISzAlloc *alloc);
SRes Lzma2Dec_Allocate(CLzma2Dec *p, uint8_t prop, ISzAlloc *alloc);
void Lzma2Dec_Init(CLzma2Dec *p);


/*
finishMode:
  It has meaning only if the decoding reaches output limit (*destLen or dicLimit).
  LZMA_FINISH_ANY - use smallest number of input bytes
  LZMA_FINISH_END - read EndOfStream marker after decoding

Returns:
  SZ_OK
    status:
      LZMA_STATUS_FINISHED_WITH_MARK
      LZMA_STATUS_NOT_FINISHED
      LZMA_STATUS_NEEDS_MORE_INPUT
  SZ_ERROR_DATA - Data error
*/

SRes Lzma2Dec_DecodeToDic(CLzma2Dec *p, size_t dicLimit,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status);

SRes Lzma2Dec_DecodeToBuf(CLzma2Dec *p, uint8_t *dest, size_t *destLen,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status);


/* ---------- One Call Interface ---------- */

/*
finishMode:
  It has meaning only if the decoding reaches output limit (*destLen).
  LZMA_FINISH_ANY - use smallest number of input bytes
  LZMA_FINISH_END - read EndOfStream marker after decoding

Returns:
  SZ_OK
    status:
      LZMA_STATUS_FINISHED_WITH_MARK
      LZMA_STATUS_NOT_FINISHED
  SZ_ERROR_DATA - Data error
  SZ_ERROR_MEM  - Memory allocation error
  SZ_ERROR_UNSUPPORTED - Unsupported properties
  SZ_ERROR_INPUT_EOF - It needs more bytes in input buffer (src).
*/

SRes Lzma2Decode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    uint8_t prop, ELzmaFinishMode finishMode, ELzmaStatus *status, ISzAlloc *alloc);

#ifdef __cplusplus
}
#endif

#endif
