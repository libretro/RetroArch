/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memmap.h).
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

#ifndef _LIBRETRO_MEMMAP_H
#define _LIBRETRO_MEMMAP_H

#include <stdio.h>
#include <stdint.h>

#if defined(PSP) || defined(PS2) || defined(GEKKO) || defined(VITA) || defined(_XBOX) || defined(_3DS) || defined(WIIU) || defined(SWITCH) || defined(HAVE_LIBNX) || defined(__PS3__) || defined(__PSL1GHT__)
/* No mman available */
#elif defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#include <errno.h>
#include <io.h>
#else
#define HAVE_MMAN
#include <sys/mman.h>
#endif

#if !defined(HAVE_MMAN) || defined(_WIN32)
/* PROT_/MAP_ request bits for the shim platforms (real <sys/mman.h>
 * provides them elsewhere).  Only the combinations the shim can
 * honour are defined. */
#ifndef PROT_NONE
#define PROT_NONE       0x0
#endif
#ifndef PROT_READ
#define PROT_READ       0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE      0x2
#endif
#ifndef PROT_EXEC
#define PROT_EXEC       0x4
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE     0x02
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS   0x20
#endif
#ifndef MAP_ANON
#define MAP_ANON        MAP_ANONYMOUS
#endif
#ifndef MAP_FAILED
#define MAP_FAILED      ((void *)-1)
#endif
void* mmap(void *addr, size_t len, int mmap_prot, int mmap_flags, int fildes, size_t off);

int munmap(void *addr, size_t len);

int mprotect(void *addr, size_t len, int prot);
#endif

int memsync(void *start, void *end);

int memprotect(void *addr, size_t len);

#endif
