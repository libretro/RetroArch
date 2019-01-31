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
#ifndef KHRN_CLIENT_H
#define KHRN_CLIENT_H

typedef struct CLIENT_PROCESS_STATE CLIENT_PROCESS_STATE_T;
typedef struct CLIENT_THREAD_STATE CLIENT_THREAD_STATE_T;


#include "interface/khronos/common/khrn_client_platform.h"

#include "interface/khronos/egl/egl_client_context.h"
#include "interface/khronos/egl/egl_client_surface.h"
#include "interface/khronos/include/EGL/eglext.h"

#include "interface/khronos/common/khrn_client_pointermap.h"

#ifdef RPC_LIBRARY
#include "middleware/khronos/common/khrn_misc.h"
#include "applications/vmcs/khronos/khronos_server.h"
#elif defined(RPC_DIRECT_MULTI)
#include "middleware/khronos/common/khrn_misc.h"
#endif


/* must be after EGL/eglext.h */
#if EGL_BRCM_global_image && EGL_KHR_image
   #include "interface/khronos/common/khrn_client_global_image_map.h"
#endif

extern void client_try_unload_server(CLIENT_PROCESS_STATE_T *process);

/*
   per-thread state

   - EGL error
   - EGL bound API
   - EGL context and surfaces for each API

   - RPC merge buffer
*/

#define MERGE_BUFFER_SIZE  4080

typedef struct {
   EGL_CONTEXT_T *context;
   EGL_SURFACE_T *draw;
   EGL_SURFACE_T *read;
} EGL_CURRENT_T;

struct CLIENT_THREAD_STATE {
   /*
      error

      Invariant:
      (CLIENT_THREAD_STATE_ERROR)
      error is one of
         EGL_SUCCESS
         EGL_NOT_INITIALIZED
         EGL_BAD_ACCESS
         EGL_BAD_ALLOC
         EGL_BAD_ATTRIBUTE
         EGL_BAD_CONTEXT
         EGL_BAD_CONFIG
         EGL_BAD_CURRENT SURFACE
         EGL_BAD_DISPLAY
         EGL_BAD_SURFACE
         EGL_BAD_MATCH
         EGL_BAD_PARAMETER
         EGL_BAD_NATIVE PIXMAP
         EGL_BAD_NATIVE WINDOW
         EGL_CONTEXT_LOST
   */
   EGLint error;

   EGLenum bound_api;

   /*
      handles to current display, context and surfaces for each API

      Availability:

      Thread owns EGL lock
   */

   EGL_CURRENT_T opengl;
   EGL_CURRENT_T openvg;

   /*
      rpc stuff
   */

   bool high_priority;

   uint8_t merge_buffer[MERGE_BUFFER_SIZE];

   uint32_t merge_pos;
   uint32_t merge_end;

	/* Try to reduce impact of repeated consecutive glGetError() calls */
	int32_t glgeterror_hack;
	bool async_error_notification;
};

extern void client_thread_state_init(CLIENT_THREAD_STATE_T *state);
extern void client_thread_state_term(CLIENT_THREAD_STATE_T *state);

extern PLATFORM_TLS_T client_tls;

/*
   CLIENT_GET_THREAD_STATE

   Implementation notes:

   TODO: make sure this gets code-reviewed

   Preconditions:

   -

   Postconditions:

   Result is a valid pointer to a thread-local CLIENT_THREAD_STATE_T structure.

   Invariants preserved:

   -

   Invariants used:

   -
*/

static INLINE CLIENT_THREAD_STATE_T *CLIENT_GET_THREAD_STATE(void)
{
	CLIENT_THREAD_STATE_T *tls;
	tls = (CLIENT_THREAD_STATE_T *)platform_tls_get(client_tls);
	if( tls && tls->glgeterror_hack ) {
		tls->glgeterror_hack--;
	}
   return tls;
}

static INLINE CLIENT_THREAD_STATE_T *CLIENT_GET_CHECK_THREAD_STATE(void)
{
   return (CLIENT_THREAD_STATE_T *)platform_tls_get_check(client_tls);
}

/*
   per-process state

   - EGL initialization stage
   - EGL contexts and surfaces
   - EGL counters
*/

struct CLIENT_PROCESS_STATE {
#ifdef RPC_LIBRARY
   /*
   called khronos_server_connect? this is valid even if !inited
   */
   bool connected;
#endif

   /*
   number of current contexts across all threads in this process. this is valid
   even if !inited
   */
   uint32_t context_current_count;

   /*
   inited

   Specifies whether the structure has been initialised and all of the other members are valid.
   inited is true between eglInitialise/eglTerminate. threads can still have
   things current when !inited

   Invariants:
   (CLIENT_PROCESS_STATE_INITED_SANITY)
   Only client_process_state_init/client_process_state_term modify this value
   */
   bool inited;

   /*
   contexts

   A map from context id (EGLContext) to EGL_CONTEXT_T
   TODO: use pointers as key rather than integers

   Validity: inited is true

   Invariants:
   (CLIENT_PROCESS_STATE_CONTEXTS)
   If id is a key in contexts:
      contexts[id].name == id
      contexts[id].is_destroyed == false
   */
   KHRN_POINTER_MAP_T contexts;

   /*
   surfaces

   A map from context id (EGLContext) to EGL_SURFACE_T

   Validity: inited is true

   Invariants:
   (CLIENT_PROCESS_STATE_SURFACES)
   If id is a key in surfaces:
      surfaces[id].name == id
      surfaces[id].is_destroyed == false
   */
   KHRN_POINTER_MAP_T surfaces;
   
   
   /*
    * some platforms (e.g. Android) need to maintain a list of
    * known windows
    */
   KHRN_POINTER_MAP_T windows;
   
#if EGL_KHR_sync

   /*
   syncs

   Validity: inited is true
   */
   KHRN_POINTER_MAP_T syncs;
#endif
#if EGL_BRCM_global_image && EGL_KHR_image
   KHRN_GLOBAL_IMAGE_MAP_T global_image_egl_images;
#endif


   /*
      next_surface

      Implementation notes:
      TODO: these could theoretically overflow

      Validity: inited is true

      Invariant:
      (CLIENT_PROCESS_STATE_NEXT_SURFACE)
      next_surface is greater than every key in surfaces
      next_surface >= 1
   */
   uint32_t next_surface;
   /*
      next_context

      Validity: inited is true
   */
   uint32_t next_context;
#if EGL_KHR_sync
   /*
      next_sync

      Validity: inited is true
   */
   uint32_t next_sync;
#endif
#if EGL_BRCM_global_image && EGL_KHR_image
   uint32_t next_global_image_egl_image;
#endif

#if EGL_BRCM_perf_monitor
   /*
      perf_monitor_inited

      Validity: inited is true
   */
   bool perf_monitor_inited;
#endif

#if EGL_BRCM_driver_monitor
   /*
      driver_monitor_inited

      Validity: inited is true
   */
   bool driver_monitor_inited;
#endif

#ifdef RPC_LIBRARY
   KHRONOS_SERVER_CONNECTION_T khrn_connection;
#endif
};

extern bool client_process_state_init(CLIENT_PROCESS_STATE_T *process);
extern void client_process_state_term(CLIENT_PROCESS_STATE_T *process);

extern CLIENT_PROCESS_STATE_T client_process_state;

/*
   CLIENT_GET_PROCESS_STATE()

   Returns the process-global CLIENT_PROCESS_STATE_T object.
*/
#ifdef CLIENT_THREAD_IS_PROCESS
extern PLATFORM_TLS_T client_tls_process;
extern PLATFORM_TLS_T client_tls_mutex;
extern void *platform_tls_get_process(PLATFORM_TLS_T tls);
#endif
static INLINE CLIENT_PROCESS_STATE_T *CLIENT_GET_PROCESS_STATE(void)
{
#ifdef CLIENT_THREAD_IS_PROCESS
	//each thread has its own client_process_state	
	return (CLIENT_PROCESS_STATE_T *)platform_tls_get_process(client_tls_process);
#else
   return &client_process_state;
#endif
}

/*
   exposed bits of EGL
*/

CLIENT_PROCESS_STATE_T *client_egl_get_process_state(CLIENT_THREAD_STATE_T *thread, EGLDisplay dpy, EGLBoolean check_inited);
EGL_CONTEXT_T *client_egl_get_context(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLContext ctx);
EGL_SURFACE_T *client_egl_get_surface(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLSurface surf);
EGL_SURFACE_T *client_egl_get_locked_surface(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLSurface surf);

EGLNativeWindowType client_egl_get_window(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLNativeWindowType window);
/*
   client state
*/

#define CLIENT_MAKE_CURRENT_SIZE 36 /* RPC_CALL8 */
extern void client_send_make_current(CLIENT_THREAD_STATE_T *thread);

extern void client_set_error(uint32_t server_context_name);
/*
   big giant lock
*/

extern PLATFORM_MUTEX_T client_mutex;

/*
   CLIENT_LOCK()

   Acquires EGL lock.

   Implementation notes:

   TODO make sure this gets reviewed

   Preconditions:

   TODO: check mutex hierarchy methodology
   Mutex: >(MUTEX_EGL_LOCK)
   Is being called from a function which _always_ subsequently calls CLIENT_UNLOCK()

   Postconditions:

   Mutex: (MUTEX_EGL_LOCK)
   Thread owns EGL lock
*/

static INLINE void CLIENT_LOCK(void)
{
      platform_client_lock();
}

/*
   CLIENT_UNLOCK()

   Releases EGL lock.

   Implementation notes:

   TODO make sure this gets reviewed

   Preconditions:

   Mutex: (MUTEX_EGL_LOCK)
   Thread owns EGL lock
   Is being called from a function which has _always_ previously called CLIENT_LOCK()

   Postconditions:
   Mutex: >(MUTEX_EGL_LOCK)
*/

static INLINE void CLIENT_UNLOCK(void)
{
    platform_client_release();
}

/*
   bool CLIENT_LOCK_AND_GET_STATES(EGLDisplay dpy, CLIENT_THREAD_STATE_T **thread, CLIENT_PROCESS_STATE_T **process)

   Try to acquire EGL lock and get thread and process state.

   Implementation notes:

   TODO make sure this gets reviewed

   Preconditions:

   thread is a valid pointer to a thread*
   process is a valid pointer to a process*
   Mutex: >(MUTEX_EGL_LOCK)
   Is being called from a function which calls CLIENT_UNLOCK() if we return true

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized for the specified display.

   if more than one condition holds, the first error is generated.

   Either:
      Mutex: (MUTEX_EGL_LOCK)
      Thread owns EGL lock
      result is true
   Or:
      Nothing changes
      result is false
*/

static INLINE bool CLIENT_LOCK_AND_GET_STATES(EGLDisplay dpy, CLIENT_THREAD_STATE_T **thread, CLIENT_PROCESS_STATE_T **process)
{
   *thread = CLIENT_GET_THREAD_STATE();
   CLIENT_LOCK();
   *process = client_egl_get_process_state(*thread, dpy, EGL_TRUE);
   if (*process != NULL)
      return true;
   else
   {
      CLIENT_UNLOCK();
      return false;
   }
}

/*
   process and thread attach/detach hooks
*/

#ifdef __cplusplus
extern "C" {
#endif
extern bool client_process_attach(void);
extern bool client_thread_attach(void);
extern void client_thread_detach(void *dummy);
extern void client_process_detach(void);

#ifdef RPC_LIBRARY
extern KHRONOS_SERVER_CONNECTION_T *client_library_get_connection(void);
extern void client_library_send_make_current(const KHRONOS_FUNC_TABLE_T *func_table);
#elif defined(RPC_DIRECT_MULTI)
extern void client_library_send_make_current(const KHRONOS_FUNC_TABLE_T *func_table);
#endif

#ifdef __cplusplus
}
#endif

#endif
