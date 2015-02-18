#ifdef _MSC_VER
#include <cruntime.h>
#endif
#include <stdio.h>
#include <string.h>

void *memset(void *dst, int val, size_t count)
{
   void *start = dst;

#if defined(_M_IA64) || defined (_M_AMD64) || defined(_M_ALPHA) || defined (_M_PPC)
   extern void RtlFillMemory(void *, size_t count, char);

   RtlFillMemory(dst, count, (char)val);
#else
   while (count--)
   {
      *(char*)dst = (char)val;
      dst = (char*)dst + 1;
   }
#endif

   return start;
}

void *memcpy(void *dst, const void *src, size_t len)
{
   size_t i;

   for (i = 0; i < len; i++)
      ((unsigned char *)dst)[i] = ((unsigned char *)src)[i];

   return dst;
}
