/*  RetroArch - A frontend for libretro.
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

#ifndef __EGL_GBM_FRAMES_H
#define __EGL_GBM_FRAMES_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A software core's frames in buffers the GPU samples where the core
 * wrote them: linear GBM buffers, mapped once, each an EGLImage that a
 * frame texture takes as its storage. Used where the texture is made
 * and drawn, on the thread that owns the GL context. */
#define EGL_GBM_FRAMES_MAX 16

typedef struct egl_gbm_frames egl_gbm_frames_t;

/* @count buffers of @width x @height at @bpp (4 XRGB8888, 2 RGB565) on
 * the card behind @drm_fd. NULL when the display cannot import them,
 * or when a pattern written into one is not what the GPU samples, the
 * first time each format is asked for: then never again this run. */
egl_gbm_frames_t *egl_gbm_frames_new(int drm_fd, unsigned width,
      unsigned height, unsigned bpp, unsigned count);

bool egl_gbm_frames_fits(const egl_gbm_frames_t *f, unsigned width,
      unsigned height, unsigned bpp, unsigned count);

/* A buffer for a @width x @height frame that no texture holds, no draw
 * still reads and that is not the last frame's, blank outside the
 * frame; never waited for; NULL when none is free */
void *egl_gbm_frames_lend(egl_gbm_frames_t *f, unsigned width,
      unsigned height, size_t *pitch);

/* When @frame lies at the start of a buffer, the texture bound to
 * GL_TEXTURE_2D takes it as storage, as frame texture @slot; false,
 * with nothing bound, otherwise */
bool egl_gbm_frames_attach(egl_gbm_frames_t *f, const void *frame,
      unsigned slot);

/* Frame texture @slot lets go of its buffer: true when it held one,
 * and its storage is to be made again before an upload */
bool egl_gbm_frames_detach(egl_gbm_frames_t *f, unsigned slot);

/* The cached frame may name a buffer: retire it first */
void egl_gbm_frames_free(egl_gbm_frames_t **f);

RETRO_END_DECLS

#endif
