/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (switch_arbiter_shim.c).
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

/* The Horizon address arbiter, stood in for over futex, so the Switch
 * backend of retro_eventcount runs on a Linux host.  The contract kept
 * is the one the backend relies on, as libnx documents it:
 *
 *   svcWaitForAddress(WaitIfEqual)  returns InvalidState at once when
 *                                   the word no longer holds the value,
 *                                   TimedOut when the bound (in
 *                                   nanoseconds, negative for none)
 *                                   passes, and 0 on a signal;
 *   svcSignalToAddress(Signal, -1)  wakes every waiter on the word.
 *
 * The version query answers as a console on current firmware would;
 * HOSTNX_OLD_FIRMWARE in the environment answers as one before 4.0.0,
 * which is the fallback the backend must then take. */

#define _GNU_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <time.h>

#include "hostnx/switch.h"

Result svcWaitForAddress(void *address, u32 arb_type, s64 value, s64 timeout)
{
   struct timespec ts;
   struct timespec *tp = NULL;
   (void)arb_type;   /* only WaitIfEqual is asked for */
   if (timeout >= 0)
   {
      ts.tv_sec  = (time_t)(timeout / 1000000000);
      ts.tv_nsec = (long)(timeout % 1000000000);
      tp = &ts;
   }
   for (;;)
   {
      if (syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE,
               (int)value, tp, NULL, 0) == 0)
         return 0;
      if (errno == EAGAIN)
         return KERNELRESULT(InvalidState);
      if (errno == ETIMEDOUT)
         return KERNELRESULT(TimedOut);
      /* EINTR: the arbiter does not return for a signal; wait on */
   }
}

Result svcSignalToAddress(void *address, u32 signal_type, s32 value, s32 count)
{
   (void)signal_type; (void)value;
   syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE,
         count < 0 ? INT_MAX : count, NULL, NULL, 0);
   return 0;
}

bool hosversionAtLeast(u8 major, u8 minor, u8 micro)
{
   (void)minor; (void)micro;
   if (getenv("HOSTNX_OLD_FIRMWARE"))
      return major < 4;
   return true;
}
