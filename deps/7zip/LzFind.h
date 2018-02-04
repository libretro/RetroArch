/* LzFind.h -- Match finder for LZ algorithms
2015-10-15 : Igor Pavlov : Public domain */

#ifndef __LZ_FIND_H
#define __LZ_FIND_H

#include "7zTypes.h"

EXTERN_C_BEGIN

typedef uint32_t CLzRef;

typedef struct _CMatchFinder
{
  unsigned char *buffer;
  uint32_t pos;
  uint32_t posLimit;
  uint32_t streamPos;
  uint32_t lenLimit;

  uint32_t cyclicBufferPos;
  uint32_t cyclicBufferSize; /* it must be = (historySize + 1) */

  unsigned char streamEndWasReached;
  unsigned char btMode;
  unsigned char bigHash;
  unsigned char directInput;

  uint32_t matchMaxLen;
  CLzRef *hash;
  CLzRef *son;
  uint32_t hashMask;
  uint32_t cutValue;

  unsigned char *bufferBase;
  ISeqInStream *stream;

  uint32_t blockSize;
  uint32_t keepSizeBefore;
  uint32_t keepSizeAfter;

  uint32_t numHashBytes;
  size_t directInputRem;
  uint32_t historySize;
  uint32_t fixedHashSize;
  uint32_t hashSizeSum;
  SRes result;
  uint32_t crc[256];
  size_t numRefs;
} CMatchFinder;

#define Inline_MatchFinder_GetPointerToCurrentPos(p) ((p)->buffer)

#define Inline_MatchFinder_GetNumAvailableBytes(p) ((p)->streamPos - (p)->pos)

#define Inline_MatchFinder_IsFinishedOK(p) \
    ((p)->streamEndWasReached \
        && (p)->streamPos == (p)->pos \
        && (!(p)->directInput || (p)->directInputRem == 0))

int MatchFinder_NeedMove(CMatchFinder *p);
unsigned char *MatchFinder_GetPointerToCurrentPos(CMatchFinder *p);
void MatchFinder_MoveBlock(CMatchFinder *p);
void MatchFinder_ReadIfRequired(CMatchFinder *p);

void MatchFinder_Construct(CMatchFinder *p);

/* Conditions:
     historySize <= 3 GB
     keepAddBufferBefore + matchMaxLen + keepAddBufferAfter < 511MB
*/
int MatchFinder_Create(CMatchFinder *p, uint32_t historySize,
    uint32_t keepAddBufferBefore, uint32_t matchMaxLen, uint32_t keepAddBufferAfter,
    ISzAlloc *alloc);
void MatchFinder_Free(CMatchFinder *p, ISzAlloc *alloc);
void MatchFinder_Normalize3(uint32_t subValue, CLzRef *items, size_t numItems);
void MatchFinder_ReduceOffsets(CMatchFinder *p, uint32_t subValue);

uint32_t * GetMatchesSpec1(uint32_t lenLimit, uint32_t curMatch, uint32_t pos, const unsigned char *buffer, CLzRef *son,
    uint32_t _cyclicBufferPos, uint32_t _cyclicBufferSize, uint32_t _cutValue,
    uint32_t *distances, uint32_t maxLen);

/*
Conditions:
  Mf_GetNumAvailableBytes_Func must be called before each Mf_GetMatchLen_Func.
  Mf_GetPointerToCurrentPos_Func's result must be used only before any other function
*/

typedef void (*Mf_Init_Func)(void *object);
typedef uint32_t (*Mf_GetNumAvailableBytes_Func)(void *object);
typedef const unsigned char * (*Mf_GetPointerToCurrentPos_Func)(void *object);
typedef uint32_t (*Mf_GetMatches_Func)(void *object, uint32_t *distances);
typedef void (*Mf_Skip_Func)(void *object, uint32_t);

typedef struct _IMatchFinder
{
  Mf_Init_Func Init;
  Mf_GetNumAvailableBytes_Func GetNumAvailableBytes;
  Mf_GetPointerToCurrentPos_Func GetPointerToCurrentPos;
  Mf_GetMatches_Func GetMatches;
  Mf_Skip_Func Skip;
} IMatchFinder;

void MatchFinder_CreateVTable(CMatchFinder *p, IMatchFinder *vTable);

void MatchFinder_Init_2(CMatchFinder *p, int readData);
void MatchFinder_Init(CMatchFinder *p);

uint32_t Bt3Zip_MatchFinder_GetMatches(CMatchFinder *p, uint32_t *distances);
uint32_t Hc3Zip_MatchFinder_GetMatches(CMatchFinder *p, uint32_t *distances);

void Bt3Zip_MatchFinder_Skip(CMatchFinder *p, uint32_t num);
void Hc3Zip_MatchFinder_Skip(CMatchFinder *p, uint32_t num);

EXTERN_C_END

#endif
