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

#include "interface/khronos/common/khrn_client_mangle.h"

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/glxx/glxx_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#ifdef RPC_DIRECT
#include "interface/khronos/glxx/glxx_int_impl.h"
#include "interface/khronos/glxx/gl11_int_impl.h"
#endif

#include "interface/khronos/include/GLES/gl.h"
#include "interface/khronos/include/GLES/glext.h"

/* GL_OES_draw_texture */

GL_API void GL_APIENTRY glDrawTexsOES (GLshort x, GLshort y, GLshort z, GLshort width, GLshort height) 
{
   glDrawTexfOES((GLfloat)x,(GLfloat)y,(GLfloat)x, (GLfloat)width,(GLfloat)height);
}

GL_API void GL_APIENTRY glDrawTexiOES (GLint x, GLint y, GLint z, GLint width, GLint height)
{
   glDrawTexfOES((GLfloat)x,(GLfloat)y,(GLfloat)x, (GLfloat)width,(GLfloat)height);
}

GL_API void GL_APIENTRY glDrawTexxOES (GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height)
{
   glDrawTexfOES(fixed_to_float(x), fixed_to_float(y), fixed_to_float(x), fixed_to_float(width), fixed_to_float(height));
}

GL_API void GL_APIENTRY glDrawTexsvOES (const GLshort *coords)
{
   glDrawTexfOES((GLfloat)coords[0],(GLfloat)coords[1],(GLfloat)coords[2], (GLfloat)coords[3],(GLfloat)coords[4]);
}

GL_API void GL_APIENTRY glDrawTexivOES (const GLint *coords)
{
   glDrawTexfOES((GLfloat)coords[0],(GLfloat)coords[1],(GLfloat)coords[2], (GLfloat)coords[3],(GLfloat)coords[4]);
}

GL_API void GL_APIENTRY glDrawTexxvOES (const GLfixed *coords)
{
   glDrawTexfOES(fixed_to_float(coords[0]), fixed_to_float(coords[1]), fixed_to_float(coords[2]), fixed_to_float(coords[3]), fixed_to_float(coords[4]));
}

GL_API void GL_APIENTRY glDrawTexfOES (GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (IS_OPENGLES_11(thread)) {
      RPC_CALL5(glDrawTexfOES_impl_11,
                thread,
                GLDRAWTEXFOES_ID_11,
                RPC_FLOAT(x),
                RPC_FLOAT(y),
                RPC_FLOAT(z),
                RPC_FLOAT(width),
                RPC_FLOAT(height));
   }
}

GL_API void GL_APIENTRY glDrawTexfvOES (const GLfloat *coords)
{
   glDrawTexfOES((GLfloat)coords[0],(GLfloat)coords[1],(GLfloat)coords[2], (GLfloat)coords[3],(GLfloat)coords[4]);
}
