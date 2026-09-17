/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (faulthandler.h).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* faulthandler: a page fault handed to the core that caused it.
 *
 * A recompiler with a fastmem window reads and writes guest memory with
 * ordinary loads and stores into a reserved region, and lets the MMU
 * tell it when a page is not there: the fault handler maps the page, or
 * rewrites the offending instruction to a slow path, and execution
 * resumes at the same instruction. That is a normal part of running,
 * not a crash, and it happens on the recompiler's own threads while
 * every other thread's faults must go on meaning what they meant.
 *
 * What this provides is the platform half: a filter installed on
 * SIGSEGV and SIGBUS, or a vectored exception handler on Windows, that
 * identifies the faulting thread, hands the pc and the faulting page to
 * the callback, and resumes if it says it handled the fault. Anything
 * it does not claim -- a different thread, a fault inside the callback,
 * a callback that declines -- goes to the handler that was installed
 * before, so a real crash still produces the core dump it would have.
 *
 * The callback runs in a signal handler. It may touch only
 * async-signal-safe things: no malloc, no stdio, no locks that a
 * non-handler path can hold. Mapping a page and patching code are fine;
 * logging is not.
 *
 * Threads that run such code call retro_faulthandler_register_thread
 * once, and unregister before they exit. The registry is a small fixed
 * table scanned without locks, and the identity is the platform's
 * thread id rather than a thread-local: a core is usually a shared
 * object, whose thread-locals are global-dynamic, and the first access
 * on a thread can allocate -- inside a signal handler, at exactly the
 * wrong moment.
 */

#ifndef __LIBRETRO_SDK_FAULTHANDLER_H__
#define __LIBRETRO_SDK_FAULTHANDLER_H__

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* How many threads may be registered at once. A recompiler has a few:
 * the CPU thread and its worker threads. */
#ifndef RETRO_FAULT_MAX_THREADS
#define RETRO_FAULT_MAX_THREADS 4
#endif

typedef struct retro_fault_info
{
   /* Where the faulting instruction is, or NULL where the platform
    * does not report it. */
   uintptr_t pc;
   /* The faulting address, rounded down to a page. */
   uintptr_t addr;
} retro_fault_info_t;

/**
 * retro_fault_handler_t:
 *
 * Returns true when the fault was handled and execution should resume
 * at the faulting instruction; false to pass it on to whatever handled
 * such faults before.
 */
typedef bool (*retro_fault_handler_t)(const retro_fault_info_t *info);

/**
 * retro_faulthandler_install:
 * @handler    : the callback, which must be async-signal-safe.
 *
 * Installs the filter, chaining whatever was installed before. One
 * handler at a time; installing a second while one is active fails.
 *
 * Returns: true on success.
 */
bool retro_faulthandler_install(retro_fault_handler_t handler);

/**
 * retro_faulthandler_remove:
 * @handler    : the callback given to install, checked against the
 *               active one.
 *
 * Removes the filter and restores what was there before. Call it with
 * the threads that fault quiesced: the filter itself takes no lock, so
 * there is nothing to serialise against a fault in flight.
 */
void retro_faulthandler_remove(retro_fault_handler_t handler);

/**
 * retro_faulthandler_register_thread:
 *
 * Registers the calling thread as one whose faults the handler should
 * see. Idempotent. Returns false when the table is full, and a thread
 * that is not registered is not a thread this filter claims.
 */
bool retro_faulthandler_register_thread(void);

/**
 * retro_faulthandler_unregister_thread:
 *
 * Before the calling thread exits.
 */
void retro_faulthandler_unregister_thread(void);

RETRO_END_DECLS

#endif
