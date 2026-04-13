/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_transfer.c).
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>

#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#ifdef HAVE_RJPEG
#include <formats/rjpeg.h>
#endif
#ifdef HAVE_RTGA
#include <formats/rtga.h>
#endif
#ifdef HAVE_RBMP
#include <formats/rbmp.h>
#endif

#include <formats/image.h>

void image_transfer_free(void *data, enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         rtga_free((rtga_t*)data);
#endif
         break;
      case IMAGE_TYPE_PNG:
         {
#ifdef HAVE_RPNG
            rpng_t *rpng = (rpng_t*)data;
            if (rpng)
               rpng_free(rpng);
#endif
         }
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_free((rjpeg_t*)data);
#endif
         break;
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         rbmp_free((rbmp_t*)data);
#endif
         break;
      case IMAGE_TYPE_NONE:
         break;
   }
}

void *image_transfer_new(enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return rtga_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         return rbmp_alloc();
#else
         break;
#endif
      default:
         break;
   }

   return NULL;
}

bool image_transfer_start(void *data, enum image_type_enum type)
{

   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         if (!rpng_start((rpng_t*)data))
            break;
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         if (!rjpeg_start((rjpeg_t*)data))
            break;
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
         return true;
      case IMAGE_TYPE_NONE:
         break;
   }

   return false;
}

bool image_transfer_is_valid(
      void *data,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_is_valid((rpng_t*)data);
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_is_valid((rjpeg_t*)data);
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
         return true;
      case IMAGE_TYPE_NONE:
         break;
   }

   return false;
}

void image_transfer_set_buffer_ptr(
      void *data,
      enum image_type_enum type,
      void *ptr,
      size_t len)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_set_buf_ptr((rpng_t*)data, (uint8_t*)ptr, len);
#endif
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_set_buf_ptr((rjpeg_t*)data, (uint8_t*)ptr, len);
#endif
         break;
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         rtga_set_buf_ptr((rtga_t*)data, (uint8_t*)ptr);
#endif
         break;
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         rbmp_set_buf_ptr((rbmp_t*)data, (uint8_t*)ptr);
#endif
         break;
      case IMAGE_TYPE_NONE:
         break;
   }
}

int image_transfer_process(
      void *data,
      enum image_type_enum type,
      uint32_t **buf, size_t len,
      unsigned *width, unsigned *height,
      bool supports_rgba)
{
   int ret = 0;

   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         ret = rpng_process_image(
               (rpng_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         ret = rjpeg_process_image((rjpeg_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         ret = rtga_process_image((rtga_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         ret = rbmp_process_image((rbmp_t*)data,
               (void**)buf, len, width, height, supports_rgba);
         break;
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         break;
   }

#ifdef GEKKO
   /* Convert from linear ARGB to the Wii's tiled texture format.
    * Applied once when decoding finishes (IMAGE_PROCESS_END),
    * not during intermediate iterations. */
   if (ret == IMAGE_PROCESS_END && *buf && *width && *height)
   {
      unsigned tmp_pitch, width2, i;
      const uint16_t *src = NULL;
      uint16_t *dst       = NULL;
      void *tmp           = malloc((*width) * (*height) * sizeof(uint32_t));

      if (!tmp)
         return IMAGE_PROCESS_ERROR;

      memcpy(tmp, *buf, (*width) * (*height) * sizeof(uint32_t));
      tmp_pitch = ((*width) * sizeof(uint32_t)) >> 1;

      *width  &= ~3;
      *height &= ~3;
      width2   = (*width) << 1;
      src      = (const uint16_t*)tmp;
      dst      = (uint16_t*)*buf;

      for (i = 0; i < *height; i += 4, dst += 4 * width2)
      {
#define GX_BLIT_LINE_32(off) \
         { \
            unsigned x; \
            const uint16_t *tmp_src = src; \
            uint16_t       *tmp_dst = dst; \
            for (x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
            { \
               tmp_dst[  0 + off] = tmp_src[0]; \
               tmp_dst[ 16 + off] = tmp_src[1]; \
               tmp_dst[  1 + off] = tmp_src[2]; \
               tmp_dst[ 17 + off] = tmp_src[3]; \
               tmp_dst[  2 + off] = tmp_src[4]; \
               tmp_dst[ 18 + off] = tmp_src[5]; \
               tmp_dst[  3 + off] = tmp_src[6]; \
               tmp_dst[ 19 + off] = tmp_src[7]; \
            } \
            src += tmp_pitch; \
         }
         GX_BLIT_LINE_32(0)
         GX_BLIT_LINE_32(4)
         GX_BLIT_LINE_32(8)
         GX_BLIT_LINE_32(12)
#undef GX_BLIT_LINE_32
      }

      free(tmp);
   }
#endif

   return ret;
}

bool image_transfer_iterate(void *data, enum image_type_enum type)
{

   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         if (!rpng_iterate_image((rpng_t*)data))
            return false;
#endif
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         if (!rjpeg_iterate_image((rjpeg_t*)data))
            return false;
#endif
         break;
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return false;
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
         return false;
      case IMAGE_TYPE_NONE:
         return false;
   }

   return true;
}
