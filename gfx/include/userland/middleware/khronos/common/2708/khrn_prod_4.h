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

#ifndef KHRN_PROD_4_H
#define KHRN_PROD_4_H

#include "middleware/khronos/egl/egl_disp.h"
#include "middleware/khronos/common/khrn_hw.h"
#include "interface/khronos/common/khrn_int_util.h"
#if defined(SIMPENROSE)
   #include "tools/v3d/simpenrose/simpenrose.h"
   #ifdef SIMPENROSE_RECORD_OUTPUT
      #include "tools/v3d/simpenrose/record.h"
   #endif
#elif defined(ABSTRACT_PLATFORM)
   /* nothing included */
#else
   #include "vcfw/vclib/vclib.h"
   #include "hardware_alias.h"
#include "hardware_v3d.h"
#endif

#if defined(KHRN_CARBON)
#include "tools/v3d/carbon/carbon_lib/carbon_functions.h"
#endif

/******************************************************************************
constants that come from the hw
******************************************************************************/

#if defined(SIMPENROSE) || defined(KHRN_CARBON)
   #define KHRN_HW_SYSTEM_CACHE_LINE_SIZE 4
#else
   #define KHRN_HW_SYSTEM_CACHE_LINE_SIZE 32
#endif
#define KHRN_HW_TLB_ALIGN 16 /* todo: is this right? */
#define KHRN_HW_LOG2_BRCM1_WIDTH 6 /* non-ms */
#define KHRN_HW_LOG2_BRCM1_HEIGHT 6 /* non-ms */
#define KHRN_HW_BRCM1_WIDTH (1 << KHRN_HW_LOG2_BRCM1_WIDTH)
#define KHRN_HW_BRCM1_HEIGHT (1 << KHRN_HW_LOG2_BRCM1_HEIGHT)
#define KHRN_HW_BRCM1_STATE_SIZE 48
#define KHRN_HW_BIN_MEM_ALIGN 256
#define KHRN_HW_BIN_MEM_GRANULARITY 4096
#define KHRN_HW_CL_BLOCK_SIZE_MIN 32
#define KHRN_HW_CL_BLOCK_SIZE_MAX 256
#define KHRN_HW_TEX_SIZE_MAX 2048 /* max width/height */
#define KHRN_HW_TEX_ALIGN 4096
#define KHRN_HW_VPM_BLOCKS_N (192 / 4)
#define KHRN_HW_USER_QUEUE_LENGTH 16
#define KHRN_HW_QPUS_N 12

/******************************************************************************
misc stuff
******************************************************************************/

#if defined(SIMPENROSE) || defined(KHRN_CARBON)
   #define khrn_hw_alias_direct(addr) (addr)
   #define khrn_hw_alias_normal(addr) (addr)
   #define khrn_hw_alias_l1_nonallocating(addr) (addr)
#if defined(SIMPENROSE)
   #define khrn_hw_addr simpenrose_hw_addr
   #define khrn_hw_unaddr simpenrose_pc_addr
#else
   #define khrn_hw_addr(addr) carbon_hw_addr(addr)
   #define khrn_hw_unaddr(addr) carbon_hw_unaddr(addr)
#endif
   static INLINE void khrn_hw_flush_dcache(void) {}
   static INLINE void khrn_hw_flush_dcache_range(void *p, uint32_t size) {}
   static INLINE void khrn_hw_flush_l1cache_range(void *p, uint32_t size) {}
   static INLINE void khrn_hw_invalidate_dcache_range(void *p, uint32_t size) {}
   static INLINE void khrn_hw_invalidate_l1cache_range(void *p, uint32_t size) {}
#elif defined(ABSTRACT_PLATFORM)
   /* NEXUS has to convert to physical addresses from the cached version and back */
   #define khrn_hw_alias_direct(addr) (addr)
   #define khrn_hw_alias_normal(addr) (addr)
   #define khrn_hw_alias_l1_nonallocating(addr) (addr)
   #define khrn_hw_alias_l1l2_nonallocating(addr) (addr)

   static INLINE uint32_t khrn_hw_addr(const void *addr)
   {
      uint32_t addr_device;
      addr_device = mem_map_cached_to_physical((void*)addr);
      return addr_device;
   }

   static INLINE void *khrn_hw_unaddr(uint32_t addr)
   {
      void * addr_cached = mem_map_physical_to_cached(addr);
      return addr_cached;
   }

   static INLINE void *khrn_hw_cached_to_uncached(const void *addr)
   {
      void * addr_uncached = mem_map_cached_to_uncached((void*)addr);
      return addr_uncached;
   }

   static INLINE void *khrn_hw_uncached_to_cached(const void *addr)
   {
      void * addr_cached = mem_map_uncached_to_cached((void*)addr);
      return addr_cached;
   }

   static INLINE void khrn_hw_full_memory_barrier(void) {}

   static INLINE void khrn_hw_flush_dcache(void) { mem_flush_cache(); }
   static INLINE void khrn_hw_flush_dcache_range(void *p, uint32_t size) { mem_flush_cache_range(p, size); }
   static INLINE void khrn_hw_flush_l1cache_range(void *p, uint32_t size) { UNUSED(p); UNUSED(size); }
   static INLINE void khrn_hw_invalidate_dcache_range(void *p, uint32_t size) { mem_flush_cache_range(p, size); }
   static INLINE void khrn_hw_invalidate_l1cache_range(void *p, uint32_t size) { UNUSED(p); UNUSED(size); }
#else
   #define khrn_hw_alias_direct ALIAS_DIRECT
   #define khrn_hw_alias_normal ALIAS_NORMAL
   #define khrn_hw_alias_l1_nonallocating ALIAS_L1_NONALLOCATING

   static INLINE uint32_t khrn_hw_addr(const void *addr)
   {
      return (uint32_t)(uintptr_t)addr;
   }

   static INLINE void *khrn_hw_unaddr(uint32_t addr)
   {
      return (void *)(uintptr_t)addr;
   }

   static INLINE void khrn_hw_flush_dcache(void) { vclib_dcache_flush(); }
   static INLINE void khrn_hw_flush_dcache_range(void *p, uint32_t size) { vclib_dcache_flush_range(p, size); }
   static INLINE void khrn_hw_flush_l1cache_range(void *p, uint32_t size) { vclib_l1cache_flush_range(p, size); }
   static INLINE void khrn_hw_invalidate_dcache_range(void *p, uint32_t size) { vclib_dcache_invalidate_range(p, size); }
   static INLINE void khrn_hw_invalidate_l1cache_range(void *p, uint32_t size) { vclib_l1cache_invalidate_range(p, size); }
#endif

/******************************************************************************
stuff for writing control lists
******************************************************************************/

typedef uint64_t KHRN_SHADER_T;
static INLINE uint8_t get_byte(const uint8_t *p)
{
   #ifdef BIG_ENDIAN_CPU
      return ((uint8_t)*(uint8_t*)((uint32_t)&p[0]^0x3));
   #else
      return p[0];
   #endif
}

static INLINE uint16_t get_short(const uint8_t *p)
{
   #ifdef BIG_ENDIAN_CPU
      return ((uint16_t)*(uint8_t*)((uint32_t)&p[0]^0x3)) |
             ((uint16_t)*(uint8_t*)(((uint32_t)&p[1]^0x3)) << 8);
   #else
      return  (uint16_t)p[0] |
             ((uint16_t)p[1] << 8);
   #endif
}

static INLINE uint32_t get_word(const uint8_t *p)
{
   #ifdef BIG_ENDIAN_CPU
      return ((uint32_t)*(uint8_t*)((uint32_t)&p[0]^0x3)) |
             ((uint32_t)*(uint8_t*)(((uint32_t)&p[1]^0x3)) << 8) |
             ((uint32_t)*(uint8_t*)(((uint32_t)&p[2]^0x3)) << 16) |
             ((uint32_t)*(uint8_t*)(((uint32_t)&p[3]^0x3)) << 24);
   #else
      return  (uint32_t)p[0] |
             ((uint32_t)p[1] << 8) |
             ((uint32_t)p[2] << 16) |
             ((uint32_t)p[3] << 24);
   #endif
}

static INLINE void put_byte(uint8_t *p, uint8_t n)
{
   #ifdef BIG_ENDIAN_CPU
      *(uint8_t*)((uint32_t)&p[0]^0x3) = n;
   #else
      p[0] = n;
   #endif
}
static INLINE void put_short(uint8_t *p, uint16_t n)
{
   #ifdef BIG_ENDIAN_CPU
      *(uint8_t*)((uint32_t)&p[0]^0x3) = (uint8_t)n;
      *(uint8_t*)((uint32_t)&p[1]^0x3) = (uint8_t)(n >> 8);
   #else
      p[0] = (uint8_t)n;
      p[1] = (uint8_t)(n >> 8);
   #endif
}

static INLINE void put_word(uint8_t *p, uint32_t n)
{
   #ifdef BIG_ENDIAN_CPU
      *(uint8_t*)((uint32_t)&p[0]^0x3) = (uint8_t)n;
      *(uint8_t*)((uint32_t)&p[1]^0x3) = (uint8_t)(n >> 8);
      *(uint8_t*)((uint32_t)&p[2]^0x3) = (uint8_t)(n >> 16);
      *(uint8_t*)((uint32_t)&p[3]^0x3) = (uint8_t)(n >> 24);
   #else
      p[0] = (uint8_t)n;
      p[1] = (uint8_t)(n >> 8);
      p[2] = (uint8_t)(n >> 16);
      p[3] = (uint8_t)(n >> 24);
   #endif
}

static INLINE void put_float(uint8_t *p, float f)
{
   put_word(p, float_to_bits(f));
}

static INLINE void add_byte(uint8_t **p, uint8_t n)
{
   put_byte(*p, n);
   (*p) += 1;
}

static INLINE void add_short(uint8_t **p, uint16_t n)
{
   put_short(*p, n);
   (*p) += 2;
}

static INLINE void add_word(uint8_t **p, uint32_t n)
{
   put_word(*p, n);
   (*p) += 4;
}

static INLINE void add_float(uint8_t **p, float f)
{
   add_word(p, float_to_bits(f));
}

#if V3D_VER_AT_LEAST(3, 0)
static INLINE void add_float16(uint8_t **p, float16_t f16)
{
   add_short(p, (uint16_t) f16);
}
#endif

static INLINE void add_pointer(uint8_t **p, void *ptr)
{
   add_word(p, khrn_hw_addr(khrn_hw_alias_direct(ptr)));   //PTR
}

#define ADD_BYTE(p, n) add_byte(&(p), (n))
#define ADD_SHORT(p, n) add_short(&(p), (n))
#define ADD_WORD(p, n) add_word(&(p), (n))
#define ADD_FLOAT(p, n) add_float(&(p), (n))
#define ADD_POINTER(p, n) add_pointer(&(p), (n))

#define KHRN_HW_INSTR_HALT                   0
#define KHRN_HW_INSTR_NOP                    1
#define KHRN_HW_INSTR_MARKER                 2
#define KHRN_HW_INSTR_RESET_MARKER_COUNT     3
#define KHRN_HW_INSTR_FLUSH                  4
#define KHRN_HW_INSTR_FLUSH_ALL_STATE        5
#define KHRN_HW_INSTR_START_BRCM1_BINNING     6
#define KHRN_HW_INSTR_INCR_SEMAPHORE         7
#define KHRN_HW_INSTR_WAIT_SEMAPHORE         8
#define KHRN_HW_INSTR_BRANCH                16
#define KHRN_HW_INSTR_BRANCH_SUB            17
#define KHRN_HW_INSTR_RETURN                18
#define KHRN_HW_INSTR_REPEAT_START_MARKER   19
#define KHRN_HW_INSTR_REPEAT_FROM_START_MARKER 20
#define KHRN_HW_INSTR_STORE_SUBSAMPLE       24
#define KHRN_HW_INSTR_STORE_SUBSAMPLE_EOF   25
#define KHRN_HW_INSTR_STORE_FULL            26
#define KHRN_HW_INSTR_LOAD_FULL             27
#ifndef __BCM2708A0__
#define KHRN_HW_INSTR_STORE_GENERAL         28
#define KHRN_HW_INSTR_LOAD_GENERAL          29
#endif
#define KHRN_HW_INSTR_GLDRAWELEMENTS        32
#define KHRN_HW_INSTR_GLDRAWARRAYS          33
#define KHRN_HW_INSTR_VG_COORD_LIST         41
#ifndef __BCM2708A0__
#define KHRN_HW_INSTR_VG_INLINE_PRIMS       42
#endif
#define KHRN_HW_INSTR_COMPRESSED_LIST       48
#define KHRN_HW_INSTR_CLIPPED_PRIM          49
#define KHRN_HW_INSTR_PRIMITIVE_LIST_FORMAT 56
#define KHRN_HW_INSTR_GL_SHADER             64
#define KHRN_HW_INSTR_NV_SHADER             65
#define KHRN_HW_INSTR_VG_SHADER             66
#ifndef __BCM2708A0__
#define KHRN_HW_INSTR_INLINE_VG_SHADER      67
#endif
/* ... */
#if V3D_VER_AT_LEAST(3, 0)
#define KHRN_HW_INSTR_STATE_BLEND_MODE          84
#define KHRN_HW_INSTR_STATE_BLEND_CCOLOR_RGBA32 85
#define KHRN_HW_INSTR_STATE_BLEND_CCOLOR_HDR16  86
#endif
/* ... */
#define KHRN_HW_INSTR_STATE_CFG             96
#define KHRN_HW_INSTR_STATE_FLATSHADE       97
#define KHRN_HW_INSTR_STATE_POINT_SIZE      98
#define KHRN_HW_INSTR_STATE_LINE_WIDTH      99
#define KHRN_HW_INSTR_STATE_RHTX           100
#define KHRN_HW_INSTR_STATE_DEPTH_OFFSET   101
#define KHRN_HW_INSTR_STATE_CLIP           102
#define KHRN_HW_INSTR_STATE_VIEWPORT_OFFSET 103
#define KHRN_HW_INSTR_STATE_CLIPZ          104
#define KHRN_HW_INSTR_STATE_CLIPPER_XY     105
#define KHRN_HW_INSTR_STATE_CLIPPER_Z      106
#define KHRN_HW_INSTR_STATE_BRCM1_BINNING_MODE 112
#define KHRN_HW_INSTR_STATE_BRCM1_RENDERING_MODE 113
#define KHRN_HW_INSTR_STATE_CLEARCOL       114
#define KHRN_HW_INSTR_STATE_BRCM1_COORDS    115

/******************************************************************************
simpenrose recording
******************************************************************************/

#ifdef SIMPENROSE_RECORD_OUTPUT

static INLINE void record_map_pointer(const void *p, unsigned int len, LABEL_T type, unsigned int flags, unsigned int align)
{
   record_map_buffer(khrn_hw_addr(p), len, type, flags, align);
}

static INLINE void record_map_extent(const void *begin, const void *end, LABEL_T type, unsigned int flags, unsigned int align)
{
   record_map_pointer(begin, (uint8_t *)end - (uint8_t *)begin, type, flags, align);
}

void record_map_mem_buffer_section(MEM_HANDLE_T handle, unsigned int begin, unsigned int len, LABEL_T type, unsigned int flags, unsigned int align);

static INLINE void record_map_mem_buffer(MEM_HANDLE_T handle, LABEL_T type, unsigned int flags, unsigned int align)
{
   record_map_mem_buffer_section(handle, 0, mem_get_size(handle), type, flags, align);
}

void record_set_frame_config(uint32_t width, uint32_t height, uint32_t bpp, bool tformat);

#endif

/******************************************************************************
hw fifo
******************************************************************************/

extern bool khrn_hw_init(void);
extern void khrn_hw_term(void);

#define KHRN_HW_SPECIAL_0            ((MEM_HANDLE_T)0) /* used for extra thrsw removing hack. todo: no need for this hack on b0 */
#define KHRN_HW_SPECIAL_BIN_MEM      ((MEM_HANDLE_T)1)
#define KHRN_HW_SPECIAL_BIN_MEM_END  ((MEM_HANDLE_T)2)
#define KHRN_HW_SPECIAL_BIN_MEM_SIZE ((MEM_HANDLE_T)3)

/*
   if we don't run out of memory at any point, callback will be called:
   - after locking everything in place, with reason
     KHRN_HW_CALLBACK_REASON_FIXUP, from the llat task. khrn_hw_fixup_done
     should be called at some point after this to signal that fixup is complete
     and binning can begin
   - after binning has completed, first with reason
     KHRN_HW_CALLBACK_REASON_BIN_FINISHED_LLAT from the llat task, then with
     KHRN_HW_CALLBACK_REASON_BIN_FINISHED from the master task
   - after rendering has completed, with reason
     KHRN_HW_CALLBACK_REASON_RENDER_FINISHED, from the master task

   we can run out of memory before or during binning (but not after). if we do
   run out of memory, callback will be called first with reason
   KHRN_HW_CALLBACK_REASON_OUT_OF_MEM_LLAT from the llat task, then with reason
   KHRN_HW_CALLBACK_REASON_OUT_OF_MEM from the master task, after which it will
   not be called again
*/

typedef enum {
   KHRN_HW_CALLBACK_REASON_FIXUP,
   KHRN_HW_CALLBACK_REASON_OUT_OF_MEM_LLAT,
   KHRN_HW_CALLBACK_REASON_OUT_OF_MEM,
   KHRN_HW_CALLBACK_REASON_BIN_FINISHED_LLAT,
   KHRN_HW_CALLBACK_REASON_BIN_FINISHED,
   KHRN_HW_CALLBACK_REASON_RENDER_FINISHED,
   KHRN_HW_CALLBACK_REASON_UNFIXUP,
} KHRN_HW_CALLBACK_REASON_T;

typedef void (*KHRN_HW_CALLBACK_T)(KHRN_HW_CALLBACK_REASON_T reason, void *data, const uint32_t *specials);

/*
   calling code should look something like this:

   void my_callback(KHRN_HW_CALLBACK_REASON_T reason, void *data)
   {
      MY_STRUCT_T *my_struct = (MY_STRUCT_T *)data;
      ...
   }

   MY_STRUCT_T *my_struct = (MY_STRUCT_T *)khrn_hw_queue(..., sizeof(MY_STRUCT_T));
   my_struct->x = x;
   ...

   khrn_hw_ready(true, my_struct); // may be called at some later point from any thread
*/

typedef enum {
   KHRN_HW_CC_FLAG_NONE = 0,

   KHRN_HW_CC_FLAG_L2 = 1 << 0,
   KHRN_HW_CC_FLAG_IC = 1 << 1,
   KHRN_HW_CC_FLAG_UC = 1 << 2,
   KHRN_HW_CC_FLAG_TU = 1 << 3
} KHRN_HW_CC_FLAG_T;

typedef enum {
   KHRN_HW_TYPE_GL,
   KHRN_HW_TYPE_VG
} KHRN_HW_TYPE_T;

extern void *khrn_hw_queue(
   uint8_t *bin_begin, uint8_t *bin_end, KHRN_HW_CC_FLAG_T bin_cc,
   uint8_t *render_begin, uint8_t *render_end, KHRN_HW_CC_FLAG_T render_cc, uint32_t render_n,
   uint32_t special_0, /* used for extra thrsw removing hack. todo: no need for this hack on b0 */
   uint32_t bin_mem_size_min, /* KHRN_HW_SPECIAL_BIN_MEM_SIZE will be >= this */
   uint32_t actual_user_vpm, uint32_t max_user_vpm,
   KHRN_HW_TYPE_T type,
   KHRN_HW_CALLBACK_T callback,
   uint32_t callback_data_size);
extern void khrn_hw_ready(bool ok, void *callback_data); /* if ok is false, callback will later be called with KHRN_HW_CALLBACK_REASON_OUT_OF_MEM_LLAT/KHRN_HW_CALLBACK_REASON_OUT_OF_MEM */
extern void khrn_hw_fixup_done(bool in_callback, void *callback_data); /* todo: i'm not sure i like this fixup done thing... */

extern void khrn_hw_queue_wait_for_worker(uint64_t pos);

extern void khrn_hw_queue_wait_for_display(EGL_DISP_HANDLE_T disp_handle, uint32_t pos);
extern void khrn_hw_queue_display(EGL_DISP_SLOT_HANDLE_T slot_handle);

extern void khrn_hw_wait(void);
#ifdef KHRN_BCG_LINUX_GNUC
extern void khrn_monitored_event_wait(void * sem);
extern bool khrn_event_wait_timeout(void * sem, unsigned int t);
#endif

extern void khrn_hw_update_perf_counters(KHRN_DRIVER_COUNTERS_T *counters);
extern void khrn_hw_reset_perf_counters(uint32_t hw_bank, uint32_t l3c_bank);

extern void khrn_hw_notify(void);
extern void khrn_hw_cleanup(void); /* called from khrn_sync_master_wait */

/*
   fifo pos stuff for interlocking with other fifos (eg worker)

   khrn_hw_get_enter_pos should only be called from the master task and should
   always return the right value

   khrn_hw_get_bin_exit_pos/khrn_hw_get_render_exit_pos can be called from any
   task, but might return the wrong value. the value returned will always be <=
   the right value however
*/

#if defined(SIMPENROSE)
/* no fifo... */
static INLINE void khrn_hw_advance_enter_pos(void) {}
static INLINE uint64_t khrn_hw_get_enter_pos(void) { return 0; }
static INLINE void khrn_hw_advance_bin_exit_pos(void) {}
static INLINE uint64_t khrn_hw_get_bin_exit_pos(void) { return 0; }
static INLINE void khrn_hw_advance_render_exit_pos(void) {}
static INLINE uint64_t khrn_hw_get_render_exit_pos(void) { return 0; }
#else
extern uint64_t khrn_hw_enter_pos;

static INLINE void khrn_hw_advance_enter_pos(void)
{
   ++khrn_hw_enter_pos;
}

static INLINE uint64_t khrn_hw_get_enter_pos(void)
{
   return khrn_hw_enter_pos;
}

extern uint32_t khrn_hw_bin_exit_pos_0, khrn_hw_bin_exit_pos_1;

static INLINE void khrn_hw_advance_bin_exit_pos(void)
{
   uint64_t next_exit_pos = (((uint64_t)khrn_hw_bin_exit_pos_1 << 32) | khrn_hw_bin_exit_pos_0) + 1;
   khrn_barrier();
   khrn_hw_bin_exit_pos_0 = (uint32_t)next_exit_pos;
   khrn_barrier();
   khrn_hw_bin_exit_pos_1 = (uint32_t)(next_exit_pos >> 32);
}

static INLINE uint64_t khrn_hw_get_bin_exit_pos(void)
{
   uint32_t exit_pos_1;
   uint32_t exit_pos_0;

   exit_pos_1 = khrn_hw_bin_exit_pos_1;
   khrn_barrier();
   exit_pos_0 = khrn_hw_bin_exit_pos_0;
   khrn_barrier();
   return ((uint64_t)exit_pos_1 << 32) | exit_pos_0;
}

extern uint32_t khrn_hw_render_exit_pos_0, khrn_hw_render_exit_pos_1;

static INLINE void khrn_hw_advance_render_exit_pos(void)
{
   uint64_t next_exit_pos = (((uint64_t)khrn_hw_render_exit_pos_1 << 32) | khrn_hw_render_exit_pos_0) + 1;
   khrn_barrier();
   khrn_hw_render_exit_pos_0 = (uint32_t)next_exit_pos;
   khrn_barrier();
   khrn_hw_render_exit_pos_1 = (uint32_t)(next_exit_pos >> 32);
}

static INLINE uint64_t khrn_hw_get_render_exit_pos(void)
{
   uint32_t exit_pos_1;
   uint32_t exit_pos_0;

   exit_pos_1 = khrn_hw_render_exit_pos_1;
   khrn_barrier();
   exit_pos_0 = khrn_hw_render_exit_pos_0;
   khrn_barrier();
   return ((uint64_t)exit_pos_1 << 32) | exit_pos_0;
}
#endif

#endif
