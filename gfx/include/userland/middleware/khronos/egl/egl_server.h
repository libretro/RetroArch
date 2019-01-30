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

#ifndef EGL_SERVER_H
#define EGL_SERVER_H

#include "interface/khronos/egl/egl_int.h"
#include "middleware/khronos/common/khrn_map.h"
#include "middleware/khronos/common/khrn_pid_map.h"
#include "middleware/khronos/common/khrn_server_pointermap.h"
#include "middleware/khronos/common/khrn_image.h"
#include "middleware/khronos/common/khrn_hw.h"
#include "middleware/khronos/egl/egl_disp.h"
#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"
#include "middleware/khronos/vg/vg_image.h"

#include "interface/khronos/include/WF/wfc.h"

// Must be enough for triple-buffering (windows) and mipmaps (pbuffers)
#define EGL_MAX_BUFFERS       12

// There is a single global instance of this
typedef struct
{
   KHRN_MAP_T surfaces;
   KHRN_MAP_T glcontexts;
   KHRN_MAP_T vgcontexts;
   KHRN_PID_MAP_T eglimages;
   VCOS_MUTEX_T eglimages_lock;
   KHRN_MAP_T wintoeglimage;//TODO window ids should be per process?
   VCOS_MUTEX_T wintoeglimage_mutex;
   
#if EGL_KHR_sync
   KHRN_MAP_T syncs;
#endif

   uint32_t next_surface;
   uint32_t next_context;
   uint32_t next_eglimage;
#if EGL_KHR_sync
   uint32_t next_sync;
#endif

   uint64_t pid;                   //currently selected process id

   uint32_t glversion;             //EGL_SERVER_GL11 or EGL_SERVER_GL20. (0 if invalid)
   MEM_HANDLE_T glcontext;
   MEM_HANDLE_T gldrawsurface;     //EGL_SERVER_SURFACE_T
   MEM_HANDLE_T glreadsurface;     //EGL_SERVER_SURFACE_T
   MEM_HANDLE_T vgcontext;
   MEM_HANDLE_T vgsurface;         //EGL_SERVER_SURFACE_T

   /*
      locked_glcontext

      Invariants:

      (EGL_SERVER_STATE_LOCKED_GLCONTEXT)
      locked_glcontext == NULL or locked_glcontext is the locked version of glcontext (and we own the lock)
   */
   void *locked_glcontext;
   /*
      locked_vgcontext

      Invariants:

      (EGL_SERVER_STATE_LOCKED_VGCONTEXT)
      locked_vgcontext == NULL or locked_vgcontext is the locked version of vgcontext (and we own the lock)
   */
   void *locked_vgcontext;
   /*
      locked_vgcontext_shared

      Invariants:

      (EGL_SERVER_STATE_LOCKED_VGCONTEXT_SHARED)
      locked_vgcontext_shared == NULL or locked_vgcontext_shared is the locked version of vgcontext->shared_state (and we own the lock)
   */
   void *locked_vgcontext_shared;
   /*
      locked_vgcontext_shared_objects_storage

      Invariants:

      (EGL_SERVER_STATE_LOCKED_VGCONTEXT_SHARED_OBJECTS_STORAGE)
      locked_vgcontext_shared_objects_storage == NULL or locked_vgcontext_shared_objects_storage is the locked version of vgcontext->shared_state->objects->storage (and we own the lock)
   */
   void *locked_vgcontext_shared_objects_storage;

#if EGL_BRCM_perf_monitor
   uint32_t perf_monitor_refcount;
   uint32_t perf_monitor_lasttime;

   KHRN_PERF_COUNTERS_T perf_monitor_counters;

   MEM_HANDLE_T perf_monitor_images[2];      //KHRN_IMAGE_T
#endif

#if EGL_BRCM_driver_monitor
   uint32_t driver_monitor_refcount;
   KHRN_DRIVER_COUNTERS_T driver_monitor_counters;
#endif

} EGL_SERVER_STATE_T;

typedef struct
{
   uint32_t name;

   bool mipmap;
   uint32_t buffers;
   uint32_t back_buffer_index;
   /*
      mh_color

      Invariant:

      For 0 <= i < buffers
         mh_color[i] is a handle to a valid KHRN_IMAGE_T
   */
   MEM_HANDLE_T mh_color[EGL_MAX_BUFFERS];
   MEM_HANDLE_T mh_depth;  //floating KHRN_IMAGE_T
   MEM_HANDLE_T mh_multi;  //floating KHRN_IMAGE_T
   MEM_HANDLE_T mh_mask;   //floating KHRN_IMAGE_T

   uint8_t config_depth_bits;   // How many depth bits were requested in config. May not match actual buffer.
   uint8_t config_stencil_bits; // How many stencil bits were requested in config. May not match actual buffer.

   uint32_t win;                    // Opaque handle passed to egl_server_platform_display
   uint64_t pid;                    // Opaque handle to creating process
   uint32_t sem;                    // Opaque handle (probably semaphore name) passed on KHAN channel

   MEM_HANDLE_T mh_bound_texture;
   uint32_t swap_interval;
   uint32_t semaphoreId;  //Symbian needs a handle passed back in Khan, not just surface number


   //for android, the mh_storage this points at needs updating to point to where rendering happened
   uint32_t egl_render_image; 

   EGL_DISP_HANDLE_T disp_handle;
} EGL_SERVER_SURFACE_T;

typedef struct
{
   uint32_t type;
   uint32_t condition;
   int32_t threshold;

   uint64_t pid;
   uint32_t sem;

   bool state;
} EGL_SERVER_SYNC_T;

#define EGL_SERVER_FIFO_LEN 4

typedef struct
{
   uint64_t pid;

   EGLImageKHR egl_image_id;

   struct {
      EGLImageKHR    egl_images[EGL_SERVER_FIFO_LEN];
      unsigned       count;
      unsigned       read;
      unsigned       write;
   } fifo;

} EGL_SERVER_WIN_TO_EGL_IMAGE_T;

#ifdef __cplusplus
extern "C" {
#endif
EGLAPI void EGLAPIENTRY egl_server_startup_hack(void);
#ifdef __cplusplus
}
#endif

extern bool egl_server_is_empty(void);
extern void egl_server_shutdown(void);

/*
   egl_server_state_initted

   Invariants:

   True iff valid EGL server state exists
*/
extern bool egl_server_state_initted;
extern EGL_SERVER_STATE_T egl_server_state;

/*
   EGL_SERVER_STATE_T *EGL_GET_SERVER_STATE()

   Returns pointer to EGL server state.

   Implementation notes:

   There is only one of these globally, and it does not need locking and unlocking.

   Preconditions:

   Valid EGL server state exists

   Postconditions:

   Return value is a valid pointer
*/

static INLINE EGL_SERVER_STATE_T *EGL_GET_SERVER_STATE(void)
{
   vcos_assert(egl_server_state_initted);
   return &egl_server_state;
}

extern void egl_server_unlock(void);

#include "interface/khronos/egl/egl_int_impl.h"

extern void egl_khr_fence_update(void);

extern void egl_update_current_rendering_image(uint64_t pid, uint32_t window, MEM_HANDLE_T himage);

#if EGL_BRCM_perf_monitor
extern void egl_brcm_perf_monitor_update();
#endif

#endif
