/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RPNG_H__
#define __LIBRETRO_SDK_FORMAT_RPNG_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>
#include <file/archive_file.h>

RETRO_BEGIN_DECLS

typedef struct rpng rpng_t;

rpng_t *rpng_nbio_load_image_argb_init(const char *path);

bool rpng_is_valid(rpng_t *rpng);

bool rpng_set_buf_ptr(rpng_t *rpng, void *data);

rpng_t *rpng_alloc(void);

void rpng_nbio_load_image_free(rpng_t *rpng);

bool rpng_nbio_load_image_argb_iterate(rpng_t *rpng);

int rpng_nbio_load_image_argb_process(rpng_t *rpng,
      uint32_t **data, unsigned *width, unsigned *height);

bool rpng_nbio_load_image_argb_start(rpng_t *rpng);

#ifdef HAVE_ZLIB_DEFLATE
bool rpng_save_image_argb(const char *path, const uint32_t *data,
      unsigned width, unsigned height, unsigned pitch);
bool rpng_save_image_bgr24(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch);
#endif

RETRO_END_DECLS

#endif

