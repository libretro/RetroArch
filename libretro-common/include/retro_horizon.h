/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_horizon.h).
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_RETRO_HORIZON_H
#define __LIBRETRO_SDK_RETRO_HORIZON_H

/* Horizon (Switch, Switch 2) primitives taken straight from the CPU
 * and the kernel's supervisor-call ABI, so they do not depend on what
 * a given SDK (libnx, libounce) exports. Each writes its result into
 * the lvalue it is given. aarch64 GCC only. */

#include <stdint.h>

/* The system counter, cntpct_el0 */
#define RETRO_HORIZON_TICK(out) \
   __asm__ __volatile__ ("mrs %0, cntpct_el0" : "=r" (out))

/* The system counter's rate in Hz, cntfrq_el0: 19.2 MHz on the
 * Switch, 31.25 MHz on the Switch 2 */
#define RETRO_HORIZON_TICK_FREQ(out) \
   __asm__ ("mrs %0, cntfrq_el0" : "=r" (out))

/* What a supervisor call may leave changed: every caller-saved
 * register, as across a call to an SDK's svc wrapper */
#define RETRO_HORIZON_SVC_CLOBBERS \
   "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", \
   "x13", "x14", "x15", "x16", "x17", "x18", "cc", "memory"

/* The core the calling thread runs on (svc 0x10) */
#define RETRO_HORIZON_CURRENT_CORE(out) \
   do { \
      register uint64_t retro_hz_x0_ __asm__ ("x0"); \
      __asm__ __volatile__ ("svc 0x10" : "=r" (retro_hz_x0_) \
            : : "x1", "x2", "x3", RETRO_HORIZON_SVC_CLOBBERS); \
      (out) = (int)(uint32_t)retro_hz_x0_; \
   } while (0)

/* The cores the process may run on (svc 0x29, GetInfo: InfoType 0 on
 * the current-process pseudo-handle); @rc is the kernel's result, 0
 * on success */
#define RETRO_HORIZON_CORE_MASK(rc, out) \
   do { \
      register uint64_t retro_hz_x0_ __asm__ ("x0"); \
      register uint64_t retro_hz_x1_ __asm__ ("x1") = 0; \
      register uint64_t retro_hz_x2_ __asm__ ("x2") = 0xFFFF8001u; \
      register uint64_t retro_hz_x3_ __asm__ ("x3") = 0; \
      __asm__ __volatile__ ("svc 0x29" \
            : "=r" (retro_hz_x0_), "+r" (retro_hz_x1_), \
              "+r" (retro_hz_x2_), "+r" (retro_hz_x3_) \
            : : RETRO_HORIZON_SVC_CLOBBERS); \
      (rc)  = (uint32_t)retro_hz_x0_; \
      (out) = retro_hz_x1_; \
   } while (0)

#endif
