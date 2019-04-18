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

#ifndef VCFW_VCLIB_H
#define VCFW_VCLIB_H

 #ifdef __VIDEOCORE__

#include "vcfw/rtos/rtos.h"
#include "vcfw/logging/logging.h"

int32_t vclib_vcfw_init( void );
int32_t vclib_vcfw_driver_init( void );

typedef enum {
   FATAL_RED = 1,
   FATAL_GREEN,
   FATAL_BLUE,
   FATAL_CYAN,
   FATAL_MAGENTA,
   FATAL_YELLOW,
   FATAL_WHITE,
   FATAL_BLACK,

   FATAL_SUB_RED     = FATAL_RED     << 4,
   FATAL_SUB_GREEN   = FATAL_GREEN   << 4,
   FATAL_SUB_BLUE    = FATAL_BLUE    << 4,
   FATAL_SUB_CYAN    = FATAL_CYAN    << 4,
   FATAL_SUB_MAGENTA = FATAL_MAGENTA << 4,
   FATAL_SUB_YELLOW  = FATAL_YELLOW  << 4,
   FATAL_SUB_WHITE   = FATAL_WHITE   << 4,
   FATAL_SUB_BLACK   = FATAL_BLACK   << 4,
} FATAL_COLOUR;

void vclib_fatal_fn(FATAL_COLOUR colour);
  #ifdef FATAL_ASSERTS
#define vclib_fatal_assert(x, c) if (x) {} else vclib_fatal_fn(c)
  #else
#define vclib_fatal_assert(x, c) assert(x)
  #endif

extern void vclib_init();
extern int  vclib_obtain_VRF(int block);
extern void vclib_release_VRF(void);
extern int  vclib_check_VRF(void);

#define get_free_mem(pool) (rtos_get_free_mem(pool))

#define malloc_setpool( external)         rtos_malloc_setpool( external)
#define malloc_256bit( size)              rtos_malloc_256bit( size)
#define free_256bit(ret)                  rtos_free_256bit(ret)
#define realloc_256bit(ret,  size)        rtos_realloc_256bit(ret,  size)
#define malloc_external( size)            rtos_malloc_external( size)
#define malloc_external_256bit( size)     rtos_malloc_external_256bit( size )

#define malloc_priority( size,  align,  priority,  description) rtos_prioritymalloc( size,  align,  priority,  description)
#define calloc_priority( size,  align,  priority,  description) rtos_prioritycalloc( size,  align,  priority,  description)

#define vclib_setpriority rtos_setpriority
#define vclib_getpriority rtos_getpriority

#define vclib_prioritymalloc(size, align, priority, description) rtos_prioritymalloc(size, align, priority, description)
#define vclib_prioritycalloc(size, align, priority, description) rtos_prioritycalloc(size, align, priority, description)

#define vclib_priorityfree(buffer) rtos_priorityfree(buffer)
#define VCLIB_ALIGN_DEFAULT                  RTOS_ALIGN_DEFAULT
#define VCLIB_ALIGN_128BIT                   RTOS_ALIGN_128BIT
#define VCLIB_ALIGN_256BIT                   RTOS_ALIGN_256BIT
#define VCLIB_ALIGN_AXI                      RTOS_ALIGN_AXI
#define VCLIB_ALIGN_512BIT                   RTOS_ALIGN_512BIT
#define VCLIB_ALIGN_4KBYTE                   RTOS_ALIGN_4KBYTE
#define VCLIB_PRIORITY_INTERNAL              RTOS_PRIORITY_INTERNAL
#define VCLIB_PRIORITY_EXTERNAL              RTOS_PRIORITY_EXTERNAL
#define VCLIB_PRIORITY_UNIMPORTANT           RTOS_PRIORITY_UNIMPORTANT
#define VCLIB_PRIORITY_COHERENT              RTOS_PRIORITY_COHERENT
#define VCLIB_PRIORITY_DIRECT                RTOS_PRIORITY_DIRECT

extern void vclib_save_vrf( unsigned char *vrf_strorage );
extern void vclib_restore_vrf( unsigned char *vrf_strorage );
extern void vclib_save_quarter_vrf( unsigned char *vrf_strorage );
extern void vclib_restore_quarter_vrf( unsigned char *vrf_strorage );

extern void vclib_cache_flush(void);
extern void vclib_dcache_flush(void);
extern void vclib_icache_flush(void);
extern void vclib_dcache_flush_range(void *start_addr, int length);
extern void vclib_l1cache_flush_range(void *start_addr, int length);

/* Use these function to identify the condition where you don't care if the data is
 * written back -- if it becomes possible then we save ourselves the bother of
 * hunting for places where it should be done that way.
 */
extern void vclib_dcache_invalidate_range(void *start_addr, int length);
extern void vclib_l1cache_invalidate_range(void *start_addr, int length);

#define vclib_timer_t RTOS_TIMER_T
#define vclib_timer_done_op RTOS_TIMER_DONE_OPERATION_T
#define vclib_timer_time_t RTOS_TIMER_TIME_T
#define vclib_timer_ticks_t RTOS_TIMER_TICKS_T

#define vclib_timer_init rtos_timer_init
#define vclib_timer_is_running rtos_timer_is_running
#define vclib_timer_set rtos_timer_set
#define vclib_timer_reset rtos_timer_reset
#define vclib_timer_cancel rtos_timer_cancel

extern int vclib_disableint(void);
extern void vclib_restoreint(int sr);

/* Some support for automatic logging. */

/* Have a macro that is guaranteed to be a simple "assert" even when logging is on. */
  #ifdef NDEBUG
#define simple_assert(cond)
  #else
#define simple_assert(cond) if (cond) {} else _bkpt()
  #endif

/*********
** HACK!!!!!! Temp function definition (code in cache_flush.s) used to enable the run domain in VCIII
**********/
extern void vc_enable_run_domain( void );

#define vclib_memory_is_valid rtos_memory_is_valid

/* Latch events are fast and small event notifications */
typedef RTOS_LATCH_T latch_event_t;

#define latch_event_present() ((latch_event_t)rtos_latch_unlocked())

#define latch_event_absent() ((latch_event_t)rtos_latch_locked())

#define latch_event_is_present( latch ) (latch_event_present() == *(latch))

  #if defined(__HIGHC__) /* MetaWare tools */

   static inline unsigned vclib_status_register(void)
   {
      /* Would have liked use == syntax for assigning a variable to be a register
      but the compiler issues a warning about that */
      return _vasm("mov %D,%sr");
   }
   static inline int vclib_interrupts_enabled(void)
   {
      unsigned sr = vclib_status_register();
      return !!(sr&(1<<30));
   }

  #else

   static inline unsigned vclib_status_register(void)
   {
      unsigned i;
      asm("mov %0, sr" : "=r"(i));
      return i;
   }
   static inline int vclib_interrupts_enabled(void)
   {
      unsigned sr = vclib_status_register();
      return !!(sr&(1<<30));
   }

  #endif

 #endif // defined __VIDEOCORE__

// A few helpful things for windows / standalone Linux
# ifndef __VIDEOCORE__

#  ifdef __GNUC__
    static inline int vclib_obtain_VRF(int block) { return 1; }
    static inline void vclib_release_VRF(void) {}
    static inline int vclib_check_VRF(void) { return 1; }
#  else
#   define vclib_obtain_VRF(b) (1)
#   define vclib_release_VRF()
#   define vclib_check_VRF() (1)
#  endif

#  ifdef VIDEOCORE_CODE_IN_SIMULATION

#   undef abs // prior to including stdlib.h
#   include <stdlib.h> // For malloc() et al.

#   define vclib_dcache_flush_range(start_addr, length)

#   define VCLIB_ALIGN_DEFAULT   4
#   define VCLIB_ALIGN_256BIT    32
#   define VCLIB_ALIGN_4KBYTE    4096

#   define VCLIB_PRIORITY_COHERENT      0
#   define VCLIB_PRIORITY_DIRECT        0
#   define VCLIB_PRIORITY_UNIMPORTANT   0

#   define vclib_fatal_assert(x, c) assert(x)

#   define vclib_prioritymalloc(size, align, priority, description) \
           rtos_prioritymalloc((size), (align), (priority), (description))
#   define vclib_prioritycalloc(size, align, priority, description) \
           rtos_prioritycalloc((size), (align), (priority), (description))

#   define vclib_priorityfree(buffer) rtos_priorityfree((buffer))
#   define malloc_256bit(size) rtos_malloc_256bit((size))
#   define free_256bit(ret) rtos_free_256bit((ret))

#  endif

# endif // !defined(__VIDEOCORE__)

#endif // defined VCFW_VCLIB_H
