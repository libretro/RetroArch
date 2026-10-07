/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdlib.h>

#include <glsym/glsym.h>

#include "gl_common.h"

void gl_flush(void)
{
   glFlush();
}

void gl_clear(void)
{
   glClear(GL_COLOR_BUFFER_BIT);
}

void gl_disable(unsigned _cap)
{
   GLenum cap = (GLenum)_cap;
   glDisable(cap);
}

void gl_enable(unsigned _cap)
{
   GLenum cap = (GLenum)_cap;
   glEnable(cap);
}

void gl_finish(void)
{
   glFinish();
}

#ifdef HAVE_GL_TEXTURE_LEND
#include <gfx/gl_capabilities.h>

#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT   0x0080
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F            0x881A
#endif
/* Not in every header the drivers build against (the old Mac OS X
 * gl.h among them); the entry points are resolved at run time */
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT              0x0002
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER        0x88EC
#endif
#ifndef GL_TEXTURE_INTERNAL_FORMAT
#define GL_TEXTURE_INTERNAL_FORMAT    0x1003
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED           0x911A
#endif
#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED        0x911C
#endif

#define GL_LEND_SLOTS 2

struct gl_texture_lend
{
   GLsync   fence[GL_LEND_SLOTS];
   void    *mapped[GL_LEND_SLOTS];
   struct gl_texture_lend *next;
   size_t   size;
   size_t   pitch;
   GLuint   pbo[GL_LEND_SLOTS];
   GLuint   tex;
   /* An upload from the slot has been issued and not seen to finish.
    * Kept apart from the fence, which can fail to be made: a slot with
    * no fence is not thereby free. */
   bool     busy[GL_LEND_SLOTS];
};

static gl_texture_lend_t *gl_texture_lend_find(gl_texture_lend_t *list,
      unsigned tex)
{
   while (list && list->tex != (GLuint)tex)
      list = list->next;
   return list;
}

/* Persistent mapping is GL 4.4 or ARB_buffer_storage, fences GL 3.2
 * or ARB_sync */
static bool gl_texture_lend_supported(void)
{
   const char *ver = (const char*)glGetString(GL_VERSION);
   int major       = 0, minor = 0;
   if (!glBufferStorage || !glMapBufferRange || !glFenceSync
         || !glClientWaitSync)
      return false;
   if (     ver && ver[0] >= '0' && ver[0] <= '9' && ver[1] == '.'
         && ver[2] >= '0' && ver[2] <= '9')
   {
      major = ver[0] - '0';
      minor = ver[2] - '0';
   }
   if (major > 4 || (major == 4 && minor >= 4))
      return true;
   return    gl_query_extension("ARB_buffer_storage")
          && (major > 3 || (major == 3 && minor >= 2)
             || gl_query_extension("ARB_sync"));
}

static void gl_texture_lend_destroy(gl_texture_lend_t *st)
{
   unsigned i;
   for (i = 0; i < GL_LEND_SLOTS; i++)
      if (st->fence[i])
         glDeleteSync(st->fence[i]);
   /* Deleting a mapped buffer unmaps it */
   glDeleteBuffers(GL_LEND_SLOTS, st->pbo);
   free(st);
}

/* Both slots at once, sized from the texture as it was made */
static gl_texture_lend_t *gl_texture_lend_new(unsigned tex, size_t pitch)
{
   unsigned i;
   GLint w = 0, h = 0, ifmt = 0;
   size_t bpp;
   gl_texture_lend_t *st;
   GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT
      | GL_MAP_COHERENT_BIT;

   if (!gl_texture_lend_supported())
      return NULL;
   glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
   glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w);
   glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
   glGetTexLevelParameteriv(GL_TEXTURE_2D, 0,
         GL_TEXTURE_INTERNAL_FORMAT, &ifmt);
   glBindTexture(GL_TEXTURE_2D, 0);
   bpp = (ifmt == GL_RGBA16F) ? 8 : 4;
   if (w <= 0 || h <= 0 || pitch != (size_t)w * bpp)
      return NULL;
   if (!(st = (gl_texture_lend_t*)calloc(1, sizeof(*st))))
      return NULL;
   st->tex   = (GLuint)tex;
   st->pitch = pitch;
   st->size  = pitch * (size_t)h;
   glGenBuffers(GL_LEND_SLOTS, st->pbo);
   for (i = 0; i < GL_LEND_SLOTS; i++)
   {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, st->pbo[i]);
      glBufferStorage(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)st->size,
            NULL, flags);
      st->mapped[i] = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
            (GLsizeiptr)st->size, flags);
      if (!st->mapped[i])
         break;
   }
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (i < GL_LEND_SLOTS)
   {
      gl_texture_lend_destroy(st);
      return NULL;
   }
   return st;
}

void *gl_texture_lend(gl_texture_lend_t **list, unsigned tex,
      unsigned slot, size_t pitch)
{
   gl_texture_lend_t *st;
   if (!list || !tex || slot >= GL_LEND_SLOTS)
      return NULL;
   if (!(st = gl_texture_lend_find(*list, tex)))
   {
      if (!(st = gl_texture_lend_new(tex, pitch)))
         return NULL;
      st->next = *list;
      *list    = st;
   }
   else if (pitch != st->pitch)
      return NULL;
   return st->mapped[slot];
}

static bool gl_texture_lend_slot_ready(gl_texture_lend_t *st,
      unsigned slot)
{
   GLenum r;
   if (!st->busy[slot])
      return true;
   /* The upload's own fence could not be made: one made now follows
    * it in the command stream, so its signal covers the upload too */
   if (     !st->fence[slot]
         && !(st->fence[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)))
      return false;
   r = glClientWaitSync(st->fence[slot], 0, 0);
   if (r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED)
      return false;
   glDeleteSync(st->fence[slot]);
   st->fence[slot] = 0;
   st->busy[slot]  = false;
   return true;
}

bool gl_texture_lend_ready(gl_texture_lend_t *list, unsigned tex,
      unsigned slot)
{
   gl_texture_lend_t *st = gl_texture_lend_find(list, tex);
   if (!st || slot >= GL_LEND_SLOTS)
      return true;
   return gl_texture_lend_slot_ready(st, slot);
}

int gl_texture_lend_bind(gl_texture_lend_t *list, unsigned tex,
      const void *pixels)
{
   unsigned i;
   gl_texture_lend_t *st = gl_texture_lend_find(list, tex);
   if (!st)
      return -1;
   for (i = 0; i < GL_LEND_SLOTS; i++)
      if (pixels == st->mapped[i])
      {
         if (!gl_texture_lend_slot_ready(st, i))
            return -2;
         glBindBuffer(GL_PIXEL_UNPACK_BUFFER, st->pbo[i]);
         return (int)i;
      }
   return -1;
}

void gl_texture_lend_unbind(gl_texture_lend_t *list, unsigned tex,
      int slot)
{
   gl_texture_lend_t *st = gl_texture_lend_find(list, tex);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (st && slot >= 0 && slot < GL_LEND_SLOTS)
   {
      st->busy[slot]  = true;
      st->fence[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   }
}

void gl_texture_lend_forget(gl_texture_lend_t **list, unsigned tex)
{
   gl_texture_lend_t **p;
   if (!list)
      return;
   for (p = list; *p; p = &(*p)->next)
      if ((*p)->tex == (GLuint)tex)
      {
         gl_texture_lend_t *st = *p;
         *p = st->next;
         gl_texture_lend_destroy(st);
         return;
      }
}

void gl_texture_lend_free(gl_texture_lend_t **list)
{
   if (!list)
      return;
   while (*list)
   {
      gl_texture_lend_t *st = *list;
      *list = st->next;
      gl_texture_lend_destroy(st);
   }
}
#endif
