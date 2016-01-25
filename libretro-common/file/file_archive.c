/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_archive.c).
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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MMAP
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#endif

#include <compat/strl.h>
#include <file/file_archive.h>
#include <file/file_path.h>
#include <retro_file.h>
#include <retro_stat.h>
#include <retro_miscellaneous.h>
#include <string/string_list.h>

#ifndef CENTRAL_FILE_HEADER_SIGNATURE
#define CENTRAL_FILE_HEADER_SIGNATURE 0x02014b50
#endif

#ifndef END_OF_CENTRAL_DIR_SIGNATURE
#define END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
#endif

struct zip_extract_userdata
{
   char *zip_path;
   char *first_extracted_file_path;
   const char *extraction_directory;
   size_t zip_path_size;
   struct string_list *ext;
   bool found_content;
};

enum file_archive_compression_mode
{
   ZLIB_MODE_UNCOMPRESSED = 0,
   ZLIB_MODE_DEFLATE      = 8
};

typedef struct
{
#ifdef HAVE_MMAP
   int fd;
#endif
   void *data;
   size_t size;
} file_archive_file_data_t;

#ifdef HAVE_MMAP
/* Closes, unmaps and frees. */
static void file_archive_free(void *handle)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)handle;

   if (!data)
      return;

   if (data->data)
      munmap(data->data, data->size);
   if (data->fd >= 0)
      close(data->fd);
   free(data);
}

static const uint8_t *file_archive_data(void *handle)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)handle;
   if (!data)
      return NULL;
   return (const uint8_t*)data->data;
}

static size_t file_archive_size(void *handle)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)handle;
   if (!data)
      return 0;
   return data->size;
}

static void *file_archive_open(const char *path)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)calloc(1, sizeof(*data));

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
   file_archive_free(data);
   return NULL;
}
#else

/* Closes, unmaps and frees. */
static void file_archive_free(void *handle)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)handle;
   if (!data)
      return;
   free(data->data);
   free(data);
}

static const uint8_t *file_archive_data(void *handle)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)handle;
   if (!data)
      return NULL;
   return (const uint8_t*)data->data;
}

static size_t file_archive_size(void *handle)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)handle;
   if (!data)
      return 0;
   return data->size;
}

static void *file_archive_open(const char *path)
{
   ssize_t ret            = -1;
   bool read_from_file    = false;
   file_archive_file_data_t *data = (file_archive_file_data_t*)
      calloc(1, sizeof(*data));

   if (!data)
      return NULL;

   read_from_file = retro_read_file(path, &data->data, &ret);

   /* Failed to open archive? */
   if (!read_from_file || ret < 0)
      goto error;

   data->size = ret;
   return data;

error:
   file_archive_free(data);
   return NULL;
}
#endif

static int file_archive_get_file_list_cb(
      const char *path,
      const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode,
      uint32_t csize,
      uint32_t size,
      uint32_t checksum,
      void *userdata)
{
   union string_list_elem_attr attr;
   struct string_list *ext_list = NULL;
   const char *file_ext         = NULL;
   struct string_list *list     = (struct string_list*)userdata;

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
      /* Checks if this entry is a directory or a file. */
      char last_char = path[strlen(path)-1];

      /* Skip if directory. */
      if (last_char == '/' || last_char == '\\' )
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

static int file_archive_extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t checksum, void *userdata)
{
   const char *ext                   = path_get_extension(name);
   struct zip_extract_userdata *data = (struct zip_extract_userdata*)userdata;

   /* Extract first content that matches our list. */
   if (ext && string_list_find_elem(data->ext, ext))
   {
      char new_path[PATH_MAX_LENGTH] = {0};

      if (data->extraction_directory)
         fill_pathname_join(new_path, data->extraction_directory,
               path_basename(name), sizeof(new_path));
      else
         fill_pathname_resolve_relative(new_path, data->zip_path,
               path_basename(name), sizeof(new_path));

      data->first_extracted_file_path = strdup(new_path);
      data->found_content             = file_archive_perform_mode(new_path,
            valid_exts, cdata, cmode, csize, size,
            0, NULL);
      return 0;
   }

   return 1;
}

static uint32_t read_le(const uint8_t *data, unsigned size)
{
   unsigned i;
   uint32_t val = 0;

   size *= 8;
   for (i = 0; i < size; i += 8)
      val |= (uint32_t)*data++ << i;

   return val;
}

static int file_archive_parse_file_iterate_step_internal(
      file_archive_transfer_t *state, char *filename,
      const uint8_t **cdata,
      unsigned *cmode, uint32_t *size, uint32_t *csize,
      uint32_t *checksum, unsigned *payback)
{
   uint32_t offset;
   uint32_t namelength, extralength, commentlength,
            offsetNL, offsetEL;
   uint32_t signature = read_le(state->directory + 0, 4);

   if (signature != CENTRAL_FILE_HEADER_SIGNATURE)
      return 0;

   *cmode         = read_le(state->directory + 10, 2);
   *checksum      = read_le(state->directory + 16, 4);
   *csize         = read_le(state->directory + 20, 4);
   *size          = read_le(state->directory + 24, 4);

   namelength     = read_le(state->directory + 28, 2);
   extralength    = read_le(state->directory + 30, 2);
   commentlength  = read_le(state->directory + 32, 2);

   if (namelength >= PATH_MAX_LENGTH)
      return -1;

   memcpy(filename, state->directory + 46, namelength);

   offset         = read_le(state->directory + 42, 4);
   offsetNL       = read_le(state->data + offset + 26, 2);
   offsetEL       = read_le(state->data + offset + 28, 2);

   *cdata         = state->data + offset + 30 + offsetNL + offsetEL;

   *payback       = 46 + namelength + extralength + commentlength;

   return 1;
}

static int file_archive_parse_file_iterate_step(file_archive_transfer_t *state,
      const char *valid_exts, void *userdata, file_archive_file_cb file_cb)
{
   const uint8_t *cdata = NULL;
   uint32_t checksum    = 0;
   uint32_t size        = 0;
   uint32_t csize       = 0;
   unsigned cmode       = 0;
   unsigned payload     = 0;
   char filename[PATH_MAX_LENGTH] = {0};
   int ret = file_archive_parse_file_iterate_step_internal(state, filename,
         &cdata, &cmode, &size, &csize,
         &checksum, &payload);

   if (ret != 1)
      return ret;

#if 0
   RARCH_LOG("OFFSET: %u, CSIZE: %u, SIZE: %u.\n", offset + 30 + 
         offsetNL + offsetEL, csize, size);
#endif

   if (!file_cb(filename, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   state->directory += payload;

   return 1;
}

static int file_archive_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   state->backend = file_archive_get_default_file_backend();

   if (!state->backend)
      return -1;

   state->handle = file_archive_open(file);
   if (!state->handle)
      return -1;

   state->zip_size = file_archive_size(state->handle);
   if (state->zip_size < 22)
      return -1;

   state->data   = file_archive_data(state->handle);
   state->footer = state->data + state->zip_size - 22;

   for (;; state->footer--)
   {
      if (state->footer <= state->data + 22)
         return -1;
      if (read_le(state->footer, 4) == END_OF_CENTRAL_DIR_SIGNATURE)
      {
         unsigned comment_len = read_le(state->footer + 20, 2);
         if (state->footer + 22 + comment_len == state->data + state->zip_size)
            break;
      }
   }

   state->directory = state->data + read_le(state->footer + 16, 4);

   return 0;
}

/**
 * file_archive_decompress_data_to_file:
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
static int file_archive_decompress_data_to_file(
      file_archive_file_handle_t *handle,
      int ret,
      const char *path,
      const char *valid_exts,
      const uint8_t *cdata,
      uint32_t csize,
      uint32_t size,
      uint32_t checksum)
{
   if (handle)
   {
      handle->backend->stream_free(handle->stream);
      free(handle->stream);
   }

   if (!handle || ret == -1)
   {
      ret = 0;
      goto end;
   }

   handle->real_checksum = handle->backend->stream_crc_calculate(
         0, handle->data, size);

#if 0
   if (handle->real_checksum != checksum)
   {
      /* File CRC difers from ZIP CRC. */
      printf("File CRC differs from ZIP CRC. File: 0x%x, ZIP: 0x%x.\n",
            (unsigned)handle->real_checksum, (unsigned)checksum);
   }
#endif

   if (!retro_write_file(path, handle->data, size))
   {
      ret = false;
      goto end;
   }

end:
   if (handle->data)
      free(handle->data);
   return ret;
}

int file_archive_parse_file_iterate(
      file_archive_transfer_t *state,
      bool *returnerr,
      const char *file,
      const char *valid_exts,
      file_archive_file_cb file_cb,
      void *userdata)
{
   if (!state)
      return -1;

   switch (state->type)
   {
      case ZLIB_TRANSFER_NONE:
         break;
      case ZLIB_TRANSFER_INIT:
         if (file_archive_parse_file_init(state, file) == 0)
            state->type = ZLIB_TRANSFER_ITERATE;
         else
            state->type = ZLIB_TRANSFER_DEINIT_ERROR;
         break;
      case ZLIB_TRANSFER_ITERATE:
         {
            int ret = file_archive_parse_file_iterate_step(state,
                  valid_exts, userdata, file_cb);
            if (ret != 1)
               state->type = ZLIB_TRANSFER_DEINIT;
            if (ret == -1)
               state->type = ZLIB_TRANSFER_DEINIT_ERROR;
         }
         break;
      case ZLIB_TRANSFER_DEINIT_ERROR:
         *returnerr = false;
      case ZLIB_TRANSFER_DEINIT:
         if (state->handle)
            file_archive_free(state->handle);
         state->handle = NULL;
         break;
   }

   if (state->type == ZLIB_TRANSFER_DEINIT ||
         state->type == ZLIB_TRANSFER_DEINIT_ERROR)
      return -1;

   return 0;
}

void file_archive_parse_file_iterate_stop(file_archive_transfer_t *state)
{
   if (!state || !state->handle)
      return;

   state->type = ZLIB_TRANSFER_DEINIT;
   file_archive_parse_file_iterate(state, NULL, NULL, NULL, NULL, NULL);
}

/**
 * file_archive_parse_file:
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
bool file_archive_parse_file(const char *file, const char *valid_exts,
      file_archive_file_cb file_cb, void *userdata)
{
   file_archive_transfer_t state = {0};
   bool returnerr        = true;

   state.type = ZLIB_TRANSFER_INIT;

   for (;;)
   {
      int ret = file_archive_parse_file_iterate(&state, &returnerr, file,
            valid_exts, file_cb, userdata);

      if (ret != 0)
         break;
   }

   return returnerr;
}

int file_archive_parse_file_progress(file_archive_transfer_t *state)
{
   /* FIXME: this estimate is worse than before */
   ptrdiff_t delta = state->directory - state->data;
   return delta * 100 / state->zip_size;
}

/**
 * file_archive_extract_first_content_file:
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
bool file_archive_extract_first_content_file(
      char *zip_path,
      size_t zip_path_size,
      const char *valid_exts,
      const char *extraction_directory,
      char *out_path, size_t len)
{
   struct string_list *list             = NULL;
   bool ret                             = true;
   struct zip_extract_userdata userdata = {0};

   if (!valid_exts)
   {
      /* Libretro implementation does not have any valid extensions.
       * Cannot unzip without knowing this. */
      return false;
   }

   list = string_split(valid_exts, "|");
   if (!list)
   {
      ret = false;
      goto end;
   }

   userdata.zip_path             = zip_path;
   userdata.zip_path_size        = zip_path_size;
   userdata.extraction_directory = extraction_directory;
   userdata.ext                  = list;

   if (!file_archive_parse_file(zip_path, valid_exts,
            file_archive_extract_cb, &userdata))
   {
      /* Parsing file archive failed. */
      ret = false;
      goto end;
   }

   if (!userdata.found_content)
   {
      /* Didn't find any content that matched valid extensions
       * for libretro implementation. */
      ret = false;
      goto end;
   }

   if (*userdata.first_extracted_file_path)
      strlcpy(out_path, userdata.first_extracted_file_path, len);

end:
   if (userdata.first_extracted_file_path)
      free(userdata.first_extracted_file_path);
   if (list)
      string_list_free(list);
   return ret;
}

/**
 * file_archive_get_file_list:
 * @path                        : filename path of archive
 *
 * Returns: string listing of files from archive on success, otherwise NULL.
 **/
struct string_list *file_archive_get_file_list(const char *path,
      const char *valid_exts)
{
   struct string_list *list = string_list_new();

   if (!list)
      goto error;

   if (!file_archive_parse_file(path, valid_exts,
            file_archive_get_file_list_cb, list))
      goto error;

   return list;

error:
   if (list)
      string_list_free(list);
   return NULL;
}

bool file_archive_perform_mode(const char *path, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   switch (cmode)
   {
      case ZLIB_MODE_UNCOMPRESSED:
         if (!retro_write_file(path, cdata, size))
            return false;
         break;

      case ZLIB_MODE_DEFLATE:
         {
            int ret = 0;
            file_archive_file_handle_t handle = {0};
            handle.backend = file_archive_get_default_file_backend();

            if (!handle.backend->stream_decompress_data_to_file_init(&handle,
                     cdata, csize, size))
               return false;

            do{
               ret = handle.backend->stream_decompress_data_to_file_iterate(handle.stream);
            }while(ret == 0);

            if (!file_archive_decompress_data_to_file(&handle,
                     ret, path, valid_exts,
                     cdata, csize, size, crc32))
               return false;
         }
         break;
      default:
         return false;
   }

   return true;
}

const struct file_archive_file_backend *file_archive_get_default_file_backend(void)
{
   return &zlib_backend;
}
