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

/* Bigger than this, a file bound for disk is written out a block at a
 * time */
#define ZSTD_STREAM_OVER   (256 * 1024)

struct zstd_file_context
{
   file_archive_transfer_t *state;
   uint8_t *decompressed_data;
   rzstd_blocks_t *zs;      /* the frame going out a block at a time */
   uint8_t *in;             /* a block of it read, where it is not lent */
   int64_t in_at;           /* where the next is read from */
   char inner_filename[PATH_MAX_LENGTH];
   int64_t archive_size;
   size_t decompressed_size;
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

/* The frame to be decoded a block at a time: from where the stream lends
 * it whole, or else read a block at a time into a buffer of one */
static bool zstd_src_begin(struct zstd_file_context *ctx)
{
   RFILE         *f   = ctx->state->archive_file;
   const uint8_t *src = NULL;

   rzstd_blocks_free(ctx->zs);
   free(ctx->in);
   ctx->zs    = NULL;
   ctx->in    = NULL;
   ctx->in_at = 0;
   if ((uint64_t)ctx->archive_size <= (uint64_t)((size_t)-1))
   {
      filestream_seek(f, 0, RETRO_VFS_SEEK_POSITION_START);
      if (filestream_view(f, ctx->archive_size, &src) == ctx->archive_size)
         return (ctx->zs = rzstd_blocks_begin(src,
                  (size_t)ctx->archive_size)) != NULL;
   }
   return    (ctx->in = (uint8_t*)malloc(RZSTD_WANT_MAX))
          && (ctx->zs = rzstd_blocks_open());
}

/* The frame's next decoded octets: RZSTD_PROCESS_NEXT with some, or
 * RZSTD_PROCESS_END or RZSTD_PROCESS_ERROR */
static int zstd_src_next(struct zstd_file_context *ctx,
      const uint8_t **data, size_t *len)
{
   RFILE *f = ctx->state->archive_file;

   *data = NULL;
   *len  = 0;
   if (!ctx->in)
      return rzstd_blocks_step(ctx->zs, data, len);
   for (;;)
   {
      size_t want = rzstd_blocks_want(ctx->zs);
      /* all of it out, and nothing after it */
      if (!want)
         return ctx->in_at == ctx->archive_size
            ? RZSTD_PROCESS_END : RZSTD_PROCESS_ERROR;
      if (want > RZSTD_WANT_MAX)
         return RZSTD_PROCESS_ERROR;
      filestream_seek(f, ctx->in_at, RETRO_VFS_SEEK_POSITION_START);
      if (filestream_read(f, ctx->in, (int64_t)want) != (int64_t)want)
         return RZSTD_PROCESS_ERROR;
      ctx->in_at += (int64_t)want;
      if (rzstd_blocks_give(ctx->zs, ctx->in, want, data, len)
            != RZSTD_PROCESS_NEXT)
         return RZSTD_PROCESS_ERROR;
      if (*len)
         return RZSTD_PROCESS_NEXT;
   }
}

static void zstd_src_end(struct zstd_file_context *ctx)
{
   rzstd_blocks_free(ctx->zs);
   free(ctx->in);
   ctx->zs = NULL;
   ctx->in = NULL;
}

/* The CRC of what the frame holds, decoded a block at a time */
static bool zstd_frame_crc(struct zstd_file_context *ctx, uint32_t *crc)
{
   const uint8_t *blk;
   size_t         len;
   int            e = RZSTD_PROCESS_ERROR;

   *crc = 0;
   if (zstd_src_begin(ctx))
      while ((e = zstd_src_next(ctx, &blk, &len)) == RZSTD_PROCESS_NEXT)
         *crc = encoding_crc32(*crc, blk, len);
   zstd_src_end(ctx);
   return e == RZSTD_PROCESS_END;
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

   /* decompressed_size feeds a later malloc; reject values that would
    * truncate on a 32-bit host and mismatch the length the decode
    * actually produces. */
   if ((uint64_t)content_size > (uint64_t)SIZE_MAX)
      return -1;

   ctx->decompressed_size = (size_t)content_size;

   strlcpy(userdata->current_file_path, ctx->inner_filename,
         sizeof(userdata->current_file_path));

   userdata->crc  = 0;
   userdata->size = (uint64_t)content_size;

   /* A frame holds no CRC of its content */
   if (userdata->crc_wanted && !zstd_frame_crc(ctx, &userdata->crc))
      return -1;

   if (file_cb && !file_cb(ctx->inner_filename, valid_exts,
            NULL, 0,
            (uint64_t)state->archive_size, (uint64_t)content_size,
            userdata->crc, userdata))
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
   rzstd_blocks_free(ctx->zs);
   free(ctx->in);
   free(ctx);
}

static bool zstd_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size)
{
   struct zstd_file_context *ctx = (struct zstd_file_context*)context;
   if (!ctx)
      return false;
   if (!ctx->state->pending_sink || size <= ZSTD_STREAM_OVER)
      return true;
   return zstd_src_begin(ctx);
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

   if (ctx->zs)
   {
      /* a block to the file per call */
      const uint8_t *blk;
      size_t         len;
      e = zstd_src_next(ctx, &blk, &len);
      if (     e == RZSTD_PROCESS_ERROR
            || (len && filestream_write(ctx->state->pending_sink, blk,
                  (int64_t)len) != (int64_t)len))
         return -1;
      if (e == RZSTD_PROCESS_NEXT)
         return 0;
      zstd_src_end(ctx);
      handle->data = NULL;
      return 1;
   }

   if (ctx->decompressed_data)
   {
      handle->data = ctx->decompressed_data;
      return 1;
   }

   if (!(src = file_archive_whole(ctx->state->archive_file, ctx->archive_size,
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
         || !(src = file_archive_whole(file, file_size, &owned)))
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
