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
#include <string/stdstring.h>

enum image_type_enum image_texture_get_type(const char *path)
{
   /* We are comparing against a fixed list of file
    * extensions, the longest (jpeg) being 4 characters
    * in length. We therefore only need to extract the first
    * 5 characters from the extension of the input path
    * to correctly validate a match */
   const char *ext = NULL;
   char ext_lower[6];

   ext_lower[0] = '\0';

   if (string_is_empty(path))
      return IMAGE_TYPE_NONE;

   /* Get file extension */
   ext = strrchr(path, '.');

   if (!ext || (*(++ext) == '\0'))
      return IMAGE_TYPE_NONE;

   /* Copy and convert to lower case */
   strlcpy(ext_lower, ext, sizeof(ext_lower));
   string_to_lower(ext_lower);

#ifdef HAVE_RPNG
   if (string_is_equal(ext_lower, "png"))
      return IMAGE_TYPE_PNG;
#endif
#ifdef HAVE_RJPEG
   if (string_is_equal(ext_lower, "jpg") ||
       string_is_equal(ext_lower, "jpeg"))
      return IMAGE_TYPE_JPEG;
#endif
#ifdef HAVE_RBMP
   if (string_is_equal(ext_lower, "bmp"))
      return IMAGE_TYPE_BMP;
#endif
#ifdef HAVE_RTGA
   if (string_is_equal(ext_lower, "tga"))
      return IMAGE_TYPE_TGA;
#endif

   return IMAGE_TYPE_NONE;
}

bool image_texture_set_color_shifts(
      unsigned *r_shift, unsigned *g_shift, unsigned *b_shift,
      unsigned *a_shift,
      struct texture_image *out_img
      )
{
   *a_shift             = 24;
   *r_shift             = 16;
   *g_shift             = 8;
   *b_shift             = 0;

   if (out_img->supports_rgba)
   {
      *r_shift = 0;
      *b_shift = 16;
      return true;
   }

   return false;
}

bool image_texture_color_convert(unsigned r_shift,
      unsigned g_shift, unsigned b_shift, unsigned a_shift,
      struct texture_image *out_img)
{
   /* This is quite uncommon. */
   if (a_shift != 24 || r_shift != 16 || g_shift != 8 || b_shift != 0)
   {
      uint32_t i;
      uint32_t num_pixels = out_img->width * out_img->height;
      uint32_t *pixels    = (uint32_t*)out_img->pixels;

      for (i = 0; i < num_pixels; i++)
      {
         uint32_t col = pixels[i];
         uint8_t a    = (uint8_t)(col >> 24);
         uint8_t r    = (uint8_t)(col >> 16);
         uint8_t g    = (uint8_t)(col >>  8);
         uint8_t b    = (uint8_t)(col >>  0);
         /* Explicitly cast these to uint32_t to prevent
          * ASAN runtime error: left shift of 255 by 24 places
          * cannot be represented in type 'int' */
         pixels[i]    = ((uint32_t)a << a_shift) |
                        ((uint32_t)r << r_shift) |
                        ((uint32_t)g << g_shift) |
                        ((uint32_t)b << b_shift);
      }

      return true;
   }

   return false;
}

#ifdef GEKKO

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

static bool image_texture_internal_gx_convert_texture32(
      struct texture_image *image)
{
   unsigned tmp_pitch, width2, i;
   const uint16_t *src = NULL;
   uint16_t *dst       = NULL;
   /* Memory allocation in libogc is extremely primitive so try
    * to avoid gaps in memory when converting by copying over to
    * a temporary buffer first, then converting over into
    * main buffer again. */
   void *tmp           = malloc(image->width
         * image->height * sizeof(uint32_t));

   if (!tmp)
      return false;

   memcpy(tmp, image->pixels, image->width
         * image->height * sizeof(uint32_t));
   tmp_pitch = (image->width * sizeof(uint32_t)) >> 1;

   image->width       &= ~3;
   image->height      &= ~3;
   width2              = image->width << 1;
   src                 = (uint16_t*)tmp;
   dst                 = (uint16_t*)image->pixels;

   for (i = 0; i < image->height; i += 4, dst += 4 * width2)
   {
      GX_BLIT_LINE_32(0)
      GX_BLIT_LINE_32(4)
      GX_BLIT_LINE_32(8)
      GX_BLIT_LINE_32(12)
   }

   free(tmp);
   return true;
}
#endif

static bool image_texture_load_internal(
      enum image_type_enum type,
      void *ptr,
      size_t len,
      struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   int ret;
   bool success = false;
   void *img    = image_transfer_new(type);

   if (!img)
      goto end;

   image_transfer_set_buffer_ptr(img, type, (uint8_t*)ptr, len);

   if (!image_transfer_start(img, type))
      goto end;

   while (image_transfer_iterate(img, type));

   if (!image_transfer_is_valid(img, type))
      goto end;

   do
   {
      ret = image_transfer_process(img, type,
            (uint32_t**)&out_img->pixels, len, &out_img->width,
            &out_img->height);
   } while (ret == IMAGE_PROCESS_NEXT);

   if (ret == IMAGE_PROCESS_ERROR || ret == IMAGE_PROCESS_ERROR_END)
      goto end;

   image_texture_color_convert(r_shift, g_shift, b_shift,
         a_shift, out_img);

#ifdef GEKKO
   if (!image_texture_internal_gx_convert_texture32(out_img))
   {
      image_texture_free(out_img);
      goto end;
   }
#endif

   success = true;

end:
   if (img)
      image_transfer_free(img, type);

   return success;
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
   unsigned r_shift, g_shift, b_shift, a_shift;
   image_texture_set_color_shifts(&r_shift, &g_shift, &b_shift,
      &a_shift, out_img);

   if (type != IMAGE_TYPE_NONE)
   {
      if (image_texture_load_internal(
         type, buffer, buffer_len, out_img,
         a_shift, r_shift, g_shift, b_shift))
      {
         return true;
      }
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
   unsigned r_shift, g_shift, b_shift, a_shift;
   size_t file_len             = 0;
   struct nbio_t      *handle  = NULL;
   void                   *ptr = NULL;
   enum image_type_enum type  = image_texture_get_type(path);

   image_texture_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift, out_img);

   if (type != IMAGE_TYPE_NONE)
   {
      handle = (struct nbio_t*)nbio_open(path, NBIO_READ);
      if (!handle)
         goto error;
      nbio_begin_read(handle);

      while (!nbio_iterate(handle));

      ptr = nbio_get_ptr(handle, &file_len);

      if (!ptr)
         goto error;

      if (image_texture_load_internal(
               type,
               ptr, file_len, out_img,
               a_shift, r_shift, g_shift, b_shift))
         goto success;
   }

error:
   out_img->supports_rgba = false;
   out_img->pixels        = NULL;
   out_img->width         = 0;
   out_img->height        = 0;
   if (handle)
      nbio_free(handle);

   return false;

success:
   if (handle)
      nbio_free(handle);

   return true;
}
