/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (memcpy_nt.h).
 * ---------------------------------------------------------------------
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBRETRO_MEMCPY_NT_H
#define _LIBRETRO_MEMCPY_NT_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * memcpy_nt:
 *
 * memcpy for write-once destinations.  Where the platform supports it
 * (SSE2, AArch64), the bulk of the copy is performed with non-temporal
 * or non-allocating stores, so the destination does not displace the
 * caller's working set from the cache.  A plain frame blit through
 * memcpy makes the CPU read every destination line from memory before
 * overwriting all of it (read-for-ownership) and then keeps those
 * lines resident; for a buffer that will next be read by the GPU or
 * display engine, both effects are pure waste.
 *
 * Semantics are exactly memcpy's: any size, any alignment, regions
 * must not overlap.  On platforms without a streaming-store path this
 * is memcpy.  Ordering against later stores is taken care of
 * internally (sfence on x86); callers need no barrier of their own.
 *
 * Worth using when the destination is large (multiple KB), will not
 * be read back by the CPU soon, and cache pressure matters -- frame
 * blits into display surfaces being the canonical case.  For small or
 * soon-re-read destinations, plain memcpy is the right call.
 */
void *memcpy_nt(void *dst, const void *src, size_t len);

RETRO_END_DECLS

#endif
