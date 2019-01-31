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

//gl 2.0 specific
FN(void, glAttachShader_impl_20, (GLuint program, GLuint shader))
FN(void, glBindAttribLocation_impl_20, (GLuint program, GLuint index, const char *name))
FN(void, glBlendColor_impl_20, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)) // S
FN(void, glBlendEquationSeparate_impl_20, (GLenum modeRGB, GLenum modeAlpha)) // S
FN(GLuint, glCreateProgram_impl_20, (void))
FN(GLuint, glCreateShader_impl_20, (GLenum type))
FN(void, glDeleteProgram_impl_20, (GLuint program))
FN(void, glDeleteShader_impl_20, (GLuint shader))
FN(void, glDetachShader_impl_20, (GLuint program, GLuint shader))
//FN(void, glDisableVertexAttribArray_impl_20, (GLuint index))
//FN(void, glEnableVertexAttribArray_impl_20, (GLuint index))
FN(void, glGetActiveAttrib_impl_20, (GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name))
FN(void, glGetActiveUniform_impl_20, (GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name))
FN(void, glGetAttachedShaders_impl_20, (GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders))
FN(int, glGetAttribLocation_impl_20, (GLuint program, const char *name))
FN(int, glGetProgramiv_impl_20, (GLuint program, GLenum pname, GLint *params))
FN(void, glGetProgramInfoLog_impl_20, (GLuint program, GLsizei bufsize, GLsizei *length, char *infolog))
FN(int, glGetUniformfv_impl_20, (GLuint program, GLint location, GLfloat *params))
FN(int, glGetUniformiv_impl_20, (GLuint program, GLint location, GLint *params))
FN(int, glGetUniformLocation_impl_20, (GLuint program, const char *name))
//FN(void, glGetVertexAttribfv_impl_20, (GLuint index, GLenum pname, GLfloat *params))
//FN(void, glGetVertexAttribiv_impl_20, (GLuint index, GLenum pname, GLint *params))
//FN(void, glGetVertexAttribPointerv_impl_20, (GLuint index, GLenum pname, void **pointer))
FN(GLboolean, glIsProgram_impl_20, (GLuint program))
FN(GLboolean, glIsShader_impl_20, (GLuint shader))
FN(void, glLinkProgram_impl_20, (GLuint program))
FN(void, glPointSize_impl_20, (GLfloat size)) // S
FN(void, glUniform1i_impl_20, (GLint location, GLint x))
FN(void, glUniform2i_impl_20, (GLint location, GLint x, GLint y))
FN(void, glUniform3i_impl_20, (GLint location, GLint x, GLint y, GLint z))
FN(void, glUniform4i_impl_20, (GLint location, GLint x, GLint y, GLint z, GLint w))
FN(void, glUniform1f_impl_20, (GLint location, GLfloat x))
FN(void, glUniform2f_impl_20, (GLint location, GLfloat x, GLfloat y))
FN(void, glUniform3f_impl_20, (GLint location, GLfloat x, GLfloat y, GLfloat z))
FN(void, glUniform4f_impl_20, (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w))
FN(void, glUniform1iv_impl_20, (GLint location, GLsizei count, int size, const GLint *v))
FN(void, glUniform2iv_impl_20, (GLint location, GLsizei count, int size, const GLint *v))
FN(void, glUniform3iv_impl_20, (GLint location, GLsizei count, int size, const GLint *v))
FN(void, glUniform4iv_impl_20, (GLint location, GLsizei count, int size, const GLint *v))
FN(void, glUniform1fv_impl_20, (GLint location, GLsizei count, int size, const GLfloat *v))
FN(void, glUniform2fv_impl_20, (GLint location, GLsizei count, int size, const GLfloat *v))
FN(void, glUniform3fv_impl_20, (GLint location, GLsizei count, int size, const GLfloat *v))
FN(void, glUniform4fv_impl_20, (GLint location, GLsizei count, int size, const GLfloat *v))
FN(void, glUniformMatrix2fv_impl_20, (GLint location, GLsizei count, GLboolean transpose, int size, const GLfloat *value))
FN(void, glUniformMatrix3fv_impl_20, (GLint location, GLsizei count, GLboolean transpose, int size, const GLfloat *value))
FN(void, glUniformMatrix4fv_impl_20, (GLint location, GLsizei count, GLboolean transpose, int size, const GLfloat *value))
FN(void, glUseProgram_impl_20, (GLuint program)) // S
FN(void, glValidateProgram_impl_20, (GLuint program))
//FN(void, glVertexAttrib1f_impl_20, (GLuint indx, GLfloat x))
//FN(void, glVertexAttrib2f_impl_20, (GLuint indx, GLfloat x, GLfloat y))
//FN(void, glVertexAttrib3f_impl_20, (GLuint indx, GLfloat x, GLfloat y, GLfloat z))
//FN(void, glVertexAttrib4f_impl_20, (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w))
//FN(void, glVertexAttrib1fv_impl_20, (GLuint indx, const GLfloat *values))
//FN(void, glVertexAttrib2fv_impl_20, (GLuint indx, const GLfloat *values))
//FN(void, glVertexAttrib3fv_impl_20, (GLuint indx, const GLfloat *values))
//FN(void, glVertexAttrib4fv_impl_20, (GLuint indx, const GLfloat *values))


/* OES_shader_source */
FN(void, glCompileShader_impl_20, (GLuint shader))
FN(int, glGetShaderiv_impl_20, (GLuint shader, GLenum pname, GLint *params))
FN(void, glGetShaderInfoLog_impl_20, (GLuint shader, GLsizei bufsize, GLsizei *length, char *infolog))
FN(void, glGetShaderSource_impl_20, (GLuint shader, GLsizei bufsize, GLsizei *length, char *source))
FN(void, glShaderSource_impl_20, (GLuint shader, GLsizei count, const char **string, const GLint *length))
//FN(void, glGetShaderPrecisionFormat_impl_20, (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision))
FN(void, glGetShaderPrecisionFormat_impl_20, (GLenum shadertype, GLenum precisiontype, GLint *result))

/*****************************************************************************************/
/*                                 OES extension functions                               */
/*****************************************************************************************/

//gl 2.0 specific

//FN(void, glVertexAttribPointer_impl_20, (GLuint indx))

#if GL_OES_EGL_image
FN(void, glEGLImageTargetRenderbufferStorageOES_impl_20, (GLenum target, GLeglImageOES image))
#if EGL_BRCM_global_image
FN(void, glGlobalImageRenderbufferStorageOES_impl_20, (GLenum target, GLuint id_0, GLuint id_1))
#endif
#endif

#undef FN
