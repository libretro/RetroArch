/* 7zBuf.h -- Byte Buffer
2009-02-07 : Igor Pavlov : Public domain */

#ifndef __7Z_BUF_H
#define __7Z_BUF_H

#include "7zTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint8_t *data;
  size_t size;
} CBuf;

void Buf_Init(CBuf *p);
int Buf_Create(CBuf *p, size_t size, ISzAlloc *alloc);
void Buf_Free(CBuf *p, ISzAlloc *alloc);

#ifdef __cplusplus
}
#endif

#endif
