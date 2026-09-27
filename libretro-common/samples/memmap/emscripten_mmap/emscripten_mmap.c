/* Copyright (C) 2026 Gwendall Esnault
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (emscripten_mmap.c).
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

/* Both headers must agree on mmap's off_t argument. */
#include <sys/types.h>
#include <memmap.h>
#include <sys/mman.h>
#include <stdio.h>

int main(void)
{
   void *(*map_fn)(void *, size_t, int, int, int, off_t) = mmap;
   size_t size = 65536;
   unsigned char *data = (unsigned char *)map_fn(NULL, size,
         PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

   if (data == MAP_FAILED)
      return 1;
   data[0] = 0x12;
   data[size - 1] = 0x34;
   if (data[0] != 0x12 || data[size - 1] != 0x34)
      return 1;
#ifdef __EMSCRIPTEN__
   /* mprotect resolves to libc here as well, so memprotect() has to be
    * checked alongside mmap: it is the other call whose implementation
    * the header selection decides. */
   if (memprotect(data, size) != 0)
      return 1;
#endif
   if (munmap(data, size) != 0)
      return 1;
#ifdef __EMSCRIPTEN__
   if (mempagesize() != 0 || memreserve(size) != NULL ||
       memcommit(NULL, 0) || memrearm(NULL, 0) ||
       memsync(NULL, NULL) != 0)
      return 1;
#endif
   puts("PASS: libc mmap signature and anonymous mapping");
   return 0;
}
