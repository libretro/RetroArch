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

#ifndef GLXX_CLIENT_H
#define GLXX_CLIENT_H

#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_platform.h"
#include "interface/khronos/common/khrn_client_cache.h"

#include "interface/khronos/egl/egl_client_context.h"

#include "interface/khronos/glxx/glxx_int_attrib.h"
#include "interface/khronos/glxx/glxx_int_config.h"

/*
   Called just before a rendering command (i.e. anything which could modify
   the draw surface) is executed
 */
typedef void (*GL_RENDER_CALLBACK_T)(void);

/*
   Called just after rendering has been compeleted (i.e. flush or finish).
   wait should be true for finish-like behaviour, false for flush-like
   behaviour
*/
typedef void (*GL_FLUSH_CALLBACK_T)(bool wait);

/*
   GL 1.1 and 2.0 client state structure
*/

typedef struct buffer_info {
   GLuint id;
   GLsizeiptr cached_size;
   void * mapped_pointer;
   GLsizeiptr mapped_size;
} GLXX_BUFFER_INFO_T;

typedef struct {

   GLenum error;

   /*
      Open GL version

      Invariants:

      OPENGL_ES_11 or OPENGL_ES_20
   */

   unsigned int type;

   /*
      alignments

      used to work out how much data to send for glTexImage2D()
   */

   struct {
      GLint pack;
      GLint unpack;
   } alignment;

   struct {
      GLuint array;
      GLuint element_array;
   } bound_buffer;

   GLXX_ATTRIB_T attrib[GLXX_CONFIG_MAX_VERTEX_ATTRIBS];

   GL_RENDER_CALLBACK_T render_callback;
   GL_FLUSH_CALLBACK_T flush_callback;

   KHRN_CACHE_T cache;

   //gl 1.1 specific
   struct {
      GLenum client;
      GLenum server;
   } active_texture;

   //gl 2.0 specific
   bool default_framebuffer;   //render_callback only called if we're rendering to default framebuffer

   KHRN_POINTER_MAP_T buffers;

} GLXX_CLIENT_STATE_T;

extern int gl11_client_state_init(GLXX_CLIENT_STATE_T *state);
extern int gl20_client_state_init(GLXX_CLIENT_STATE_T *state);

extern void glxx_client_state_free(GLXX_CLIENT_STATE_T *state);

#define GLXX_GET_CLIENT_STATE(thread) glxx_get_client_state(thread)

static INLINE GLXX_CLIENT_STATE_T *glxx_get_client_state(CLIENT_THREAD_STATE_T *thread)
{
   EGL_CONTEXT_T *context = thread->opengl.context;
   GLXX_CLIENT_STATE_T * state;
   vcos_assert( context != NULL );
   vcos_assert(context->type == OPENGL_ES_11 || context->type == OPENGL_ES_20);
   state = (GLXX_CLIENT_STATE_T *)context->state;
   vcos_assert(context->type == state->type);
   return state;
}

#define GLXX_API_11 (1<<(OPENGL_ES_11))
#define GLXX_API_20 (1<<(OPENGL_ES_20))
#define GLXX_API_11_OR_20 (GLXX_API_11|GLXX_API_20)

static INLINE bool glxx_api_ok(uint32_t api, EGL_CONTEXT_TYPE_T type)
{
   return !!(api & (1<<type));
}

#define IS_OPENGLES_11(thread)       is_opengles_api(thread, GLXX_API_11)
#define IS_OPENGLES_20(thread)       is_opengles_api(thread, GLXX_API_20)
#define IS_OPENGLES_11_OR_20(thread) is_opengles_api(thread, GLXX_API_11_OR_20)
#define IS_OPENGLES_API(thread, api) is_opengles_api(thread, api)

static INLINE bool is_opengles_api(CLIENT_THREAD_STATE_T *thread, uint32_t api)
{
   EGL_CONTEXT_T *context = thread->opengl.context;
   return context && glxx_api_ok(api, context->type);
}

extern void glxx_buffer_info_get(GLXX_CLIENT_STATE_T *state, GLenum target, GLXX_BUFFER_INFO_T* buffer);
extern void glxx_buffer_info_set(GLXX_CLIENT_STATE_T *state, GLenum target, GLXX_BUFFER_INFO_T* buffer);
extern void glxx_set_error(GLXX_CLIENT_STATE_T *state, GLenum error);
extern void glxx_set_error_api(uint32_t api, GLenum error);

/* Fake GL API calls */
void glintAttribPointer (uint32_t api, uint32_t indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *ptr);
void glintAttrib (uint32_t api, uint32_t indx, float x, float y, float z, float w);
void glintColor(float red, float green, float blue, float alpha);
void glintAttribEnable(uint32_t api, uint32_t indx, bool enabled);
void *glintAttribGetPointer(uint32_t api, uint32_t indx);

#endif
