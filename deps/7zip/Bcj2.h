/* Bcj2.h -- Converter for x86 code (BCJ2)
2009-02-07 : Igor Pavlov : Public domain */

#ifndef __BCJ2_H
#define __BCJ2_H

#include "7zTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
Conditions:
  outSize <= FullOutputSize,
  where FullOutputSize is full size of output stream of x86_2 filter.

If buf0 overlaps outBuf, there are two required conditions:
  1) (buf0 >= outBuf)
  2) (buf0 + size0 >= outBuf + FullOutputSize).

Returns:
  SZ_OK
  SZ_ERROR_DATA - Data error
*/

int Bcj2_Decode(
    const uint8_t *buf0, size_t size0,
    const uint8_t *buf1, size_t size1,
    const uint8_t *buf2, size_t size2,
    const uint8_t *buf3, size_t size3,
    uint8_t *outBuf, size_t outSize);

#ifdef __cplusplus
}
#endif

#endif
