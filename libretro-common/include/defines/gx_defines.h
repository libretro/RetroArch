/* Copyright (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gx_defines.h).
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

#ifndef _GX_DEFINES_H
#define _GX_DEFINES_H

#ifdef GEKKO

#define SYSMEM1_SIZE 0x01800000

#define _SHIFTL(v, s, w)	((uint32_t) (((uint32_t)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	((uint32_t)(((uint32_t)(v) >> (s)) & ((0x01 << (w)) - 1)))

#define OSThread lwp_t
#define OSCond lwpq_t
#define OSThreadQueue lwpq_t

#define OSInitMutex(mutex) LWP_MutexInit(mutex, 0)
#define OSLockMutex(mutex) LWP_MutexLock(mutex)
#define OSUnlockMutex(mutex) LWP_MutexUnlock(mutex)
#define OSTryLockMutex(mutex) LWP_MutexTryLock(mutex)

#define OSInitCond(cond) LWP_CondInit(cond)
#define OSSignalCond(cond) LWP_ThreadSignal(cond)
#define OSWaitCond(cond, mutex) LWP_CondWait(cond, mutex)

#define OSInitThreadQueue(queue) LWP_InitQueue(queue)
#define OSCloseThreadQueue(queue) LWP_CloseQueue(queue)
#define OSSleepThread(queue) LWP_ThreadSleep(queue)
#define OSJoinThread(thread, val) LWP_JoinThread(thread, val)

#define OSCreateThread(thread, func, intarg, ptrarg, stackbase, stacksize, priority, attrs) LWP_CreateThread(thread, func, ptrarg, stackbase, stacksize, priority)

#define BLIT_LINE_16(off) \
{ \
   const uint32_t *tmp_src = src; \
   uint32_t       *tmp_dst = dst; \
   for (unsigned x = 0; x < width2 >> 1; x++, tmp_src += 2, tmp_dst += 8) \
   { \
      tmp_dst[ 0 + off] = BLIT_LINE_16_CONV(tmp_src[0]); \
      tmp_dst[ 1 + off] = BLIT_LINE_16_CONV(tmp_src[1]); \
   } \
   src += tmp_pitch; \
}

#define BLIT_LINE_32(off) \
{ \
   const uint16_t *tmp_src = src; \
   uint16_t       *tmp_dst = dst; \
   for (unsigned x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
   { \
      tmp_dst[  0 + off] = tmp_src[0] | 0xFF00; \
      tmp_dst[ 16 + off] = tmp_src[1]; \
      tmp_dst[  1 + off] = tmp_src[2] | 0xFF00; \
      tmp_dst[ 17 + off] = tmp_src[3]; \
      tmp_dst[  2 + off] = tmp_src[4] | 0xFF00; \
      tmp_dst[ 18 + off] = tmp_src[5]; \
      tmp_dst[  3 + off] = tmp_src[6] | 0xFF00; \
      tmp_dst[ 19 + off] = tmp_src[7]; \
   } \
   src += tmp_pitch; \
}

#define CHUNK_FRAMES 64
#define CHUNK_SIZE (CHUNK_FRAMES * sizeof(uint32_t))
#define BLOCKS 16

#define AIInit AUDIO_Init
#define AIInitDMA AUDIO_InitDMA
#define AIStartDMA AUDIO_StartDMA
#define AIStopDMA AUDIO_StopDMA
#define AIRegisterDMACallback AUDIO_RegisterDMACallback
#define AISetDSPSampleRate AUDIO_SetDSPSampleRate

#endif

#endif
