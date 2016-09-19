/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_zlib.c).
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

#include <compat/zlib.h>
#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <string.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>

#ifndef CENTRAL_FILE_HEADER_SIGNATURE
#define CENTRAL_FILE_HEADER_SIGNATURE 0x02014b50
#endif

#ifndef END_OF_CENTRAL_DIR_SIGNATURE
#define END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
#endif

static void* zlib_stream_new(void)
{
   return (z_stream*)calloc(1, sizeof(z_stream));
}

static void zlib_stream_free(void *data)
{
   z_stream *ret = (z_stream*)data;
   if (ret)
      inflateEnd(ret);
}

static void zlib_stream_set(void *data,
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

static uint32_t zlib_stream_get_avail_in(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->avail_in;
}

static uint32_t zlib_stream_get_avail_out(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->avail_out;
}

static uint64_t zlib_stream_get_total_out(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return 0;

   return stream->total_out;
}

static void zlib_stream_decrement_total_out(void *data, unsigned subtraction)
{
   z_stream *stream = (z_stream*)data;

   if (stream)
      stream->total_out  -= subtraction;
}

static void zlib_stream_compress_free(void *data)
{
   z_stream *ret = (z_stream*)data;
   if (ret)
      deflateEnd(ret);
}

static int zlib_stream_compress_data_to_file(void *data)
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

static bool zlib_stream_decompress_init(void *data)
{
   z_stream *stream = (z_stream*)data;

   if (!stream)
      return false;
   if (inflateInit(stream) != Z_OK)
      return false;
   return true;
}

static bool zlib_stream_decompress_data_to_file_init(
      file_archive_file_handle_t *handle,
      const uint8_t *cdata,  uint32_t csize, uint32_t size)
{
   if (!handle)
      return false;

   if (!(handle->stream = (z_stream*)zlib_stream_new()))
      goto error;

   if (inflateInit2((z_streamp)handle->stream, -MAX_WBITS) != Z_OK)
      goto error;

   handle->data = (uint8_t*)malloc(size);

   if (!handle->data)
      goto error;

   zlib_stream_set(handle->stream, csize, size,
         (const uint8_t*)cdata, handle->data);

   return true;

error:
   zlib_stream_free(handle->stream);
   free(handle->stream);
   if (handle->data)
      free(handle->data);

   return false;
}

static int zlib_stream_decompress_data_to_file_iterate(void *data)
{
   int zstatus;
   z_stream *stream = (z_stream*)data;

   if (!stream)
      goto error;

   zstatus = inflate(stream, Z_NO_FLUSH);

   if (zstatus == Z_STREAM_END)
      return 1;

   if (zstatus != Z_OK && zstatus != Z_BUF_ERROR)
      goto error;

   return 0;

error:
   return -1;
}

static void zlib_stream_compress_init(void *data, int level)
{
   z_stream *stream = (z_stream*)data;

   if (stream)
      deflateInit(stream, level);
}

static uint32_t zlib_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t length)
{
   return encoding_crc32(crc, data, length);
}

static bool zip_file_decompressed_handle(
      file_archive_file_handle_t *handle,
      const uint8_t *cdata, uint32_t csize,
      uint32_t size, uint32_t crc32)
{
   int ret   = 0;

   handle->backend = &zlib_backend;

   if (!handle->backend->stream_decompress_data_to_file_init(
            handle, cdata, csize, size))
      return false;

   do{
      ret = handle->backend->stream_decompress_data_to_file_iterate(
            handle->stream);
   }while(ret == 0);
#if 0
   handle->real_checksum = handle->backend->stream_crc_calculate(0,
         handle->data, size);

   if (handle->real_checksum != crc32)
      goto error;
#endif

   if (handle->stream)
      free(handle->stream);

   return true;
#if 0
error:
   if (handle->stream)
      free(handle->stream);
   if (handle->data)
      free(handle->data);

   handle->stream = NULL;
   handle->data   = NULL;
   return false;
#endif
}

/* Extract the relative path (needle) from a
 * ZIP archive (path) and allocate a buffer for it to write it in.
 *
 * optional_outfile if not NULL will be used to extract the file to.
 * buf will be 0 then.
 */

static int zip_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      return 1;

#if 0
   RARCH_LOG("[deflate] Path: %s, CRC32: 0x%x\n", name, crc32);
#endif

   if (strstr(name, userdata->decomp_state.needle))
   {
      bool goto_error = false;
      file_archive_file_handle_t handle = {0};

      userdata->decomp_state.found = true;

      if (zip_file_decompressed_handle(&handle,
               cdata, csize, size, crc32))
      {
         if (userdata->decomp_state.opt_file != 0)
         {
            /* Called in case core has need_fullpath enabled. */
            char *buf       = (char*)malloc(size);

            if (buf)
            {
               /*RARCH_LOG("%s: %s\n",
                     msg_hash_to_str(MSG_EXTRACTING_FILE),
                     userdata->decomp_state.opt_file);*/
               memcpy(buf, handle.data, size);

               if (!filestream_write_file(userdata->decomp_state.opt_file, buf, size))
                  goto_error = true;
            }

            free(buf);

            userdata->decomp_state.size = 0;
         }
         else
         {
            /* Called in case core has need_fullpath disabled.
             * Will copy decompressed content directly into
             * RetroArch's ROM buffer. */
            *userdata->decomp_state.buf = malloc(size);
            memcpy(*userdata->decomp_state.buf, handle.data, size);

            userdata->decomp_state.size = size;
         }
      }

      if (handle.data)
         free(handle.data);

      if (goto_error)
         return 0;
   }

   return 1;
}

static int zip_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   file_archive_transfer_t zlib;
   bool returnerr = true;
   int ret        = 0;
   struct archive_extract_userdata userdata = {0};

   zlib.type      = ARCHIVE_TRANSFER_INIT;

   userdata.decomp_state.needle      = NULL;
   userdata.decomp_state.opt_file    = NULL;
   userdata.decomp_state.found       = false;
   userdata.decomp_state.buf         = buf;

   if (needle)
      userdata.decomp_state.needle   = strdup(needle);
   if (optional_outfile)
      userdata.decomp_state.opt_file = strdup(optional_outfile);

   do
   {
      ret = file_archive_parse_file_iterate(&zlib, &returnerr, path,
            "", zip_file_decompressed, &userdata);
      if (!returnerr)
         break;
   }while(ret == 0 && !userdata.decomp_state.found);

   file_archive_parse_file_iterate_stop(&zlib);

   if (userdata.decomp_state.opt_file)
      free(userdata.decomp_state.opt_file);
   if (userdata.decomp_state.needle)
      free(userdata.decomp_state.needle);

   if (!userdata.decomp_state.found)
      return -1;

   return userdata.decomp_state.size;
}

static int zip_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   if (state->archive_size < 22)
      return -1;

   state->footer = state->data + state->archive_size - 22;

   for (;; state->footer--)
   {
      if (state->footer <= state->data + 22)
         return -1;
      if (read_le(state->footer, 4) == END_OF_CENTRAL_DIR_SIGNATURE)
      {
         unsigned comment_len = read_le(state->footer + 20, 2);
         if (state->footer + 22 + comment_len == state->data + state->archive_size)
            break;
      }
   }

   state->directory = state->data + read_le(state->footer + 16, 4);

   return 0;
}

static int zip_parse_file_iterate_step_internal(
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

   *cmode         = read_le(state->directory + 10, 2); /* compression mode, 0 = store, 8 = deflate */
   *checksum      = read_le(state->directory + 16, 4); /* CRC32 */
   *csize         = read_le(state->directory + 20, 4); /* compressed size */
   *size          = read_le(state->directory + 24, 4); /* uncompressed size */

   namelength     = read_le(state->directory + 28, 2); /* file name length */
   extralength    = read_le(state->directory + 30, 2); /* extra field length */
   commentlength  = read_le(state->directory + 32, 2); /* file comment length */

   if (namelength >= PATH_MAX_LENGTH)
      return -1;

   memcpy(filename, state->directory + 46, namelength); /* file name */

   offset         = read_le(state->directory + 42, 4); /* relative offset of local file header */
   offsetNL       = read_le(state->data + offset + 26, 2); /* file name length */
   offsetEL       = read_le(state->data + offset + 28, 2); /* extra field length */

   *cdata         = state->data + offset + 30 + offsetNL + offsetEL;

   *payback       = 46 + namelength + extralength + commentlength;

   return 1;
}

static int zip_parse_file_iterate_step(file_archive_transfer_t *state,
      const char *valid_exts, struct archive_extract_userdata *userdata, file_archive_file_cb file_cb)
{
   const uint8_t *cdata = NULL;
   uint32_t checksum    = 0;
   uint32_t size        = 0;
   uint32_t csize       = 0;
   unsigned cmode       = 0;
   unsigned payload     = 0;
   char filename[PATH_MAX_LENGTH] = {0};
   int ret = zip_parse_file_iterate_step_internal(state, filename,
         &cdata, &cmode, &size, &csize,
         &checksum, &payload);

   if (ret != 1)
      return ret;

   userdata->extracted_file_path = filename;

   if (!file_cb(filename, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   state->directory += payload;

   return 1;
}

const struct file_archive_file_backend zlib_backend = {
   zlib_stream_new,
   zlib_stream_free,
   zlib_stream_set,
   zlib_stream_get_avail_in,
   zlib_stream_get_avail_out,
   zlib_stream_get_total_out,
   zlib_stream_decrement_total_out,
   zlib_stream_decompress_init,
   zlib_stream_decompress_data_to_file_init,
   zlib_stream_decompress_data_to_file_iterate,
   zlib_stream_compress_init,
   zlib_stream_compress_free,
   zlib_stream_compress_data_to_file,
   zlib_stream_crc32_calculate,
   zip_file_read,
   zip_parse_file_init,
   zip_parse_file_iterate_step,
   "zlib"
};
