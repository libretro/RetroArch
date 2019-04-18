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

#ifndef EGL_INT_H
#define EGL_INT_H

typedef enum {
   OPENGL_ES_11,
   OPENGL_ES_20,
   OPENVG
} EGL_CONTEXT_TYPE_T;

// A single colour buffer. Optional ancillary buffers. If triple-buffered,
// three EGL_SERVER_SURFACE_T's share ancillary buffers
typedef uint32_t EGL_SURFACE_ID_T;

// Either a GLES1.1 or GLES2.0 server state
typedef uint32_t EGL_GL_CONTEXT_ID_T;

typedef uint32_t EGL_VG_CONTEXT_ID_T;

typedef uint32_t EGL_CONTEXT_ID_T;

#define EGL_SERVER_NO_SURFACE 0
#define EGL_SERVER_NO_GL_CONTEXT 0
#define EGL_SERVER_NO_VG_CONTEXT 0

#define EGL_SERVER_GL11 1
#define EGL_SERVER_GL20 2

typedef uint32_t EGL_SYNC_ID_T;

#endif
