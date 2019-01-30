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
#define VCOS_LOG_CATEGORY (&khrn_client_log)
#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/common/khrn_client_check_types.h"
#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"
#include "interface/khronos/egl/egl_client_config.h"
#include "interface/khronos/glxx/glxx_client.h"

#if defined(V3D_LEAN)
#include "interface/khronos/common/khrn_int_misc_impl.h"
#endif

#if EGL_KHR_sync
#include "interface/khronos/ext/egl_khr_sync_client.h"
#endif

#if EGL_BRCM_perf_monitor
#include "interface/khronos/ext/egl_brcm_perf_monitor_client.h"
#endif

#if EGL_BRCM_driver_monitor
#include "interface/khronos/ext/egl_brcm_driver_monitor_client.h"
#endif

#ifdef RPC_LIBRARY
#include "middleware/dlloader/dlfcn.h"
#include "applications/vmcs/khronos/khronos_server.h"
#endif

VCOS_LOG_CAT_T khrn_client_log = VCOS_LOG_INIT("khrn_client", VCOS_LOG_WARN);

/*
   void client_try_unload_server(CLIENT_PROCESS_STATE_T *process)

   Preconditions:

   -

   Postconditions:

   -

   Invariants preserved:

   -

   Invariants used:

   -
*/

void client_try_unload_server(CLIENT_PROCESS_STATE_T *process)
{
   if (/* some context is current */
      (process->context_current_count != 0) ||
      /* egl is initialised */
      process->inited) {
      return;
   }

   /*
      Prompt the server to unload Khronos VLL if it can,
      and wait until it is done
   */
#ifdef RPC_LIBRARY   //TODO: not thread safe
   if (process->connected) {
      const KHRONOS_FUNC_TABLE_T *func_table;
      func_table = khronos_server_lock_func_table(client_library_get_connection());
      if (func_table != NULL)
      {
         func_table->khrn_misc_try_unload_impl();
         khronos_server_disconnect();
      }
      khronos_server_unlock_func_table();

      process->connected = false;
   }
#else
   {
      CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
      RPC_INT_RES(RPC_CALL0_RES(khrn_misc_try_unload_impl, thread, KHRNMISCTRYUNLOAD_ID)); // return unimportant - read is just to cause blocking
   }
#endif
}

/*
   void client_process_state_init(CLIENT_PROCESS_STATE_T *state)

   Implementation notes:

   Does nothing if already initialised.

   Preconditions:

   process is a valid pointer
   Thread owns EGL lock
   client_process_state_term must be called at some point after calling this function if we return true.

   Postconditions:

   Either:
   - an error occurred and false is returned, or
   - process->inited is true

   Invariants preserved:

   All invariants on CLIENT_PROCESS_STATE_T
   (CLIENT_PROCESS_STATE_INITED_SANITY)

   Invariants used:

   -
*/

bool client_process_state_init(CLIENT_PROCESS_STATE_T *process)
{
   if (!process->inited) {
      if (!khrn_pointer_map_init(&process->contexts, 64))
         return false;

      if (!khrn_pointer_map_init(&process->surfaces, 64))
      {
         khrn_pointer_map_term(&process->contexts);
         return false;
      }
      if (!khrn_pointer_map_init(&process->windows, 64))
      {
         khrn_pointer_map_term(&process->contexts);
         khrn_pointer_map_term(&process->surfaces);
         return false;
      }
      
#if EGL_KHR_sync
      if (!khrn_pointer_map_init(&process->syncs, 64))
      {
         khrn_pointer_map_term(&process->contexts);
         khrn_pointer_map_term(&process->surfaces);
         khrn_pointer_map_term(&process->windows);
         return false;
      }
#endif
#if EGL_BRCM_global_image && EGL_KHR_image
      khrn_global_image_map_init(&process->global_image_egl_images, 8);
#endif
      process->next_surface = 1;
      process->next_context = 1;
#if EGL_KHR_sync
      process->next_sync = 0x80000001;
#endif
#if EGL_BRCM_global_image && EGL_KHR_image
      process->next_global_image_egl_image = 1 << 31;
#endif

#if EGL_BRCM_perf_monitor
      process->perf_monitor_inited = false;
#endif

#ifdef RPC_LIBRARY
      if (!process->connected) {
         process->khrn_connection.make_current_func = client_library_send_make_current;
         khronos_server_lock_func_table(&process->khrn_connection);
         khronos_server_connect(&process->khrn_connection);
         khronos_server_unlock_func_table();
         RPC_CALL0(khrn_misc_startup_impl, NULL, no_id);

         process->connected = true;
      }
#endif

      process->inited = true;
   }

#ifndef ABSTRACT_PLATFORM
#if defined(ANDROID) && !defined (ANDROID_HWCOMPOSER)
   egl_config_install_configs(1); // T-format configs
#else
   egl_config_install_configs(0); // RSO configs
#endif
#endif

   return true;
}

#include "interface/khronos/common/khrn_client_cr.c"

/*
   void client_process_state_term(CLIENT_PROCESS_STATE_T *process)

   Implementation notes:

   Does nothing if already terminated.

   Preconditions:

   process is a valid pointer
   Thread owns EGL lock

   Postconditions:

   process->inited is false.

   Invariants preserved:

   (EGL_CONTEXT_IS_DESTROYED)
   (CLIENT_PROCESS_STATE_INITED_SANITY)

   Invariants used:

   -
*/

void client_process_state_term(CLIENT_PROCESS_STATE_T *process)
{
   if (process->inited) {
      khrn_pointer_map_iterate(&process->contexts, callback_destroy_context, NULL);
      khrn_pointer_map_term(&process->contexts);

      khrn_pointer_map_iterate(&process->surfaces, callback_destroy_surface, NULL);
      khrn_pointer_map_term(&process->surfaces);

      khrn_pointer_map_term(&process->windows);
      
#if EGL_KHR_sync
      egl_sync_destroy_all(&process->syncs);
      khrn_pointer_map_term(&process->syncs);
#endif

#if EGL_BRCM_global_image && EGL_KHR_image
      khrn_global_image_map_term(&process->global_image_egl_images);
#endif

#if EGL_BRCM_perf_monitor
      egl_perf_monitor_term(process);
#endif

#if EGL_BRCM_driver_monitor
      egl_driver_monitor_term(process);
#endif

      process->inited = false;
   }
}

CLIENT_PROCESS_STATE_T client_process_state = {
#ifdef RPC_LIBRARY
   false, /* not connected */
#endif
   0, /* nothing current */
   false}; /* not inited */

void client_thread_state_init(CLIENT_THREAD_STATE_T *state)
{
   state->error = EGL_SUCCESS;

   state->bound_api = EGL_OPENGL_ES_API;

   state->opengl.context = 0;
   state->opengl.draw = 0;
   state->opengl.read = 0;

   state->openvg.context = 0;
   state->openvg.draw = 0;
   state->openvg.read = 0;

   state->high_priority = false;

   state->merge_pos = 0;
   state->merge_end = 0;

	state->glgeterror_hack = 0;
	state->async_error_notification = false;
}

void client_thread_state_term(CLIENT_THREAD_STATE_T *state)
{
   // TODO: termination
   platform_term_rpc( state );
}

EGL_CONTEXT_T *client_egl_get_context(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLContext ctx)
{
   EGL_CONTEXT_T *context = (EGL_CONTEXT_T *)khrn_pointer_map_lookup(&process->contexts, (uint32_t)(size_t)ctx);

   vcos_assert(!context || !context->is_destroyed);

   if (!context)
      thread->error = EGL_BAD_CONTEXT;

   return context;
}

EGL_SURFACE_T *client_egl_get_surface(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLSurface surf)
{
   EGL_SURFACE_T *surface = (EGL_SURFACE_T *)khrn_pointer_map_lookup(&process->surfaces, (uint32_t)(size_t)surf);

   vcos_assert (!surface || !surface->is_destroyed);

   if (!surface)
      thread->error = EGL_BAD_SURFACE;

#if EGL_KHR_lock_surface
   if (surface && surface->is_locked) {
      thread->error = EGL_BAD_ACCESS;
      surface = NULL;
   }
#endif

   return surface;
}

/*
   We don't actually insist that the surface is locked. But unlike client_egl_get_surface, we don't throw an
   error if it isn't.
*/
EGL_SURFACE_T *client_egl_get_locked_surface(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLSurface surf)
{
   EGL_SURFACE_T *surface = (EGL_SURFACE_T *)khrn_pointer_map_lookup(&process->surfaces, (uint32_t)(size_t)surf);

   vcos_assert (!surface || !surface->is_destroyed);

   if (!surface)
      thread->error = EGL_BAD_SURFACE;

   return surface;
}

/*
 * just return if we've seen window before 
 */
EGLNativeWindowType client_egl_get_window(CLIENT_THREAD_STATE_T *thread, CLIENT_PROCESS_STATE_T *process, EGLNativeWindowType window)
{
   EGLNativeWindowType win = (EGLNativeWindowType)khrn_pointer_map_lookup(&process->windows, (uint32_t)(size_t)window);

   return win;
}

static uint32_t convert_gltype(EGL_CONTEXT_TYPE_T type)
{
   switch (type) {
   case OPENGL_ES_11: return EGL_SERVER_GL11;
   case OPENGL_ES_20: return EGL_SERVER_GL20;
   default:           UNREACHABLE(); return 0;
   }
}

void client_send_make_current(CLIENT_THREAD_STATE_T *thread)
{
   uint64_t pid                  = rpc_get_client_id(thread);
   uint32_t gltype               = thread->opengl.context ? convert_gltype(thread->opengl.context->type) : 0;
   EGL_GL_CONTEXT_ID_T servergl  = thread->opengl.context ? thread->opengl.context->servercontext : EGL_SERVER_NO_GL_CONTEXT;
   EGL_SURFACE_ID_T servergldraw = thread->opengl.draw    ? thread->opengl.draw->serverbuffer     : EGL_SERVER_NO_SURFACE;
   EGL_SURFACE_ID_T serverglread = thread->opengl.read    ? thread->opengl.read->serverbuffer     : EGL_SERVER_NO_SURFACE;
   EGL_VG_CONTEXT_ID_T servervg  = thread->openvg.context ? thread->openvg.context->servercontext : EGL_SERVER_NO_VG_CONTEXT;
   EGL_SURFACE_ID_T servervgsurf = thread->openvg.draw    ? thread->openvg.draw->serverbuffer     : EGL_SERVER_NO_SURFACE;

   /*
      if the size of this call in the merge buffer changes,
      CLIENT_MAKE_CURRENT_SIZE in khrn_client.h should be updated
   */


   if (!thread->opengl.context || !thread->opengl.draw)
   {
      vcos_log_trace("Send null make current %x %x",
                 (unsigned int)(char *)thread->opengl.context, (unsigned int)(char *)thread->opengl.draw);
   }
   else
   {
      vcos_log_trace("Send make current %d[%d %s%s] %d[%d %d%s]",
            (int)thread->opengl.context->name,
            thread->opengl.context->servercontext,
            thread->opengl.context->is_current ? " C" : "",
            thread->opengl.context->is_destroyed ? " D" : "",
            (int)thread->opengl.draw->name,
            thread->opengl.draw->serverbuffer,
            thread->opengl.draw->context_binding_count,
            thread->opengl.draw->is_destroyed ? " D" : "");
   }

   RPC_CALL8_MAKECURRENT(eglIntMakeCurrent_impl,
                         thread,
                         EGLINTMAKECURRENT_ID,
                         RPC_UINT((uint32_t)pid),
                         RPC_UINT((uint32_t)(pid >> 32)),
                         RPC_UINT(gltype),
                         RPC_UINT(servergl),
                         RPC_UINT(servergldraw),
                         RPC_UINT(serverglread),
                         RPC_UINT(servervg),
                         RPC_UINT(servervgsurf));
}

PLATFORM_TLS_T client_tls;
PLATFORM_MUTEX_T client_mutex;
#ifdef CLIENT_THREAD_IS_PROCESS 
PLATFORM_TLS_T client_tls_process;
PLATFORM_TLS_T client_tls_mutex;
#endif

bool client_process_attach()
{
   KHR_STATUS_T status;
   status = platform_tls_create(&client_tls);
   if (status != KHR_SUCCESS) {
      return false;
   }

#ifdef CLIENT_THREAD_IS_PROCESS
   status = platform_tls_create(&client_tls_process);
   if (status != KHR_SUCCESS) {
      return false;
   }
	
   status = platform_tls_create(&client_tls_mutex);
   if (status != KHR_SUCCESS) {
      return false;
   }
#endif

   status = platform_mutex_create(&client_mutex);

   if (status != KHR_SUCCESS) {
      platform_tls_destroy(client_tls);
      return false;
   }
   if (!RPC_INIT()) {
      platform_mutex_destroy(&client_mutex);
      platform_tls_destroy(client_tls);
      return false;
   }
   return true;
}

bool client_thread_attach()
{
   CLIENT_THREAD_STATE_T *state = (CLIENT_THREAD_STATE_T *)khrn_platform_malloc(sizeof(CLIENT_THREAD_STATE_T), "CLIENT_THREAD_STATE_T");

   if (!state)
      return false;

   client_thread_state_init(state);

   platform_tls_set(client_tls, state);

#ifdef CLIENT_THREAD_IS_PROCESS
	{  //add mutex into thread's tls
		KHR_STATUS_T status;
		PLATFORM_MUTEX_T *local_mutex = (PLATFORM_MUTEX_T*)vcos_tls_get(client_tls_mutex);
		
		if (!local_mutex)
		{			
			local_mutex = (PLATFORM_MUTEX_T*)khrn_platform_malloc(sizeof(PLATFORM_MUTEX_T),"thread mutex");
			if (!local_mutex)
				return false;
			
			status = platform_mutex_create(local_mutex);			
			if (status != KHR_SUCCESS) {
				khrn_platform_free(local_mutex);
				return false;
			}
			
			vcos_tls_set(client_tls_mutex,local_mutex);
		}
	}
#endif	

#ifndef RPC_LIBRARY    //TODO
   client_send_make_current(state);
#endif
   return true;
}

void client_thread_detach(void *dummy)
{
   CLIENT_THREAD_STATE_T *state = CLIENT_GET_THREAD_STATE();
   UNUSED(dummy);

   platform_tls_remove(client_tls);
   client_thread_state_term(state);

   khrn_platform_free(state);
   platform_maybe_free_process();
	
#ifdef CLIENT_THREAD_IS_PROCESS
	{
		CLIENT_PROCESS_STATE_T *process = CLIENT_GET_PROCESS_STATE();	
   	khrn_platform_free(process);
		platform_tls_remove(client_tls_process);
	}

	{
		PLATFORM_MUTEX_T *local_mutex = (PLATFORM_MUTEX_T*)vcos_tls_get(client_tls_mutex);
		vcos_assert(local_mutex);

		platform_mutex_destroy(local_mutex);

		khrn_platform_free(local_mutex);
		platform_tls_remove(client_tls_mutex);
	}
#endif
}

void client_process_detach()
{
   RPC_TERM();
   platform_tls_destroy(client_tls);
   platform_mutex_destroy(&client_mutex);

#ifdef CLIENT_THREAD_IS_PROCESS	
	platform_tls_destroy(client_tls_process);
#endif
}

#ifdef RPC_LIBRARY
KHRONOS_SERVER_CONNECTION_T *client_library_get_connection(void)
{
   return &client_process_state.khrn_connection;
}

#endif

#if defined(RPC_LIBRARY) || defined(RPC_DIRECT_MULTI)

void client_library_send_make_current(const KHRONOS_FUNC_TABLE_T *func_table)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (thread->opengl.context || thread->openvg.context)
   {
      uint64_t pid                  = rpc_get_client_id(thread);
      uint32_t gltype               = thread->opengl.context ? convert_gltype(thread->opengl.context->type) : 0;
      EGL_GL_CONTEXT_ID_T servergl  = thread->opengl.context ? thread->opengl.context->servercontext : EGL_SERVER_NO_GL_CONTEXT;
      EGL_SURFACE_ID_T servergldraw = thread->opengl.draw    ? thread->opengl.draw->serverbuffer     : EGL_SERVER_NO_SURFACE;
      EGL_SURFACE_ID_T serverglread = thread->opengl.read    ? thread->opengl.read->serverbuffer     : EGL_SERVER_NO_SURFACE;
      EGL_VG_CONTEXT_ID_T servervg  = thread->openvg.context ? thread->openvg.context->servercontext : EGL_SERVER_NO_VG_CONTEXT;
      EGL_SURFACE_ID_T servervgsurf = thread->openvg.draw    ? thread->openvg.draw->serverbuffer     : EGL_SERVER_NO_SURFACE;

      /*
         if the size of this call in the merge buffer changes,
         CLIENT_MAKE_CURRENT_SIZE in khrn_client.h should be updated
      */

      func_table->eglIntMakeCurrent_impl(
                (uint32_t)pid,
                (uint32_t)(pid >> 32),
                gltype,
                servergl,
                servergldraw,
                serverglread,
                servervg,
                servervgsurf);
   }
}

#endif

#ifdef GL_GET_ERROR_ASYNC
static void callback_set_error(KHRN_POINTER_MAP_T *map, uint32_t key, void *value, void *data)
{
   EGL_CONTEXT_T *context = (EGL_CONTEXT_T *)value;

   UNUSED(map);
   UNUSED_NDEBUG(key);
   
   vcos_assert( context != NULL );
   vcos_assert((uintptr_t)key == (uintptr_t)context->name);
   
   if (context->servercontext == *((uint32_t *)data)){
      CLIENT_THREAD_STATE_T *thread = context->thread;
      /* todo: VG */      
      if (thread && IS_OPENGLES_11_OR_20(thread)) {
         vcos_log_error("GL OOM context %d", context->servercontext);
         glxx_set_error(GLXX_GET_CLIENT_STATE(thread), GL_OUT_OF_MEMORY);
      }
   }
}

void client_set_error(uint32_t server_context_name)
{
   CLIENT_PROCESS_STATE_T *process;
   CLIENT_LOCK();
   process = CLIENT_GET_PROCESS_STATE();
   khrn_pointer_map_iterate(&process->contexts, callback_set_error, &server_context_name);
   CLIENT_UNLOCK();
}
#endif
