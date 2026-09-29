/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (switch.h).
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

/* The slice of libnx's <switch.h> that retro_eventcount's arbiter
 * backend uses, for building that backend on a Linux host: the types,
 * the result macros and the two system calls, with the values libnx
 * gives them.  The calls are stood in for over futex in
 * switch_arbiter_shim.c. */

#ifndef RETRO_EC_HOSTNX_SWITCH_H
#define RETRO_EC_HOSTNX_SWITCH_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t Result;
typedef int64_t  s64;
typedef int32_t  s32;
typedef uint32_t u32;
typedef uint8_t  u8;

#define R_VALUE(res)  (res)
#define R_FAILED(res) ((res) != 0)
#define R_SUCCEEDED(res) ((res) == 0)

#define Module_Kernel 1
#define MAKERESULT(module, description) \
   ((((module) & 0x1FF)) | ((description) & 0x1FFF) << 9)
#define KERNELRESULT(description) \
   MAKERESULT(Module_Kernel, KernelError_##description)

enum
{
   KernelError_TimedOut     = 117,
   KernelError_InvalidState = 125
};

typedef enum
{
   ArbitrationType_WaitIfLessThan             = 0,
   ArbitrationType_DecrementAndWaitIfLessThan = 1,
   ArbitrationType_WaitIfEqual                = 2
} ArbitrationType;

typedef enum
{
   SignalType_Signal                                          = 0,
   SignalType_SignalAndIncrementIfEqual                       = 1,
   SignalType_SignalAndModifyBasedOnWaitingThreadCountIfEqual = 2
} SignalType;

Result svcWaitForAddress(void *address, u32 arb_type, s64 value, s64 timeout);
Result svcSignalToAddress(void *address, u32 signal_type, s32 value, s32 count);
bool   hosversionAtLeast(u8 major, u8 minor, u8 micro);

#endif
