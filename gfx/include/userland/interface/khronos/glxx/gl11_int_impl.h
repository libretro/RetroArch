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

//gl 1.1 specific functions
FN(void, glAlphaFunc_impl_11, (GLenum func, GLclampf ref))
FN(void, glAlphaFuncx_impl_11, (GLenum func, GLclampx ref))
FN(void, glClearColorx_impl_11, (GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha))
FN(void, glClearDepthx_impl_11, (GLclampx depth))
FN(void, glClientActiveTexture_impl_11, (GLenum texture))
FN(void, glClipPlanef_impl_11, (GLenum plane, const GLfloat *equation))
FN(void, glClipPlanex_impl_11, (GLenum plane, const GLfixed *equation))
FN(void, glDepthRangex_impl_11, (GLclampx zNear, GLclampx zFar))
FN(void, glFogf_impl_11, (GLenum pname, GLfloat param))
FN(void, glFogfv_impl_11, (GLenum pname, const GLfloat *params))
FN(void, glFrustumf_impl_11, (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar))
FN(void, glFogx_impl_11, (GLenum pname, GLfixed param))
FN(void, glFogxv_impl_11, (GLenum pname, const GLfixed *params))
FN(void, glFrustumx_impl_11, (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed zNear, GLfixed zFar))
FN(void, glGetClipPlanex_impl_11, (GLenum pname, GLfixed eqn[4]))
FN(int, glGetLightfv_impl_11, (GLenum light, GLenum pname, GLfloat *params))
FN(int, glGetLightxv_impl_11, (GLenum light, GLenum pname, GLfixed *params))
FN(int, glGetMaterialxv_impl_11, (GLenum face, GLenum pname, GLfixed *params))
FN(int, glGetMaterialfv_impl_11, (GLenum face, GLenum pname, GLfloat *params))
FN(void, glGetClipPlanef_impl_11, (GLenum pname, GLfloat eqn[4]))
FN(int, glGetFixedv_impl_11, (GLenum pname, GLfixed *params))
FN(int, glGetTexEnvfv_impl_11, (GLenum env, GLenum pname, GLfloat *params))
FN(int, glGetTexEnviv_impl_11, (GLenum env, GLenum pname, GLint *params))
FN(int, glGetTexEnvxv_impl_11, (GLenum env, GLenum pname, GLfixed *params))
FN(int, glGetTexParameterxv_impl_11, (GLenum target, GLenum pname, GLfixed *params))
FN(void, glLightModelf_impl_11, (GLenum pname, GLfloat param))
FN(void, glLightModelfv_impl_11, (GLenum pname, const GLfloat *params))
FN(void, glLightf_impl_11, (GLenum light, GLenum pname, GLfloat param))
FN(void, glLightfv_impl_11, (GLenum light, GLenum pname, const GLfloat *params))
FN(void, glLightModelx_impl_11, (GLenum pname, GLfixed param))
FN(void, glLightModelxv_impl_11, (GLenum pname, const GLfixed *params))
FN(void, glLightx_impl_11, (GLenum light, GLenum pname, GLfixed param))
FN(void, glLightxv_impl_11, (GLenum light, GLenum pname, const GLfixed *params))
FN(void, glLineWidthx_impl_11, (GLfixed width))
FN(void, glLoadIdentity_impl_11, (void))
FN(void, glLoadMatrixf_impl_11, (const GLfloat *m))
FN(void, glLoadMatrixx_impl_11, (const GLfixed *m))
FN(void, glLogicOp_impl_11, (GLenum opcode))
FN(void, glMaterialf_impl_11, (GLenum face, GLenum pname, GLfloat param))
FN(void, glMaterialfv_impl_11, (GLenum face, GLenum pname, const GLfloat *params))
FN(void, glMultMatrixf_impl_11, (const GLfloat *m))
FN(void, glOrthof_impl_11, (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar))
FN(void, glPolygonOffsetx_impl_11, (GLfixed factor, GLfixed units))
FN(void, glPointParameterf_impl_11, (GLenum pname, GLfloat param))
FN(void, glPointParameterfv_impl_11, (GLenum pname, const GLfloat *params))
FN(void, glRotatef_impl_11, (GLfloat angle, GLfloat x, GLfloat y, GLfloat z))
FN(void, glSampleCoveragex_impl_11, (GLclampx value, GLboolean invert))
FN(void, glScalef_impl_11, (GLfloat x, GLfloat y, GLfloat z))
FN(void, glShadeModel_impl_11, (GLenum model))
FN(void, glTexEnvf_impl_11, (GLenum target, GLenum pname, GLfloat param))
FN(void, glTexEnvfv_impl_11, (GLenum target, GLenum pname, const GLfloat *params))
FN(void, glTexEnvi_impl_11, (GLenum target, GLenum pname, GLint param))
FN(void, glTexEnviv_impl_11, (GLenum target, GLenum pname, const GLint *params))
FN(void, glTexEnvx_impl_11, (GLenum target, GLenum pname, GLfixed param))
FN(void, glTexEnvxv_impl_11, (GLenum target, GLenum pname, const GLfixed *params))
FN(void, glTexParameterx_impl_11, (GLenum target, GLenum pname, GLfixed param))
FN(void, glTexParameterxv_impl_11, (GLenum target, GLenum pname, const GLfixed *params))
FN(void, glTranslatef_impl_11, (GLfloat x, GLfloat y, GLfloat z))
FN(void, glMaterialx_impl_11, (GLenum face, GLenum pname, GLfixed param))
FN(void, glMaterialxv_impl_11, (GLenum face, GLenum pname, const GLfixed *params))
FN(void, glMatrixMode_impl_11, (GLenum mode))
FN(void, glMultMatrixx_impl_11, (const GLfixed *m))
FN(void, glOrthox_impl_11, (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed zNear, GLfixed zFar))
FN(void, glPointParameterx_impl_11, (GLenum pname, GLfixed param))
FN(void, glPointParameterxv_impl_11, (GLenum pname, const GLfixed *params))
//FN(void, glPointSizePointerOES_impl_11, (void))
FN(void, glPopMatrix_impl_11, (void))
FN(void, glPushMatrix_impl_11, (void))
FN(void, glRotatex_impl_11, (GLfixed angle, GLfixed x, GLfixed y, GLfixed z))
FN(void, glScalex_impl_11, (GLfixed x, GLfixed y, GLfixed z))
FN(void, glTranslatex_impl_11, (GLfixed x, GLfixed y, GLfixed z))

//FN(void, glColorPointer_impl_11, (void))
//FN(void, glNormalPointer_impl_11, (void))
//FN(void, glTexCoordPointer_impl_11, (GLenum unit))
//FN(void, glVertexPointer_impl_11, (void))

/*****************************************************************************************/
/*                                 OES extension functions                               */
/*****************************************************************************************/

//gl 1.1 specific
FN(void, glintColor_impl_11, (float red, float green, float blue, float alpha))
FN(void, glQueryMatrixxOES_impl_11, (GLfixed mantissa[16]))
FN(void, glDrawTexfOES_impl_11, (GLfloat Xs, GLfloat Ys, GLfloat Zs, GLfloat Ws, GLfloat Hs))

#if GL_OES_matrix_palette
FN(void, glCurrentPaletteMatrixOES_impl, (GLuint index))
FN(void, glLoadPaletteFromModelViewMatrixOES_impl, (void))
//FN(void, glMatrixIndexPointerOES_impl, (void))
//FN(void, glWeightPointerOES_impl, (void))
#endif

#undef FN
