/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_test.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef HAVE_IMLIB2
#include <Imlib2.h>
#endif

#include <file/nbio.h>
#include <formats/rpng.h>
#include <formats/image.h>

static bool rpng_load_image_argb(const char *path, uint32_t **data,
      unsigned *width, unsigned *height)
{
   int retval;
   size_t file_len;
   bool              ret = true;
   rpng_t          *rpng = NULL;
   void             *ptr = NULL;
   struct nbio_t* handle = (struct nbio_t*)nbio_open(path, NBIO_READ);

   if (!handle)
      goto end;

   nbio_begin_read(handle);

   while (!nbio_iterate(handle));

   ptr = nbio_get_ptr(handle, &file_len);

   if (!ptr)
   {
      ret = false;
      goto end;
   }

   rpng = rpng_alloc();

   if (!rpng)
   {
      ret = false;
      goto end;
   }

   if (!rpng_set_buf_ptr(rpng, (uint8_t*)ptr, file_len))
   {
      ret = false;
      goto end;
   }

   if (!rpng_start(rpng))
   {
      ret = false;
      goto end;
   }

   while (rpng_iterate_image(rpng));

   if (!rpng_is_valid(rpng))
   {
      ret = false;
      goto end;
   }

   do
   {
      retval = rpng_process_image(rpng,
            (void**)data, file_len, width, height);
   }while(retval == IMAGE_PROCESS_NEXT);

   if (retval == IMAGE_PROCESS_ERROR || retval == IMAGE_PROCESS_ERROR_END)
      ret = false;

end:
   if (handle)
      nbio_free(handle);
   if (rpng)
      rpng_free(rpng);
   rpng = NULL;
   if (!ret)
      free(*data);
   return ret;
}

static int test_rpng(const char *in_path)
{
#ifdef HAVE_IMLIB2
   Imlib_Image img;
   const uint32_t *imlib_data = NULL;
#endif
   const uint32_t test_data[] = {
      0xff000000 | 0x50, 0xff000000 | 0x80,
      0xff000000 | 0x40, 0xff000000 | 0x88,
      0xff000000 | 0x50, 0xff000000 | 0x80,
      0xff000000 | 0x40, 0xff000000 | 0x88,
      0xff000000 | 0xc3, 0xff000000 | 0xd3,
      0xff000000 | 0xc3, 0xff000000 | 0xd3,
      0xff000000 | 0xc3, 0xff000000 | 0xd3,
      0xff000000 | 0xc3, 0xff000000 | 0xd3,
   };
   uint32_t *data = NULL;
   unsigned width = 0;
   unsigned height = 0;

   if (!rpng_save_image_argb("/tmp/test.png", test_data, 4, 4, 16))
      return 1;

   if (!rpng_load_image_argb(in_path, &data, &width, &height))
      return 2;

   fprintf(stderr, "Path: %s.\n", in_path);
   fprintf(stderr, "Got image: %u x %u.\n", width, height);

#if 0
   fprintf(stderr, "\nRPNG:\n");
   for (unsigned h = 0; h < height; h++)
   {
      unsigned w;
      for (w = 0; w < width; w++)
         fprintf(stderr, "[%08x] ", data[h * width + w]);
      fprintf(stderr, "\n");
   }
#endif

#ifdef HAVE_IMLIB2
   /* Validate with imlib2 as well. */
   img = imlib_load_image(in_path);
   if (!img)
      return 4;

   imlib_context_set_image(img);

   width      = imlib_image_get_width();
   height     = imlib_image_get_width();
   imlib_data = imlib_image_get_data_for_reading_only();

#if 0
   fprintf(stderr, "\nImlib:\n");
   for (unsigned h = 0; h < height; h++)
   {
      for (unsigned w = 0; w < width; w++)
         fprintf(stderr, "[%08x] ", imlib_data[h * width + w]);
      fprintf(stderr, "\n");
   }
#endif

   if (memcmp(imlib_data, data, width * height * sizeof(uint32_t)) != 0)
   {
      fprintf(stderr, "Imlib and RPNG differs!\n");
      return 5;
   }
   else
      fprintf(stderr, "Imlib and RPNG are equivalent!\n");

   imlib_free_image();
#endif
   free(data);

   return 0;
}

int main(int argc, char *argv[])
{
   const char *in_path = "/tmp/test.png";

   if (argc > 2)
   {
      fprintf(stderr, "Usage: %s <png file>\n", argv[0]);
      return 1;
   }

   if (argc == 2)
      in_path = argv[1];

   fprintf(stderr, "Doing tests...\n");

   if (test_rpng(in_path) != 0)
   {
      fprintf(stderr, "Test failed.\n");
      return -1;
   }

   return 0;
}
