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

#ifndef KHRN_HW_H
#define KHRN_HW_H

#include "interface/khronos/common/khrn_int_common.h"
#include "middleware/khronos/common/khrn_image.h"
#include "middleware/khronos/egl/egl_disp.h"
#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"
#include "interface/vcos/vcos.h"

bool khrn_hw_common_init(void);
void khrn_hw_common_term(void);

void khrn_delayed_display(KHRN_IMAGE_T *image, EGL_DISP_SLOT_HANDLE_T slot_handle);
void khrn_delayed_wait_for_display(uint32_t fifo, EGL_DISP_HANDLE_T disp_handle, uint32_t pos);
void khrn_delayed_copy_buffer(MEM_HANDLE_T dst_handle, MEM_HANDLE_T src_handle);

bool khrn_hw_supports_early_z(void);

/* flush all queued stuff */
extern void khrn_hw_common_flush(void);

/* wait for all flushed stuff to finish */
extern void khrn_hw_common_wait(void);

/*
   void khrn_hw_common_finish()

   Flush all queued stuff. Wait for all flushed stuff to finish.

   Preconditions:

   -

   Postcondtions:

   For all KHRN_IMAGE_T image:
      image.conceptual_readable_by_master is true
      image.conceptual_writeable_by_master is true
   conceptual_buffers_owned_by_master is true
   conceptual_programs_owned_by_master is true
*/

static INLINE void khrn_hw_common_finish(void)
{
   khrn_hw_common_flush();
   khrn_hw_common_wait();
}

extern void khrn_sync_init(void);
extern void khrn_sync_notify_master(void);
extern void khrn_sync_master_wait(void);
#ifdef KHRN_BCG_WAIT_TIMEOUT
extern bool khrn_sync_master_wait_timeout(unsigned int t);
#endif
extern void khrn_sync_display_returned_notify(void); /* called when something comes off the display */
extern void khrn_specify_event(VCOS_EVENT_T *ev);
extern int32_t khrn_do_suspend_resume(uint32_t up);

typedef void KHRN_SYNC_MASTER_WAIT_FUNC(void);
typedef void KHRN_SPECIFY_EVENT_FUNC(VCOS_EVENT_T *ev);
typedef int32_t KHRN_DO_SUSPEND_RESUME_FUNC(uint32_t up);

typedef struct {
   uint32_t acquire;
   uint32_t release;
} KHRN_CACHE_COUNTERS_T;

extern void khrn_memcpy(void *dest, const void *src, uint32_t size);
extern void khrn_memset(void *dest, uint32_t val, uint32_t size);

typedef struct {
   /*
      primitive counters
   */

   uint32_t fovcull;
   uint32_t fovclip;
   uint32_t revcull;
   uint32_t nofepix;

   /*
      texture units
   */

   uint32_t tu0access;
   uint32_t tu0miss;
   uint32_t tu1access;
   uint32_t tu1miss;

   /*
      backend
   */

   uint32_t dpthfail;
   uint32_t stclfail;
   uint32_t dpthstclpass;
} KHRN_PERF_COUNTERS_T;

typedef struct {
   uint64_t qpu_cycles_idle;
   uint64_t qpu_cycles_vert_shade;
   uint64_t qpu_cycles_frag_shade;
   uint64_t qpu_cycles_exe_valid;
   uint64_t qpu_cycles_wait_tmu;
   uint64_t qpu_cycles_wait_scb;
   uint64_t qpu_cycles_wait_vary;
   uint64_t qpu_icache_hits;
   uint64_t qpu_icache_miss;
   uint64_t qpu_ucache_hits;
   uint64_t qpu_ucache_miss;

   uint64_t tmu_total_quads;
   uint64_t tmu_cache_miss;

   uint64_t l2c_hits;
   uint64_t l2c_miss;
} KHRN_DRIVER_HW_COUNTERS0_T;

typedef struct {
   uint64_t fep_valid_prims;
   uint64_t fep_valid_prims_no_pixels;
   uint64_t fep_earlyz_clipped_quads;
   uint64_t fep_valid_quads;

   uint64_t tlb_quads_no_stencil_pass_pixels;
   uint64_t tlb_quads_no_z_stencil_pass_pixels;
   uint64_t tlb_quads_z_stencil_pass_pixels;
   uint64_t tlb_quads_all_pixels_zero_cvg;
   uint64_t tlb_quads_all_pixels_nonzero_cvg;
   uint64_t tlb_quads_valid_pixels_written;

   uint64_t ptb_prims_viewport_discarded;
   uint64_t ptb_prims_needing_clip;

   uint64_t pse_prims_reverse_discarded;

   uint64_t vpm_cycles_vdw_stalled;
   uint64_t vpm_cycles_vcd_stalled;
} KHRN_DRIVER_HW_COUNTERS1_T;

typedef struct {
   uint32_t hard_clears;
   uint32_t soft_clears;
   uint32_t tb_grp_color_loads;
   uint32_t tb_grp_ms_color_loads;
   uint32_t tb_grp_ds_loads;
   uint32_t tb_grp_color_stores;
   uint32_t tb_grp_ms_color_stores;
   uint32_t tb_grp_ds_stores;
   uint32_t tb_color_loads;
   uint32_t tb_ms_color_loads;
   uint32_t tb_ds_loads;
   uint32_t tb_color_stores;
   uint32_t tb_ms_color_stores;
   uint32_t tb_ds_stores;
   uint32_t tex_submissions;
   uint32_t tex_fast_paths;
   uint32_t mipmap_gens;
   uint32_t mipmap_gens_fast;
   uint32_t draw_calls;
   uint32_t num_swaps;

   uint32_t hw_group_active;

#ifdef __linux__
   uint32_t reset_time;       /* time in ms when the statistics were last reset */
   uint32_t in_time;          /* time at which acquire happened */
   uint32_t total_time;       /* total of all (release time - acquire time) */

   uint32_t last_cpu_time;
   uint32_t last_cpu_ticks;
#endif

   union
   {
      KHRN_DRIVER_HW_COUNTERS0_T hw_0;
      KHRN_DRIVER_HW_COUNTERS1_T hw_1;
   } hw;

   uint32_t l3c_group_active;
   union
   {
      uint64_t l3c_read_bw_0;
      uint64_t l3c_write_bw_1;
   } l3c;
   union
   {
      uint64_t l3c_mem_read_bw_0;
      uint64_t l3c_mem_write_bw_1;
   } l3c_mem;

} KHRN_DRIVER_COUNTERS_T;

extern void khrn_hw_register_perf_counters(KHRN_DRIVER_COUNTERS_T *counters);
extern void khrn_hw_unregister_perf_counters(void);
extern void khrn_reset_driver_counters(int32_t hw_bank, int32_t l3c_bank);

extern KHRN_DRIVER_COUNTERS_T *khrn_driver_counters(void);

#if EGL_BRCM_driver_monitor
#define INCR_DRIVER_COUNTER(counter) khrn_driver_counters()->counter++;
#else
#define INCR_DRIVER_COUNTER(counter) ;
#endif

extern void khrn_hw_kick(void);

#endif
