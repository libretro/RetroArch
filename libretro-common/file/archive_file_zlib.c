/* Copyright  (C) 2010-2018 The RetroArch team
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
#include <string.h>

#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <streams/trans_stream.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>

/* Only for MAX_WBITS */
#include <zlib.h>

#ifndef CENTRAL_FILE_HEADER_SIGNATURE
#define CENTRAL_FILE_HEADER_SIGNATURE 0x02014b50
#endif

#ifndef END_OF_CENTRAL_DIR_SIGNATURE
#define END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
#endif

static INLINE uint32_t read_le(const uint8_t *data, unsigned size)
{
   unsigned i;
   uint32_t val = 0;

   size *= 8;
   for (i = 0; i < size; i += 8)
      val |= (uint32_t)*data++ << i;

   return val;
}

static void *zlib_stream_new(void)
{
   return zlib_inflate_backend.stream_new();
}

static void zlib_stream_free(void *stream)
{
   zlib_inflate_backend.stream_free(stream);
}

static bool zlib_stream_decompress_data_to_file_init(
      file_archive_file_handle_t *handle,
      const uint8_t *cdata,  uint32_t csize, uint32_t size)
{
   if (!handle)
      return false;

   handle->stream = zlib_inflate_backend.stream_new();

   if (!handle->stream)
      goto error;

   if (zlib_inflate_backend.define)
      zlib_inflate_backend.define(handle->stream, "window_bits", (uint32_t)-MAX_WBITS);

   handle->data = (uint8_t*)malloc(size);

   if (!handle->data)
      goto error;

   zlib_inflate_backend.set_in(handle->stream,
         (const uint8_t*)cdata, csize);
   zlib_inflate_backend.set_out(handle->stream,
         handle->data, size);

   return true;

error:
   if (handle->stream)
      zlib_inflate_backend.stream_free(handle->stream);
   if (handle->data)
      free(handle->data);

   return false;
}

static int zlib_stream_decompress_data_to_file_iterate(void *stream)
{
   bool zstatus;
   uint32_t rd, wn;
   enum trans_stream_error terror;

   if (!stream)
      return -1;

   zstatus = zlib_inflate_backend.trans(stream, false, &rd, &wn, &terror);

   if (!zstatus && terror != TRANS_STREAM_ERROR_BUFFER_FULL)
      return -1;

   if (zstatus && !terror)
      return 1;

   return 0;
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

   do
   {
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
   char last_char = name[strlen(name) - 1];
   /* Ignore directories. */
   if (last_char == '/' || last_char == '\\')
      return 1;

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
   struct archive_extract_userdata userdata = {{0}};
   bool returnerr                           = true;
   int ret                                  = 0;

   zlib.type                                = ARCHIVE_TRANSFER_INIT;
   zlib.archive_size                        = 0;
   zlib.start_delta                         = 0;
   zlib.handle                              = NULL;
   zlib.stream                              = NULL;
   zlib.footer                              = NULL;
   zlib.directory                           = NULL;
   zlib.data                                = NULL;
   zlib.backend                             = NULL;

   userdata.decomp_state.needle             = NULL;
   userdata.decomp_state.opt_file           = NULL;
   userdata.decomp_state.found              = false;
   userdata.decomp_state.buf                = buf;

   if (needle)
      userdata.decomp_state.needle          = strdup(needle);
   if (optional_outfile)
      userdata.decomp_state.opt_file        = strdup(optional_outfile);

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

   return (int)userdata.decomp_state.size;
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
      const char *valid_exts, struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb)
{
   char filename[PATH_MAX_LENGTH] = {0};
   const uint8_t *cdata           = NULL;
   uint32_t checksum              = 0;
   uint32_t size                  = 0;
   uint32_t csize                 = 0;
   unsigned cmode                 = 0;
   unsigned payload               = 0;
   int ret                        = zip_parse_file_iterate_step_internal(
         state, filename, &cdata, &cmode, &size, &csize, &checksum, &payload);

   if (ret != 1)
      return ret;

   userdata->extracted_file_path = filename;
   userdata->crc = checksum;

   if (file_cb && !file_cb(filename, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   state->directory += payload;

   return 1;
}

const struct file_archive_file_backend zlib_backend = {
   zlib_stream_new,
   zlib_stream_free,
   zlib_stream_decompress_data_to_file_init,
   zlib_stream_decompress_data_to_file_iterate,
   zlib_stream_crc32_calculate,
   zip_file_read,
   zip_parse_file_init,
   zip_parse_file_iterate_step,
   "zlib"
};
