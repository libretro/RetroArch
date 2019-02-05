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

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/glxx/glxx_client.h"
#include "interface/khronos/vg/vg_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#include "interface/khronos/egl/egl_client_context.h"
#include "interface/khronos/egl/egl_client_surface.h"

#ifdef RPC_DIRECT
#include "interface/khronos/egl/egl_int_impl.h"
#endif

#include <string.h>
#include <stdlib.h>

EGLBoolean egl_context_check_attribs(const EGLint *attrib_list, EGLint max_version, EGLint *version)
{
   if (!attrib_list)
      return EGL_TRUE;

   while (1) {
      switch (*attrib_list++) {
      case EGL_CONTEXT_CLIENT_VERSION:
      {
         EGLint value = *attrib_list++;

         if (value < 1 || value > max_version)
            return EGL_FALSE;
         else
            *version = value;

         break;
      }
      case EGL_NONE:
         return EGL_TRUE;
      default:
         return EGL_FALSE;
      }
   }
}

EGL_CONTEXT_T *egl_context_create(EGL_CONTEXT_T *share_context, EGLContext name, EGLDisplay display, EGLConfig configname, EGL_CONTEXT_TYPE_T type)
{
   EGL_CONTEXT_T *context = (EGL_CONTEXT_T *)khrn_platform_malloc(sizeof(EGL_CONTEXT_T), "EGL_CONTEXT_T");
   if (!context)
      return 0;

   context->name = name;
   context->display = display;
   context->configname = configname;

   context->type = type;

   context->renderbuffer = EGL_NONE;

   context->is_current = false;
   context->is_destroyed = false;

   switch (type) {
#ifndef NO_OPENVG
   case OPENVG:
   {
      VG_CLIENT_SHARED_STATE_T *shared_state;
      if (share_context) {
         shared_state = ((VG_CLIENT_STATE_T *)share_context->state)->shared_state;
         vg_client_shared_state_acquire(shared_state);
      } else {
         shared_state = vg_client_shared_state_alloc();
         if (!shared_state) {
            khrn_platform_free(context);
            return 0;
         }
      }

      context->state = vg_client_state_alloc(shared_state);
      vg_client_shared_state_release(shared_state);
      if (!context->state) {
         khrn_platform_free(context);
         return 0;
      }

      {
      CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
         /* uint64_t pid = khronos_platform_get_process_id(); */ /* unused */
      context->servercontext = RPC_UINT_RES(RPC_CALL2_RES(eglIntCreateVG_impl,
                                                             thread,
                                                             EGLINTCREATEVG_ID,
                                                             share_context ? share_context->servercontext : 0,
                                                          share_context ? share_context->type : OPENVG/*ignored*/));
      }
      if (!context->servercontext) {
         vg_client_state_free((VG_CLIENT_STATE_T *)context->state);
         khrn_platform_free(context);
         return 0;
      }

      break;
   }
#endif /* NO_OPENVG */
   case OPENGL_ES_11:
   {
      GLXX_CLIENT_STATE_T *state = (GLXX_CLIENT_STATE_T *)khrn_platform_malloc(sizeof(GLXX_CLIENT_STATE_T), "GLXX_CLIENT_STATE_T");
      if (!state) {
         khrn_platform_free(context);
         return 0;
      }

      context->state = state;
      if (gl11_client_state_init(state)) {
         CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
         context->servercontext = RPC_UINT_RES(RPC_CALL2_RES(eglIntCreateGLES11_impl,
                                                             thread,
                                                             EGLINTCREATEGLES11_ID,
                                                             share_context ? share_context->servercontext : 0,
                                                             share_context ? share_context->type : OPENGL_ES_11/*ignored*/));
         if (!context->servercontext) {
            glxx_client_state_free(state);
            khrn_platform_free(context);
            return 0;
         }
      }
      break;
   }
   case OPENGL_ES_20:
   {
      GLXX_CLIENT_STATE_T *state = (GLXX_CLIENT_STATE_T *)khrn_platform_malloc(sizeof(GLXX_CLIENT_STATE_T), "GLXX_CLIENT_STATE_T");
      if (!state) {
         khrn_platform_free(context);
         return 0;
      }

      context->state = state;

      if (gl20_client_state_init(state)) {
         CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
         context->servercontext = RPC_UINT_RES(RPC_CALL2_RES(eglIntCreateGLES20_impl,
                                                             thread,
                                                             EGLINTCREATEGLES20_ID,
                                                             share_context ? share_context->servercontext : 0,
                                                             share_context ? share_context->type : OPENGL_ES_20/*ignored*/));
         if (!context->servercontext) {
            glxx_client_state_free(state);
            khrn_platform_free(context);
            return 0;
         }
      }
      break;
   }
   default:
      UNREACHABLE();
      break;
   }

   return context;
}

void egl_context_term(EGL_CONTEXT_T *context)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   /* If we're current then there should still be a reference to us */
   /* (if this wasn't the case we should call egl_context_release_surfaces here) */
   vcos_assert(!context->is_current);
   vcos_assert(context->is_destroyed);

   switch (context->type) {
#ifndef NO_OPENVG
   case OPENVG:
      RPC_CALL1(eglIntDestroyVG_impl,
                thread,
                EGLINTDESTROYVG_ID,
                RPC_UINT(context->servercontext));
      RPC_FLUSH(thread);
      vg_client_state_free((VG_CLIENT_STATE_T *)context->state);
      break;
#endif
   case OPENGL_ES_11:
   case OPENGL_ES_20:
       RPC_CALL1(eglIntDestroyGL_impl,
                thread,
                EGLINTDESTROYGL_ID,
                RPC_UINT(context->servercontext));
      RPC_FLUSH(thread);
      glxx_client_state_free((GLXX_CLIENT_STATE_T *)context->state);
      break;
   default:
      UNREACHABLE();
   }

   context->state = 0;
}

EGLBoolean egl_context_get_attrib(EGL_CONTEXT_T *context, EGLint attrib, EGLint *value)
{
   switch (attrib) {
   case EGL_CONFIG_ID:
      *value = (int)(intptr_t)context->configname;
      return EGL_TRUE;
   case EGL_CONTEXT_CLIENT_TYPE:
      switch (context->type) {
      case OPENGL_ES_11:
      case OPENGL_ES_20:
         *value = EGL_OPENGL_ES_API;
         break;
      case OPENVG:
         *value = EGL_OPENVG_API;
         break;
      default:
         UNREACHABLE();
         break;
      }
      return EGL_TRUE;
   case EGL_CONTEXT_CLIENT_VERSION:
      switch (context->type) {
      case OPENGL_ES_11:
      case OPENVG:
         *value = 1;
         break;
      case OPENGL_ES_20:
         *value = 2;
         break;
      default:
         UNREACHABLE();
         break;
      }
      return EGL_TRUE;
   case EGL_RENDER_BUFFER:
   {
      /* TODO: GLES supposedly doesn't support single-buffered rendering. Should we take this into account? */
      *value = context->renderbuffer;
      return EGL_TRUE;
   }
   default:
      return EGL_FALSE;
   }
}

void egl_context_set_callbacks(EGL_CONTEXT_T *context,
                               void (*gl_render_callback)(void),
                               void (*gl_flush_callback)(bool),
                               void (*vg_render_callback)(void),
                               void (*vg_flush_callback)(bool))
{
   switch (context->type) {
      case OPENGL_ES_11:
      {
         GLXX_CLIENT_STATE_T *state = (GLXX_CLIENT_STATE_T *)context->state;
         state->render_callback = gl_render_callback;
         state->flush_callback = gl_flush_callback;
         break;
      }
      case OPENGL_ES_20:
      {
         GLXX_CLIENT_STATE_T *state = (GLXX_CLIENT_STATE_T *)context->state;
         state->render_callback = gl_render_callback;
         state->flush_callback = gl_flush_callback;
         break;
      }
      case OPENVG:
      {
         VG_CLIENT_STATE_T *state = (VG_CLIENT_STATE_T *)context->state;
         state->render_callback = vg_render_callback;
         state->flush_callback = vg_flush_callback;
         break;
      }
      default:
         UNREACHABLE();
   }
}

/*
   void egl_context_maybe_free(EGL_CONTEXT_T *context)

   Frees a map together with its server-side resources if:
    - it has been destroyed
    - it is no longer current

   Implementation notes:

   -

   Preconditions:

   context is a valid pointer

   Postconditions:

   Either:
   - context->is_destroyed is false (we don't change this), or
   - context->is_current is true, or
   - context has been deleted.

   Invariants preserved:

   -

   Invariants used:

   -
 */
void egl_context_maybe_free(EGL_CONTEXT_T *context)
{
   vcos_assert(context);

   if (!context->is_destroyed)
      return;

   if (context->is_current)
      return;

   egl_context_term(context);
   khrn_platform_free(context);
}
