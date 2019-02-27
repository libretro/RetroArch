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
#if defined(KHRN_IMPL_STRUCT)
#define FN(type, name, args) type (*name) args;
#elif defined(KHRN_IMPL_STRUCT_INIT)
#define FN(type, name, args) name,
#else
#define FN(type, name, args) extern type name args;
#endif

#if defined(V3D_LEAN)
#include "interface/khronos/include/GLES/gl.h"
#include "interface/khronos/include/GLES/glext.h"
#include "interface/khronos/glxx/glxx_int_attrib.h"
#endif

FN(void, glActiveTexture_impl, (GLenum texture))
FN(void, glBindBuffer_impl, (GLenum target, GLuint buffer))
FN(void, glBindTexture_impl, (GLenum target, GLuint texture))
FN(void, glBlendFuncSeparate_impl, (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)) // S
FN(void, glBufferData_impl, (GLenum target, GLsizeiptr size, GLenum usage, const GLvoid *data))
FN(void, glBufferSubData_impl, (GLenum target, GLintptr offset, GLsizeiptr size, const void *data))
FN(void, glClear_impl, (GLbitfield mask))
FN(void, glClearColor_impl, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
FN(void, glClearDepthf_impl, (GLclampf depth)) // S
FN(void, glClearStencil_impl, (GLint s)) // S
FN(void, glColorMask_impl, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)) // S
FN(GLboolean, glCompressedTexImage2D_impl, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data))
FN(void, glCompressedTexSubImage2D_impl, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data))
FN(void, glCopyTexImage2D_impl, (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border))
FN(void, glCopyTexSubImage2D_impl, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height))
FN(void, glCullFace_impl, (GLenum mode)) // S
FN(void, glDeleteBuffers_impl, (GLsizei n, const GLuint *buffers))
FN(void, glDeleteTextures_impl, (GLsizei n, const GLuint *textures))
FN(void, glDepthFunc_impl, (GLenum func)) // S
FN(void, glDepthMask_impl, (GLboolean flag)) // S
FN(void, glDepthRangef_impl, (GLclampf zNear, GLclampf zFar)) // S
FN(void, glDisable_impl, (GLenum cap)) // S
FN(void, glEnable_impl, (GLenum cap)) // S
FN(GLuint, glFinish_impl, (void))
FN(void, glFlush_impl, (void))
FN(void, glFrontFace_impl, (GLenum mode)) // S
FN(void, glGenBuffers_impl, (GLsizei n, GLuint *buffers))
FN(void, glGenTextures_impl, (GLsizei n, GLuint *textures))
FN(GLenum, glGetError_impl, (void))
FN(int, glGetBooleanv_impl, (GLenum pname, GLboolean *params))
FN(int, glGetBufferParameteriv_impl, (GLenum target, GLenum pname, GLint *params))
FN(int, glGetFloatv_impl, (GLenum pname, GLfloat *params))
FN(int, glGetIntegerv_impl, (GLenum pname, GLint *params))
FN(int, glGetTexParameteriv_impl, (GLenum target, GLenum pname, GLint *params))
FN(int, glGetTexParameterfv_impl, (GLenum target, GLenum pname, GLfloat *params))
FN(void, glHint_impl, (GLenum target, GLenum mode))
FN(GLboolean, glIsBuffer_impl, (GLuint buffer))
FN(GLboolean, glIsEnabled_impl, (GLenum cap))
FN(GLboolean, glIsTexture_impl, (GLuint texture))
FN(void, glLineWidth_impl, (GLfloat width)) // S
FN(void, glPolygonOffset_impl, (GLfloat factor, GLfloat units)) // S
FN(void, glReadPixels_impl, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint alignment, void *pixels))
FN(void, glSampleCoverage_impl, (GLclampf value, GLboolean invert)) // S
FN(void, glScissor_impl, (GLint x, GLint y, GLsizei width, GLsizei height)) // S
FN(void, glStencilFuncSeparate_impl, (GLenum face, GLenum func, GLint ref, GLuint mask)) // S
FN(void, glStencilMaskSeparate_impl, (GLenum face, GLuint mask)) // S
FN(void, glStencilOpSeparate_impl, (GLenum face, GLenum fail, GLenum zfail, GLenum zpass)) // S
FN(GLboolean, glTexImage2D_impl, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLint alignment, const GLvoid *pixels))
FN(void, glTexParameteri_impl, (GLenum target, GLenum pname, GLint param))
FN(void, glTexParameterf_impl, (GLenum target, GLenum pname, GLfloat param))
FN(void, glTexParameteriv_impl, (GLenum target, GLenum pname, const GLint *params))
FN(void, glTexParameterfv_impl, (GLenum target, GLenum pname, const GLfloat *params))
FN(void, glTexSubImage2D_impl, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint alignment, const void *pixels))
FN(void, texSubImage2DAsync_impl, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint alignment, GLint hdata))
FN(void, glViewport_impl, (GLint x, GLint y, GLsizei width, GLsizei height)) // S

/*****************************************************************************************/
/*                                 EXT extension functions                               */
/*****************************************************************************************/
FN(void, glDiscardFramebufferEXT_impl, (GLenum target, GLsizei numAttachments, const GLenum *attachments))

/*****************************************************************************************/
/*                                 OES extension functions                               */
/*****************************************************************************************/

FN(int, glintFindMax_impl, (GLsizei count, GLenum type, uint32_t indices_offset))
FN(void, glintCacheCreate_impl, (GLsizei offset))
FN(void, glintCacheDelete_impl, (GLsizei offset))
FN(void, glintCacheData_impl, (GLsizei offset, GLsizei length, const GLvoid *data))
FN(GLboolean, glintCacheGrow_impl, (void))
FN(void, glintCacheUse_impl, (GLsizei count, GLsizei *offset))

FN(void, glintAttribPointer_impl, (uint32_t api, uint32_t indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr ptr))
FN(void, glintAttrib_impl, (uint32_t api, uint32_t indx, float x, float y, float z, float w))
FN(void, glintAttribEnable_impl, (uint32_t api, uint32_t indx, bool enabled))
FN(void, glintDrawElements_impl, (GLenum mode, GLsizei count, GLenum type, uint32_t indices_offset, GLXX_CACHE_INFO_T *cache_info))

#if GL_OES_EGL_image
FN(void, glEGLImageTargetTexture2DOES_impl, (GLenum target, GLeglImageOES image))
#if EGL_BRCM_global_image
FN(void, glGlobalImageTexture2DOES_impl, (GLenum target, GLuint id_0, GLuint id_1))
#endif
#endif

/* OES_framebuffer_object for ES 1.1 and core in ES 2.0 */
FN(GLboolean, glIsRenderbuffer_impl, (GLuint renderbuffer))
FN(void, glBindRenderbuffer_impl, (GLenum target, GLuint renderbuffer))
FN(void, glDeleteRenderbuffers_impl, (GLsizei n, const GLuint *renderbuffers))
FN(void, glGenRenderbuffers_impl, (GLsizei n, GLuint *renderbuffers))
FN(void, glRenderbufferStorage_impl, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height))
FN(int, glGetRenderbufferParameteriv_impl, (GLenum target, GLenum pname, GLint* params))
FN(GLboolean, glIsFramebuffer_impl, (GLuint framebuffer))
FN(void, glBindFramebuffer_impl, (GLenum target, GLuint framebuffer))
FN(void, glDeleteFramebuffers_impl, (GLsizei n, const GLuint *framebuffers))
FN(void, glGenFramebuffers_impl, (GLsizei n, GLuint *framebuffers))
FN(GLenum, glCheckFramebufferStatus_impl, (GLenum target))
FN(void, glFramebufferTexture2D_impl, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level))
FN(void, glFramebufferRenderbuffer_impl, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer))
FN(int, glGetFramebufferAttachmentParameteriv_impl, (GLenum target, GLenum attachment, GLenum pname, GLint *params))
FN(void, glGenerateMipmap_impl, (GLenum target))

#undef FN
