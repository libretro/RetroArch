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

#ifndef GLXX_INT_ATTRIB_H
#define GLXX_INT_ATTRIB_H

#include "interface/khronos/include/GLES/gl.h"
#include "interface/khronos/include/GLES2/gl2.h"
#include "interface/khronos/glxx/gl11_int_config.h"
#include "interface/khronos/glxx/glxx_int_config.h"

#include <stddef.h>

typedef struct {
   GLboolean enabled;

   GLint size;
   GLenum type;
   GLboolean normalized;
   GLsizei stride;

   const GLvoid *pointer;

   GLuint buffer;

   GLfloat value[4];
} GLXX_ATTRIB_T;


/* GL 1.1 specific For indexing into arrays of handles/pointers */
#define GL11_IX_COLOR 1//0
#define GL11_IX_NORMAL 2//1
#define GL11_IX_VERTEX 0//2
#define GL11_IX_TEXTURE_COORD 3
#define GL11_IX_POINT_SIZE 7
#define GL11_IX_MATRIX_WEIGHT 8
#define GL11_IX_MATRIX_INDEX 9
#define GL11_IX_MAX_ATTRIBS 10

/* Special values passed to glintAttrib etc. instead of indices */
#define GL11_IX_CLIENT_ACTIVE_TEXTURE 0x80000000

static INLINE void gl20_attrib_init(GLXX_ATTRIB_T *attrib)
{
   uint32_t i;
   for (i = 0; i < GLXX_CONFIG_MAX_VERTEX_ATTRIBS; i++)
   {
      attrib[i].enabled = GL_FALSE;
      attrib[i].size = 4;
      attrib[i].type = GL_FLOAT;
      attrib[i].normalized = GL_FALSE;
      attrib[i].stride = 0;
      attrib[i].pointer = NULL;
      attrib[i].buffer = 0;
      attrib[i].value[0] = 0.0f;
      attrib[i].value[1] = 0.0f;
      attrib[i].value[2] = 0.0f;
      attrib[i].value[3] = 1.0f;
   }
}

static INLINE void gl11_attrib_init(GLXX_ATTRIB_T *attrib)
{
   int32_t i, indx;

   gl20_attrib_init(attrib);

   //vertex
   attrib[GL11_IX_VERTEX].size = 4;
   attrib[GL11_IX_VERTEX].normalized = GL_FALSE;
   attrib[GL11_IX_VERTEX].value[0] = 0.0f;
   attrib[GL11_IX_VERTEX].value[1] = 0.0f;
   attrib[GL11_IX_VERTEX].value[2] = 0.0f;
   attrib[GL11_IX_VERTEX].value[3] = 1.0f;

   //color
   attrib[GL11_IX_COLOR].size = 4;
   attrib[GL11_IX_COLOR].normalized = GL_TRUE;
   attrib[GL11_IX_COLOR].value[0] = 1.0f;
   attrib[GL11_IX_COLOR].value[1] = 1.0f;
   attrib[GL11_IX_COLOR].value[2] = 1.0f;
   attrib[GL11_IX_COLOR].value[3] = 1.0f;

   //normal
   attrib[GL11_IX_NORMAL].size = 3;
   attrib[GL11_IX_NORMAL].normalized = GL_TRUE;
   attrib[GL11_IX_NORMAL].value[0] = 0.0f;
   attrib[GL11_IX_NORMAL].value[1] = 0.0f;
   attrib[GL11_IX_NORMAL].value[2] = 1.0f;

   for (i = 0; i < GL11_CONFIG_MAX_TEXTURE_UNITS; i++) {
      indx = GL11_IX_TEXTURE_COORD + i;
      attrib[indx].size = 4;
      attrib[indx].normalized = GL_FALSE;
      attrib[indx].value[0] = 0.0f;
      attrib[indx].value[1] = 0.0f;
      attrib[indx].value[2] = 0.0f;
      attrib[indx].value[3] = 1.0f;
   }

   //point size
   attrib[GL11_IX_POINT_SIZE].size = 1;
   attrib[GL11_IX_POINT_SIZE].normalized = GL_FALSE;
   attrib[GL11_IX_POINT_SIZE].value[0] = 1.0f;
}

typedef struct
{
   uint32_t cache_offset;     /* How far into the cache this attrib starts */
   uint32_t has_interlock;    /* (Boolean) Whether there is an interlock immediately before this attrib. */
} GLXX_CACHE_INFO_ENTRY_T;

typedef struct
{
   uint32_t send_any;         /* True if we're sending any vertices. If false, remainder of structure is invalid. */
   GLXX_CACHE_INFO_ENTRY_T entries[GLXX_CONFIG_MAX_VERTEX_ATTRIBS];
} GLXX_CACHE_INFO_T;

#endif
