/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memalign.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>

#include <memalign.h>

void *memalign_alloc(size_t boundary, size_t size)
{
   void **place   = NULL;
   uintptr_t addr = 0;
   void *ptr      = (void*)malloc(boundary + size + sizeof(uintptr_t));
   if (!ptr)
      return NULL;

   addr           = ((uintptr_t)ptr + sizeof(uintptr_t) + boundary)
      & ~(boundary - 1);
   place          = (void**)addr;
   place[-1]      = ptr;

   return (void*)addr;
}

void memalign_free(void *ptr)
{
   void **p = NULL;
   if (!ptr)
      return;

   p = (void**)ptr;
   free(p[-1]);
}

void *memalign_alloc_aligned(size_t size)
{
#if defined(__x86_64__) || defined(__LP64) || defined(__IA64__) || defined(_M_X64) || defined(_M_X64) || defined(_WIN64)
   return memalign_alloc(64, size);
#elif defined(__i386__) || defined(__i486__) || defined(__i686__) || defined(GEKKO) || defined(_M_IX86)
   return memalign_alloc(32, size);
#else
   return memalign_alloc(32, size);
#endif
}
