/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_texture.c).
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
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <boolean.h>
#include <formats/image.h>
#include <file/nbio.h>

enum image_type_enum image_texture_get_type(const char *path)
{
   /* We are comparing against a fixed list of file
    * extensions, the longest (jpeg) being 4 characters
    * in length. We therefore only need to extract the first
    * 5 characters from the extension of the input path
    * to correctly validate a match */
   size_t len;
   const char *ext = NULL;
   if (!path || *path == '\0')
      return IMAGE_TYPE_NONE;

   ext = strrchr(path, '.');
   if (!ext || (*(++ext) == '\0'))
      return IMAGE_TYPE_NONE;

   len = strlen(ext);

   /* All supported extensions are 3 or 4 characters */
   if (len < 3 || len > 4)
      return IMAGE_TYPE_NONE;

   /* Compare with inline lowering — avoids copy + tolower pass */
   switch (len)
   {
      case 3:
#ifdef HAVE_RPNG
         if ((ext[0] | 0x20) == 'p' &&
             (ext[1] | 0x20) == 'n' &&
             (ext[2] | 0x20) == 'g')
            return IMAGE_TYPE_PNG;
#endif
#ifdef HAVE_RJPEG
         if ((ext[0] | 0x20) == 'j' &&
             (ext[1] | 0x20) == 'p' &&
             (ext[2] | 0x20) == 'g')
            return IMAGE_TYPE_JPEG;
#endif
#ifdef HAVE_RBMP
         if ((ext[0] | 0x20) == 'b' &&
             (ext[1] | 0x20) == 'm' &&
             (ext[2] | 0x20) == 'p')
            return IMAGE_TYPE_BMP;
#endif
#ifdef HAVE_RTGA
         if ((ext[0] | 0x20) == 't' &&
             (ext[1] | 0x20) == 'g' &&
             (ext[2] | 0x20) == 'a')
            return IMAGE_TYPE_TGA;
#endif
         break;

      case 4:
#ifdef HAVE_RJPEG
         if ((ext[0] | 0x20) == 'j' &&
             (ext[1] | 0x20) == 'p' &&
             (ext[2] | 0x20) == 'e' &&
             (ext[3] | 0x20) == 'g')
            return IMAGE_TYPE_JPEG;
#endif
         break;
   }

   return IMAGE_TYPE_NONE;
}

static bool image_texture_load_internal(
      enum image_type_enum type,
      void *ptr,
      size_t len,
      struct texture_image *out_img)
{
   int ret;
   void *img    = image_transfer_new(type);

   if (!img)
      return false;

   image_transfer_set_buffer_ptr(img, type, (uint8_t*)ptr, len);

   if (!image_transfer_start(img, type))
   {
      image_transfer_free(img, type);
      return false;
   }

   while (image_transfer_iterate(img, type));

   if (!image_transfer_is_valid(img, type))
   {
      image_transfer_free(img, type);
      return false;
   }

   do
   {
      ret = image_transfer_process(img, type,
            (uint32_t**)&out_img->pixels, len, &out_img->width,
            &out_img->height, out_img->supports_rgba);
   } while (ret == IMAGE_PROCESS_NEXT);

   if (ret == IMAGE_PROCESS_ERROR || ret == IMAGE_PROCESS_ERROR_END)
   {
      image_transfer_free(img, type);
      return false;
   }

   image_transfer_free(img, type);
   return true;
}

void image_texture_free(struct texture_image *img)
{
   if (!img)
      return;

   if (img->pixels)
      free(img->pixels);
   img->width  = 0;
   img->height = 0;
   img->pixels = NULL;
}

bool image_texture_load_buffer(struct texture_image *out_img,
   enum image_type_enum type, void *buffer, size_t buffer_len)
{
   if (type != IMAGE_TYPE_NONE)
   {
      if (image_texture_load_internal(
         type, buffer, buffer_len, out_img))
         return true;
   }

   out_img->supports_rgba = false;
   out_img->pixels = NULL;
   out_img->width = 0;
   out_img->height = 0;

   return false;
}

bool image_texture_load(struct texture_image *out_img,
      const char *path)
{
   enum image_type_enum type  = image_texture_get_type(path);

   if (type != IMAGE_TYPE_NONE)
   {
      struct nbio_t *handle = (struct nbio_t*)
         nbio_open(path, NBIO_READ);
      if (handle)
      {
         void *ptr       = NULL;
         size_t file_len = 0;

         /* Fast path: collapses begin_read + iterate-loop + get_ptr
          * into a single call. For mmap this is zero-copy (instant),
          * for AIO a single blocking syscall, for stdio one fread. */
         if ((ptr = nbio_load_entire(handle, &file_len)))
         {
            if (image_texture_load_internal(
                     type,
                     ptr, file_len, out_img))
            {
               nbio_free(handle);
               return true;
            }
         }
         nbio_free(handle);
      }
   }

   out_img->supports_rgba = false;
   out_img->pixels        = NULL;
   out_img->width         = 0;
   out_img->height        = 0;

   return false;
}
