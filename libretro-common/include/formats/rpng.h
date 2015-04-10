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

#include <boolean.h>
#include <file/file_extract.h>

#ifdef __cplusplus
extern "C" {
#endif

struct idat_buffer
{
   uint8_t *data;
   size_t size;
};

struct png_chunk
{
   uint32_t size;
   char type[4];
   uint8_t *data;
};

struct png_ihdr
{
   uint32_t width;
   uint32_t height;
   uint8_t depth;
   uint8_t color_type;
   uint8_t compression;
   uint8_t filter;
   uint8_t interlace;
};

struct rpng_process_t
{
   bool initialized;
   bool inflate_initialized;
   bool adam7_pass_initialized;
   bool pass_initialized;
   uint32_t *data;
   uint32_t *palette;
   struct png_ihdr ihdr;
   uint8_t *prev_scanline;
   uint8_t *decoded_scanline;
   uint8_t *inflate_buf;
   size_t restore_buf_size;
   size_t adam7_restore_buf_size;
   size_t data_restore_buf_size;
   size_t inflate_buf_size;
   unsigned bpp;
   unsigned pitch;
   unsigned h;
   struct
   {
      unsigned width;
      unsigned height;
      size_t   size;
      unsigned pos;
   } pass;
   void *stream;
   zlib_file_handle_t handle;
};

struct rpng_t
{
   struct rpng_process_t process;
   bool has_ihdr;
   bool has_idat;
   bool has_iend;
   bool has_plte;
   struct idat_buffer idat_buf;
   struct png_ihdr ihdr;
   uint8_t *buff_data;
   uint32_t palette[256];
};

bool rpng_load_image_argb(const char *path, uint32_t **data,
      unsigned *width, unsigned *height);

struct rpng_t *rpng_nbio_load_image_argb_init(const char *path);

void rpng_nbio_load_image_free(struct rpng_t *rpng);

bool rpng_nbio_load_image_argb_iterate(uint8_t *buf,
      struct rpng_t *rpng, unsigned *ret);

int rpng_nbio_load_image_argb_process(struct rpng_t *rpng,
      uint32_t **data, unsigned *width, unsigned *height);

int rpng_load_image_argb_process_inflate_init(struct rpng_t *rpng,
      uint32_t **data, unsigned *width, unsigned *height);

bool rpng_nbio_load_image_argb_start(struct rpng_t *rpng);

#ifdef HAVE_ZLIB_DEFLATE
bool rpng_save_image_argb(const char *path, const uint32_t *data,
      unsigned width, unsigned height, unsigned pitch);
bool rpng_save_image_bgr24(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch);
#endif

#ifdef __cplusplus
}
#endif

#endif

