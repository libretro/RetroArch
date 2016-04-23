/* Copyright (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro SDK code part (glsmsym.h).
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

#ifndef LIBRETRO_SDK_GLSM_SYM_H
#define LIBRETRO_SDK_GLSM_SYM_H

#include <glsm/glsm.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* deprecated old FF-style GL symbols */
#define glTexCoord2f                rglTexCoord2f

/* more forward-compatible GL subset symbols */
#define glBlitFramebuffer           rglBlitFramebuffer
#define glVertexAttrib4f            rglVertexAttrib4f
#define glVertexAttrib4fv           rglVertexAttrib4fv
#define glDrawArrays                rglDrawArrays
#define glDrawElements              rglDrawElements
#define glCompressedTexImage2D      rglCompressedTexImage2D
#define glBindTexture               rglBindTexture
#define glActiveTexture             rglActiveTexture
#define glFramebufferTexture        rglFramebufferTexture
#define glFramebufferTexture2D      rglFramebufferTexture2D
#define glFramebufferRenderbuffer   rglFramebufferRenderbuffer
#define glDeleteFramebuffers        rglDeleteFramebuffers
#define glDeleteTextures            rglDeleteTextures
#define glDeleteBuffers             rglDeleteBuffers
#define glRenderbufferStorage       rglRenderbufferStorage
#define glBindRenderbuffer          rglBindRenderbuffer
#define glDeleteRenderbuffers       rglDeleteRenderbuffers
#define glGenRenderbuffers          rglGenRenderbuffers
#define glGenFramebuffers           rglGenFramebuffers
#define glGenTextures               rglGenTextures
#define glBindFramebuffer           rglBindFramebuffer
#define glGenerateMipmap            rglGenerateMipmap
#define glCheckFramebufferStatus    rglCheckFramebufferStatus
#define glBindFragDataLocation      rglBindFragDataLocation
#define glBindAttribLocation        rglBindAttribLocation
#define glLinkProgram               rglLinkProgram
#define glGetProgramiv              rglGetProgramiv
#define glGetShaderiv               rglGetShaderiv
#define glAttachShader              rglAttachShader
#define glDetachShader              rglDetachShader
#define glShaderSource              rglShaderSource
#define glCompileShader             rglCompileShader
#define glCreateProgram             rglCreateProgram
#define glGetShaderInfoLog          rglGetShaderInfoLog
#define glGetProgramInfoLog         rglGetProgramInfoLog
#define glIsProgram                 rglIsProgram
#define glEnableVertexAttribArray   rglEnableVertexAttribArray
#define glDisableVertexAttribArray  rglDisableVertexAttribArray
#define glVertexAttribPointer       rglVertexAttribPointer
#define glGetUniformLocation        rglGetUniformLocation
#define glGenBuffers                rglGenBuffers
#define glDisable(T)                rglDisable(S##T)
#define glEnable(T)                 rglEnable(S##T)
#define glIsEnabled(T)              rglIsEnabled(S##T)
#define glUseProgram                rglUseProgram
#define glDepthMask                 rglDepthMask
#define glStencilMask               rglStencilMask
#define glBufferData                rglBufferData
#define glBufferSubData             rglBufferSubData
#define glBindBuffer                rglBindBuffer
#define glCreateShader              rglCreateShader
#define glDeleteShader              rglDeleteShader
#define glDeleteProgram             rglDeleteProgram
#define glUniform1f                 rglUniform1f
#define glUniform1i                 rglUniform1i
#define glUniform2f                 rglUniform2f
#define glUniform2i                 rglUniform2i
#define glUniform2fv                rglUniform2fv
#define glUniform3f                 rglUniform3f
#define glUniform3fv                rglUniform3fv
#define glUniform4f                 rglUniform4f
#define glUniform4fv                rglUniform4fv
#define glBlendFunc                 rglBlendFunc
#define glBlendFuncSeparate         rglBlendFuncSeparate
#define glDepthFunc                 rglDepthFunc
#define glColorMask                 rglColorMask
#define glClearColor                rglClearColor
#define glViewport                  rglViewport
#define glScissor                   rglScissor
#define glStencilFunc               rglStencilFunc
#define glCullFace                  rglCullFace
#define glStencilOp                 rglStencilOp
#define glFrontFace                 rglFrontFace
#define glDepthRange                rglDepthRange
#define glClearDepth                rglClearDepth
#define glPolygonOffset             rglPolygonOffset
#define glPixelStorei               rglPixelStorei
#define glReadBuffer                rglReadBuffer
#define glUniformMatrix4fv          rglUniformMatrix4fv
#define glGetAttribLocation         rglGetAttribLocation
#define glTexStorage2D              rglTexStorage2D
#define glDrawBuffers               rglDrawBuffers
#define glGenVertexArrays           rglGenVertexArrays
#define glBindVertexArray           rglBindVertexArray
#define glBlendEquation             rglBlendEquation
#define glBlendColor                rglBlendColor

void rglBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void rglBlendEquation(GLenum mode);
void rglGenVertexArrays(GLsizei n, GLuint *arrays);
void rglReadBuffer(GLenum mode);
void rglPixelStorei(GLenum pname, GLint param);
void rglTexCoord2f(GLfloat s, GLfloat t);
void rglDrawElements(GLenum mode, GLsizei count, GLenum type,
                           const GLvoid * indices);
void rglTexStorage2D(GLenum target, GLsizei levels, GLenum internalFormat,
      GLsizei width, GLsizei height);
void rglCompressedTexImage2D(GLenum target, GLint level,
      GLenum internalformat, GLsizei width, GLsizei height,
      GLint border, GLsizei imageSize, const GLvoid *data);
void glBindTexture(GLenum target, GLuint texture);
void glActiveTexture(GLenum texture);
void rglFramebufferTexture(GLenum target, GLenum attachment,
  	GLuint texture, GLint level);
void rglFramebufferTexture2D(GLenum target, GLenum attachment,
      GLenum textarget, GLuint texture, GLint level);
void rglFramebufferRenderbuffer(GLenum target, GLenum attachment,
      GLenum renderbuffertarget, GLuint renderbuffer);
void rglDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
void rglRenderbufferStorage(GLenum target, GLenum internalFormat,
      GLsizei width, GLsizei height);
void rglDeleteTextures(GLsizei n, const GLuint *textures);
void rglBindRenderbuffer(GLenum target, GLuint renderbuffer);
void rglDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers);
void rglGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
void rglGenFramebuffers(GLsizei n, GLuint *ids);
void rglGenTextures(GLsizei n, GLuint *textures);
void rglBindFramebuffer(GLenum target, GLuint framebuffer);
void rglGenerateMipmap(GLenum target);
GLenum rglCheckFramebufferStatus(GLenum target);
void rglBindFragDataLocation(GLuint program, GLuint colorNumber,
                                   const char * name);
void rglBindAttribLocation(GLuint program, GLuint index, const GLchar *name);
void rglLinkProgram(GLuint program);
void rglGetProgramiv(GLuint shader, GLenum pname, GLint *params);
void rglGetShaderiv(GLuint shader, GLenum pname, GLint *params);
void rglAttachShader(GLuint program, GLuint shader);
void rglShaderSource(GLuint shader, GLsizei count,
      const GLchar **string, const GLint *length);
void rglCompileShader(GLuint shader);
GLuint rglCreateProgram(void);
void rglGetShaderInfoLog(GLuint shader, GLsizei maxLength,
      GLsizei *length, GLchar *infoLog);
void rglGetProgramInfoLog(GLuint shader, GLsizei maxLength,
      GLsizei *length, GLchar *infoLog);
GLboolean rglIsProgram(GLuint program);
void rglEnableVertexAttribArray(GLuint index);
void rglDisableVertexAttribArray(GLuint index);
void rglVertexAttribPointer(GLuint name, GLint size,
      GLenum type, GLboolean normalized, GLsizei stride,
      const GLvoid* pointer);
GLint rglGetUniformLocation(GLuint program, const GLchar *name);
void rglGenBuffers(GLsizei n, GLuint *buffers);
void rglDisable(GLenum cap);
void rglEnable(GLenum cap);
void rglUseProgram(GLuint program);
void rglDepthMask(GLboolean flag);
void rglStencilMask(GLenum mask);
void rglBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
void rglBufferSubData(GLenum target, GLintptr offset,
      GLsizeiptr size, const GLvoid *data);
void rglBindBuffer(GLenum target, GLuint buffer);
GLuint rglCreateShader(GLenum shader);
void rglDeleteShader(GLuint shader);
void rglUniform1f(GLint location, GLfloat v0);
void rglUniform1i(GLint location, GLint v0);
void rglUniform2f(GLint location, GLfloat v0, GLfloat v1);
void rglUniform2i(GLint location, GLint v0, GLint v1);
void rglUniform2fv(GLint location, GLsizei count, const GLfloat *value);
void rglUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void rglUniform3fv(GLint location, GLsizei count, const GLfloat *value);
void rglUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void rglUniform4fv(GLint location, GLsizei count, const GLfloat *value);
void rglBlendFunc(GLenum sfactor, GLenum dfactor);
void rglBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
      GLenum dstAlpha);
void rglDepthFunc(GLenum func);
void rglColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void rglClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void rglViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void rglScissor(GLint x, GLint y, GLsizei width, GLsizei height);
GLboolean rglIsEnabled(GLenum cap);
void rglStencilFunc(GLenum func, GLint ref, GLuint mask);
void rglCullFace(GLenum mode);
void rglStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
void rglFrontFace(GLenum mode);
void rglDepthRange(GLclampd zNear, GLclampd zFar);
void rglClearDepth(GLdouble depth);
void rglPolygonOffset(GLfloat factor, GLfloat units);
void rglDrawArrays(GLenum mode, GLint first, GLsizei count);
void rglVertexAttrib4f(GLuint name, GLfloat x, GLfloat y,
      GLfloat z, GLfloat w);
void rglVertexAttrib4fv(GLuint name, GLfloat* v);
void rglDeleteProgram(GLuint program);
void rglDeleteBuffers(GLsizei n, const GLuint *buffers);
void rglBlitFramebuffer(
      GLint srcX0, GLint srcY0,
      GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0,
      GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter);
void rglDetachShader(GLuint program, GLuint shader);
void rglUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
      const GLfloat *value);
GLint rglGetAttribLocation(GLuint program, const GLchar *name);
void rglDrawBuffers(GLsizei n, const GLenum *bufs);
void rglBindVertexArray(GLuint array);

RETRO_END_DECLS

#endif
