/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbmp_encode.c).
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

#include <stdlib.h>
#include <string.h>

#include <retro_file.h>
#include <formats/rbmp.h>

static bool write_header_bmp(RFILE *file, unsigned width, unsigned height)
{
   unsigned line_size  = (width * 3 + 3) & ~3;
   unsigned size       = line_size * height + 54;
   unsigned size_array = line_size * height;

   /* Generic BMP stuff. */
   const uint8_t header[] = {
      /* signature */
      'B', 'M',
      /* file size */
      (uint8_t)(size >> 0), (uint8_t)(size >> 8),
      (uint8_t)(size >> 16), (uint8_t)(size >> 24),
      /* reserved */
      0, 0, 0, 0,
      /* offset */
      54, 0, 0, 0,
      /* DIB size */
      40, 0, 0, 0,
      /* width */
      (uint8_t)(width >> 0), (uint8_t)(width >> 8),
      (uint8_t)(width >> 16), (uint8_t)(width >> 24),
      /* height */
      (uint8_t)(height >> 0), (uint8_t)(height >> 8),
      (uint8_t)(height >> 16), (uint8_t)(height >> 24),
      /* color planes */
      1, 0,
      /* bits per pixel */
      24, 0,
      /* compression method */
      0, 0, 0, 0,
      /* image data size */
      (uint8_t)(size_array >> 0), (uint8_t)(size_array >> 8),
      (uint8_t)(size_array >> 16), (uint8_t)(size_array >> 24),
      /* horizontal resolution */
      19, 11, 0, 0,
      /* vertical resolution */
      19, 11, 0, 0,
      /* palette size */
      0, 0, 0, 0,
      /* important color count */
      0, 0, 0, 0
   };

   return retro_fwrite(file, header, sizeof(header)) == sizeof(header);
}

static void dump_lines_file(RFILE *file, uint8_t **lines,
      size_t line_size, unsigned height)
{
   unsigned i;

   for (i = 0; i < height; i++)
      retro_fwrite(file, lines[i], line_size);
}

static void dump_line_bgr(uint8_t *line, const uint8_t *src, unsigned width)
{
   memcpy(line, src, width * 3);
}

static void dump_line_16(uint8_t *line, const uint16_t *src, unsigned width)
{
   unsigned i;

   for (i = 0; i < width; i++)
   {
      uint16_t pixel = *src++;
      uint8_t b = (pixel >>  0) & 0x1f;
      uint8_t g = (pixel >>  5) & 0x3f;
      uint8_t r = (pixel >> 11) & 0x1f;
      *line++   = (b << 3) | (b >> 2);
      *line++   = (g << 2) | (g >> 4);
      *line++   = (r << 3) | (r >> 2);
   }
}

static void dump_line_32(uint8_t *line, const uint32_t *src, unsigned width)
{
   unsigned i;

   for (i = 0; i < width; i++)
   {
      uint32_t pixel = *src++;
      *line++ = (pixel >>  0) & 0xff;
      *line++ = (pixel >>  8) & 0xff;
      *line++ = (pixel >> 16) & 0xff;
   }
}

static void dump_content(RFILE *file, const void *frame,
      int width, int height, int pitch, bool bgr24, bool xrgb8888)
{
   size_t line_size;
   int i, j;
   union
   {
      const uint8_t *u8;
      const uint16_t *u16;
      const uint32_t *u32;
   } u;
   uint8_t **lines = (uint8_t**)calloc(height, sizeof(uint8_t*));

   if (!lines)
      return;

   u.u8      = (const uint8_t*)frame;
   line_size = (width * 3 + 3) & ~3;

   for (i = 0; i < height; i++)
   {
      lines[i] = (uint8_t*)calloc(1, line_size);
      if (!lines[i])
         goto end;
   }

   if (bgr24) /* BGR24 byte order. Can directly copy. */
   {
      for (j = 0; j < height; j++, u.u8 += pitch)
         dump_line_bgr(lines[j], u.u8, width);
   }
   else if (xrgb8888)
   {
      for (j = 0; j < height; j++, u.u8 += pitch)
         dump_line_32(lines[j], u.u32, width);
   }
   else /* RGB565 */
   {
      for (j = 0; j < height; j++, u.u8 += pitch)
         dump_line_16(lines[j], u.u16, width);
   }

   dump_lines_file(file, lines, line_size, height);

end:
   for (i = 0; i < height; i++)
      free(lines[i]);
   free(lines);
}

bool rbmp_save_image(const char *filename, const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, bool bgr24, bool xrgb8888)
{
   bool ret;
   RFILE *file = retro_fopen(filename, RFILE_MODE_WRITE, -1);
   if (!file)
      return false;

   ret = write_header_bmp(file, width, height);

   if (ret)
      dump_content(file, frame, width, height, pitch, bgr24, xrgb8888);

   retro_fclose(file);

   return ret;
}
