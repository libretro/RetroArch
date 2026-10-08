/* PPMd var.H model and the range decoder RAR pairs it with.
 *
 * This is Igor Pavlov's Ppmd7 from the LZMA SDK (2010-03-12, public
 * domain), itself based on Dmitry Shkarin's PPMd var.H (2001, public
 * domain), as libarchive carries it - the model, and of the two range
 * decoders only RAR's. The encoder is left out. It is kept in the SDK's
 * own style so that it can be compared with its source line by line.
 */

#ifndef __LIBRETRO_SDK_RRAR_PPMD7_H
#define __LIBRETRO_SDK_RRAR_PPMD7_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef unsigned char Byte;
typedef uint16_t UInt16;
typedef int32_t Int32;
typedef uint32_t UInt32;
typedef int Bool;
#define True 1
#define False 0

/* Where the range decoder gets its bytes. */
typedef struct
{
  void *ud;
  Byte (*Read)(void *ud); /* reads one byte, returns 0 in case of EOF or error */
} IByteIn;

/* References into the model's memory are pointers where a pointer is 32
 * bits, and offsets from its base elsewhere. */
#if (defined(_M_IX86) || defined(__i386__) || defined(_M_ARM) || defined(__arm__)) && !defined(__aarch64__)
#define PPMD_32BIT
#endif

#define PPMD_INT_BITS 7
#define PPMD_PERIOD_BITS 7
#define PPMD_BIN_SCALE (1 << (PPMD_INT_BITS + PPMD_PERIOD_BITS))

#define PPMD_GET_MEAN_SPEC(summ, shift, round) (((summ) + (1 << ((shift) - (round)))) >> (shift))
#define PPMD_GET_MEAN(summ) PPMD_GET_MEAN_SPEC((summ), PPMD_PERIOD_BITS, 2)
#define PPMD_UPDATE_PROB_0(prob) ((prob) + (1 << PPMD_INT_BITS) - PPMD_GET_MEAN(prob))
#define PPMD_UPDATE_PROB_1(prob) ((prob) - PPMD_GET_MEAN(prob))

#define PPMD_N1 4
#define PPMD_N2 4
#define PPMD_N3 4
#define PPMD_N4 ((128 + 3 - 1 * PPMD_N1 - 2 * PPMD_N2 - 3 * PPMD_N3) / 4)
#define PPMD_NUM_INDEXES (PPMD_N1 + PPMD_N2 + PPMD_N3 + PPMD_N4)

/* SEE-contexts for PPM-contexts with masked symbols */
typedef struct
{
  UInt16 Summ; /* Freq */
  Byte Shift;  /* Speed of Freq change; low Shift is for fast change */
  Byte Count;  /* Count to next change of Shift */
} CPpmd_See;

#define Ppmd_See_Update(p) do {                  \
   if ((p)->Shift < PPMD_PERIOD_BITS && --(p)->Count == 0) {   \
      (p)->Summ <<= 1;               \
      (p)->Count = (Byte)(3 << (p)->Shift++);         \
       }                        \
} while (0)

typedef struct
{
  Byte Symbol;
  Byte Freq;
  UInt16 SuccessorLow;
  UInt16 SuccessorHigh;
} CPpmd_State;

typedef
  #ifdef PPMD_32BIT
    CPpmd_State *
  #else
    UInt32
  #endif
  CPpmd_State_Ref;

typedef
  #ifdef PPMD_32BIT
    void *
  #else
    UInt32
  #endif
  CPpmd_Void_Ref;

typedef
  #ifdef PPMD_32BIT
    Byte *
  #else
    UInt32
  #endif
  CPpmd_Byte_Ref;

#define PPMD_SetAllBitsIn256Bytes(p) do {            \
   unsigned j;                     \
   for (j = 0; j < 256 / sizeof(p[0]); j += 8) {         \
      p[j+7] = p[j+6] = p[j+5] = p[j+4] =         \
          p[j+3] = p[j+2] = p[j+1] = p[j+0] = ~(size_t)0;   \
   }                        \
} while (0)

#define PPMD7_MIN_ORDER 2
#define PPMD7_MAX_ORDER 64

#define PPMD7_MIN_MEM_SIZE (1 << 11)
#define PPMD7_MAX_MEM_SIZE (0xFFFFFFFFu - 12 * 3)

struct CPpmd7_Context_;

typedef
  #ifdef PPMD_32BIT
    struct CPpmd7_Context_ *
  #else
    UInt32
  #endif
  CPpmd7_Context_Ref;

typedef struct CPpmd7_Context_
{
  UInt16 NumStats;
  UInt16 SummFreq;
  CPpmd_State_Ref Stats;
  CPpmd7_Context_Ref Suffix;
} CPpmd7_Context;

#define Ppmd7Context_OneState(p) ((CPpmd_State *)&(p)->SummFreq)

typedef struct
{
  CPpmd7_Context *MinContext, *MaxContext;
  CPpmd_State *FoundState;
  unsigned OrderFall, InitEsc, PrevSuccess, MaxOrder, HiBitsFlag;
  Int32 RunLength, InitRL; /* must be 32-bit at least */

  UInt32 Size;
  UInt32 GlueCount;
  Byte *Base, *LoUnit, *HiUnit, *Text, *UnitsStart;
  UInt32 AlignOffset;

  Byte Indx2Units[PPMD_NUM_INDEXES];
  Byte Units2Indx[128];
  CPpmd_Void_Ref FreeList[PPMD_NUM_INDEXES];
  Byte NS2Indx[256], NS2BSIndx[256], HB2Flag[256];
  CPpmd_See DummySee, See[25][16];
  UInt16 BinSumm[128][64];
} CPpmd7;

/* ---------- Decode ---------- */

typedef struct
{
  UInt32 (*GetThreshold)(void *p, UInt32 total);
  void (*Decode)(void *p, UInt32 start, UInt32 size);
  UInt32 (*DecodeBit)(void *p, UInt32 size0);
} IPpmd7_RangeDec;

typedef struct
{
  IPpmd7_RangeDec p;
  UInt32 Range;
  UInt32 Code;
  UInt32 Low;
  UInt32 Bottom;
  IByteIn *Stream;
} CPpmd7z_RangeDec;

void rrar_ppmd7_construct(CPpmd7 *p);
Bool rrar_ppmd7_alloc(CPpmd7 *p, UInt32 size);
void rrar_ppmd7_free(CPpmd7 *p);
void rrar_ppmd7_init(CPpmd7 *p, unsigned max_order);
/* The four bytes a block's data starts with. False if they cannot be. */
Bool rrar_ppmd7_range_init(CPpmd7z_RangeDec *rc, IByteIn *stream);
/* The next byte, or a negative number if the data is not PPMd's. */
int rrar_ppmd7_decode_symbol(CPpmd7 *p, CPpmd7z_RangeDec *rc);

RETRO_END_DECLS

#endif
