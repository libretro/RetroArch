/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwebp.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RWEBP_H__
#define __LIBRETRO_SDK_FORMAT_RWEBP_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rwebp rwebp_t;

int rwebp_process_image(rwebp_t *rwebp, void **buf,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

bool rwebp_set_buf_ptr(rwebp_t *rwebp, void *data, size_t len);

void rwebp_free(rwebp_t *rwebp);

rwebp_t *rwebp_alloc(void);

RETRO_END_DECLS

#endif
