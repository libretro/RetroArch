/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2021 - Daniel De Matteis
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

#ifndef __GL_COMMON_H
#define __GL_COMMON_H

void gl_clear(void);

void gl_enable(unsigned cap);

void gl_disable(unsigned cap);

void gl_finish(void);

void gl_flush(void);

#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)) \
      && !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
#include <stddef.h>
#include <boolean.h>

#define HAVE_GL_TEXTURE_LEND

/* A streamed texture's upload memory, lent (video_poke_interface_t
 * texture_lend): two pixel unpack buffers per texture, mapped for good,
 * each with the fence of the last upload from it. One list per driver
 * instance, used on the thread that owns its context. */
typedef struct gl_texture_lend gl_texture_lend_t;

/* Slot @slot of texture @tex's upload memory when its rows are @pitch
 * bytes apart; NULL, with nothing lent, otherwise */
void *gl_texture_lend(gl_texture_lend_t **list, unsigned tex,
      unsigned slot, size_t pitch);

/* Whether lent slot @slot may be written: its last upload has run */
bool gl_texture_lend_ready(gl_texture_lend_t *list, unsigned tex,
      unsigned slot);

/* For an upload of @pixels into @tex: when they are a lent slot, binds
 * its buffer as the unpack source and returns the slot, which the
 * upload then reads at offset 0 and gl_texture_lend_unbind() fences.
 * -1 when they are not lent, -2 when they are but the slot's last
 * upload has not run: written early, the frame is dropped. */
int gl_texture_lend_bind(gl_texture_lend_t *list, unsigned tex,
      const void *pixels);
void gl_texture_lend_unbind(gl_texture_lend_t *list, unsigned tex,
      int slot);

/* With the texture, or with every texture */
void gl_texture_lend_forget(gl_texture_lend_t **list, unsigned tex);
void gl_texture_lend_free(gl_texture_lend_t **list);
#endif

#endif
