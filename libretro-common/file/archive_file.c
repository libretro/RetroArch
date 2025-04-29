/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#ifdef HAVE_MMAP
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
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
   attr.i = RARCH_COMPRESSED_FILE_IN_ARCHIVE;

   if (valid_exts)
   {
      size_t _len                  = strlen(path);
      /* Checks if this entry is a directory or a file. */
      char last_char               = path[_len - 1];
      struct string_list ext_list  = {0};

      /* Skip if directory. */
      if (last_char == '/' || last_char == '\\' )
         return 1;

      string_list_initialize(&ext_list);
      if (string_split_noalloc(&ext_list, valid_exts, "|"))
      {
         const char *file_ext = path_get_extension(path);

         if (!file_ext)
         {
            string_list_deinitialize(&ext_list);
            return 1;
         }

         if (!string_list_find_elem_prefix(&ext_list, ".", file_ext))
         {
            /* keep iterating */
            string_list_deinitialize(&ext_list);
            return -1;
         }
      }

      string_list_deinitialize(&ext_list);
   }

   return string_list_append(userdata->list, path, attr);
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
      char new_path[PATH_MAX_LENGTH];
      const char *delim;

      if ((delim = path_get_archive_delim(userdata->archive_path)))
      {
         if (!string_is_equal_noncase(
                  userdata->current_file_path, delim + 1))
           return 1; /* keep searching for the right file */
      }

      if (userdata->extraction_directory)
         fill_pathname_join_special(new_path, userdata->extraction_directory,
               path_basename(name), sizeof(new_path));
      else
         fill_pathname_resolve_relative(new_path, userdata->archive_path,
               path_basename(name), sizeof(new_path));

      if (file_archive_perform_mode(new_path,
                valid_exts, cdata, cmode, csize, size,
                checksum, userdata))
      {
         userdata->found_file = true;
         userdata->first_extracted_file_path = strdup(new_path);
      }

      return 0;
   }

   return 1;
}

static int file_archive_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   char path[PATH_MAX_LENGTH];
   char *last                 = NULL;

   strlcpy(path, file, sizeof(path));

   if ((last = (char*)path_get_archive_delim(path)))
      *last  = '\0';

   if (!(state->backend = file_archive_get_file_backend(path)))
      return -1;

   /* Failed to open archive. */
   if (!(state->archive_file = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return -1;

   state->archive_size = filestream_get_size(state->archive_file);

#ifdef HAVE_MMAP
   if (state->archive_size <= (256*1024*1024))
   {
      state->archive_mmap_fd = open(path, O_RDONLY);
      if (state->archive_mmap_fd)
      {
         state->archive_mmap_data = (uint8_t*)mmap(NULL,
               (size_t)state->archive_size,
               PROT_READ, MAP_SHARED, state->archive_mmap_fd, 0);

         if (state->archive_mmap_data == (uint8_t*)MAP_FAILED)
         {
            close(state->archive_mmap_fd);
            state->archive_mmap_fd = 0;
            state->archive_mmap_data = NULL;
         }
      }
   }
#endif

   state->step_current = 0;
   state->step_total   = 0;

   return state->backend->archive_parse_file_init(state, path);
}

void file_archive_parse_file_iterate_stop(file_archive_transfer_t *state)
{
   if (!state || !state->archive_file)
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
            {
               userdata->transfer = state;
               strlcpy(userdata->archive_path, file,
                     sizeof(userdata->archive_path));
            }
            state->type = ARCHIVE_TRANSFER_ITERATE;
         }
         else
            state->type = ARCHIVE_TRANSFER_DEINIT_ERROR;
         break;
      case ARCHIVE_TRANSFER_ITERATE:
         if (state->backend)
         {
            int ret = state->backend->archive_parse_file_iterate_step(
                  state->context, valid_exts, userdata, file_cb);

            if (ret == 1)
               state->step_current++; /* found another file */
            if (ret != 1)
               state->type = ARCHIVE_TRANSFER_DEINIT;
            if (ret == -1)
               state->type = ARCHIVE_TRANSFER_DEINIT_ERROR;

            /* early return to prevent deinit from never firing */
            return 0;
         }
         return -1;
      case ARCHIVE_TRANSFER_DEINIT_ERROR:
         *returnerr = false;
      case ARCHIVE_TRANSFER_DEINIT:
         if (state->context)
         {
            if (state->backend->archive_parse_file_free)
               state->backend->archive_parse_file_free(state->context);
            state->context = NULL;
         }

         if (state->archive_file)
         {
            filestream_close(state->archive_file);
            state->archive_file = NULL;
         }

#ifdef HAVE_MMAP
         if (state->archive_mmap_data)
         {
            munmap(state->archive_mmap_data, (size_t)state->archive_size);
            close(state->archive_mmap_fd);
            state->archive_mmap_fd = 0;
            state->archive_mmap_data = NULL;
         }
#endif

         if (userdata)
            userdata->transfer = NULL;
         break;
   }

   if (  state->type == ARCHIVE_TRANSFER_DEINIT ||
         state->type == ARCHIVE_TRANSFER_DEINIT_ERROR)
      return -1;

   return 0;
}

/**
 * file_archive_walk:
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
static bool file_archive_walk(const char *file, const char *valid_exts,
      file_archive_file_cb file_cb, struct archive_extract_userdata *userdata)
{
   file_archive_transfer_t state;
   bool returnerr          = true;

   state.type              = ARCHIVE_TRANSFER_INIT;
   state.archive_file      = NULL;
#ifdef HAVE_MMAP
   state.archive_mmap_fd   = 0;
   state.archive_mmap_data = NULL;
#endif
   state.archive_size      = 0;
   state.context           = NULL;
   state.step_total        = 0;
   state.step_current      = 0;
   state.backend           = NULL;

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
   if (!state || state->step_total == 0)
      return 0;

   return (int)((state->step_current * 100) / (state->step_total));
}

/**
 * file_archive_extract_file:
 * @archive_path                : filename path to archive.
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
      const char *archive_path,
      const char *valid_exts,
      const char *extraction_directory,
      char *s, size_t len)
{
   struct archive_extract_userdata userdata;
   bool ret                                 = true;
   struct string_list *list                 = string_split(valid_exts, "|");

   userdata.archive_path[0]                 = '\0';
   userdata.current_file_path[0]            = '\0';
   userdata.first_extracted_file_path       = NULL;
   userdata.extraction_directory            = extraction_directory;
   userdata.ext                             = list;
   userdata.list                            = NULL;
   userdata.found_file                      = false;
   userdata.list_only                       = false;
   userdata.crc                             = 0;
   userdata.transfer                        = NULL;
   userdata.dec                             = NULL;

   if (!list)
   {
      ret = false;
      goto end;
   }

   if (!file_archive_walk(archive_path, valid_exts,
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

   if (!string_is_empty(userdata.first_extracted_file_path))
      strlcpy(s, userdata.first_extracted_file_path, len);

end:
   if (userdata.first_extracted_file_path)
      free(userdata.first_extracted_file_path);
   if (list)
      string_list_free(list);
   return ret;
}

/* Warning: 'list' must zero initialised before
 * calling this function, otherwise memory leaks/
 * undefined behaviour will occur */
bool file_archive_get_file_list_noalloc(struct string_list *list,
      const char *path,
      const char *valid_exts)
{
   struct archive_extract_userdata userdata;

   if (!list || !string_list_initialize(list))
      return false;

   strlcpy(userdata.archive_path, path, sizeof(userdata.archive_path));
   userdata.current_file_path[0]            = '\0';
   userdata.first_extracted_file_path       = NULL;
   userdata.extraction_directory            = NULL;
   userdata.ext                             = NULL;
   userdata.list                            = list;
   userdata.found_file                      = false;
   userdata.list_only                       = true;
   userdata.crc                             = 0;
   userdata.transfer                        = NULL;
   userdata.dec                             = NULL;

   if (!file_archive_walk(path, valid_exts,
            file_archive_get_file_list_cb, &userdata))
      return false;
   return true;
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
   struct archive_extract_userdata userdata;

   strlcpy(userdata.archive_path, path, sizeof(userdata.archive_path));
   userdata.current_file_path[0]            = '\0';
   userdata.first_extracted_file_path       = NULL;
   userdata.extraction_directory            = NULL;
   userdata.ext                             = NULL;
   userdata.list                            = string_list_new();
   userdata.found_file                      = false;
   userdata.list_only                       = true;
   userdata.crc                             = 0;
   userdata.transfer                        = NULL;
   userdata.dec                             = NULL;

   if (!userdata.list)
      return NULL;
   if (!file_archive_walk(path, valid_exts,
         file_archive_get_file_list_cb, &userdata))
   {
      string_list_free(userdata.list);
      return NULL;
   }
   return userdata.list;
}

bool file_archive_perform_mode(const char *path, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   int ret;
   file_archive_file_handle_t handle;

   if (!userdata->transfer || !userdata->transfer->backend)
      return false;

   handle.data          = NULL;
   handle.real_checksum = 0;

   if (!userdata->transfer->backend->stream_decompress_data_to_file_init(
            userdata->transfer->context, &handle, cdata, cmode, csize, size))
      return false;

   do
   {
      ret = userdata->transfer->backend->stream_decompress_data_to_file_iterate(
               userdata->transfer->context, &handle);
   }while (ret == 0);

   if (ret == -1 || !filestream_write_file(path, handle.data, size))
      return false;

   return true;
}

/**
 * file_archive_filename_split:
 * @str              : filename to turn into a string list
 *
 * Creates a new string list based on filename @path, delimited by a hash (#).
 *
 * Returns: new string list if successful, otherwise NULL.
 */
static struct string_list *file_archive_filename_split(const char *path)
{
   union string_list_elem_attr attr;
   struct string_list *list = string_list_new();
   const char *delim        = path_get_archive_delim(path);

   attr.i = 0;

   if (delim)
   {
      /* add archive path to list first */
      if (!string_list_append_n(list, path, (unsigned)(delim - path), attr))
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

/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
int file_archive_compressed_read(
      const char * path, void **buf,
      const char* optional_filename, int64_t *len)
{
   const struct
      file_archive_file_backend *backend = NULL;
   struct string_list *str_list          = NULL;

   /* Safety check.
    * If optional_filename and optional_filename
    * exists, we simply return 0,
    * hoping that optional_filename is the
    * same as requested.
    */
   if (optional_filename && path_is_valid(optional_filename))
   {
      *len = 0;
      return 1;
   }

   str_list       = file_archive_filename_split(path);
   /* We assure that there is something after the '#' symbol.
    *
    * This error condition happens for example, when
    * path = /path/to/file.7z, or
    * path = /path/to/file.7z#
    */
   if (str_list->size <= 1)
   {
      /* could not extract string and substring. */
      string_list_free(str_list);
      *len = 0;
      return 0;
   }

   backend = file_archive_get_file_backend(str_list->elems[0].data);
   *len    = backend->compressed_file_read(str_list->elems[0].data,
         str_list->elems[1].data, buf, optional_filename);

   string_list_free(str_list);

   if (*len != -1)
      return 1;

   return 0;
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
#if defined(HAVE_7ZIP) || defined(HAVE_ZLIB)
   char newpath[PATH_MAX_LENGTH];
   const char *file_ext          = NULL;
   char *last                    = NULL;

   strlcpy(newpath, path, sizeof(newpath));

   if ((last = (char*)path_get_archive_delim(newpath)))
      *last  = '\0';

   file_ext  = path_get_extension(newpath);

#ifdef HAVE_7ZIP
   if (string_is_equal_noncase(file_ext, "7z"))
      return &sevenzip_backend;
#endif

#ifdef HAVE_ZLIB
   if (     string_is_equal_noncase(file_ext, "zip")
         || string_is_equal_noncase(file_ext, "apk")
      )
      return &zlib_backend;
#endif
#endif

   return NULL;
}

/**
 * file_archive_get_file_crc32:
 * @path                         : filename path of archive
 *
 * Returns: CRC32 of the specified file in the archive, otherwise 0.
 * If no path within the archive is specified, the first
 * file found inside is used.
 **/
uint32_t file_archive_get_file_crc32(const char *path)
{
   file_archive_transfer_t state;
   struct archive_extract_userdata userdata        = {0};
   bool returnerr                                  = false;
   const char *archive_path                        = NULL;
   bool contains_compressed = path_contains_compressed_file(path);

   if (contains_compressed)
   {
      archive_path = path_get_archive_delim(path);

      /* move pointer right after the delimiter to give us the path */
      if (archive_path)
         archive_path += 1;
   }

   state.type              = ARCHIVE_TRANSFER_INIT;
   state.archive_file      = NULL;
#ifdef HAVE_MMAP
   state.archive_mmap_fd   = 0;
   state.archive_mmap_data = NULL;
#endif
   state.archive_size      = 0;
   state.context           = NULL;
   state.step_total        = 0;
   state.step_current      = 0;
   state.backend           = NULL;

   /* Initialize and open archive first.
      Sets next state type to ITERATE. */
   file_archive_parse_file_iterate(&state,
            &returnerr, path, NULL, NULL,
            &userdata);

   for (;;)
   {
      /* Now find the first file in the archive. */
      if (state.type == ARCHIVE_TRANSFER_ITERATE)
         file_archive_parse_file_iterate(&state,
                  &returnerr, path, NULL, NULL,
                  &userdata);

      /* If no path specified within archive, stop after
       * finding the first file.
       */
      if (!contains_compressed)
         break;

      /* Stop when the right file in the archive is found. */
      if (archive_path)
      {
         if (string_is_equal(userdata.current_file_path, archive_path))
            break;
      }
      else
         break;
   }

   file_archive_parse_file_iterate_stop(&state);

   return userdata.crc;
}
