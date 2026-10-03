/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ctr_arbiter_shim.c).
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
/* libctru's two arbiter wrappers, stood in for over futex so the 3DS
 * backend of retro_eventcount runs on a Linux host.  The 3DS arbiter
 * waits while the word is strictly below the value given; futex waits
 * while it equals one, so the wait is re-armed on the value it read
 * for as long as that value is below the bound. */

#define _GNU_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <time.h>

#include "hostctr/3ds/synchronization.h"
#include "hostctr/3ds/result.h"

static Result ctr_wait(s32 *addr, s32 value, const struct timespec *tp)
{
   for (;;)
   {
      s32 v = __atomic_load_n(addr, __ATOMIC_ACQUIRE);
      if (!(v < value))
         return 0;
      if (syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, v, tp, NULL, 0) == 0)
         return 0;
      if (errno == ETIMEDOUT)
         return (Result)RD_TIMEOUT;
      /* EAGAIN: the word moved, re-read; EINTR: wait on */
   }
}

Result syncArbitrateAddress(s32 *addr, ArbitrationType type, s32 value)
{
   if (type == ARBITRATION_SIGNAL)
   {
      syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE,
            value < 0 ? INT_MAX : value, NULL, NULL, 0);
      return 0;
   }
   return ctr_wait(addr, value, NULL);
}

Result syncArbitrateAddressWithTimeout(s32 *addr, ArbitrationType type,
      s32 value, s64 timeout_ns)
{
   struct timespec ts;
   if (type == ARBITRATION_SIGNAL)
      return syncArbitrateAddress(addr, type, value);
   ts.tv_sec  = (time_t)(timeout_ns / 1000000000);
   ts.tv_nsec = (long)(timeout_ns % 1000000000);
   return ctr_wait(addr, value, &ts);
}
