/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng.c).
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

#ifndef _RPNG_DECODE_H
#define _RPNG_DECODE_H

enum png_process_code
{
   PNG_PROCESS_ERROR     = -2,
   PNG_PROCESS_ERROR_END = -1,
   PNG_PROCESS_NEXT      =  0,
   PNG_PROCESS_END       =  1
};

enum png_chunk_type png_chunk_type(const struct png_chunk *chunk);

bool png_process_ihdr(struct png_ihdr *ihdr);

int png_reverse_filter_iterate(struct rpng_t *rpng,
      uint32_t **data);

bool rpng_load_image_argb_process_init(struct rpng_t *rpng,
      uint32_t **data, unsigned *width, unsigned *height);

#endif
