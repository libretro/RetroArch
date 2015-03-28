/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "file_extract.h"
#include "file_ops.h"
#include <file/file_path.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "hash.h"

/* File backends. Can be fleshed out later, but keep it simple for now.
 * The file is mapped to memory directly (via mmap() or just 
 * plain read_file()).
 */

struct zlib_file_backend
{
   void          *(*open)(const char *path);
   const uint8_t *(*data)(void *handle);
   size_t         (*size)(void *handle);
   void           (*free)(void *handle); /* Closes, unmaps and frees. */
};

#ifndef CENTRAL_FILE_HEADER_SIGNATURE
#define CENTRAL_FILE_HEADER_SIGNATURE 0x02014b50
#endif

#ifndef END_OF_CENTRAL_DIR_SIGNATURE
#define END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

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
   struct stat fds;
   zlib_file_data_t *data = (zlib_file_data_t*)calloc(1, sizeof(*data));

   if (!data)
      return NULL;

   data->fd = open(path, O_RDONLY);

   if (data->fd < 0)
   {
      RARCH_ERR("Failed to open archive: %s (%s).\n",
            path, strerror(errno));
      goto error;
   }

   if (fstat(data->fd, &fds) < 0)
      goto error;

   data->size = fds.st_size;
   if (!data->size)
      return data;

   data->data = mmap(NULL, data->size, PROT_READ, MAP_SHARED, data->fd, 0);
   if (data->data == MAP_FAILED)
   {
      data->data = NULL;
      RARCH_ERR("Failed to mmap() file: %s (%s).\n", path, strerror(errno));
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

   read_from_file = read_file(path, &data->data, &ret);

   if (!read_from_file || ret < 0)
   {
      RARCH_ERR("Failed to open archive: %s.\n",
            path);
      goto error;
   }

   data->size = ret;
   return data;

error:
   zlib_file_free(data);
   return NULL;
}
#endif

static const struct zlib_file_backend zlib_backend = {
   zlib_file_open,
   zlib_file_data,
   zlib_file_size,
   zlib_file_free,
};

static const struct zlib_file_backend *zlib_get_default_file_backend(void)
{
   return &zlib_backend;
}


/* Modified from nall::unzip (higan). */

#undef GOTO_END_ERROR
#define GOTO_END_ERROR() do { \
   RARCH_ERR("ZIP extraction failed at line: %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while(0)

static uint32_t read_le(const uint8_t *data, unsigned size)
{
   unsigned i;
   uint32_t val = 0;

   size *= 8;
   for (i = 0; i < size; i += 8)
      val |= *data++ << i;

   return val;
}

static void *z_stream_new(void)
{
   z_stream *ret = calloc(1, sizeof(z_stream));

   if (inflateInit2(ret, -MAX_WBITS) != Z_OK)
      return NULL;
   return ret;
}

static void z_stream_free(void *data)
{
   z_stream *ret = (z_stream*)data;
   if (!ret)
      return;

   inflateEnd(ret);
   if (ret)
      free(ret);
}

bool zlib_inflate_data_to_file_init(
      zlib_file_handle_t *handle,
      const uint8_t *cdata,  uint32_t csize, uint32_t size)
{
   z_stream *stream = NULL;

   if (!handle)
      return false;

   if (!(handle->stream = (z_stream*)z_stream_new()))
      goto error;

   handle->data = (uint8_t*)malloc(size);

   if (!handle->data)
      goto error;

   stream            = (z_stream*)handle->stream;

   if (!stream)
      goto error;

   stream->next_in   = (uint8_t*)cdata;
   stream->avail_in  = csize;
   stream->next_out  = handle->data;
   stream->avail_out = size;

   return true;

error:
   if (handle->stream)
      z_stream_free(handle->stream);
   if (handle->data)
      free(handle->data);

   return false;
}

/**
 * zlib_inflate_data_to_file:
 * @path                        : filename path of archive.
 * @valid_exts                  : Valid extensions of archive to be parsed. 
 *                                If NULL, allow all.
 * @cdata                       : input data.
 * @csize                       : size of input data.
 * @size                        : output file size
 * @checksum                    : CRC32 checksum from input data.
 *
 * Decompress data to file.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
int zlib_inflate_data_to_file(const char *path, const char *valid_exts,
      const uint8_t *cdata, uint32_t csize, uint32_t size, uint32_t checksum)
{
   z_stream *stream = NULL;
   int ret          = true;
   zlib_file_handle_t handle = {0};

   (void)valid_exts;
   
   if (!zlib_inflate_data_to_file_init(&handle, cdata, csize, size))
      GOTO_END_ERROR();

   stream = (z_stream*)handle.stream;
   
   if (inflate(stream, Z_FINISH) != Z_STREAM_END)
      ret = false;

   z_stream_free(stream);

   if (ret == false)
      goto end;

   handle.real_checksum = crc32_calculate(handle.data, size);
   if (handle.real_checksum != checksum)
      RARCH_WARN("File CRC differs from ZIP CRC. File: 0x%x, ZIP: 0x%x.\n",
            (unsigned)handle.real_checksum, (unsigned)checksum);

   if (!write_file(path, handle.data, size))
      GOTO_END_ERROR();

end:
   if (handle.data)
      free(handle.data);
   return ret;
}

/**
 * zlib_parse_file:
 * @file                        : filename path of archive
 * @valid_exts                  : Valid extensions of archive to be parsed. 
 *                                If NULL, allow all.
 * @file_cb                     : file_cb function pointer
 * @userdata                    : userdata to pass to file_cb function pointer.
 *
 * Low-level file parsing. Enumerates over all files and calls 
 * file_cb with userdata.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool zlib_parse_file(const char *file, const char *valid_exts,
      zlib_file_cb file_cb, void *userdata)
{
   void *handle;
   const uint8_t *footer    = NULL;
   const uint8_t *directory = NULL;
   const uint8_t *data      = NULL;
   ssize_t zip_size = 0;
   bool ret = true;
   const struct zlib_file_backend *backend = zlib_get_default_file_backend();

   if (!backend)
      return false;

   (void)valid_exts;

   handle = backend->open(file);
   if (!handle)
      GOTO_END_ERROR();

   zip_size = backend->size(handle);
   if (zip_size < 22)
      GOTO_END_ERROR();

   data = backend->data(handle);

   footer = data + zip_size - 22;
   for (;; footer--)
   {
      if (footer <= data + 22)
         GOTO_END_ERROR();
      if (read_le(footer, 4) == END_OF_CENTRAL_DIR_SIGNATURE)
      {
         unsigned comment_len = read_le(footer + 20, 2);
         if (footer + 22 + comment_len == data + zip_size)
            break;
      }
   }

   directory = data + read_le(footer + 16, 4);

   for (;;)
   {
      uint32_t checksum, csize, size, offset;
      unsigned cmode, namelength, extralength, commentlength,
               offsetNL, offsetEL;
      char filename[PATH_MAX_LENGTH] = {0};
      const uint8_t *cdata = NULL;
      uint32_t signature = read_le(directory + 0, 4);

      if (signature != CENTRAL_FILE_HEADER_SIGNATURE)
         break;

      cmode         = read_le(directory + 10, 2);
      checksum      = read_le(directory + 16, 4);
      csize         = read_le(directory + 20, 4);
      size          = read_le(directory + 24, 4);

      namelength    = read_le(directory + 28, 2);
      extralength   = read_le(directory + 30, 2);
      commentlength = read_le(directory + 32, 2);

      if (namelength >= PATH_MAX_LENGTH)
         GOTO_END_ERROR();

      memcpy(filename, directory + 46, namelength);

      offset        = read_le(directory + 42, 4);
      offsetNL      = read_le(data + offset + 26, 2);
      offsetEL      = read_le(data + offset + 28, 2);

      cdata = data + offset + 30 + offsetNL + offsetEL;

#if 0
      RARCH_LOG("OFFSET: %u, CSIZE: %u, SIZE: %u.\n", offset + 30 + 
      offsetNL + offsetEL, csize, size);
#endif

      if (!file_cb(filename, valid_exts, cdata, cmode,
               csize, size, checksum, userdata))
         break;

      directory += 46 + namelength + extralength + commentlength;
   }

end:
   if (handle)
      backend->free(handle);
   return ret;
}

struct zip_extract_userdata
{
   char *zip_path;
   const char *extraction_directory;
   size_t zip_path_size;
   struct string_list *ext;
   bool found_content;
};

enum
{
   ZLIB_MODE_UNCOMPRESSED = 0,
   ZLIB_MODE_DEFLATE      = 8,
} zlib_compression_mode;

static int zip_extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t checksum, void *userdata)
{
   struct zip_extract_userdata *data = (struct zip_extract_userdata*)userdata;

   /* Extract first content that matches our list. */
   const char *ext = path_get_extension(name);

   if (ext && string_list_find_elem(data->ext, ext))
   {
      char new_path[PATH_MAX_LENGTH];

      if (data->extraction_directory)
         fill_pathname_join(new_path, data->extraction_directory,
               path_basename(name), sizeof(new_path));
      else
         fill_pathname_resolve_relative(new_path, data->zip_path,
               path_basename(name), sizeof(new_path));

      switch (cmode)
      {
         case ZLIB_MODE_UNCOMPRESSED:
            data->found_content = write_file(new_path, cdata, size);
            return false;
         case ZLIB_MODE_DEFLATE:
            if (zlib_inflate_data_to_file(new_path, valid_exts,
                     cdata, csize, size, checksum))
            {
               strlcpy(data->zip_path, new_path, data->zip_path_size);
               data->found_content = true;
               return 0;
            }
            return 0;

         default:
            return 0;
      }
   }

   return 1;
}

/**
 * zlib_extract_first_content_file:
 * @zip_path                    : filename path to ZIP archive.
 * @zip_path_size               : size of ZIP archive.
 * @valid_exts                  : valid extensions for a content file.
 * @extraction_directory        : the directory to extract temporary
 *                                unzipped content to.
 *
 * Extract first content file from archive.
 *
 * Returns : true (1) on success, otherwise false (0).
 **/
bool zlib_extract_first_content_file(char *zip_path, size_t zip_path_size,
      const char *valid_exts, const char *extraction_directory)
{
   struct string_list *list;
   bool ret = true;
   struct zip_extract_userdata userdata = {0};

   if (!valid_exts)
   {
      RARCH_ERR("Libretro implementation does not have any valid extensions. Cannot unzip without knowing this.\n");
      return false;
   }

   list = string_split(valid_exts, "|");
   if (!list)
      GOTO_END_ERROR();

   userdata.zip_path             = zip_path;
   userdata.zip_path_size        = zip_path_size;
   userdata.extraction_directory = extraction_directory;
   userdata.ext                  = list;

   if (!zlib_parse_file(zip_path, valid_exts, zip_extract_cb, &userdata))
   {
      RARCH_ERR("Parsing ZIP failed.\n");
      GOTO_END_ERROR();
   }

   if (!userdata.found_content)
   {
      RARCH_ERR("Didn't find any content that matched valid extensions for libretro implementation.\n");
      GOTO_END_ERROR();
   }

end:
   if (list)
      string_list_free(list);
   return ret;
}

static int zlib_get_file_list_cb(const char *path, const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size, uint32_t checksum,
      void *userdata)
{
   union string_list_elem_attr attr;
   struct string_list *ext_list = NULL;
   const char *file_ext = NULL;
   struct string_list *list = (struct string_list*)userdata;

   (void)cdata;
   (void)cmode;
   (void)csize;
   (void)size;
   (void)checksum;
   (void)valid_exts;
   (void)file_ext;
   (void)ext_list;

   memset(&attr, 0, sizeof(attr));

   if (valid_exts)
      ext_list = string_split(valid_exts, "|");

   if (ext_list)
   {
      char last_char = ' ';

      /* Checks if this entry is a directory or a file. */
      last_char = path[strlen(path)-1];

      if (last_char == '/' || last_char == '\\' ) /* Skip if directory. */
         goto error;

      file_ext = path_get_extension(path);

      if (!file_ext || 
            !string_list_find_elem_prefix(ext_list, ".", file_ext))
         goto error;

      attr.i = RARCH_COMPRESSED_FILE_IN_ARCHIVE;
      string_list_free(ext_list);
   }

   return string_list_append(list, path, attr);
error:
   string_list_free(ext_list);
   return 0;
}

/**
 * zlib_get_file_list:
 * @path                        : filename path of archive
 *
 * Returns: string listing of files from archive on success, otherwise NULL.
 **/
struct string_list *zlib_get_file_list(const char *path, const char *valid_exts)
{
   struct string_list *list = string_list_new();

   if (!list)
      return NULL;

   if (!zlib_parse_file(path, valid_exts,
            zlib_get_file_list_cb, list))
   {
      RARCH_ERR("Parsing ZIP failed.\n");
      string_list_free(list);
      return NULL;
   }

   return list;
}
