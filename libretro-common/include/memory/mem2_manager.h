/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mem2_manager.h).
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

/* MEM2 allocation for GameCube and Wii (libogc).
 *
 * The Wii has 24 MiB of MEM1 and 64 MiB of MEM2; newlib's malloc only
 * ever sees MEM1. This puts a heap over the free part of Arena2 and
 * interposes on the standard allocator through the linker's --wrap, so
 * an allocation that MEM1 cannot satisfy falls through to MEM2 rather
 * than failing. The wrap flags live in the platform makefiles
 * (Makefile.wii, Makefile.griffin); without them this file's
 * __wrap_* functions are simply never called and nothing changes.
 *
 * gx_init_mem2() must run before any allocation that might need MEM2.
 *
 * This lives in libretro-common rather than in the frontend because it
 * is platform allocation, not application policy, and because the
 * memory accounting below is the honest answer to "how much memory is
 * free" on these consoles: SYS_GetArena1Size() covers MEM1, and the
 * MEM2 half is only knowable from this heap.
 */

#ifndef _LIBRETRO_SDK_MEM2_MANAGER_H
#define _LIBRETRO_SDK_MEM2_MANAGER_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * gx_init_mem2:
 *
 * Places a heap over the free portion of Arena2, less a reserve for
 * network and USB. Call once, before anything that may allocate from
 * MEM2.
 *
 * Returns: true on success.
 */
bool gx_init_mem2(void);

/**
 * gx_mem2_used:
 *
 * Returns: bytes currently allocated from the MEM2 heap.
 */
uint32_t gx_mem2_used(void);

/**
 * gx_mem2_total:
 *
 * Returns: the MEM2 heap's total size, used plus free.
 */
uint32_t gx_mem2_total(void);

RETRO_END_DECLS

#endif
