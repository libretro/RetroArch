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

static bool write_header_bmp(RFILE *file, unsigned width, unsigned height, bool is32bpp)
{
   unsigned line_size  = (width * (is32bpp?4:3) + 3) & ~3;
   unsigned size       = line_size * height + 54;
   unsigned size_array = line_size * height;
   uint8_t header[54];

   /* Generic BMP stuff. */

   /* signature */
   header[0] = 'B';
   header[1] = 'M';
   /* file size */
   header[2] = (uint8_t)(size >> 0);
   header[3] = (uint8_t)(size >> 8);
   header[4] = (uint8_t)(size >> 16);
   header[5] = (uint8_t)(size >> 24);
   /* reserved */
   header[6] = 0;
   header[7] = 0;
   header[8] = 0;
   header[9] = 0;
   /* offset */
   header[10] = 54;
   header[11] = 0;
   header[12] = 0;
   header[13] = 0;
   /* DIB size */
   header[14] = 40;
   header[15] = 0;
   header[16] = 0;
   header[17] = 0;
   /* Width */
   header[18] = (uint8_t)(width >> 0);
   header[19] = (uint8_t)(width >> 8);
   header[20] = (uint8_t)(width >> 16);
   header[21] = (uint8_t)(width >> 24);
   /* Height */
   header[22] = (uint8_t)(height >> 0);
   header[23] = (uint8_t)(height >> 8);
   header[24] = (uint8_t)(height >> 16);
   header[25] = (uint8_t)(height >> 24);
   /* Color planes */
   header[26] = 1;
   header[27] = 0;
   /* Bits per pixel */
   header[28] = is32bpp?32:24;
   header[29] = 0;
   /* Compression method */
   header[30] = 0;
   header[31] = 0;
   header[32] = 0;
   header[33] = 0;
   /* Image data size */
   header[34] = (uint8_t)(size_array >> 0);
   header[35] = (uint8_t)(size_array >> 8);
   header[36] = (uint8_t)(size_array >> 16);
   header[37] = (uint8_t)(size_array >> 24);
   /* Horizontal resolution */
   header[38] = 19;
   header[39] = 11;
   header[40] = 0;
   header[41] = 0;
   /* Vertical resolution */
   header[42] = 19;
   header[43] = 11;
   header[44] = 0;
   header[45] = 0;
   /* Palette size */
   header[46] = 0;
   header[47] = 0;
   header[48] = 0;
   header[49] = 0;
   /* Important color count */
   header[50] = 0;
   header[51] = 0;
   header[52] = 0;
   header[53] = 0;

   return retro_fwrite(file, header, sizeof(header)) == sizeof(header);
}

static void dump_line_565_to_24(uint8_t *line, const uint16_t *src, unsigned width)
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

static void dump_line_32_to_24(uint8_t *line, const uint32_t *src, unsigned width)
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
      int width, int height, int pitch, enum rbmp_source_type type)
{
   uint8_t *line;
   size_t line_size;
   int j;
   int bytes_per_pixel = (type==RBMP_SOURCE_TYPE_ARGB8888?4:3);
   union
   {
      const uint8_t *u8;
      const uint16_t *u16;
      const uint32_t *u32;
   } u;

   u.u8      = (const uint8_t*)frame + (height-1) * pitch;
   line_size = (width * bytes_per_pixel + 3) & ~3;

   if (type == RBMP_SOURCE_TYPE_BGR24)
   {
      /* BGR24 byte order input matches output. Can directly copy, but... need to make sure we pad it. */
      uint32_t zeros = 0;
      int pad = line_size-pitch;
      for (j = height-1; j >= 0; j--, u.u8 -= pitch)
      {
         retro_fwrite(file, u.u8, pitch);
         if(pad != 0) retro_fwrite(file, &zeros, pad);
      }
      return;
   }
   else if(type == RBMP_SOURCE_TYPE_ARGB8888)
   {
      /* ARGB8888 byte order input matches output. Can directly copy. */
      for (j = height-1; j >= 0; j--, u.u8 -= pitch)
      {
         retro_fwrite(file, u.u8, line_size);
      }
      return;
   }

   /* allocate line buffer, and initialize the final four bytes to zero, for deterministic padding */
   line = (uint8_t*)malloc(line_size);
   if (!line) return;
   *(uint32_t*)(line + line_size - 4) = 0;

   if (type == RBMP_SOURCE_TYPE_XRGB888)
   {
      for (j = height-1; j >= 0; j--, u.u8 -= pitch)
      {
         dump_line_32_to_24(line, u.u32, width);
         retro_fwrite(file, line, line_size);
      }
   }
   else /* type == RBMP_SOURCE_TYPE_RGB565 */
   {
      for (j = height-1; j >= 0; j--, u.u8 -= pitch)
      {
         dump_line_565_to_24(line, u.u16, width);
         retro_fwrite(file, line, line_size);
      }
   }

}

bool rbmp_save_image(const char *filename, const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, enum rbmp_source_type type)
{
   bool ret;
   RFILE *file = retro_fopen(filename, RFILE_MODE_WRITE, -1);
   if (!file)
      return false;

   ret = write_header_bmp(file, width, height, type==RBMP_SOURCE_TYPE_ARGB8888);

   if (ret)
      dump_content(file, frame, width, height, pitch, type);

   retro_fclose(file);

   return ret;
}
