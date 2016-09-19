/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file.c).
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
#include <file/archive_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_stat.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

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
void file_archive_free(void *handle)
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

const uint8_t *file_archive_data(void *handle)
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
void file_archive_free(void *handle)
{
   file_archive_file_data_t *data = (file_archive_file_data_t*)handle;
   if (!data)
      return;
   if(data->data)
      free(data->data);
   free(data);
}

const uint8_t *file_archive_data(void *handle)
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

   read_from_file = filestream_read_file(path, &data->data, &ret);

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
      struct archive_extract_userdata *userdata)
{
   union string_list_elem_attr attr;
   struct string_list *ext_list = NULL;
   const char *file_ext         = NULL;
   size_t pathLen = strlen(path);

   (void)cdata;
   (void)cmode;
   (void)csize;
   (void)size;
   (void)checksum;

   memset(&attr, 0, sizeof(attr));

   if (!pathLen)
      return 0;

   if (valid_exts)
      ext_list = string_split(valid_exts, "|");

   if (ext_list)
   {
      /* Checks if this entry is a directory or a file. */
      char last_char = path[pathLen-1];

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

   return string_list_append(userdata->list, path, attr);

error:
   string_list_free(ext_list);
   return 0;
}

static int file_archive_extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t checksum, struct archive_extract_userdata *userdata)
{
   const char *ext                   = path_get_extension(name);

   /* Extract first file that matches our list. */
   if (ext && string_list_find_elem(userdata->ext, ext))
   {
      char new_path[PATH_MAX_LENGTH]    = {0};
      char wanted_file[PATH_MAX_LENGTH] = {0};
      const char *delim                 = NULL;

      if (userdata->extraction_directory)
         fill_pathname_join(new_path, userdata->extraction_directory,
               path_basename(name), sizeof(new_path));
      else
         fill_pathname_resolve_relative(new_path, userdata->archive_path,
               path_basename(name), sizeof(new_path));

      userdata->first_extracted_file_path = strdup(new_path);

      delim = path_get_archive_delim(userdata->archive_path);

      if (delim)
         strlcpy(wanted_file, delim + 1, sizeof(wanted_file));

      if (!string_is_equal_noncase(userdata->extracted_file_path,
                wanted_file))
        return 1; // keep searching for the right file

      if (file_archive_perform_mode(new_path,
                valid_exts, cdata, cmode, csize, size,
                0, userdata))
         userdata->found_file = true;

      return 0;
   }

   return 1;
}

int file_archive_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   char *last                 = NULL;
   char path[PATH_MAX_LENGTH] = {0};

   strlcpy(path, file, sizeof(path));

   last = (char*)path_get_archive_delim(path);

   if (last)
      *last = '\0';

   state->backend = file_archive_get_file_backend(path);
   if (!state->backend)
      return -1;

   state->handle = file_archive_open(path);
   if (!state->handle)
      return -1;

   state->archive_size = file_archive_size(state->handle);
   state->data   = file_archive_data(state->handle);
   state->footer = 0;
   state->directory = 0;

   return state->backend->archive_parse_file_init(state, path);
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
   if (!handle || ret == -1)
   {
      ret = 0;
      goto end;
   }

#if 0
   handle->real_checksum = handle->backend->stream_crc_calculate(
         0, handle->data, size);
   if (handle->real_checksum != checksum)
   {
      /* File CRC difers from archive CRC. */
      printf("File CRC differs from archive CRC. File: 0x%x, Archive: 0x%x.\n",
            (unsigned)handle->real_checksum, (unsigned)checksum);
   }
#endif

   if (!filestream_write_file(path, handle->data, size))
   {
      ret = false;
      goto end;
   }

end:
   handle->backend->stream_free(handle->stream);

   if (handle && handle->data)
      free(handle->data);
   return ret;
}

void file_archive_parse_file_iterate_stop(file_archive_transfer_t *state)
{
   if (!state || !state->handle)
      return;

   state->type = ARCHIVE_TRANSFER_DEINIT;
   file_archive_parse_file_iterate(state, NULL, NULL, NULL, NULL, NULL);
}

int file_archive_parse_file_iterate(
      file_archive_transfer_t *state,
      bool *returnerr,
      const char *file,
      const char *valid_exts,
      file_archive_file_cb file_cb,
      struct archive_extract_userdata *userdata)
{
   if (!state)
      return -1;

   switch (state->type)
   {
      case ARCHIVE_TRANSFER_NONE:
         break;
      case ARCHIVE_TRANSFER_INIT:
         if (file_archive_parse_file_init(state, file) == 0)
         {
            if (userdata)
               userdata->context = state->stream;
            state->type = ARCHIVE_TRANSFER_ITERATE;
         }
         else
            state->type = ARCHIVE_TRANSFER_DEINIT_ERROR;
         break;
      case ARCHIVE_TRANSFER_ITERATE:
         {
            const struct file_archive_file_backend *backend =
               file_archive_get_file_backend(file);

            if (backend)
            {
               int ret = backend->archive_parse_file_iterate_step(state,
                     valid_exts, userdata, file_cb);
               if (ret != 1)
                  state->type = ARCHIVE_TRANSFER_DEINIT;
               if (ret == -1)
                  state->type = ARCHIVE_TRANSFER_DEINIT_ERROR;

               /* early return to prevent deinit from never firing */
               return 0;
            }
            else
               return -1;
         }
         break;
      case ARCHIVE_TRANSFER_DEINIT_ERROR:
         *returnerr = false;
      case ARCHIVE_TRANSFER_DEINIT:
         if (state->handle)
         {
            file_archive_free(state->handle);
            state->handle = NULL;
         }
         if (state->stream && state->backend)
         {
            state->backend->stream_free(state->stream);

            if (state->stream)
               free(state->stream);

            state->stream = NULL;

            if (userdata)
               userdata->context = NULL;
         }
         break;
   }

   if (state->type == ARCHIVE_TRANSFER_DEINIT ||
         state->type == ARCHIVE_TRANSFER_DEINIT_ERROR)
      return -1;

   return 0;
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
static bool file_archive_parse_file(const char *file, const char *valid_exts,
      file_archive_file_cb file_cb, struct archive_extract_userdata *userdata)
{
   file_archive_transfer_t state = {0};
   bool returnerr        = true;

   state.type = ARCHIVE_TRANSFER_INIT;

   for (;;)
   {
      if (file_archive_parse_file_iterate(&state, &returnerr, file,
            valid_exts, file_cb, userdata) != 0)
         break;
   }

   return returnerr;
}

int file_archive_parse_file_progress(file_archive_transfer_t *state)
{
   /* FIXME: this estimate is worse than before */
   ptrdiff_t delta = state->directory - state->data;
   return delta * 100 / state->archive_size;
}

/**
 * file_archive_extract_file:
 * @archive_path                    : filename path to archive.
 * @archive_path_size               : size of archive.
 * @valid_exts                  : valid extensions for the file.
 * @extraction_directory        : the directory to extract temporary
 *                                file to.
 *
 * Extract file from archive. If no file inside the archive is
 * specified, the first file found will be used.
 *
 * Returns : true (1) on success, otherwise false (0).
 **/
bool file_archive_extract_file(
      char *archive_path,
      size_t archive_path_size,
      const char *valid_exts,
      const char *extraction_directory,
      char *out_path, size_t len)
{
   struct string_list *list             = NULL;
   bool ret                             = true;
   struct archive_extract_userdata userdata = {0};

   /* We cannot extract if the libretro
    * implementation does not have any valid extensions. */
   if (!valid_exts)
      return false;

   list = string_split(valid_exts, "|");
   if (!list)
   {
      ret = false;
      goto end;
   }

   userdata.archive_path         = archive_path;
   userdata.archive_path_size    = archive_path_size;
   userdata.extraction_directory = extraction_directory;
   userdata.ext                  = list;
   userdata.list                 = NULL;
   userdata.context              = NULL;
   userdata.list_only            = false;

   if (!file_archive_parse_file(archive_path, valid_exts,
            file_archive_extract_cb, &userdata))
   {
      /* Parsing file archive failed. */
      ret = false;
      goto end;
   }

   if (!userdata.found_file)
   {
      /* Didn't find any file that matched valid extensions
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
   struct archive_extract_userdata userdata = {0};
   userdata.list_only = true;

   userdata.list = string_list_new();

   if (!userdata.list)
      goto error;

   if (!file_archive_parse_file(path, valid_exts,
            file_archive_get_file_list_cb, &userdata))
      goto error;

   return userdata.list;

error:
   if (userdata.list)
      string_list_free(userdata.list);
   return NULL;
}

bool file_archive_perform_mode(const char *path, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   switch (cmode)
   {
      case ARCHIVE_MODE_UNCOMPRESSED:
         if (!filestream_write_file(path, cdata, size))
            goto error;
         break;

      case ARCHIVE_MODE_COMPRESSED:
         {
            int ret = 0;
            file_archive_file_handle_t handle = {0};

            handle.backend = file_archive_get_file_backend(userdata->archive_path);
            handle.stream = userdata->context;

            if (!handle.backend->stream_decompress_data_to_file_init(&handle,
                     cdata, csize, size))
               goto error;

            do{
               ret = handle.backend->stream_decompress_data_to_file_iterate(
                     handle.stream);
            }while(ret == 0);

            if (!file_archive_decompress_data_to_file(&handle,
                     ret, path, valid_exts,
                     cdata, csize, size, crc32))
               goto error;
         }
         break;
      default:
         goto error;
   }

   return true;

error:
   return false;
}

/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
int file_archive_compressed_read(
      const char * path, void **buf,
      const char* optional_filename, ssize_t *length)
{
   const struct file_archive_file_backend *backend = NULL;
   int ret                            = 0;
   struct string_list *str_list       = file_archive_filename_split(path);

   /* Safety check.
    * If optional_filename and optional_filename
    * exists, we simply return 0,
    * hoping that optional_filename is the
    * same as requested.
    */
   if (optional_filename && path_file_exists(optional_filename))
   {
      *length = 0;
      string_list_free(str_list);
      return 1;
   }

   /* We assure that there is something after the '#' symbol.
    *
    * This error condition happens for example, when
    * path = /path/to/file.7z, or
    * path = /path/to/file.7z#
    */
   if (str_list->size <= 1)
      goto error;

   backend = file_archive_get_file_backend(str_list->elems[0].data);

   *length = backend->compressed_file_read(str_list->elems[0].data,
         str_list->elems[1].data, buf, optional_filename);

   if (*length != -1)
      ret = 1;

   string_list_free(str_list);
   return ret;

error:
   /* could not extract string and substring. */
   string_list_free(str_list);
   *length = 0;
   return 0;
}

struct string_list *file_archive_file_list_new(const char *path,
      const char* ext)
{
#ifdef HAVE_COMPRESSION
   bool compressed = path_is_compressed_file(path);

   if (compressed)
      return file_archive_get_file_list(path, ext);
#endif
   return NULL;
}

/**
 * file_archive_filename_split:
 * @str              : filename to turn into a string list
 *
 * Creates a new string list based on filename @path, delimited by a hash (#).
 *
 * Returns: new string list if successful, otherwise NULL.
 */
struct string_list *file_archive_filename_split(const char *path)
{
   union string_list_elem_attr attr;
   struct string_list *list = string_list_new();
   const char *delim        = NULL;

   memset(&attr, 0, sizeof(attr));

   delim = path_get_archive_delim(path);

   if (delim)
   {
      /* add archive path to list first */
      if (!string_list_append_n(list, path, delim - path, attr))
         goto error;

      /* now add the path within the archive */
      delim++;

      if (*delim)
      {
         if (!string_list_append(list, delim, attr))
            goto error;
      }
   }
   else
      if (!string_list_append(list, path, attr))
         goto error;

   return list;

error:
   string_list_free(list);
   return NULL;
}

const struct file_archive_file_backend *file_archive_get_zlib_file_backend(void)
{
#ifdef HAVE_ZLIB
   return &zlib_backend;
#else
   return NULL;
#endif
}

const struct file_archive_file_backend *file_archive_get_7z_file_backend(void)
{
#ifdef HAVE_7ZIP
   return &sevenzip_backend;
#else
   return NULL;
#endif
}

const struct file_archive_file_backend* file_archive_get_file_backend(const char *path)
{
   const char *file_ext          = NULL;
   char *last                    = NULL;
   char newpath[PATH_MAX_LENGTH] = {0};

   strlcpy(newpath, path, sizeof(newpath));

   last = (char*)path_get_archive_delim(newpath);

   if (last)
      *last = '\0';

   file_ext = path_get_extension(newpath);

#ifdef HAVE_7ZIP
   if (string_is_equal_noncase(file_ext, "7z"))
      return &sevenzip_backend;
#endif

#ifdef HAVE_ZLIB
   if (string_is_equal_noncase(file_ext, "zip"))
      return &zlib_backend;
#endif

   return NULL;
}
