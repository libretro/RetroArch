/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
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
