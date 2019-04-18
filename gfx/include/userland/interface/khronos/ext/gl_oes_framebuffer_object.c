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

extern GLboolean glxx_client_IsRenderbuffer(GLuint renderbuffer);
extern void glxx_client_BindRenderbuffer(GLenum target, GLuint renderbuffer);
extern void glxx_client_DeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
extern void glxx_client_GenRenderbuffers(GLsizei n, GLuint *renderbuffers);
extern void glxx_client_RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern void glxx_client_GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params);
extern GLboolean glxx_client_IsFramebuffer(GLuint framebuffer);
extern void glxx_client_BindFramebuffer(GLenum target, GLuint framebuffer);
extern void glxx_client_DeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
extern void glxx_client_GenFramebuffers(GLsizei n, GLuint *framebuffers);
extern GLenum glxx_client_CheckFramebufferStatus(GLenum target);
extern void glxx_client_FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern void glxx_client_FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
extern void glxx_client_GetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params);
extern void glxx_client_GenerateMipmap(GLenum target);

GL_API GLboolean GL_APIENTRY glIsRenderbufferOES (GLuint renderbuffer)
{
   return glxx_client_IsRenderbuffer(renderbuffer);
}

GL_API void GL_APIENTRY glBindRenderbufferOES (GLenum target, GLuint renderbuffer)
{
   glxx_client_BindRenderbuffer(target, renderbuffer);
}

GL_API void GL_APIENTRY glDeleteRenderbuffersOES (GLsizei n, const GLuint* renderbuffers)
{
   glxx_client_DeleteRenderbuffers(n, renderbuffers);
}

GL_API void GL_APIENTRY glGenRenderbuffersOES (GLsizei n, GLuint* renderbuffers)
{
   glxx_client_GenRenderbuffers(n, renderbuffers);
}

GL_API void GL_APIENTRY glRenderbufferStorageOES (GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
   glxx_client_RenderbufferStorage(target, internalformat, width, height);
}

GL_API void GL_APIENTRY glGetRenderbufferParameterivOES (GLenum target, GLenum pname, GLint* params)
{
   glxx_client_GetRenderbufferParameteriv(target, pname, params);
}

GL_API GLboolean GL_APIENTRY glIsFramebufferOES (GLuint framebuffer)
{
   return glxx_client_IsFramebuffer(framebuffer);
}

GL_API void GL_APIENTRY glBindFramebufferOES (GLenum target, GLuint framebuffer)
{
   glxx_client_BindFramebuffer(target, framebuffer);
}

GL_API void GL_APIENTRY glDeleteFramebuffersOES (GLsizei n, const GLuint* framebuffers)
{
   glxx_client_DeleteFramebuffers(n, framebuffers);
}

GL_API void GL_APIENTRY glGenFramebuffersOES (GLsizei n, GLuint* framebuffers)
{
   glxx_client_GenFramebuffers(n, framebuffers);
}

GL_API GLenum GL_APIENTRY glCheckFramebufferStatusOES (GLenum target)
{
   return glxx_client_CheckFramebufferStatus(target);
}

GL_API void GL_APIENTRY glFramebufferRenderbufferOES (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
   glxx_client_FramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

GL_API void GL_APIENTRY glFramebufferTexture2DOES (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
   glxx_client_FramebufferTexture2D(target, attachment, textarget, texture, level);
}

GL_API void GL_APIENTRY glGetFramebufferAttachmentParameterivOES (GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
   glxx_client_GetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

GL_API void GL_APIENTRY glGenerateMipmapOES (GLenum target)
{
   glxx_client_GenerateMipmap(target);
}
