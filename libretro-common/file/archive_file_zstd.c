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
#include <encodings/rzstd.h>

/* One spelling for whichever decoder is compiled in. */
#define ZSTD_FRAME_HEADER_MAX RZSTD_FRAME_HEADER_MAX
#define ZSTD_SIZE_UNKNOWN     RZSTD_CONTENT_SIZE_UNKNOWN
#define ZSTD_SIZE_ERROR       RZSTD_CONTENT_SIZE_ERROR

#define ZSTD_MAGIC         "\x28\xB5\x2F\xFD"
#define ZSTD_MAGIC_LEN     4

struct zstd_file_context
{
   file_archive_transfer_t *state;
   uint8_t *decompressed_data;
   char inner_filename[PATH_MAX_LENGTH];
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

/* The whole of @file, lent where the stream can (a mapping, a
 * read-ahead window), else read into *@owned for the caller to free */
static const uint8_t *zstd_whole(RFILE *file, int64_t size, void **owned)
{
   const uint8_t *src = NULL;
   *owned = NULL;
   filestream_seek(file, 0, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_view(file, size, &src) == size)
      return src;
   if (!(*owned = malloc((size_t)size)))
      return NULL;
   filestream_seek(file, 0, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_read(file, *owned, size) != size)
   {
      free(*owned);
      *owned = NULL;
      return NULL;
   }
   return (const uint8_t*)*owned;
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
   ctx->archive_size = state->archive_size;
   ctx->state        = state;

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

#ifdef VFS_HAVE_FILE_MAPPING
   if (state->archive_mmap_data)
   {
      content_size = rzstd_frame_content_size(
            state->archive_mmap_data, (size_t)state->archive_size);
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
      content_size = rzstd_frame_content_size(header_buf, (size_t)to_read);
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
   void          *owned = NULL;
   const uint8_t *src;
   size_t result;
   int    e;

   if (!ctx || !handle)
      return -1;

   if (ctx->decompressed_data)
   {
      handle->data = ctx->decompressed_data;
      return 1;
   }

   if (!(src = zstd_whole(ctx->state->archive_file, ctx->archive_size,
               &owned)))
      return -1;

   ctx->decompressed_data = (uint8_t*)malloc(ctx->decompressed_size);
   if (!ctx->decompressed_data)
   {
      free(owned);
      return -1;
   }

   e = (rzstd_decode(ctx->decompressed_data, ctx->decompressed_size,
         src, (size_t)ctx->archive_size, &result)
         != RZSTD_PROCESS_END);
   free(owned);

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
   int64_t        file_size;
   int64_t        content_size;
   void          *owned        = NULL;
   const uint8_t *src;
   uint8_t       *decompressed = NULL;
   size_t         result       = 0;
   int            e            = 1;
   RFILE         *file;

   zstd_derive_inner_filename(path, inner_name, sizeof(inner_name));

   if (!string_is_equal(inner_name, needle))
      return -1;

   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               file_archive_open_hints(path_get_size(path)))))
      return -1;

   /* The frame is decoded where the stream lends it, so the stream
    * stays open until it is */
   if (     (file_size = filestream_get_size(file)) <= 0
         || !(src = zstd_whole(file, file_size, &owned)))
      goto done;

   content_size = rzstd_frame_content_size(src, (size_t)file_size);
   if (  content_size == ZSTD_SIZE_UNKNOWN
      || content_size == ZSTD_SIZE_ERROR)
      goto done;

   /* Reject sizes that would overflow the "+1" NUL-byte allocation or
    * truncate when cast to size_t on 32-bit hosts.  Without this guard
    * content_size = 0xFFFFFFFF on a 32-bit host wraps the +1 to zero,
    * malloc(0) may return a non-NULL pointer, and the following
    * decode writes 4 GiB into it. */
   if ((uint64_t)content_size >= (uint64_t)SIZE_MAX)
      goto done;

   if (!(decompressed = (uint8_t*)malloc((size_t)(content_size + 1))))
      goto done;

   e = (rzstd_decode(decompressed, (size_t)content_size,
         src, (size_t)file_size, &result)
         != RZSTD_PROCESS_END);

done:
   free(owned);
   filestream_close(file);

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

const struct file_archive_file_backend zstd_backend = {
   zstd_parse_file_init,
   zstd_parse_file_iterate_step,
   zstd_parse_file_free,
   zstd_stream_decompress_data_to_file_init,
   zstd_stream_decompress_data_to_file_iterate,
   encoding_crc32,
   zstd_file_read,
   "zstd"
};
