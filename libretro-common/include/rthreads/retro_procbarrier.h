/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_procbarrier.h).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_PROCBARRIER_H
#define __LIBRETRO_SDK_PROCBARRIER_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/*
 * retro_procbarrier: a process-wide memory barrier, resolved at runtime.
 *
 * WHAT IT IS FOR
 *
 * A normal fence orders the calling thread's own memory operations. A
 * process-wide barrier does more: when it returns, every other thread of
 * this process that was running has passed through a full fence too, so
 * any store one of them made before this call is visible to the caller
 * afterwards. That lets a handshake be asymmetric. The side that runs
 * constantly -- a producer publishing work -- can use a plain store and
 * a plain load, with no locked instruction and no fence, while the side
 * that runs rarely -- a consumer about to park -- pays for this call
 * once before it commits to sleeping. The producer's store cannot slip
 * past the consumer's decision, because the barrier drains it first.
 *
 * On a producer/consumer pair that is exactly the shape of a core thread
 * pushing frames to a render thread, and the difference on the producer
 * side is a locked RMW against three ordinary moves.
 *
 * WHAT IT COSTS
 *
 * Nothing in user mode can make another core execute a fence, so every
 * implementation of this is a request to the kernel to interrupt the
 * other cores. It is a syscall at best and a signal per thread at worst,
 * so it belongs on the cold side of a protocol only. Never call it on a
 * hot path.
 *
 * HOW IT IS IMPLEMENTED
 *
 * By whichever of these the running system supports, chosen once at
 * retro_procbarrier_init():
 *
 *   RETRO_PROCBARRIER_UNIPROCESSOR
 *      One CPU: a store is globally visible with nothing to reorder
 *      against, so the barrier is a compiler fence and nothing else.
 *
 *   RETRO_PROCBARRIER_MEMBARRIER
 *      Linux 4.14+, FreeBSD 14.1+: membarrier(MEMBARRIER_CMD_PRIVATE_
 *      EXPEDITED). The kernel sends an IPI to exactly the CPUs currently
 *      running a thread of this process. The best tier where it exists.
 *
 *   RETRO_PROCBARRIER_FLUSHWRITEBUFFERS
 *      Windows Vista+: FlushProcessWriteBuffers(). One syscall; the
 *      kernel IPIs the process's affinity. Equivalent to membarrier.
 *
 *   RETRO_PROCBARRIER_PAGEFLIP
 *      x86 only, any Linux, any macOS on Intel, Windows back to 2000:
 *      change the protection of a locked dummy page. The kernel must
 *      invalidate its TLB entry on every CPU the address space is active
 *      on, and on x86 that invalidation travels as an IPI whose handler
 *      is a serialising event. The barrier is a side effect and is not
 *      documented anywhere; it is nonetheless what .NET and the JVM used
 *      before the kernels grew a real call. It is REFUSED on non-x86: ARM
 *      and PowerPC broadcast TLB invalidation without an IPI, and the
 *      trick fences nothing there.
 *
 *   RETRO_PROCBARRIER_SIGNAL
 *      Any Linux, any macOS including Apple Silicon, any Android: send a
 *      signal to every thread currently running on another CPU and wait
 *      for each to acknowledge from its handler. The kernel delivers a
 *      signal to a running thread by IPI, and entering the kernel is a
 *      fence on every architecture. Threads that are not on a CPU are
 *      skipped -- a context switch already drained them. Cost is a
 *      syscall per running thread; correct everywhere, fastest nowhere.
 *
 *   RETRO_PROCBARRIER_NONE
 *      Nothing available. retro_procbarrier() returns 0 and the caller
 *      must use a symmetric primitive instead. This is where the
 *      multi-core consoles land.
 *
 * The signal tier needs a signal number. The default is SIGRTMAX-1 where
 * real-time signals exist and SIGUSR2 otherwise; pass another to init if
 * the application uses those.
 */

enum retro_procbarrier_tier
{
   RETRO_PROCBARRIER_NONE = 0,
   RETRO_PROCBARRIER_UNIPROCESSOR,
   RETRO_PROCBARRIER_MEMBARRIER,
   RETRO_PROCBARRIER_FLUSHWRITEBUFFERS,
   RETRO_PROCBARRIER_PAGEFLIP,
   RETRO_PROCBARRIER_SIGNAL
};

/* Resolve the tier for this process. Safe to call more than once; the
 * first call wins. signum is only used by the signal tier; pass 0 for
 * the default. Returns the tier selected. */
enum retro_procbarrier_tier retro_procbarrier_init(int signum);

/* The tier in use, without initialising. RETRO_PROCBARRIER_NONE before
 * init has run. */
enum retro_procbarrier_tier retro_procbarrier_tier(void);

/* A human-readable name for a tier, for logs. */
const char *retro_procbarrier_tier_name(enum retro_procbarrier_tier tier);

/* Issue a process-wide barrier. Returns 1 if one was issued (or was
 * unnecessary), 0 if this system cannot issue one -- in which case the
 * caller must not rely on asymmetric ordering. Initialises on first use
 * with the default signal if init was not called. */
int retro_procbarrier(void);

RETRO_END_DECLS

#endif
