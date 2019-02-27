/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*=============================================================================
VideoCore OS Abstraction Layer - common postamble code
=============================================================================*/

/** \file
  *
  * Postamble code included by the platform-specific header files
  */

#define VCOS_THREAD_PRI_DEFAULT VCOS_THREAD_PRI_NORMAL

#if !defined(VCOS_THREAD_PRI_INCREASE)
#error Which way to thread priorities go?
#endif

#if VCOS_THREAD_PRI_INCREASE < 0
/* smaller numbers are higher priority */
#define VCOS_THREAD_PRI_LESS(x) ((x)<VCOS_THREAD_PRI_MAX?(x)+1:VCOS_THREAD_PRI_MAX)
#define VCOS_THREAD_PRI_MORE(x) ((x)>VCOS_THREAD_PRI_MIN?(x)-1:VCOS_THREAD_PRI_MIN)
#else
/* bigger numbers are lower priority */
#define VCOS_THREAD_PRI_MORE(x) ((x)<VCOS_THREAD_PRI_MAX?(x)+1:VCOS_THREAD_PRI_MAX)
#define VCOS_THREAD_PRI_LESS(x) ((x)>VCOS_THREAD_PRI_MIN?(x)-1:VCOS_THREAD_PRI_MIN)
#endif

/* Convenience for Brits: */
#define VCOS_APPLICATION_INITIALISE VCOS_APPLICATION_INITIALIZE

/*
 * Check for constant definitions
 */
#ifndef VCOS_TICKS_PER_SECOND
#error VCOS_TICKS_PER_SECOND not defined
#endif

#if !defined(VCOS_THREAD_PRI_MIN) || !defined(VCOS_THREAD_PRI_MAX)
#error Priority range not defined
#endif

#if !defined(VCOS_THREAD_PRI_HIGHEST) || !defined(VCOS_THREAD_PRI_LOWEST) || !defined(VCOS_THREAD_PRI_NORMAL)
#error Priority ordering not defined
#endif

#if !defined(VCOS_CAN_SET_STACK_ADDR)
#error Can stack addresses be set on this platform? Please set this macro to either 0 or 1.
#endif

#if (_VCOS_AFFINITY_CPU0|_VCOS_AFFINITY_CPU1) & (~_VCOS_AFFINITY_MASK)
#error _VCOS_AFFINITY_CPUxxx values are not consistent with _VCOS_AFFINITY_MASK
#endif

/** Append to the end of a singly-linked queue, O(1). Works with
  * any structure where list has members 'head' and 'tail' and
  * item has a 'next' pointer.
  */
#define VCOS_QUEUE_APPEND_TAIL(list, item) {\
   (item)->next = NULL;\
   if (!(list)->head) {\
      (list)->head = (list)->tail = (item); \
   } else {\
      (list)->tail->next = (item); \
      (list)->tail = (item); \
   } \
}

#ifndef VCOS_HAVE_TIMER
VCOSPRE_ void VCOSPOST_ vcos_timer_init(void);
#endif
