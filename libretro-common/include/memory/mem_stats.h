/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mem_stats.h).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_MEM_STATS_H
#define __LIBRETRO_SDK_MEM_STATS_H

#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* How much memory the machine has, and how much of it is going spare.
 *
 * One answer for every platform, so that code deciding whether it can
 * afford something does not have to know which platform it is on. What
 * "free" means is necessarily the platform's own idea of it - a linear
 * allocator's remaining arena, a kernel's available pages, the space
 * left before a hard limit - and the number is a snapshot that another
 * thread can invalidate before the caller has read it.
 *
 * Both return 0 where the platform offers no way to ask. That is the
 * important case to handle: 0 means unknown, not empty, and a caller
 * sizing an admission test against it must fall back to a fixed policy
 * rather than conclude there is no memory. Everything in tree that
 * consults these treats 0 that way.
 *
 * Neither is guaranteed cheap. A platform with no accounting of its own
 * may have to probe for the answer, and one that reads it out of a file
 * pays for the read; a caller polling every frame should cache.
 */
uint64_t mem_stats_total(void);
uint64_t mem_stats_free(void);


RETRO_END_DECLS

#endif
