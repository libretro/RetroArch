/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_extract.c).
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

#include <compat/zlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>

#include <file/file_extract.h>
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

enum zlib_compression_mode
{
   ZLIB_MODE_UNCOMPRESSED = 0,
   ZLIB_MODE_DEFLATE      = 8
};

static const struct zlib_file_backend *zlib_get_default_file_backend(void)
{
   return &zlib_backend;
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

static bool zlib_inflate_init2(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return false;
   if (inflateInit2(stream, -MAX_WBITS) != Z_OK)
      return false;
   return true;
}

void *zlib_stream_new(void)
{
   return (z_stream*)calloc(1, sizeof(z_stream));
}

void zlib_deflate_init(void *data, int level)
{
   z_stream *stream = (z_stream*)data;

   if (stream)
      deflateInit(stream, level);
}

bool zlib_inflate_init(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return false;
   if (inflateInit(stream) != Z_OK)
      return false;
   return true;
}

void zlib_stream_free(void *data)
{
   z_stream *ret = (z_stream*)data;
   if (ret)
      inflateEnd(ret);
}

void zlib_stream_deflate_free(void *data)
{
   z_stream *ret = (z_stream*)data;
   if (ret)
      deflateEnd(ret);
}

bool zlib_inflate_data_to_file_init(
      zlib_file_handle_t *handle,
      const uint8_t *cdata,  uint32_t csize, uint32_t size)
{
   z_stream *stream = NULL;

   if (!handle)
      return false;

   if (!(handle->stream = (z_stream*)zlib_stream_new()))
      goto error;
   
   if (!(zlib_inflate_init2(handle->stream)))
      goto error;

   handle->data = (uint8_t*)malloc(size);

   if (!handle->data)
      goto error;

   stream            = (z_stream*)handle->stream;

   if (!stream)
      goto error;

   zlib_set_stream(stream,
         csize,
         size,
         (const uint8_t*)cdata,
         handle->data 
         );

   return true;

error:
   if (handle->stream)
      zlib_stream_free(handle->stream);
   if (handle->data)
      free(handle->data);

   return false;
}

int zlib_deflate_data_to_file(void *data)
{
   int zstatus;
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return -1;

   zstatus = deflate(stream, Z_FINISH);

   if (zstatus == Z_STREAM_END)
      return 1;

   return 0;
}

int zlib_inflate_data_to_file_iterate(void *data)
{
   int zstatus;
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return -1;

   zstatus = inflate(stream, Z_NO_FLUSH);

   if (zstatus == Z_STREAM_END)
      return 1;

   if (zstatus != Z_OK && zstatus != Z_BUF_ERROR)
      return -1;

   return 0;
}

uint32_t zlib_crc32_calculate(uint32_t crc, const uint8_t *data, size_t length)
{
   return crc32(crc, data, length);
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
int zlib_inflate_data_to_file(zlib_file_handle_t *handle,
      int ret, const char *path, const char *valid_exts,
      const uint8_t *cdata, uint32_t csize, uint32_t size, uint32_t checksum)
{
   if (handle)
   {
      zlib_stream_free(handle->stream);
      free(handle->stream);
   }

   if (!handle || ret == -1)
   {
      ret = 0;
      goto end;
   }

   handle->real_checksum = zlib_crc32_calculate(0, handle->data, size);

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

static int zlib_parse_file_iterate_step_internal(
      zlib_transfer_t *state, char *filename,
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

   namelength    = read_le(state->directory + 28, 2);
   extralength   = read_le(state->directory + 30, 2);
   commentlength = read_le(state->directory + 32, 2);

   if (namelength >= PATH_MAX_LENGTH)
      return -1;

   memcpy(filename, state->directory + 46, namelength);

   offset        = read_le(state->directory + 42, 4);
   offsetNL      = read_le(state->data + offset + 26, 2);
   offsetEL      = read_le(state->data + offset + 28, 2);

   *cdata = state->data + offset + 30 + offsetNL + offsetEL;

   *payback = 46 + namelength + extralength + commentlength;

   return 1;
}

static int zlib_parse_file_iterate_step(zlib_transfer_t *state,
      const char *valid_exts, void *userdata, zlib_file_cb file_cb)
{
   const uint8_t *cdata = NULL;
   uint32_t checksum    = 0;
   uint32_t size        = 0;
   uint32_t csize       = 0;
   unsigned cmode       = 0;
   unsigned payload     = 0;
   char filename[PATH_MAX_LENGTH] = {0};
   int ret = zlib_parse_file_iterate_step_internal(state, filename,
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

static int zlib_parse_file_init(zlib_transfer_t *state,
      const char *file)
{
   state->backend = zlib_get_default_file_backend();

   if (!state->backend)
      return -1;

   state->handle = state->backend->open(file);
   if (!state->handle)
      return -1;

   state->zip_size = state->backend->size(state->handle);
   if (state->zip_size < 22)
      return -1;

   state->data   = state->backend->data(state->handle);
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

int zlib_parse_file_iterate(void *data, bool *returnerr, const char *file,
      const char *valid_exts, zlib_file_cb file_cb, void *userdata)
{
   zlib_transfer_t *state = (zlib_transfer_t*)data;

   if (!state)
      return -1;

   switch (state->type)
   {
      case ZLIB_TRANSFER_NONE:
         break;
      case ZLIB_TRANSFER_INIT:
         if (zlib_parse_file_init(state, file) == 0)
            state->type = ZLIB_TRANSFER_ITERATE;
         else
            state->type = ZLIB_TRANSFER_DEINIT_ERROR;
         break;
      case ZLIB_TRANSFER_ITERATE:
         {
            int ret2 = zlib_parse_file_iterate_step(state,
                  valid_exts, userdata, file_cb);
            if (ret2 != 1)
               state->type = ZLIB_TRANSFER_DEINIT;
            if (ret2 == -1)
               state->type = ZLIB_TRANSFER_DEINIT_ERROR;
         }
         break;
      case ZLIB_TRANSFER_DEINIT_ERROR:
         *returnerr = false;
      case ZLIB_TRANSFER_DEINIT:
         if (state->handle)
         {
            state->backend->free(state->handle);
            state->handle = NULL;
         }
         break;
   }

   if (state->type == ZLIB_TRANSFER_DEINIT ||
         state->type == ZLIB_TRANSFER_DEINIT_ERROR)
      return -1;

   return 0;
}

void zlib_parse_file_iterate_stop(void *data)
{
   zlib_transfer_t *state = (zlib_transfer_t*)data;
   if (!state || !state->handle)
      return;

   state->type = ZLIB_TRANSFER_DEINIT;
   zlib_parse_file_iterate(data, NULL, NULL, NULL, NULL, NULL);
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
   zlib_transfer_t state = {0};
   bool returnerr = true;

   state.type = ZLIB_TRANSFER_INIT;

   for (;;)
   {
      int ret = zlib_parse_file_iterate(&state, &returnerr, file,
            valid_exts, file_cb, userdata);

      if (ret != 0)
         break;
   }

   return returnerr;
}

int zlib_parse_file_progress(void *data)
{
   /* FIXME: this estimate is worse than before */
   zlib_transfer_t *state = (zlib_transfer_t*)data;
   ptrdiff_t delta = state->directory - state->data;
   return delta * 100 / state->zip_size;
}


static int zip_extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t checksum, void *userdata)
{
   struct zip_extract_userdata *data = (struct zip_extract_userdata*)userdata;
   /* Extract first content that matches our list. */
   const char *ext                   = path_get_extension(name);

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
      data->found_content             = zlib_perform_mode(new_path,
            valid_exts, cdata, cmode, csize, size,
            0, NULL);
      return 0;
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
      const char *valid_exts, const char *extraction_directory,
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

   if (!zlib_parse_file(zip_path, valid_exts, zip_extract_cb, &userdata))
   {
      /* Parsing ZIP failed. */
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

static int zlib_get_file_list_cb(const char *path, const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size, uint32_t checksum,
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
      goto error;

   if (!zlib_parse_file(path, valid_exts,
            zlib_get_file_list_cb, list))
      goto error;

   return list;

error:
   if (list)
      string_list_free(list);
   return NULL;
}

bool zlib_perform_mode(const char *path, const char *valid_exts,
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
            zlib_file_handle_t handle = {0};
            if (!zlib_inflate_data_to_file_init(&handle, cdata, csize, size))
               return false;

            do{
               ret = zlib_inflate_data_to_file_iterate(handle.stream);
            }while(ret == 0);

            if (!zlib_inflate_data_to_file(&handle, ret, path, valid_exts,
                     cdata, csize, size, crc32))
               return false;
         }
         break;
      default:
         return false;
   }

   return true;
}

void zlib_set_stream(void *data,
      uint32_t       avail_in,
      uint32_t       avail_out,
      const uint8_t *next_in,
      uint8_t       *next_out
      )
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return;

   stream->avail_in  = avail_in;
   stream->avail_out = avail_out;

   stream->next_in   = (uint8_t*)next_in;
   stream->next_out  = next_out;
}

uint32_t zlib_stream_get_avail_in(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->avail_in;
}

uint32_t zlib_stream_get_avail_out(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->avail_out;
}

uint64_t zlib_stream_get_total_out(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->total_out;
}

void zlib_stream_decrement_total_out(void *data, unsigned subtraction)
{
   z_stream *stream = (z_stream*)data;

   if (stream)
      stream->total_out  -= subtraction;
}
