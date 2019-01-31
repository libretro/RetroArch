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

#ifdef RPC_DIRECT
#include "interface/khronos/glxx/glxx_int_impl.h"
#include "interface/khronos/glxx/gl11_int_impl.h"
#endif

#include "interface/khronos/include/GLES/gl.h"
#include "interface/khronos/include/GLES/glext.h"


GL_API void* GL_APIENTRY glMapBufferOES (GLenum target, GLenum access)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   void *pointer = 0;
   if (IS_OPENGLES_11_OR_20(thread)) {

      GLXX_CLIENT_STATE_T *state = GLXX_GET_CLIENT_STATE(thread);

      if(target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER)
      {
         glxx_set_error(state, GL_INVALID_ENUM);
      }
      else if(access != GL_WRITE_ONLY_OES)
      {
         glxx_set_error(state, GL_INVALID_ENUM);
      }
      else
      {
         GLXX_BUFFER_INFO_T buffer;
         glxx_buffer_info_get(state, target, &buffer);

         if(buffer.id !=0 && buffer.cached_size > 0)
         {
            if(buffer.mapped_pointer != 0)
            {
               /* already mapped */
               glxx_set_error(state, GL_INVALID_OPERATION);
            }
            else
            {
               
               pointer = khrn_platform_malloc(buffer.cached_size,"glxx_mapped_buffer");

               if(pointer != 0)
               {
                  buffer.mapped_pointer = pointer;
                  buffer.mapped_size = buffer.cached_size;
               }
               else
               {
                  buffer.mapped_pointer = 0;
                  buffer.mapped_size = 0;
                  glxx_set_error(state, GL_OUT_OF_MEMORY);
               }
               glxx_buffer_info_set(state, target, &buffer);
            }
         }
         else
         {
            glxx_set_error(state, GL_INVALID_OPERATION);
         }
      }
   }
   /*
   RPC_CALL3_OUT_CTRL(glMapBufferOES_impl,
                      thread,
                      GLMAPBUFFEROES_ID,
                      RPC_ENUM(target),
                      RPC_ENUM(access),
                      &pointer);
   */

   return pointer;
}

GL_API GLboolean GL_APIENTRY glUnmapBufferOES (GLenum target)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   GLboolean success = GL_FALSE;

   if (IS_OPENGLES_11_OR_20(thread)) {
      //use buffer sub data to flush through
      GLXX_CLIENT_STATE_T *state = GLXX_GET_CLIENT_STATE(thread);

      if(target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER)
      {
         glxx_set_error(state, GL_INVALID_ENUM);
      }
      else
      {
         GLXX_BUFFER_INFO_T buffer;
         glxx_buffer_info_get(state, target, &buffer);

         if(buffer.id !=0)
         {
            if(buffer.mapped_pointer)
            {
               void * p = buffer.mapped_pointer;
               GLsizeiptr size = buffer.mapped_size;
               
               buffer.mapped_pointer = 0;
               buffer.mapped_size = 0;
               glxx_buffer_info_set(state, target, &buffer);

               glBufferSubData (target, 0, size, p);
               khrn_platform_free(p);
            }
         }         
      }
   }

   /*
   if (IS_OPENGLES_11_OR_20(thread)) {
      success = RPC_BOOLEAN_RES(RPC_CALL1_RES(glUnmapBufferOES_impl,
                         thread,      
                         GLUNMAPBUFFEROES_ID,
                         RPC_ENUM(target)));
   */

   return success;
}

GL_API void GL_APIENTRY glGetBufferPointervOES (GLenum target, GLenum pname, GLvoid ** params)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   params[0] = (void *)0;

   if (IS_OPENGLES_11_OR_20(thread)) {
      GLXX_CLIENT_STATE_T *state = GLXX_GET_CLIENT_STATE(thread);
      if(target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER)
      {
         glxx_set_error(state, GL_INVALID_ENUM);
      }
      else if(pname != GL_BUFFER_MAP_POINTER_OES)
      {
         glxx_set_error(state, GL_INVALID_ENUM);
      }
      else
      {
         GLXX_BUFFER_INFO_T buffer;
         glxx_buffer_info_get(state, target, &buffer);

         if(buffer.id !=0)
         {
            params[0] = (void *)buffer.mapped_pointer;
         }
      }
   }

   /*
   
      RPC_CALL3_OUT_CTRL(glGetBufferPointervOES_impl,
                         thread,      
                         GLGETBUFFERPOINTERVOES_ID,
                         RPC_ENUM(target),
                         RPC_ENUM(pname),
                         params);

   */
}
