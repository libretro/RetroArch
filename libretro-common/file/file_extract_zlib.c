/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_extract_zlib.c).
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

#ifdef HAVE_MMAP
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#endif

#include <compat/zlib.h>
#include <file/file_extract.h>
#include <retro_file.h>

#ifdef HAVE_MMAP
typedef struct
{
   int fd;
   void *data;
   size_t size;
} zlib_file_data_t;

static void zlib_file_free(void *handle)
{
   zlib_file_data_t *data = (zlib_file_data_t*)handle;

   if (!data)
      return;

   if (data->data)
      munmap(data->data, data->size);
   if (data->fd >= 0)
      close(data->fd);
   free(data);
}

static const uint8_t *zlib_file_data(void *handle)
{
   zlib_file_data_t *data = (zlib_file_data_t*)handle;
   if (!data)
      return NULL;
   return (const uint8_t*)data->data;
}

static size_t zlib_file_size(void *handle)
{
   zlib_file_data_t *data = (zlib_file_data_t*)handle;
   if (!data)
      return 0;
   return data->size;
}

static void *zlib_file_open(const char *path)
{
   zlib_file_data_t *data = (zlib_file_data_t*)calloc(1, sizeof(*data));

   if (!data)
      return NULL;

   data->fd = open(path, O_RDONLY);

   /* Failed to open archive. */
   if (data->fd < 0)
      goto error;

   data->size = path_get_size(path);
   if (!data->size)
      return data;

   data->data = mmap(NULL, data->size, PROT_READ, MAP_SHARED, data->fd, 0);
   if (data->data == MAP_FAILED)
   {
      data->data = NULL;

      /* Failed to mmap() file */
      goto error;
   }

   return data;

error:
   zlib_file_free(data);
   return NULL;
}
#else
typedef struct
{
   void *data;
   size_t size;
} zlib_file_data_t;

static void zlib_file_free(void *handle)
{
   zlib_file_data_t *data = (zlib_file_data_t*)handle;
   if (!data)
      return;
   free(data->data);
   free(data);
}

static const uint8_t *zlib_file_data(void *handle)
{
   zlib_file_data_t *data = (zlib_file_data_t*)handle;
   if (!data)
      return NULL;
   return (const uint8_t*)data->data;
}

static size_t zlib_file_size(void *handle)
{
   zlib_file_data_t *data = (zlib_file_data_t*)handle;
   if (!data)
      return 0;
   return data->size;
}

static void *zlib_file_open(const char *path)
{
   ssize_t ret = -1;
   bool read_from_file = false;
   zlib_file_data_t *data = (zlib_file_data_t*)calloc(1, sizeof(*data));

   if (!data)
      return NULL;

   read_from_file = retro_read_file(path, &data->data, &ret);

   if (!read_from_file || ret < 0)
   {
      /* Failed to open archive. */
      goto error;
   }

   data->size = ret;
   return data;

error:
   zlib_file_free(data);
   return NULL;
}
#endif

const struct zlib_file_backend zlib_backend = {
   zlib_file_open,
   zlib_file_data,
   zlib_file_size,
   zlib_file_free,
};
