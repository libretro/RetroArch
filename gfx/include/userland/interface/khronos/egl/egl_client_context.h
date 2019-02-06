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

#ifndef EGL_CLIENT_CONTEXT_H
#define EGL_CLIENT_CONTEXT_H

#include "interface/khronos/egl/egl_int.h"

typedef struct {
   EGLContext name;
   EGLDisplay display;
   EGLConfig configname;

   EGL_CONTEXT_TYPE_T type;

   EGLint renderbuffer;    //EGL_NONE, EGL_BACK_BUFFER or EGL_SINGLE_BUFFER

   void *state;                                 // GLXX_CLIENT_STATE_T or VG_CLIENT_STATE_T
   EGL_CONTEXT_ID_T servercontext;

   struct CLIENT_THREAD_STATE *thread;          // If we are current, which the client state for the thread are we associated with.

   /*
      is_current

      Invariant:

      (EGL_CONTEXT_IS_CURRENT)
      Iff true, the context is current to some thread.
   */
   bool is_current;
   /*
      is_destroyed

      Invariant:

      (EGL_CONTEXT_IS_DESTROYED)
      Iff true, is not a member of the CLIENT_PROCESS_STATE_T.contexts
   */
   bool is_destroyed;
} EGL_CONTEXT_T;

extern EGLBoolean egl_context_check_attribs(const EGLint *attrib_list, EGLint max_version, EGLint *version);

extern EGL_CONTEXT_T *egl_context_create(EGL_CONTEXT_T *share_context, EGLContext name, EGLDisplay display, EGLConfig configname, EGL_CONTEXT_TYPE_T type);
extern void egl_context_term(EGL_CONTEXT_T *context);

extern void egl_context_set_callbacks(EGL_CONTEXT_T *context,
                                      void (*gl_render_callback)(void),
                                      void (*gl_flush_callback)(bool),
                                      void (*vg_render_callback)(void),
                                      void (*vg_flush_callback)(bool));

extern EGLBoolean egl_context_get_attrib(EGL_CONTEXT_T *context, EGLint attrib, EGLint *value);
extern void egl_context_maybe_free(EGL_CONTEXT_T *context);

#endif
