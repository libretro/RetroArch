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
#ifndef KHRN_CLIENT_PLATFORM_H
#define KHRN_CLIENT_PLATFORM_H

#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"
#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/common/khrn_int_image.h"
#include "interface/khronos/egl/egl_int.h"
#include <stdlib.h> // for size_t

/* Per-platform types are defined in here. Most platforms can be supported
 * via vcos, but 'direct' has its own header and types, which is why
 * the indirection is required.
 */
#if defined(ABSTRACT_PLATFORM)
#include "interface/khronos/common/abstract/khrn_client_platform_filler_abstract.h"
#elif defined(RPC_DIRECT) && !defined(RPC_LIBRARY) && !defined(RPC_DIRECT_MULTI)
#include "interface/khronos/common/direct/khrn_client_platform_filler_direct.h"
#elif defined(KHRN_VCOS_VCHIQ)
#include "interface/khronos/common/vcos_vchiq/khrn_client_platform_filler_vcos_vchiq.h"
#else
#include "interface/khronos/common/vcos/khrn_client_platform_filler_vcos.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*
   named counting semaphore
*/

/* Uses forward declared ref to avoid problems with mixing direct and vcos-based
 * semaphores.
 */


/*
   workaround for broken platforms which don't detect threads exiting
*/
extern void platform_hint_thread_finished(void);

/*
   heap
*/

/*
   void *khrn_platform_malloc(size_t size, const char *desc)

   Preconditions:

   desc is a literal, null-terminated string
   The caller of this function is contracted to later call khrn_platform_free
   (or pass such responsibility on) if we don't return NULL

   Postconditions:

   Return value is NULL or a memory allocation of at least size bytes,
   valid until khrn_platform_free is called. The memory is sufficiently
   aligned that it can be used for normal data structures.
*/

extern void *khrn_platform_malloc(size_t size, const char *desc);

/*
   void khrn_platform_free(void *v)

   Preconditions:

   v is a valid pointer returned from khrn_platform_malloc

   Postconditions:

   v is freed

   Invariants preserved:

   -

   Invariants used:

   -
*/
extern void khrn_platform_free(void *v);

extern void khrn_platform_maybe_free_process(void);

/*
   uint64_t khronos_platform_get_process_id()

   get process id

   Preconditions:

   -

   Postconditions:

   Repeated calls within a process return the same value. Calls from a different process
   return a different value.
*/

extern uint64_t khronos_platform_get_process_id(void);

/*
   window system
*/

#define PLATFORM_WIN_NONE     ((uint32_t)0xffffffff)

#ifdef EGL_SERVER_SMALLINT

static INLINE EGLNativeWindowType platform_canonical_win(EGLNativeWindowType win)
{
   switch ((uintptr_t)win) {
   case (uintptr_t)NATIVE_WINDOW_800_480:   return PACK_NATIVE_WINDOW(800, 480, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_640_480:   return PACK_NATIVE_WINDOW(640, 480, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_320_240:   return PACK_NATIVE_WINDOW(320, 240, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_240_320:   return PACK_NATIVE_WINDOW(240, 320, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_64_64:     return PACK_NATIVE_WINDOW(64, 64, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_400_480_A: return PACK_NATIVE_WINDOW(400, 480, 0, 2);
   case (uintptr_t)NATIVE_WINDOW_400_480_B: return PACK_NATIVE_WINDOW(400, 480, 1, 2);
   case (uintptr_t)NATIVE_WINDOW_512_512:   return PACK_NATIVE_WINDOW(512, 512, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_360_640:   return PACK_NATIVE_WINDOW(360, 640, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_640_360:   return PACK_NATIVE_WINDOW(640, 360, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_1280_720:  return PACK_NATIVE_WINDOW(1280, 720, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_1920_1080: return PACK_NATIVE_WINDOW(1920, 1080, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_480_320:   return PACK_NATIVE_WINDOW(480, 320, 0, 1);
   case (uintptr_t)NATIVE_WINDOW_1680_1050: return PACK_NATIVE_WINDOW(1680, 1050, 0, 1);
   default:                                 return win;
   }
}

static INLINE uint32_t platform_get_handle(EGLDisplay dpy, EGLNativeWindowType win)
{
#ifdef ABSTRACT_PLATFORM
   return (uint32_t)win;
#else
   return (uint32_t)(size_t)platform_canonical_win(win);
#endif /* ABSTRACT_PLATFORM */
}

#ifndef ABSTRACT_PLATFORM
static INLINE void platform_get_dimensions(EGLDisplay dpy,
      EGLNativeWindowType win, uint32_t *width, uint32_t *height, uint32_t *swapchain_count)
{
   win = platform_canonical_win(win);
   *width = UNPACK_NATIVE_WINDOW_W(win);
   *height = UNPACK_NATIVE_WINDOW_H(win);
#ifdef KHRN_SIMPLE_MULTISAMPLE
   *width *= 2;
   *height *= 2;
#endif
   *swapchain_count = 0;
}
#else
void platform_get_dimensions(EGLDisplay dpy,
      EGLNativeWindowType win, uint32_t *width, uint32_t *height, uint32_t *swapchain_count);
void platform_lock(void * opaque_buffer_handle);
void platform_unlock(void * opaque_buffer_handle);
#endif /* ABSTRACT_PLATFORM */
#else

/*
   uint32_t platform_get_handle(EGLNativeWindowType win)

   Implementation notes:

   Platform-specific implementation.

   Preconditions:

   -

   Postconditions:

   If win is a valid client-side handle to window W
      Then return value is a server-side handle to window W.
      Else return value is PLATFORM_WIN_NONE
*/
extern uint32_t platform_get_handle(EGLDisplay dpy, EGLNativeWindowType win);

/*
   void platform_get_dimensions(EGLNativeWindowType win, uint32_t *width, uint32_t *height, uint32_t *swapchain_count)

   Implementation notes:

   Platform-specific implementation.

   Preconditions:

   win is a valid client-side window handle
   width, height are valid pointers

   Postconditions:

   -
*/
extern void platform_get_dimensions(EGLDisplay dpy, EGLNativeWindowType win,
      uint32_t *width, uint32_t *height, uint32_t *swapchain_count);
#endif
extern void platform_surface_update(uint32_t handle);

/*
   bool platform_get_pixmap_info(EGLNativePixmapType pixmap, KHRN_IMAGE_WRAP_T *image);

   Preconditions:

   image is a valid pointer

   Postconditions:

   Either:
   - false is returned because pixmap is an invalid pixmap handle, or
   - true is returned and image is set up to describe the pixmap, and if it's a
     client-side pixmap the pointer is also set
*/

extern bool platform_get_pixmap_info(EGLNativePixmapType pixmap, KHRN_IMAGE_WRAP_T *image);
/*
   should look something like this:

   if (regular server-side pixmap) {
      handle[0] = handle;
   } else if (global image server-side pixmap) {
      handle[0] = id[0];
      handle[1] = id[1];
   }
*/
extern void platform_get_pixmap_server_handle(EGLNativePixmapType pixmap, uint32_t *handle);

extern void platform_wait_EGL(uint32_t handle);
extern void platform_retrieve_pixmap_completed(EGLNativePixmapType pixmap);
extern void platform_send_pixmap_completed(EGLNativePixmapType pixmap);

/*
   bool platform_match_pixmap_api_support(EGLNativePixmapType pixmap, uint32_t api_support);

   Preconditions:

   pixmap is probably a valid native pixmap handle
   api_support is a bitmap which is a subset of (EGL_OPENGL_ES_BIT | EGL_OPENVG_BIT | EGL_OPENGL_ES2_BIT)

   Postconditions:

   If result is true then rendering to this pixmap using these APIs is supported on this platform
*/

extern bool platform_match_pixmap_api_support(EGLNativePixmapType pixmap, uint32_t api_support);

#if EGL_BRCM_global_image && EGL_KHR_image
extern bool platform_use_global_image_as_egl_image(uint32_t id_0, uint32_t id_1, EGLNativePixmapType pixmap, EGLint *error);
extern void platform_acquire_global_image(uint32_t id_0, uint32_t id_1);
extern void platform_release_global_image(uint32_t id_0, uint32_t id_1);
extern void platform_get_global_image_info(uint32_t id_0, uint32_t id_1,
   uint32_t *pixel_format, uint32_t *width, uint32_t *height);
#endif

/* Platform optimised versions of memcpy and memcmp */
extern uint32_t platform_memcmp(const void * aLeft, const void * aRight, size_t aLen);
extern void platform_memcpy(void * aTrg, const void * aSrc, size_t aLength);

struct CLIENT_THREAD_STATE;
extern void platform_client_lock(void);
extern void platform_client_release(void);
extern void platform_init_rpc(struct CLIENT_THREAD_STATE *state);
extern void platform_term_rpc(struct CLIENT_THREAD_STATE *state);
extern void platform_maybe_free_process(void);
extern void platform_destroy_winhandle(void *a, uint32_t b);

extern uint32_t platform_get_color_format ( uint32_t format );

#if !defined(__SYMBIAN32__)
// hack for now - we want prototypes
extern void egl_gce_win_change_image(void);
#endif

#ifdef __cplusplus
}
#endif

extern EGLDisplay khrn_platform_set_display_id(EGLNativeDisplayType display_id);

extern uint32_t khrn_platform_get_window_position(EGLNativeWindowType win);

extern void khrn_platform_release_pixmap_info(EGLNativePixmapType pixmap, KHRN_IMAGE_WRAP_T *image);
extern void khrn_platform_bind_pixmap_to_egl_image(EGLNativePixmapType pixmap, EGLImageKHR egl_image, bool send);
extern void khrn_platform_unbind_pixmap_from_egl_image(EGLImageKHR egl_image);
extern uint32_t platform_get_color_format ( uint32_t format );
extern void platform_dequeue(EGLDisplay dpy, EGLNativeWindowType window);
#include "interface/khronos/include/WF/wfc.h"
typedef struct
{
   WFCDevice device;
   WFCContext context;
   WFCSource source;
   WFCint src_x, src_y, src_width, src_height;
   WFCint dest_width, dest_height;
   uint32_t stop_bouncing;
   uint32_t num_of_elements;
   WFCElement *element;
} WFC_BOUNCE_DATA_T;

void *platform_wfc_bounce_thread(void *param);

#endif // KHRN_CLIENT_PLATFORM_H
