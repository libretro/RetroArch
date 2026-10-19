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

/* Upload memory mapped for good: GL 4.4 or ARB_buffer_storage on the
 * desktop, GLES 3.0 with EXT_buffer_storage (checked at run time).
 * Texture lending additionally requires GLES 3.1 level queries. */
#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)) \
      && !defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
#define HAVE_GL_FRAME_MEM
#define HAVE_GL_TEXTURE_LEND
#elif defined(HAVE_OPENGLES3) && !defined(HAVE_OPENGLES2) \
      && !defined(HAVE_PSGL)
#define HAVE_GL_FRAME_MEM
#define HAVE_GL_TEXTURE_LEND
#endif

#ifdef HAVE_GL_FRAME_MEM
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#endif

#ifdef HAVE_GL_TEXTURE_LEND

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

#ifdef HAVE_GL_FRAME_MEM
/* The threaded wrapper's frame buffers (video_poke_interface_t
 * frame_mem_alloc): pixel unpack buffers mapped for good, each with
 * the fence of the last upload from it. Made on first use; used on the
 * thread that owns the context. */
typedef struct gl_frame_mem gl_frame_mem_t;

void *gl_frame_mem_alloc(gl_frame_mem_t **fm, size_t size, uintptr_t *id);
bool gl_frame_mem_ready(gl_frame_mem_t *fm, uintptr_t id);
/* GL keeps a buffer a queued upload reads until the upload has run */
void gl_frame_mem_free(gl_frame_mem_t *fm, uintptr_t id);

/* For an upload reading @extent bytes from @pixels: when they lie in a
 * frame buffer, binds it as the unpack source, sets *offset to where
 * they start in it and returns its slot, which
 * gl_frame_mem_unbind() fences; -1, with the unpack buffer unbound,
 * otherwise */
int gl_frame_mem_bind(gl_frame_mem_t *fm, const void *pixels,
      size_t extent, size_t *offset);
void gl_frame_mem_unbind(gl_frame_mem_t *fm, int slot);
void gl_frame_mem_destroy(gl_frame_mem_t **fm);

/* The software framebuffer lent to a core (get_current_software_framebuffer):
 * three frame buffers at the frame's size and tight pitch, one never
 * the buffer the last frame was pushed from, which the cached frame
 * names, nor one an upload still reads */
#define GL_SW_FB_SLOTS 3
typedef struct gl_sw_fb
{
   void     *mapped[GL_SW_FB_SLOTS];
   uintptr_t id[GL_SW_FB_SLOTS];
   uintptr_t last;   /* the frame buffer the last frame came from */
   unsigned  dims;   /* VIDEO_SCALE_PACK'd size they were made for */
   unsigned  bpp;
   unsigned  next;
   bool      failed; /* none could be made at this size */
} gl_sw_fb_t;

/* Whether the buffers are already made for @width x @height at @bpp:
 * when not, a lend makes them anew and the caller retires the cached
 * frame first */
bool gl_sw_fb_fits(const gl_sw_fb_t *sw, unsigned width, unsigned height,
      unsigned bpp);
/* A buffer to lend, never waited for; NULL when none is free */
void *gl_sw_fb_lend(gl_frame_mem_t **fm, gl_sw_fb_t *sw,
      unsigned width, unsigned height, unsigned bpp);
/* The upload of a frame pushed from frame buffer slot @slot
 * (gl_frame_mem_bind), or -1 */
void gl_sw_fb_pushed(gl_sw_fb_t *sw, int slot);
void gl_sw_fb_free(gl_frame_mem_t *fm, gl_sw_fb_t *sw);
#endif

#if defined(__APPLE__) && !defined(HAVE_OPENGLES)
#include <AvailabilityMacros.h>
#endif

/* Planar frames (TEXTURE_GPU_FORMAT_YUV420) converted by a draw into an
 * RGBA texture. Not where framebuffer names carry a suffix (PSGL, a
 * macOS SDK before 10.12) or there are no shaders. */
#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)) \
      && !defined(HAVE_PSGL) && !defined(HAVE_OPENGLES1) \
      && !(defined(__MACH__) && !defined(HAVE_OPENGLES) \
         && defined(MAC_OS_X_VERSION_MAX_ALLOWED) \
         && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200))
#include <stdint.h>
#include <boolean.h>
#include <formats/image.h>

#define HAVE_GL_PLANAR

typedef struct gl_planar_tex gl_planar_tex_t;

/* One per driver instance, zeroed with it; used on the thread that
 * owns its context. */
typedef struct
{
   gl_planar_tex_t *list;
   unsigned prog;
   unsigned vbo;
   unsigned vao;
   int loc[5];       /* uY, uC0, uC1, uSize, uCoef */
   uint8_t flags;    /* GL_PLANAR_* */
} gl_planar_t;

#define GL_PLANAR_CORE   (1 << 0) /* R8/RG8, a VAO, GLSL 1.40 or 3.00 es */
#define GL_PLANAR_FAILED (1 << 1) /* no program: refused from now on */

/* Whether planar frames are taken; read on any thread, so it is the
 * one flag and no GL call */
#define GL_PLANAR_OK(p) (!((p)->flags & GL_PLANAR_FAILED))

/* A texture of @ti's planar frame converted, sampled @linear or
 * nearest: its name, 0 when it could not be made. @core as the context
 * is. */
unsigned gl_planar_load(gl_planar_t *p, const struct texture_image *ti,
      bool linear, bool core);

/* The next frame into a texture gl_planar_load made; false when @tex
 * is not one or the frame does not fit it */
bool gl_planar_update(gl_planar_t *p, unsigned tex,
      const struct texture_image *ti);

/* Before @tex is deleted: its planes and framebuffer go. Any texture. */
void gl_planar_forget(gl_planar_t *p, unsigned tex);

/* With the context: everything */
void gl_planar_free(gl_planar_t *p);
#endif

#endif
