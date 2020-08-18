/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image.h).
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

#ifndef __RARCH_IMAGE_CONTEXT_H
#define __RARCH_IMAGE_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

enum image_process_code
{
   IMAGE_PROCESS_ERROR     = -2,
   IMAGE_PROCESS_ERROR_END = -1,
   IMAGE_PROCESS_NEXT      =  0,
   IMAGE_PROCESS_END       =  1
};

struct texture_image
{
   uint32_t *pixels;
   unsigned width;
   unsigned height;
   bool supports_rgba;
};

enum image_type_enum
{
   IMAGE_TYPE_NONE = 0,
   IMAGE_TYPE_PNG,
   IMAGE_TYPE_JPEG,
   IMAGE_TYPE_BMP,
   IMAGE_TYPE_TGA
};

enum image_type_enum image_texture_get_type(const char *path);

bool image_texture_set_color_shifts(unsigned *r_shift, unsigned *g_shift,
      unsigned *b_shift, unsigned *a_shift,
      struct texture_image *out_img);

bool image_texture_color_convert(unsigned r_shift,
      unsigned g_shift, unsigned b_shift, unsigned a_shift,
      struct texture_image *out_img);

bool image_texture_load_buffer(struct texture_image *img,
   enum image_type_enum type, void *buffer, size_t buffer_len);

bool image_texture_load(struct texture_image *img, const char *path);
void image_texture_free(struct texture_image *img);

/* Image transfer */

void image_transfer_free(void *data, enum image_type_enum type);

void *image_transfer_new(enum image_type_enum type);

bool image_transfer_start(void *data, enum image_type_enum type);

void image_transfer_set_buffer_ptr(
      void *data,
      enum image_type_enum type,
      void *ptr,
      size_t len);

int image_transfer_process(
      void *data,
      enum image_type_enum type,
      uint32_t **buf, size_t size,
      unsigned *width, unsigned *height);

bool image_transfer_iterate(void *data, enum image_type_enum type);

bool image_transfer_is_valid(void *data, enum image_type_enum type);

RETRO_END_DECLS

#endif
