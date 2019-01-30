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

#define EGL_EGLEXT_PROTOTYPES /* we want the prototypes so the compiler will check that the signatures match */

#ifndef SYNC_FENCE_KHR_SHORTCUT
//#define SYNC_FENCE_KHR_SHORTCUT 0 /* notifications made in driver back-end */
#define SYNC_FENCE_KHR_SHORTCUT 1 /* notifications made in dispatch */
#endif

//==============================================================================

#include "interface/khronos/common/khrn_client_mangle.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#include "interface/khronos/ext/egl_khr_sync_client.h"
#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"

#if defined(V3D_LEAN)
#include "interface/khronos/common/khrn_int_misc_impl.h"
#endif

//==============================================================================

typedef struct {
   EGLint condition;
   EGLint threshold;
   EGLint status;
   EGLenum type;

   int name[3]; // Used as ID in khronos_platform_semaphore_create

   EGL_SYNC_ID_T serversync;

   /*
      we keep one master handle to the named semaphore in existence for the
      lifetime of the sync object, allowing both wait functions and the KHAN
      message handler to "open, post/wait, close".
   */

   PLATFORM_SEMAPHORE_T master;
} EGL_SYNC_T;

//==============================================================================

static EGL_SYNC_T *egl_sync_create(EGLSyncKHR sync, EGLenum type,
      EGLint condition, EGLint threshold, EGLint status)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGL_SYNC_T *sync_ptr = (EGL_SYNC_T *)khrn_platform_malloc(sizeof(EGL_SYNC_T), "EGL_SYNC_T");
   uint64_t pid = rpc_get_client_id(thread);
   uint32_t sem;

   if (!sync_ptr)
      return 0;

   sync_ptr->condition = condition;
   sync_ptr->threshold = threshold;
   sync_ptr->type = type;
   sync_ptr->status = status;

   sync_ptr->name[0] = (int)pid;
   sync_ptr->name[1] = (int)(pid >> 32);
   sync_ptr->name[2] = (int)sync;

   if (khronos_platform_semaphore_create(&sync_ptr->master, sync_ptr->name, 0) != KHR_SUCCESS) {
      khrn_platform_free(sync_ptr);
      return 0;
   }

   sem = (uint32_t) sync;
#if SYNC_FENCE_KHR_SHORTCUT == 1
   if (type == EGL_SYNC_FENCE_KHR){
      RPC_CALL3(eglIntCreateSyncFence_impl,
                               thread,
                               EGLINTCREATESYNCFENCE_ID,
                               RPC_UINT(condition),
                               RPC_INT(threshold),
                               RPC_UINT(sem));
   } else 
#endif
   {
      sync_ptr->serversync = RPC_UINT_RES(RPC_CALL4_RES(eglIntCreateSync_impl,
                                                 thread,
                                                 EGLINTCREATESYNC_ID,
                                                 RPC_UINT(type),
                                                 RPC_UINT(condition),
                                                 RPC_INT(threshold),
                                                 RPC_UINT(sem)));
      if (!sync_ptr->serversync) {
         khronos_platform_semaphore_destroy(&sync_ptr->master);
         khrn_platform_free(sync_ptr);
         return 0;
      }
   }
   return sync_ptr;
}

//------------------------------------------------------------------------------

static void egl_sync_term(EGL_SYNC_T *sync_master)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
#if SYNC_FENCE_KHR_SHORTCUT == 1
   if (sync_master->type != EGL_SYNC_FENCE_KHR)
#endif
   {
      RPC_CALL1(eglIntDestroySync_impl,
                thread,
                EGLINTDESTROYSYNC_ID,
                RPC_UINT(sync_master->serversync));
   }
   khronos_platform_semaphore_destroy(&sync_master->master);
}

//------------------------------------------------------------------------------

static void egl_sync_destroy_iterator
   (KHRN_POINTER_MAP_T *sync_map, uint32_t sync, void *sync_handle, void *data)
{
   EGL_SYNC_T *sync_ptr = (EGL_SYNC_T *) sync;

   UNUSED(sync_map);
   UNUSED(sync_handle);
   UNUSED(data);

   vcos_assert(sync_ptr != NULL);

   egl_sync_term(sync_ptr);
   khrn_platform_free(sync_ptr);
}

//------------------------------------------------------------------------------

static EGLBoolean egl_sync_check_attribs(const EGLint *attrib_list, EGLenum type,
      EGLint *condition, EGLint *threshold, EGLint *status)
{
   switch (type) {
   case EGL_SYNC_FENCE_KHR:
      *condition = EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR;
      *threshold = 0;
      *status = EGL_UNSIGNALED_KHR;
      break;
   default :
      *condition = EGL_NONE;
      *threshold = 0;
      *status = 0;
      break;
   }

   if (attrib_list) {
      while (1) {
         int name = *attrib_list++;
         if (name == EGL_NONE)
            break;
         else {
            /* int value = * */attrib_list++; /* at present no name/value pairs are handled */
            switch (name) {
            default:
               return EGL_FALSE;
            }
         }
      }
   }

   return ((type == EGL_SYNC_FENCE_KHR) || (type == 0));
}

//------------------------------------------------------------------------------

static EGLBoolean egl_sync_get_attrib(EGL_SYNC_T *sync, EGLint attrib, EGLint *value)
{
   switch (attrib) {
   case EGL_SYNC_TYPE_KHR:
      *value = sync->type;
      return EGL_TRUE;
   case EGL_SYNC_STATUS_KHR:
      *value = sync->status;
      return EGL_TRUE;
   case EGL_SYNC_CONDITION_KHR:
      *value = sync->condition;
      return EGL_TRUE;
   default:
      return EGL_FALSE;
   }
}

//==============================================================================

EGLAPI EGLSyncKHR EGLAPIENTRY eglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLSyncKHR sync = EGL_NO_SYNC_KHR;

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      EGLint condition;
      EGLint threshold;
      EGLint status;

      if (process)
      {
         if (egl_sync_check_attribs(attrib_list, type, &condition, &threshold, &status)) {
            EGL_SYNC_T *sync_ptr = egl_sync_create((EGLSyncKHR)(size_t)process->next_sync, type, condition, threshold, status);

            if (sync_ptr) {
               if (khrn_pointer_map_insert(&process->syncs, process->next_sync, sync_ptr)) {
                  thread->error = EGL_SUCCESS;
                  sync = (EGLSurface)(size_t)process->next_sync++;
               } else {
                  thread->error = EGL_BAD_ALLOC;
                  egl_sync_term(sync_ptr);
                  khrn_platform_free(sync_ptr);
               }
            } else
               thread->error = EGL_BAD_ALLOC;
         }
      }
   }

   CLIENT_UNLOCK();

   return sync;
}

//------------------------------------------------------------------------------
// TODO: should we make sure any syncs have come back before destroying the object?

EGLAPI EGLBoolean EGLAPIENTRY eglDestroySyncKHR(EGLDisplay dpy, EGLSyncKHR sync)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (process) {
         EGL_SYNC_T *sync_ptr = (EGL_SYNC_T *)khrn_pointer_map_lookup(&process->syncs, (uint32_t)(size_t)sync);

         if (sync_ptr) {
            thread->error = EGL_SUCCESS;

            khrn_pointer_map_delete(&process->syncs, (uint32_t)(uintptr_t)sync);

            egl_sync_term(sync_ptr);
            khrn_platform_free(sync_ptr);
         } else
            thread->error = EGL_BAD_PARAMETER;

         result = (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE);
      } else {
         result = EGL_FALSE;
      }
   }

   CLIENT_UNLOCK();

   return result;
}

//------------------------------------------------------------------------------

EGLAPI EGLint EGLAPIENTRY eglClientWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   UNUSED(timeout);

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (process) {
         EGL_SYNC_T *sync_ptr = (EGL_SYNC_T *)khrn_pointer_map_lookup(&process->syncs, (uint32_t)(size_t)sync);

         if (sync_ptr) {
            PLATFORM_SEMAPHORE_T semaphore;
            if( khronos_platform_semaphore_create(&semaphore, sync_ptr->name, 1) == KHR_SUCCESS) {
               if (flags & EGL_SYNC_FLUSH_COMMANDS_BIT_KHR)
                  RPC_FLUSH(thread);

               CLIENT_UNLOCK();

               khronos_platform_semaphore_acquire(&semaphore);
               khronos_platform_semaphore_release(&semaphore);
               khronos_platform_semaphore_destroy(&semaphore);
               return EGL_CONDITION_SATISFIED_KHR;
            } else
               thread->error = EGL_BAD_ALLOC;         // not strictly allowed by the spec, but indicates that we failed to create a reference to the named semaphore
         } else
            thread->error = EGL_BAD_PARAMETER;
      }
   }

   CLIENT_UNLOCK();

   return EGL_FALSE;
}

//------------------------------------------------------------------------------

EGLAPI EGLBoolean EGLAPIENTRY eglSignalSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   UNUSED(mode);

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (process) {
         EGL_SYNC_T *sync_ptr = (EGL_SYNC_T *)khrn_pointer_map_lookup(&process->syncs, (uint32_t)(size_t)sync);

         if (sync_ptr)
            thread->error = EGL_BAD_MATCH;
         else
            thread->error = EGL_BAD_PARAMETER;
      }
   }

   CLIENT_UNLOCK();

   return EGL_FALSE;
}

//------------------------------------------------------------------------------

EGLAPI EGLBoolean EGLAPIENTRY eglGetSyncAttribKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result = EGL_FALSE;

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (process)
      {
         if (value)
         {
            EGL_SYNC_T *sync_ptr = (EGL_SYNC_T *)khrn_pointer_map_lookup(&process->syncs, (uint32_t)(size_t)sync);

            if (sync_ptr) {
               if (egl_sync_get_attrib(sync_ptr, attribute, value)) {
                  thread->error = EGL_SUCCESS;
                  result = EGL_TRUE;
               } else
                  thread->error = EGL_BAD_ATTRIBUTE;
            } else
               thread->error = EGL_BAD_PARAMETER;
         }
         else
         {
            thread->error = EGL_BAD_PARAMETER;
         }
      }
   }

   CLIENT_UNLOCK();

   return result;
}

//------------------------------------------------------------------------------

void egl_sync_destroy_all(KHRN_POINTER_MAP_T *sync_map)
{
   khrn_pointer_map_iterate(sync_map, egl_sync_destroy_iterator, NULL);
}

//==============================================================================
