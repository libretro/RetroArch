/* Copyright (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro SDK code part (glsym).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef RGLGEN_DECL_H__
#define RGLGEN_DECL_H__
#ifdef __cplusplus
extern "C" {
#endif
#ifdef GL_APIENTRY
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROCKHR)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#else
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
typedef void (APIENTRY *RGLGENGLDEBUGPROCARB)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#endif
#ifndef GL_OES_EGL_image
typedef void *GLeglImageOES;
#endif
#if !defined(GL_OES_fixed_point) && !defined(HAVE_OPENGLES2)
typedef GLint GLfixed;
#endif
#if defined(OSX) && !defined(MAC_OS_X_VERSION_10_7)
typedef long long int GLint64;
typedef unsigned long long int GLuint64;
typedef unsigned long long int GLuint64EXT;
typedef struct __GLsync *GLsync;
#endif
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLKHRPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTKHRPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (GL_APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKKHRPROC) (RGLGENGLDEBUGPROCKHR callback, const void *userParam);
typedef GLuint (GL_APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGKHRPROC) (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (GL_APIENTRYP RGLSYMGLPUSHDEBUGGROUPKHRPROC) (GLenum source, GLuint id, GLsizei length, const GLchar *message);
typedef void (GL_APIENTRYP RGLSYMGLPOPDEBUGGROUPKHRPROC) (void);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTLABELKHRPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTLABELKHRPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLOBJECTPTRLABELKHRPROC) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETOBJECTPTRLABELKHRPROC) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP RGLSYMGLGETPOINTERVKHRPROC) (GLenum pname, void **params);
typedef void (GL_APIENTRYP RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GL_APIENTRYP RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GL_APIENTRYP RGLSYMGLGETPROGRAMBINARYOESPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary);
typedef void (GL_APIENTRYP RGLSYMGLPROGRAMBINARYOESPROC) (GLuint program, GLenum binaryFormat, const void *binary, GLint length);
typedef void *(GL_APIENTRYP RGLSYMGLMAPBUFFEROESPROC) (GLenum target, GLenum access);
typedef GLboolean (GL_APIENTRYP RGLSYMGLUNMAPBUFFEROESPROC) (GLenum target);
typedef void (GL_APIENTRYP RGLSYMGLGETBUFFERPOINTERVOESPROC) (GLenum target, GLenum pname, void **params);
typedef void (GL_APIENTRYP RGLSYMGLTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
typedef void (GL_APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (GL_APIENTRYP RGLSYMGLBINDVERTEXARRAYOESPROC) (GLuint array);
typedef void (GL_APIENTRYP RGLSYMGLDELETEVERTEXARRAYSOESPROC) (GLsizei n, const GLuint *arrays);
typedef void (GL_APIENTRYP RGLSYMGLGENVERTEXARRAYSOESPROC) (GLsizei n, GLuint *arrays);
typedef GLboolean (GL_APIENTRYP RGLSYMGLISVERTEXARRAYOESPROC) (GLuint array);

#define glDebugMessageControlKHR __rglgen_glDebugMessageControlKHR
#define glDebugMessageInsertKHR __rglgen_glDebugMessageInsertKHR
#define glDebugMessageCallbackKHR __rglgen_glDebugMessageCallbackKHR
#define glGetDebugMessageLogKHR __rglgen_glGetDebugMessageLogKHR
#define glPushDebugGroupKHR __rglgen_glPushDebugGroupKHR
#define glPopDebugGroupKHR __rglgen_glPopDebugGroupKHR
#define glObjectLabelKHR __rglgen_glObjectLabelKHR
#define glGetObjectLabelKHR __rglgen_glGetObjectLabelKHR
#define glObjectPtrLabelKHR __rglgen_glObjectPtrLabelKHR
#define glGetObjectPtrLabelKHR __rglgen_glGetObjectPtrLabelKHR
#define glGetPointervKHR __rglgen_glGetPointervKHR
#define glEGLImageTargetTexture2DOES __rglgen_glEGLImageTargetTexture2DOES
#define glEGLImageTargetRenderbufferStorageOES __rglgen_glEGLImageTargetRenderbufferStorageOES
#define glGetProgramBinaryOES __rglgen_glGetProgramBinaryOES
#define glProgramBinaryOES __rglgen_glProgramBinaryOES
#define glMapBufferOES __rglgen_glMapBufferOES
#define glUnmapBufferOES __rglgen_glUnmapBufferOES
#define glGetBufferPointervOES __rglgen_glGetBufferPointervOES
#define glTexImage3DOES __rglgen_glTexImage3DOES
#define glTexSubImage3DOES __rglgen_glTexSubImage3DOES
#define glCopyTexSubImage3DOES __rglgen_glCopyTexSubImage3DOES
#define glCompressedTexImage3DOES __rglgen_glCompressedTexImage3DOES
#define glCompressedTexSubImage3DOES __rglgen_glCompressedTexSubImage3DOES
#define glFramebufferTexture3DOES __rglgen_glFramebufferTexture3DOES
#define glBindVertexArrayOES __rglgen_glBindVertexArrayOES
#define glDeleteVertexArraysOES __rglgen_glDeleteVertexArraysOES
#define glGenVertexArraysOES __rglgen_glGenVertexArraysOES
#define glIsVertexArrayOES __rglgen_glIsVertexArrayOES

extern RGLSYMGLDEBUGMESSAGECONTROLKHRPROC __rglgen_glDebugMessageControlKHR;
extern RGLSYMGLDEBUGMESSAGEINSERTKHRPROC __rglgen_glDebugMessageInsertKHR;
extern RGLSYMGLDEBUGMESSAGECALLBACKKHRPROC __rglgen_glDebugMessageCallbackKHR;
extern RGLSYMGLGETDEBUGMESSAGELOGKHRPROC __rglgen_glGetDebugMessageLogKHR;
extern RGLSYMGLPUSHDEBUGGROUPKHRPROC __rglgen_glPushDebugGroupKHR;
extern RGLSYMGLPOPDEBUGGROUPKHRPROC __rglgen_glPopDebugGroupKHR;
extern RGLSYMGLOBJECTLABELKHRPROC __rglgen_glObjectLabelKHR;
extern RGLSYMGLGETOBJECTLABELKHRPROC __rglgen_glGetObjectLabelKHR;
extern RGLSYMGLOBJECTPTRLABELKHRPROC __rglgen_glObjectPtrLabelKHR;
extern RGLSYMGLGETOBJECTPTRLABELKHRPROC __rglgen_glGetObjectPtrLabelKHR;
extern RGLSYMGLGETPOINTERVKHRPROC __rglgen_glGetPointervKHR;
extern RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC __rglgen_glEGLImageTargetTexture2DOES;
extern RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC __rglgen_glEGLImageTargetRenderbufferStorageOES;
extern RGLSYMGLGETPROGRAMBINARYOESPROC __rglgen_glGetProgramBinaryOES;
extern RGLSYMGLPROGRAMBINARYOESPROC __rglgen_glProgramBinaryOES;
extern RGLSYMGLMAPBUFFEROESPROC __rglgen_glMapBufferOES;
extern RGLSYMGLUNMAPBUFFEROESPROC __rglgen_glUnmapBufferOES;
extern RGLSYMGLGETBUFFERPOINTERVOESPROC __rglgen_glGetBufferPointervOES;
extern RGLSYMGLTEXIMAGE3DOESPROC __rglgen_glTexImage3DOES;
extern RGLSYMGLTEXSUBIMAGE3DOESPROC __rglgen_glTexSubImage3DOES;
extern RGLSYMGLCOPYTEXSUBIMAGE3DOESPROC __rglgen_glCopyTexSubImage3DOES;
extern RGLSYMGLCOMPRESSEDTEXIMAGE3DOESPROC __rglgen_glCompressedTexImage3DOES;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DOESPROC __rglgen_glCompressedTexSubImage3DOES;
extern RGLSYMGLFRAMEBUFFERTEXTURE3DOESPROC __rglgen_glFramebufferTexture3DOES;
extern RGLSYMGLBINDVERTEXARRAYOESPROC __rglgen_glBindVertexArrayOES;
extern RGLSYMGLDELETEVERTEXARRAYSOESPROC __rglgen_glDeleteVertexArraysOES;
extern RGLSYMGLGENVERTEXARRAYSOESPROC __rglgen_glGenVertexArraysOES;
extern RGLSYMGLISVERTEXARRAYOESPROC __rglgen_glIsVertexArrayOES;

struct rglgen_sym_map { const char *sym; void *ptr; };
extern const struct rglgen_sym_map rglgen_symbol_map[];
#ifdef __cplusplus
}
#endif
#endif
