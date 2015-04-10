/* Copyright  (C) 2010-2015 The RetroArch team
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

#include <formats/rpng.h>
#include <file/nbio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef HAVE_IMLIB2
#include <Imlib2.h>
#endif

enum image_process_code
{
   IMAGE_PROCESS_ERROR     = -2,
   IMAGE_PROCESS_ERROR_END = -1,
   IMAGE_PROCESS_NEXT      =  0,
   IMAGE_PROCESS_END       =  1,
};

static bool rpng_nbio_load_image_argb(const char *path, uint32_t **data,
      unsigned *width, unsigned *height)
{
   int retval;
   size_t file_len;
   bool ret = true;
   struct rpng_t *rpng = NULL;
   void *ptr = NULL;
   unsigned val = 0;
   struct nbio_t* handle = (void*)nbio_open(path, NBIO_READ);

   if (!handle)
      goto end;

   ptr  = nbio_get_ptr(handle, &file_len);

   nbio_begin_read(handle);

   while (!nbio_iterate(handle));

   ptr = nbio_get_ptr(handle, &file_len);

   if (!ptr)
   {
      ret = false;
      goto end;
   }

   rpng = (struct rpng_t*)calloc(1, sizeof(struct rpng_t));

   if (!rpng)
   {
      ret = false;
      goto end;
   }

   rpng->buff_data = (uint8_t*)ptr;

   if (!rpng->buff_data)
   {
      ret = false;
      goto end;
   }

   if (!rpng_nbio_load_image_argb_start(rpng))
   {
      ret = false;
      goto end;
   }

   while (rpng_nbio_load_image_argb_iterate(
            rpng->buff_data, rpng, &val))
   {
      rpng->buff_data += val;
   }

#if 0
   fprintf(stderr, "has_ihdr: %d\n", rpng->has_ihdr);
   fprintf(stderr, "has_idat: %d\n", rpng->has_idat);
   fprintf(stderr, "has_iend: %d\n", rpng->has_iend);
#endif

   if (!rpng->has_ihdr || !rpng->has_idat || !rpng->has_iend)
   {
      ret = false;
      goto end;
   }
   
   do
   {
      retval = rpng_nbio_load_image_argb_process(rpng, data, width, height);
   }while(retval == IMAGE_PROCESS_NEXT);

   if (retval == IMAGE_PROCESS_ERROR || retval == IMAGE_PROCESS_ERROR_END)
      ret = false;

end:
   if (handle)
      nbio_free(handle);
   if (rpng)
      rpng_nbio_load_image_free(rpng);
   rpng = NULL;
   if (!ret)
      free(*data);
   return ret;
}

static int test_nonblocking_rpng(const char *in_path)
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

   if (!rpng_nbio_load_image_argb(in_path, &data, &width, &height))
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

static int test_blocking_rpng(const char *in_path)
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

   fprintf(stderr, "Doing nonblocking tests...\n");

   if (test_nonblocking_rpng(in_path) != 0)
   {
      fprintf(stderr, "Nonblocking test failed.\n");
      return -1;
   }

   fprintf(stderr, "Doing blocking tests...\n");

   return test_blocking_rpng(in_path);
}
