/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_zstd.c).
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
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <compat/strl.h>

/* Either decoder will do; rzstd is preferred where a build has it, and
 * the reference remains available until every platform has moved. */
#ifdef HAVE_RZSTD
#include <encodings/rzstd.h>
#else
#include <zstd.h>
#endif

/* One spelling for whichever decoder is compiled in. */
#ifdef HAVE_RZSTD
#define ZSTD_FRAME_HEADER_MAX RZSTD_FRAME_HEADER_MAX
#define ZSTD_SIZE_UNKNOWN     RZSTD_CONTENT_SIZE_UNKNOWN
#define ZSTD_SIZE_ERROR       RZSTD_CONTENT_SIZE_ERROR
#else
#define ZSTD_FRAME_HEADER_MAX 18
#define ZSTD_SIZE_UNKNOWN     ((int64_t)ZSTD_CONTENTSIZE_UNKNOWN)
#define ZSTD_SIZE_ERROR       ((int64_t)ZSTD_CONTENTSIZE_ERROR)
#endif

#define ZSTD_MAGIC         "\x28\xB5\x2F\xFD"
#define ZSTD_MAGIC_LEN     4

struct zstd_file_context
{
   uint8_t *decompressed_data;
   char inner_filename[PATH_MAX_LENGTH];
   char archive_path[PATH_MAX_LENGTH];
   int64_t archive_size;
   uint32_t decompressed_size;
   bool parsed;
};

/* Derive the inner filename from the .zst path by stripping the .zst extension */
static void zstd_derive_inner_filename(const char *path, char *s, size_t len)
{
   const char *base = path_basename(path);
   const char *ext;

   strlcpy(s, base, len);
   ext = strrchr(s, '.');
   if (ext && string_is_equal_noncase(ext, ".zst"))
      ((char*)ext)[0] = '\0';
}

static int zstd_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   uint8_t magic_buf[ZSTD_MAGIC_LEN];
   struct zstd_file_context *ctx = NULL;

   if (state->archive_size < ZSTD_MAGIC_LEN)
      goto error;

   filestream_seek(state->archive_file, 0, SEEK_SET);
   if (filestream_read(state->archive_file, magic_buf, ZSTD_MAGIC_LEN)
         != ZSTD_MAGIC_LEN)
      goto error;

   if (memcmp(magic_buf, ZSTD_MAGIC, ZSTD_MAGIC_LEN) != 0)
      goto error;

   ctx = (struct zstd_file_context*)calloc(1, sizeof(*ctx));
   if (!ctx)
      goto error;

   zstd_derive_inner_filename(file, ctx->inner_filename,
         sizeof(ctx->inner_filename));
   strlcpy(ctx->archive_path, file, sizeof(ctx->archive_path));
   ctx->archive_size = state->archive_size;

   state->context    = ctx;
   state->step_total = 1;

   return 0;

error:
   if (ctx)
      free(ctx);
   state->context = NULL;
   return -1;
}

static int zstd_parse_file_iterate_step(void *context,
      const char *valid_exts,
      struct archive_extract_userdata *userdata, file_archive_file_cb file_cb)
{
   struct zstd_file_context *ctx = (struct zstd_file_context*)context;
   file_archive_transfer_t *state;
   int64_t            content_size;

   if (!ctx || ctx->parsed)
      return 0;

   ctx->parsed = true;
   state       = userdata->transfer;

   /* Read the frame header to get decompressed size */
   filestream_seek(state->archive_file, 0, SEEK_SET);

#ifdef HAVE_MMAP
   if (state->archive_mmap_data)
   {
#ifdef HAVE_RZSTD
      content_size = rzstd_frame_content_size(
            state->archive_mmap_data, (size_t)state->archive_size);
#else
      content_size = (int64_t)ZSTD_getFrameContentSize(
            state->archive_mmap_data, (size_t)state->archive_size);
#endif
   }
   else
#endif
   {
      /* Only the header is needed, and it is at most
       * ZSTD_FRAME_HEADER_MAX bytes. */
      uint8_t header_buf[ZSTD_FRAME_HEADER_MAX];
      int64_t to_read = state->archive_size < ZSTD_FRAME_HEADER_MAX
                       ? state->archive_size : ZSTD_FRAME_HEADER_MAX;
      if (filestream_read(state->archive_file, header_buf, to_read) != to_read)
         return -1;
#ifdef HAVE_RZSTD
      content_size = rzstd_frame_content_size(header_buf, (size_t)to_read);
#else
      content_size = (int64_t)ZSTD_getFrameContentSize(header_buf,
            (size_t)to_read);
#endif
   }

   /* A frame that does not state its size is no use here: the whole
    * member has to be allocated before it is decoded. */
   if (  content_size == ZSTD_SIZE_UNKNOWN
      || content_size == ZSTD_SIZE_ERROR)
      return -1;

   /* decompressed_size is a uint32_t and feeds a later malloc; reject
    * values that would truncate to a smaller size and mismatch the
    * length the decode actually produces. */
   if (content_size > UINT32_MAX)
      return -1;

   ctx->decompressed_size = (uint32_t)content_size;

   strlcpy(userdata->current_file_path, ctx->inner_filename,
         sizeof(userdata->current_file_path));

   userdata->crc  = 0;
   userdata->size = (uint64_t)content_size;

   if (file_cb && !file_cb(ctx->inner_filename, valid_exts,
            NULL, 0,
            (uint32_t)state->archive_size, (uint32_t)content_size,
            0, userdata))
      return 0;

   return 1;
}

static void zstd_parse_file_free(void *context)
{
   struct zstd_file_context *ctx = (struct zstd_file_context*)context;
   if (!ctx)
      return;
   if (ctx->decompressed_data)
      free(ctx->decompressed_data);
   free(ctx);
}

static bool zstd_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size)
{
   struct zstd_file_context *ctx = (struct zstd_file_context*)context;
   if (!ctx)
      return false;
   return true;
}

static int zstd_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   struct zstd_file_context *ctx = (struct zstd_file_context*)context;
   void *compressed_data = NULL;
   size_t result;
   int    e;
   RFILE *file;

   if (!ctx || !handle)
      return -1;

   if (ctx->decompressed_data)
   {
      handle->data = ctx->decompressed_data;
      return 1;
   }

   /* Read the entire compressed file */
   file = filestream_open(ctx->archive_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return -1;

   compressed_data = malloc((size_t)ctx->archive_size);
   if (!compressed_data)
   {
      filestream_close(file);
      return -1;
   }

   if (filestream_read(file, compressed_data, ctx->archive_size)
         != ctx->archive_size)
   {
      free(compressed_data);
      filestream_close(file);
      return -1;
   }
   filestream_close(file);

   ctx->decompressed_data = (uint8_t*)malloc(ctx->decompressed_size);
   if (!ctx->decompressed_data)
   {
      free(compressed_data);
      return -1;
   }

#ifdef HAVE_RZSTD
   e = (rzstd_decode(ctx->decompressed_data, ctx->decompressed_size,
         compressed_data, (size_t)ctx->archive_size, &result)
         != RZSTD_PROCESS_END);
#else
   result = ZSTD_decompress(ctx->decompressed_data, ctx->decompressed_size,
         compressed_data, (size_t)ctx->archive_size);
   e      = ZSTD_isError(result);
#endif
   free(compressed_data);

   if (e)
   {
      free(ctx->decompressed_data);
      ctx->decompressed_data = NULL;
      return -1;
   }

   handle->data = ctx->decompressed_data;
   return 1;
}

/* Extract the file from a .zst archive (single-file container).
 * If needle doesn't match the derived inner filename, returns -1.
 * If optional_outfile is set, writes to that file instead of buf. */
static int64_t zstd_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   char inner_name[PATH_MAX_LENGTH];
   int64_t file_size;
   void *compressed_data    = NULL;
   uint8_t *decompressed    = NULL;
   int64_t            content_size;
   size_t result;
   int    e;
   RFILE *file;

   zstd_derive_inner_filename(path, inner_name, sizeof(inner_name));

   if (!string_is_equal(inner_name, needle))
      return -1;

   file = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return -1;

   file_size = filestream_get_size(file);
   if (file_size <= 0)
   {
      filestream_close(file);
      return -1;
   }

   compressed_data = malloc((size_t)file_size);
   if (!compressed_data)
   {
      filestream_close(file);
      return -1;
   }

   if (filestream_read(file, compressed_data, (int64_t)file_size) != file_size)
   {
      free(compressed_data);
      filestream_close(file);
      return -1;
   }

   filestream_close(file);

#ifdef HAVE_RZSTD
   content_size = rzstd_frame_content_size(compressed_data,
         (size_t)file_size);
#else
   content_size = (int64_t)ZSTD_getFrameContentSize(compressed_data,
         (size_t)file_size);
#endif
   if (  content_size == ZSTD_SIZE_UNKNOWN
      || content_size == ZSTD_SIZE_ERROR)
   {
      free(compressed_data);
      return -1;
   }

   /* Reject sizes that would overflow the "+1" NUL-byte allocation or
    * truncate when cast to size_t on 32-bit hosts.  Without this guard
    * content_size = 0xFFFFFFFF on a 32-bit host wraps the +1 to zero,
    * malloc(0) may return a non-NULL pointer, and the following
    * decode writes 4 GiB into it. */
   if ((uint64_t)content_size >= (uint64_t)SIZE_MAX)
   {
      free(compressed_data);
      return -1;
   }

   decompressed = (uint8_t*)malloc((size_t)(content_size + 1));
   if (!decompressed)
   {
      free(compressed_data);
      return -1;
   }

#ifdef HAVE_RZSTD
   e = (rzstd_decode(decompressed, (size_t)content_size,
         compressed_data, (size_t)file_size, &result)
         != RZSTD_PROCESS_END);
#else
   result = ZSTD_decompress(decompressed, (size_t)content_size,
         compressed_data, (size_t)file_size);
   e      = ZSTD_isError(result);
#endif
   free(compressed_data);

   if (e)
   {
      free(decompressed);
      return -1;
   }

   if (optional_outfile)
   {
      if (!filestream_write_file(optional_outfile, decompressed, (int64_t)result))
      {
         free(decompressed);
         return -1;
      }
      free(decompressed);
   }
   else
   {
      decompressed[result] = '\0';
      *buf = decompressed;
   }

   return (int64_t)result;
}

static uint32_t zstd_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t len)
{
   return encoding_crc32(crc, data, len);
}

const struct file_archive_file_backend zstd_backend = {
   zstd_parse_file_init,
   zstd_parse_file_iterate_step,
   zstd_parse_file_free,
   zstd_stream_decompress_data_to_file_init,
   zstd_stream_decompress_data_to_file_iterate,
   zstd_stream_crc32_calculate,
   zstd_file_read,
   "zstd"
};
