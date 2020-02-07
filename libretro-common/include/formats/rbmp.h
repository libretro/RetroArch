/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbmp.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RBMP_H__
#define __LIBRETRO_SDK_FORMAT_RBMP_H__

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

enum rbmp_source_type
{
   RBMP_SOURCE_TYPE_DONT_CARE,
   RBMP_SOURCE_TYPE_BGR24,
   RBMP_SOURCE_TYPE_XRGB888,
   RBMP_SOURCE_TYPE_RGB565,
   RBMP_SOURCE_TYPE_ARGB8888
};

typedef struct rbmp rbmp_t;

bool rbmp_save_image(
      const char *filename,
      const void *frame,
      unsigned width,
      unsigned height,
      unsigned pitch,
      enum rbmp_source_type type);

int rbmp_process_image(rbmp_t *rbmp, void **buf,
      size_t size, unsigned *width, unsigned *height);

void form_bmp_header(uint8_t *header,
      unsigned width, unsigned height,
      bool is32bpp);

bool rbmp_set_buf_ptr(rbmp_t *rbmp, void *data);

void rbmp_free(rbmp_t *rbmp);

rbmp_t *rbmp_alloc(void);

RETRO_END_DECLS

#endif
