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

#define GL_GLEXT_PROTOTYPES /* we want the prototypes so the compiler will check that the signatures match */

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/glxx/glxx_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#include "interface/khronos/glxx/gl11_int_config.h"

#include "interface/khronos/include/GLES/glext.h"

#ifdef RPC_DIRECT
#include "interface/khronos/glxx/gl11_int_impl.h"
#endif

#if GL_OES_matrix_palette

static void set_error(GLXX_CLIENT_STATE_T *state, GLenum e) {
   if (state->error == GL_NO_ERROR) state->error = e;
}

static void set_client_state_error(GLenum e) {
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   GLXX_CLIENT_STATE_T *state = GLXX_GET_CLIENT_STATE(thread);
   set_error(state, e);
}

GL_API void GL_APIENTRY glCurrentPaletteMatrixOES(GLuint index) {
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (IS_OPENGLES_11(thread)) {
      if (index > GL11_CONFIG_MAX_PALETTE_MATRICES_OES - 1) set_client_state_error(GL_INVALID_VALUE);
      else {
         RPC_CALL1(glCurrentPaletteMatrixOES_impl,
                   thread,
                   GLCURRENTPALETTEMATRIXOES_ID_11,
                   RPC_UINT(index)                 );
      }
   }
}

GL_API void GL_APIENTRY glLoadPaletteFromModelViewMatrixOES() {
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (IS_OPENGLES_11(thread)) {
      RPC_CALL0(glLoadPaletteFromModelViewMatrixOES_impl,
                thread,
                GLLOADPALETTEFROMMODELVIEWMATRIXOES_ID_11);
   }
}

static GLboolean is_matrix_index_type(GLenum type) {
   return (type == GL_UNSIGNED_BYTE);
}

static GLboolean is_matrix_palette_size(GLint size) {
   /* TODO: Should size 0 be allowed or not? */
   return size > 0 && size <= GL11_CONFIG_MAX_VERTEX_UNITS_OES;
}

/* TODO: This is copied from glxx_client.c. Find a better method */
static GLboolean is_aligned( GLenum type, size_t value)
{
   switch (type) {
   case GL_BYTE:
   case GL_UNSIGNED_BYTE:
      return GL_TRUE;
   case GL_SHORT:
   case GL_UNSIGNED_SHORT:
      return (value & 1) == 0;
   case GL_FIXED:
   case GL_FLOAT:
      return (value & 3) == 0;
   default:
      UNREACHABLE();
      return GL_FALSE;
   }
}

GL_API void GL_APIENTRY glMatrixIndexPointerOES(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
   if (is_matrix_index_type(type)) {
      if (is_matrix_palette_size(size) && is_aligned(type, (size_t)pointer) && is_aligned(type, (size_t)stride) && stride >= 0) {
         glintAttribPointer(GLXX_API_11, GL11_IX_MATRIX_INDEX, size, type, GL_FALSE, stride, pointer);
      } else
         glxx_set_error_api(GLXX_API_11, GL_INVALID_VALUE);
   } else
      glxx_set_error_api(GLXX_API_11, GL_INVALID_ENUM);
}

static GLboolean is_matrix_weight_type(GLenum type) {
   return type == GL_FIXED ||
          type == GL_FLOAT;
}

GL_API void GL_APIENTRY glWeightPointerOES(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
   if (is_matrix_weight_type(type)) {
      if (is_matrix_palette_size(size) && is_aligned(type, (size_t)pointer) && is_aligned(type, (size_t)stride) && stride >= 0) {
         glintAttribPointer(GLXX_API_11, GL11_IX_MATRIX_WEIGHT, size, type, GL_FALSE, stride, pointer);
      } else
         glxx_set_error_api(GLXX_API_11, GL_INVALID_VALUE);
   } else
      glxx_set_error_api(GLXX_API_11, GL_INVALID_ENUM);
}

#endif /* GL_OES_matrix_palette */
