/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbuf.h).
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

#ifndef __LIBRETRO_SDK_ARRAY_RBUF_H__
#define __LIBRETRO_SDK_ARRAY_RBUF_H__

/*
 * This file implements stretchy buffers as invented (?) by Sean Barrett.
 * Based on the implementation from the public domain Bitwise project
 * by Per Vognsen - https://github.com/pervognsen/bitwise
 *
 * It's a super simple type safe dynamic array for C with no need
 * to predeclare any type or anything.
 * The first time an element is added, memory for 16 elements are allocated.
 * Then every time length is about to exceed capacity, capacity is doubled.
 *
 * Be careful not to supply modifying statements to the macro arguments.
 * Something like RBUF_REMOVE(buf, i--); would have unintended results.
 *
 * Sample usage:
 *
 * mytype_t* buf = NULL;
 * RBUF_PUSH(buf, some_element);
 * RBUF_PUSH(buf, other_element);
 * -- now RBUF_LEN(buf) == 2, buf[0] == some_element, buf[1] == other_element
 *
 * -- Free allocated memory:
 * RBUF_FREE(buf);
 * -- now buf == NULL, RBUF_LEN(buf) == 0, RBUF_CAP(buf) == 0
 *
 * -- Explicitly increase allocated memory and set capacity:
 * RBUF_FIT(buf, 100);
 * -- now RBUF_LEN(buf) == 0, RBUF_CAP(buf) == 100
 *
 * -- Resize buffer (does not initialize or zero memory!)
 * RBUF_RESIZE(buf, 200);
 * -- now RBUF_LEN(buf) == 200, RBUF_CAP(buf) == 200
 *
 * -- To handle running out of memory:
 * bool ran_out_of_memory = !RBUF_TRYFIT(buf, 1000);
 * -- before RESIZE or PUSH. When out of memory, buf will stay unmodified.
 */

#include <retro_math.h> /* for MAX */
#include <stdlib.h> /* for malloc, realloc */

#define RBUF__HDR(b) (((struct rbuf__hdr *)(b))-1)

#define RBUF_LEN(b) ((b) ? RBUF__HDR(b)->len : 0)
#define RBUF_CAP(b) ((b) ? RBUF__HDR(b)->cap : 0)
#define RBUF_END(b) ((b) + RBUF_LEN(b))
#define RBUF_SIZEOF(b) ((b) ? RBUF_LEN(b)*sizeof(*b) : 0)

#define RBUF_FREE(b) ((b) ? (free(RBUF__HDR(b)), (b) = NULL) : 0)
#define RBUF_FIT(b, n) ((size_t)(n) <= RBUF_CAP(b) ? 0 : (*(void**)(&(b)) = rbuf__grow((b), (n), sizeof(*(b)))))
#define RBUF_PUSH(b, val) (RBUF_FIT((b), 1 + RBUF_LEN(b)), (b)[RBUF__HDR(b)->len++] = (val))
#define RBUF_POP(b) (b)[--RBUF__HDR(b)->len]
#define RBUF_RESIZE(b, sz) (RBUF_FIT((b), (sz)), ((b) ? RBUF__HDR(b)->len = (sz) : 0))
#define RBUF_CLEAR(b) ((b) ? RBUF__HDR(b)->len = 0 : 0)
#define RBUF_TRYFIT(b, n) (RBUF_FIT((b), (n)), (((b) && RBUF_CAP(b) >= (size_t)(n)) || !(n)))
#define RBUF_REMOVE(b, idx) memmove((b) + (idx), (b) + (idx) + 1, (--RBUF__HDR(b)->len - (idx)) * sizeof(*(b)))

struct rbuf__hdr
{
   size_t len;
   size_t cap;
};

#ifdef __GNUC__
__attribute__((__unused__))
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4505) /* Unreferenced local function has been removed */
#endif
static void *rbuf__grow(void *buf,
      size_t new_len, size_t elem_size)
{
   struct rbuf__hdr *new_hdr;
   size_t new_cap  = MAX(2 * RBUF_CAP(buf), MAX(new_len, 16));
   size_t new_size = sizeof(struct rbuf__hdr) + new_cap*elem_size;
   if (buf)
   {
      new_hdr      = (struct rbuf__hdr *)realloc(RBUF__HDR(buf), new_size);
      if (!new_hdr)
         return buf; /* out of memory, return unchanged */
   }
   else
   {
      new_hdr      = (struct rbuf__hdr *)malloc(new_size);
      if (!new_hdr)
         return NULL; /* out of memory */
      new_hdr->len = 0;
   }
   new_hdr->cap    = new_cap;
   return new_hdr + 1;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
