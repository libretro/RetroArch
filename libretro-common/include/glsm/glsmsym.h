/* Copyright (C) 2010-2017 The RetroArch team
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
#define glVertexAttribIPointer      rglVertexAttribIPointer
#define glVertexAttribLPointer      rglVertexAttribLPointer
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
#define glUniform4i                 rglUniform4i
#define glUniform4f                 rglUniform4f
#define glUniform4fv                rglUniform4fv
#define glUniform1ui                rglUniform1ui
#define glUniform2ui                rglUniform2ui
#define glUniform3ui                rglUniform3ui
#define glUniform4ui                rglUniform4ui
#define glGetActiveUniform          rglGetActiveUniform
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
#define glBlendEquationSeparate     rglBlendEquationSeparate
#define glCopyImageSubData          rglCopyImageSubData
#define glMapBuffer                 rglMapBuffer
#define glUnmapBuffer               rglUnmapBuffer
#define glMapBufferRange            rglMapBufferRange
#define glUniformBlockBinding       rglUniformBlockBinding
#define glGetUniformBlockIndex      rglGetUniformBlockIndex
#define glGetActiveUniformBlockiv   rglGetActiveUniformBlockiv
#define glBindBufferBase            rglBindBufferBase
#define glGetUniformIndices         rglGetUniformIndices
#define glGetActiveUniformsiv       rglGetActiveUniformsiv
#define glGetError                  rglGetError
#define glClear                     rglClear
#define glPolygonMode               rglPolygonMode
#define glLineWidth                 rglLineWidth
#define glTexImage2DMultisample     rglTexImage2DMultisample
#define glTexStorage2DMultisample   rglTexStorage2DMultisample
#define glMemoryBarrier             rglMemoryBarrier
#define glBindImageTexture          rglBindImageTexture
#define glProgramBinary             rglProgramBinary
#define glGetProgramBinary          rglGetProgramBinary
#define glProgramParameteri         rglProgramParameteri
#define glTexSubImage2D             rglTexSubImage2D
#define glDeleteVertexArrays        rglDeleteVertexArrays
#define glRenderbufferStorageMultisample rglRenderbufferStorageMultisample
#define glUniform1iv                rglUniform1iv
#define glUniform1fv                rglUniform1fv
#define glValidateProgram           rglValidateProgram
#define glGetStringi                rglGetStringi
#define glTexBuffer                 rglTexBuffer
#define glClearBufferfv             rglClearBufferfv
#define glClearBufferfi             rglClearBufferfi
#define glWaitSync                  rglWaitSync
#define glFenceSync                 rglFenceSync
#define glDeleteSync                rglDeleteSync
#define glBufferStorage             rglBufferStorage
#define glFlushMappedBufferRange    rglFlushMappedBufferRange
#define glClientWaitSync            rglClientWaitSync
#define glDrawElementsBaseVertex    rglDrawElementsBaseVertex

const GLubyte* rglGetStringi(GLenum name, GLuint index);
void rglTexBuffer(GLenum target, GLenum internalFormat, GLuint buffer);
void rglClearBufferfv( 	GLenum buffer,
  	GLint drawBuffer,
  	const GLfloat * value);
void rglClearBufferfi( 	GLenum buffer,
  	GLint drawBuffer,
  	GLfloat depth,
  	GLint stencil);
void rglValidateProgram(GLuint program);
void rglRenderbufferStorageMultisample( 	GLenum target,
  	GLsizei samples,
  	GLenum internalformat,
  	GLsizei width,
  	GLsizei height);
void rglUniform1iv(GLint location,  GLsizei count,  const GLint *value);
void rglUniform1fv(GLint location,  GLsizei count,  const GLfloat *value);
void rglProgramParameteri( 	GLuint program,
  	GLenum pname,
  	GLint value);
void rglGetProgramBinary( 	GLuint program,
  	GLsizei bufsize,
  	GLsizei *length,
  	GLenum *binaryFormat,
  	void *binary);
void rglProgramBinary(GLuint program,
  	GLenum binaryFormat,
  	const void *binary,
  	GLsizei length);
void rglBindImageTexture( 	GLuint unit,
  	GLuint texture,
  	GLint level,
  	GLboolean layered,
  	GLint layer,
  	GLenum access,
  	GLenum format);
void rglTexStorage2DMultisample(GLenum target, GLsizei samples,
      GLenum internalformat, GLsizei width, GLsizei height,
      GLboolean fixedsamplelocations);
void rglGetActiveUniformsiv( 	GLuint program,
  	GLsizei uniformCount,
  	const GLuint *uniformIndices,
  	GLenum pname,
  	GLint *params);
void rglGetUniformIndices( 	GLuint program,
  	GLsizei uniformCount,
  	const GLchar **uniformNames,
  	GLuint *uniformIndices);
void rglBindBufferBase( 	GLenum target,
  	GLuint index,
  	GLuint buffer);
void rglGetActiveUniformBlockiv( 	GLuint program,
  	GLuint uniformBlockIndex,
  	GLenum pname,
  	GLint *params);
GLuint rglGetUniformBlockIndex( 	GLuint program,
  	const GLchar *uniformBlockName);
void * rglMapBuffer(	GLenum target, GLenum access);
void *rglMapBufferRange( 	GLenum target,
  	GLintptr offset,
  	GLsizeiptr length,
  	GLbitfield access);
GLboolean rglUnmapBuffer( 	GLenum target);
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
void rglUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
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

void rglGetActiveUniform(GLuint program, GLuint index, GLsizei bufsize,
      GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void rglUniform1ui(GLint location, GLuint v);
void rglUniform2ui(GLint location, GLuint v0, GLuint v1);
void rglUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
void rglUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void rglBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
void rglCopyImageSubData( 	GLuint srcName,
  	GLenum srcTarget,
  	GLint srcLevel,
  	GLint srcX,
  	GLint srcY,
  	GLint srcZ,
  	GLuint dstName,
  	GLenum dstTarget,
  	GLint dstLevel,
  	GLint dstX,
  	GLint dstY,
  	GLint dstZ,
  	GLsizei srcWidth,
  	GLsizei srcHeight,
  	GLsizei srcDepth);
void rglVertexAttribIPointer(
      GLuint index,
      GLint size,
      GLenum type,
      GLsizei stride,
      const GLvoid * pointer);
void rglVertexAttribLPointer(
      GLuint index,
      GLint size,
      GLenum type,
      GLsizei stride,
      const GLvoid * pointer);
void rglUniformBlockBinding( 	GLuint program,
  	GLuint uniformBlockIndex,
  	GLuint uniformBlockBinding);
GLenum rglGetError(void);
void rglClear(GLbitfield mask);
void rglPolygonMode(GLenum face, GLenum mode);
void rglLineWidth(GLfloat width);
void rglTexImage2DMultisample( 	GLenum target,
  	GLsizei samples,
  	GLenum internalformat,
  	GLsizei width,
  	GLsizei height,
  	GLboolean fixedsamplelocations);
void rglMemoryBarrier( 	GLbitfield barriers);
void rglTexSubImage2D( 	GLenum target,
  	GLint level,
  	GLint xoffset,
  	GLint yoffset,
  	GLsizei width,
  	GLsizei height,
  	GLenum format,
  	GLenum type,
  	const GLvoid * pixels);
void rglDeleteVertexArrays(GLsizei n, const GLuint *arrays);
void *rglFenceSync(GLenum condition, GLbitfield flags);
void rglDeleteSync(void *sync);
void rglWaitSync(void *sync, GLbitfield flags, uint64_t timeout);
void rglBufferStorage(GLenum target, GLsizeiptr size, const GLvoid *data, GLbitfield flags);
void rglFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);
GLenum rglClientWaitSync(void *sync, GLbitfield flags, uint64_t timeout);
void rglDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
			       GLvoid *indices, GLint basevertex);

RETRO_END_DECLS

#endif
