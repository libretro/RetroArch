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

#ifndef KHRN_CLIENT_CHECK_TYPES_H
#define KHRN_CLIENT_CHECK_TYPES_H

#include "interface/khronos/common/khrn_int_util.h"

#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/GLES/gl.h"
#include "interface/khronos/include/VG/openvg.h"

/*
   egl types
*/

vcos_static_assert(sizeof(EGLint) == 4);
vcos_static_assert(sizeof(EGLBoolean) == 4);
vcos_static_assert(sizeof(EGLenum) == 4);
vcos_static_assert(sizeof(EGLConfig) == 4);
vcos_static_assert(sizeof(EGLContext) == 4);
vcos_static_assert(sizeof(EGLDisplay) == 4);
vcos_static_assert(sizeof(EGLSurface) == 4);
vcos_static_assert(sizeof(EGLClientBuffer) == 4);
vcos_static_assert(sizeof(NativeDisplayType) == 4);
vcos_static_assert(sizeof(NativePixmapType) == 4);
vcos_static_assert(sizeof(NativeWindowType) == 4);

/*
   gl types
*/

vcos_static_assert(sizeof(GLenum) == 4);
vcos_static_assert(sizeof(GLboolean) == 1);
vcos_static_assert(sizeof(GLbitfield) == 4);
vcos_static_assert(sizeof(GLbyte) == 1);
vcos_static_assert(sizeof(GLshort) == 2);
vcos_static_assert(sizeof(GLint) == 4);
vcos_static_assert(sizeof(GLsizei) == 4);
vcos_static_assert(sizeof(GLubyte) == 1);
vcos_static_assert(sizeof(GLushort) == 2);
vcos_static_assert(sizeof(GLuint) == 4);
vcos_static_assert(sizeof(GLfloat) == 4);
vcos_static_assert(sizeof(GLclampf) == 4);
vcos_static_assert(sizeof(GLfixed) == 4);
vcos_static_assert(sizeof(GLclampx) == 4);
vcos_static_assert(sizeof(GLintptr) == 4);
vcos_static_assert(sizeof(GLsizeiptr) == 4);

/*
   vg types
*/

vcos_static_assert(sizeof(VGfloat) == 4);
vcos_static_assert(sizeof(VGbyte) == 1);
vcos_static_assert(sizeof(VGubyte) == 1);
vcos_static_assert(sizeof(VGshort) == 2);
vcos_static_assert(sizeof(VGint) == 4);
vcos_static_assert(sizeof(VGuint) == 4);
vcos_static_assert(sizeof(VGbitfield) == 4);
vcos_static_assert(sizeof(VGboolean) == 4);

#endif
