/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (svc.h).
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
/* The slice of libctru's <3ds/svc.h> that retro_eventcount's arbiter
 * backend uses, for building that backend on a Linux host: the types
 * and the arbitration kinds, with the values libctru gives them.  The
 * calls are stood in for over futex in ctr_arbiter_shim.c. */
#ifndef RETRO_EC_HOSTCTR_SVC_H
#define RETRO_EC_HOSTCTR_SVC_H
#include <stdint.h>

typedef int32_t  Result;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint32_t u32;

typedef enum
{
   ARBITRATION_SIGNAL                                  = 0,
   ARBITRATION_WAIT_IF_LESS_THAN                       = 1,
   ARBITRATION_DECREMENT_AND_WAIT_IF_LESS_THAN         = 2,
   ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT               = 3,
   ARBITRATION_DECREMENT_AND_WAIT_IF_LESS_THAN_TIMEOUT = 4
} ArbitrationType;

#define ARBITRATION_SIGNAL_ALL (-1)

#endif
