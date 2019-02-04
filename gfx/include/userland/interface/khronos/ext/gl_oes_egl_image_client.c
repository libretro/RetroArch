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
#define VCOS_LOG_CATEGORY (&gl_oes_egl_image_client_log)

#include "interface/khronos/common/khrn_client_mangle.h"

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/glxx/glxx_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#ifdef RPC_DIRECT
#include "interface/khronos/glxx/glxx_int_impl.h"
#include "interface/khronos/glxx/gl20_int_impl.h"
#endif

#include "interface/khronos/include/GLES2/gl2.h"
#include "interface/khronos/include/GLES2/gl2ext.h"

VCOS_LOG_CAT_T gl_oes_egl_image_client_log = VCOS_LOG_INIT("gl_oes_egl_image", VCOS_LOG_WARN);

static void set_error(GLXX_CLIENT_STATE_T *state, GLenum error)
{
   if (state->error == GL_NO_ERROR)
      state->error = error;
}

static bool check_global_image_egl_image(GLuint global_image_id[2],
   GLeglImageOES image, CLIENT_THREAD_STATE_T *thread,
   bool render) /* else texture */
{
   CLIENT_PROCESS_STATE_T *process = CLIENT_GET_PROCESS_STATE();
   uint64_t id;
   uint32_t format, width, height;

   CLIENT_LOCK();
   id = process->inited ? khrn_global_image_map_lookup(&process->global_image_egl_images, (uint32_t)(uintptr_t)image) : 0;
   CLIENT_UNLOCK();
   if (!id) {
      return false;
   }
   global_image_id[0] = (GLuint)id;
   global_image_id[1] = (GLuint)(id >> 32);

   platform_get_global_image_info(global_image_id[0], global_image_id[1], &format, &width, &height);

   if (!(format & ((thread->opengl.context->type == OPENGL_ES_11) ?
      (render ? EGL_PIXEL_FORMAT_RENDER_GLES_BRCM : EGL_PIXEL_FORMAT_GLES_TEXTURE_BRCM) :
      (render ? EGL_PIXEL_FORMAT_RENDER_GLES2_BRCM : EGL_PIXEL_FORMAT_GLES2_TEXTURE_BRCM))) ||
      (width == 0) || (height == 0)) {
      return false;
   }

   /* format and max width/height checks done on server */

   return true;
}

GL_API void GL_APIENTRY glEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   if (IS_OPENGLES_11_OR_20(thread)) {
#if EGL_BRCM_global_image
      if ((uintptr_t)image & (1u << 31)) {
         GLuint global_image_id[2];
         if (check_global_image_egl_image(global_image_id, image, thread, false)) {
            RPC_CALL3(glGlobalImageTexture2DOES_impl,
                      thread,
                      GLGLOBALIMAGETEXTURE2DOES_ID,
                      RPC_ENUM(target),
                      RPC_UINT(global_image_id[0]),
                      RPC_UINT(global_image_id[1]));
         } else {
            set_error(GLXX_GET_CLIENT_STATE(thread), GL_INVALID_VALUE);
         }
      } else {
#endif
         RPC_CALL2(glEGLImageTargetTexture2DOES_impl,
                   thread,
                   GLEGLIMAGETARGETTEXTURE2DOES_ID,
                   RPC_ENUM(target),
                   RPC_EGLID(image));
         RPC_FLUSH(thread);
#if EGL_BRCM_global_image
      }
#endif
   }
}

GL_API void GL_APIENTRY glEGLImageTargetRenderbufferStorageOES (GLenum target, GLeglImageOES image)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   if (IS_OPENGLES_11(thread)) {
      /* OES_framebuffer_object not supported for GLES1.1 */
      GLXX_CLIENT_STATE_T *state = GLXX_GET_CLIENT_STATE(thread);
      if (state->error == GL_NO_ERROR)
         state->error = GL_INVALID_OPERATION;
   }
   else if (IS_OPENGLES_20(thread)) {
#if EGL_BRCM_global_image
      if ((uintptr_t)image & (1u << 31)) {
         GLuint global_image_id[2];
         if (check_global_image_egl_image(global_image_id, image, thread, true)) {
            RPC_CALL3(glGlobalImageRenderbufferStorageOES_impl_20,
                      thread,
                      GLGLOBALIMAGERENDERBUFFERSTORAGEOES_ID_20,
                      RPC_ENUM(target),
                      RPC_UINT(global_image_id[0]),
                      RPC_UINT(global_image_id[1]));
         } else {
            set_error(GLXX_GET_CLIENT_STATE(thread), GL_INVALID_VALUE);
         }
      } else {
#endif
         RPC_CALL2(glEGLImageTargetRenderbufferStorageOES_impl_20,
                   thread,
                   GLEGLIMAGETARGETRENDERBUFFERSTORAGEOES_ID_20,
                   RPC_ENUM(target),
                   RPC_EGLID(image));
#if EGL_BRCM_global_image
      }
#endif
   }
}
