/* Copyright  (C) 2010-2018 The RetroArch team
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
#include <errno.h>

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
         rjpeg_set_buf_ptr((rjpeg_t*)data, (uint8_t*)ptr);
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
      unsigned *width, unsigned *height)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_process_image(
               (rpng_t*)data,
               (void**)buf, len, width, height);
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_process_image((rjpeg_t*)data,
               (void**)buf, len, width, height);
#else
         break;
#endif
      case IMAGE_TYPE_TGA:
#ifdef HAVE_RTGA
         return rtga_process_image((rtga_t*)data,
               (void**)buf, len, width, height);
#else
         break;
#endif
      case IMAGE_TYPE_BMP:
#ifdef HAVE_RBMP
         return rbmp_process_image((rbmp_t*)data,
               (void**)buf, len, width, height);
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         break;
   }

   return 0;
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
         return false;
#else
         break;
#endif
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
