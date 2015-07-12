/* 7zAlloc.c -- Allocation functions
   2010-10-29 : Igor Pavlov : Public domain */

#include "7zAlloc.h"

void *SzAlloc(void *p, size_t size)
{
   (void)p;
   if (size == 0)
      return 0;
   return malloc(size);
}

void SzFree(void *p, void *address)
{
   (void)p;
   free(address);
}

void *SzAllocTemp(void *p, size_t size)
{
   (void)p;
   if (size == 0)
      return 0;
   return malloc(size);
}

void SzFreeTemp(void *p, void *address)
{
   (void)p;
   free(address);
}
