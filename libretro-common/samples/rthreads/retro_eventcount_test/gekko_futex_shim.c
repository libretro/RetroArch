/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gekko_futex_shim.c).
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
/* os/gekko's futex, stood in for over Linux's so the GameCube and Wii
 * backend of retro_eventcount runs on a Linux host.  The time base runs
 * at the Wii's rate, so the tick conversion is the real one.
 * HOSTGEKKO_LOSE_WAKES makes every wake a no-op, which the suite must
 * not survive. */

#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <time.h>

#include "hostgekko/gekko/thread.h"

uint32_t gk_tb_hz = 60750000u;

int gk_futex_wait(volatile uint32_t *addr, uint32_t expected,
      uint64_t timeout)
{
   struct timespec ts, *tp = NULL;
   if (timeout != GK_WAIT_FOREVER)
   {
      uint64_t ns = timeout / gk_tb_hz * 1000000000u
         + timeout % gk_tb_hz * 1000000000u / gk_tb_hz;
      ts.tv_sec  = (time_t)(ns / 1000000000u);
      ts.tv_nsec = (long)(ns % 1000000000u);
      tp         = &ts;
   }
   if (syscall(SYS_futex, (void*)addr, FUTEX_WAIT_PRIVATE, expected, tp,
            NULL, 0) == 0)
      return GK_OK;
   if (errno == ETIMEDOUT)
      return GK_ETIMEDOUT;
   /* EAGAIN: the word had moved; EINTR: woken early, as os/gekko may */
   return errno == EAGAIN ? GK_EAGAIN : GK_OK;
}

int gk_futex_wake(volatile uint32_t *addr, int count)
{
   if (getenv("HOSTGEKKO_LOSE_WAKES"))
      return 0;
   return (int)syscall(SYS_futex, (void*)addr, FUTEX_WAKE_PRIVATE,
         count, NULL, NULL, 0);
}
