/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_bz2.c).
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

/* .bz2 files: one bzip2 stream, standing for the file named as the
 * .bz2 is without its extension. A stream does not say how long it
 * decodes to, so it is decoded when it is listed, a block at a time,
 * for its size and CRC; a member that comes to no more than
 * ARCHIVE_BZ2_KEEP octets is kept for the extraction that usually
 * follows, a bigger one is decoded again when it is extracted, to its
 * file a block at a time. The stream is decoded where the file stream
 * lends it whole, or else read through a ring that holds at least the
 * most a block can take ahead of the decoder. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <encodings/crc32.h>
#include <encodings/rbzip2.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#ifndef ARCHIVE_BZ2_KEEP
#define ARCHIVE_BZ2_KEEP  (8 * 1024 * 1024)
#endif

/* How far the stream is read ahead of the decoder, in a ring of twice
 * it */
#ifndef ARCHIVE_BZ2_AHEAD
#define ARCHIVE_BZ2_AHEAD RBZIP2_AHEAD
#endif

/* The stream, a block at a time */
struct bz2_src
{
   rbzip2_t *bz;
   RFILE    *file;
   uint8_t  *ring;      /* where the stream is not lent whole */
   uint64_t  at;        /* read into the ring up to here */
   uint64_t  size;
   size_t    cap;       /* the ring's */
};

struct bz2_file_context
{
   file_archive_transfer_t *state;
   struct bz2_src src;
   uint8_t *data;             /* the member, kept or decoded whole */
   size_t   size;
   char     inner_filename[PATH_MAX_LENGTH];
   bool     parsed;
   bool     streaming;        /* extracting to the sink a block a call */
};

/* The member's name: the .bz2's without its extension */
static void bz2_inner_filename(const char *path, char *s, size_t len)
{
   char *ext;
   strlcpy(s, path_basename(path), len);
   if ((ext = strrchr(s, '.')) && string_is_equal_noncase(ext, ".bz2"))
      *ext = '\0';
}

/* The ring read on, up to a whole ring past @taken or the end */
static bool bz2_fill(struct bz2_src *s, uint64_t taken)
{
   uint64_t to = taken + s->cap;
   if (to > s->size)
      to = s->size;
   filestream_seek(s->file, (int64_t)s->at, RETRO_VFS_SEEK_POSITION_START);
   while (s->at < to)
   {
      size_t at = (size_t)(s->at % s->cap);
      size_t n  = s->cap - at;
      if (n > to - s->at)
         n = (size_t)(to - s->at);
      if (filestream_read(s->file, s->ring + at, (int64_t)n) != (int64_t)n)
         return false;
      s->at += n;
   }
   return true;
}

static bool bz2_src_begin(struct bz2_src *s, RFILE *file, int64_t size)
{
   const uint8_t *src = NULL;
   size_t         cap = 2 * (size_t)ARCHIVE_BZ2_AHEAD;

   if (size < 4 || (uint64_t)size > (uint64_t)((size_t)-1))
      return false;
   if (!s->bz && !(s->bz = rbzip2_new()))
      return false;
   s->file = file;
   s->size = (uint64_t)size;
   s->at   = 0;
   filestream_seek(file, 0, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_view(file, size, &src) == size)
   {
      free(s->ring);
      s->ring = NULL;
      return rbzip2_begin_blocks(s->bz, src, (size_t)size) == RBZIP2_OK;
   }
   if ((uint64_t)size < (uint64_t)cap)
      cap = (size_t)size;
   if (!s->ring || s->cap < cap)
   {
      free(s->ring);
      if (!(s->ring = (uint8_t*)malloc(cap)))
         return false;
   }
   s->cap = cap;
   return   bz2_fill(s, 0)
         && rbzip2_begin_ring(s->bz, s->ring, (size_t)s->at, NULL, 0)
            == RBZIP2_OK;
}

/* The next block: RBZIP2_PENDING with it, RBZIP2_OK at the stream's
 * end, or an error */
static int bz2_src_next(struct bz2_src *s, const uint8_t **blk, size_t *len)
{
   if (     s->ring
         && s->at < s->size
         && s->at - rbzip2_taken(s->bz) < ARCHIVE_BZ2_AHEAD)
   {
      /* the ring filled again up to what the decoder still needs, and
       * that handed over again, in two where it wraps */
      uint64_t t    = rbzip2_taken(s->bz);
      size_t   from = (size_t)(t % s->cap);
      size_t   have;
      *blk = NULL;
      *len = 0;
      if (!bz2_fill(s, t))
         return RBZIP2_ERROR_DATA;
      have = (size_t)(s->at - t);
      if (have > s->cap - from)
         rbzip2_input(s->bz, s->ring + from, s->cap - from,
               s->ring, have - (s->cap - from));
      else
         rbzip2_input(s->bz, s->ring + from, have, NULL, 0);
   }
   return rbzip2_step_block(s->bz, blk, len);
}

static void bz2_src_free(struct bz2_src *s)
{
   rbzip2_free(s->bz);
   free(s->ring);
   s->bz   = NULL;
   s->ring = NULL;
}

/* What every block is handed to by bz2_pass() */
typedef bool (*bz2_sink_t)(void *ud, const uint8_t *blk, size_t len);

/* The whole stream of @file through @sink; false if it is not one */
static bool bz2_pass(struct bz2_src *s, RFILE *file, int64_t size,
      bz2_sink_t sink, void *ud)
{
   const uint8_t *blk;
   size_t         len;
   int            e;
   if (!bz2_src_begin(s, file, size))
      return false;
   while ((e = bz2_src_next(s, &blk, &len)) == RBZIP2_PENDING)
      if (!sink(ud, blk, len))
         return false;
   return e == RBZIP2_OK;
}

/* A member gathered whole: grown as it comes when its size is not
 * known, or put where it goes when it is; @max past which it is not
 * kept, which is not a failure */
struct bz2_whole
{
   uint8_t *data;
   size_t   len;
   size_t   cap;
   size_t   max;
   uint32_t crc;
   uint64_t total;
   bool     dropped;
};

static bool bz2_gather(void *ud, const uint8_t *blk, size_t len)
{
   struct bz2_whole *w = (struct bz2_whole*)ud;
   w->crc    = encoding_crc32(w->crc, blk, len);
   w->total += len;
   if (w->dropped)
      return true;
   if (len > w->max - w->len)
   {
      free(w->data);
      w->data    = NULL;
      w->dropped = true;
      return true;
   }
   if (len > w->cap - w->len)
   {
      /* the octet to spare after it, for a NUL */
      size_t   cap = w->cap ? w->cap + 1 : 65536;
      uint8_t *d;
      while (cap - 1 - w->len < len)
      {
         if (cap > ((size_t)-1) / 2)
            return false;
         cap *= 2;
      }
      if (!(d = (uint8_t*)realloc(w->data, cap)))
         return false;
      w->data = d;
      w->cap  = cap - 1;
   }
   memcpy(w->data + w->len, blk, len);
   w->len += len;
   return true;
}

static bool bz2_write(void *ud, const uint8_t *blk, size_t len)
{
   return filestream_write((RFILE*)ud, blk, (int64_t)len) == (int64_t)len;
}

static int bz2_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   uint8_t magic[4];
   struct bz2_file_context *ctx;

   state->context = NULL;
   if (state->archive_size < 4)
      return -1;
   filestream_seek(state->archive_file, 0, RETRO_VFS_SEEK_POSITION_START);
   if (     filestream_read(state->archive_file, magic, 4) != 4
         || memcmp(magic, "BZh", 3) || magic[3] < '1' || magic[3] > '9')
      return -1;
   if (!(ctx = (struct bz2_file_context*)calloc(1, sizeof(*ctx))))
      return -1;
   bz2_inner_filename(file, ctx->inner_filename, sizeof(ctx->inner_filename));
   ctx->state        = state;
   state->context    = ctx;
   state->step_total = 1;
   return 0;
}

static int bz2_parse_file_iterate_step(void *context,
      const char *valid_exts,
      struct archive_extract_userdata *userdata, file_archive_file_cb file_cb)
{
   struct bz2_file_context *ctx = (struct bz2_file_context*)context;
   struct bz2_whole         w;

   if (!ctx || ctx->parsed)
      return 0;
   ctx->parsed = true;

   memset(&w, 0, sizeof(w));
   w.max = ARCHIVE_BZ2_KEEP;
   if (     !bz2_pass(&ctx->src, ctx->state->archive_file,
               ctx->state->archive_size, bz2_gather, &w)
         || w.total > UINT32_MAX
         || w.total > (uint64_t)((size_t)-1))
   {
      free(w.data);
      return -1;
   }
   ctx->data = w.data;
   ctx->size = (size_t)w.total;

   strlcpy(userdata->current_file_path, ctx->inner_filename,
         sizeof(userdata->current_file_path));
   userdata->crc  = w.crc;
   userdata->size = w.total;

   if (file_cb && !file_cb(ctx->inner_filename, valid_exts, NULL, 0,
            (uint32_t)ctx->state->archive_size, (uint32_t)ctx->size,
            w.crc, userdata))
      return 0;
   return 1;
}

static void bz2_parse_file_free(void *context)
{
   struct bz2_file_context *ctx = (struct bz2_file_context*)context;
   if (!ctx)
      return;
   bz2_src_free(&ctx->src);
   free(ctx->data);
   free(ctx);
}

static bool bz2_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size)
{
   struct bz2_file_context *ctx = (struct bz2_file_context*)context;
   (void)handle;
   (void)cdata;
   (void)cmode;
   (void)csize;
   (void)size;
   if (!ctx)
      return false;
   ctx->streaming = false;
   if (ctx->data || !ctx->state->pending_sink)
      return true;
   ctx->streaming = true;
   return bz2_src_begin(&ctx->src, ctx->state->archive_file,
         ctx->state->archive_size);
}

/* The member kept from its listing; or to the file a block a call; or
 * else decoded whole, where it was listed too big to keep */
static int bz2_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   struct bz2_file_context *ctx = (struct bz2_file_context*)context;
   if (!ctx || !handle)
      return -1;
   if (ctx->streaming)
   {
      const uint8_t *blk;
      size_t         len;
      int            e = bz2_src_next(&ctx->src, &blk, &len);
      if (e == RBZIP2_PENDING)
         return bz2_write(ctx->state->pending_sink, blk, len) ? 0 : -1;
      ctx->streaming = false;
      handle->data   = NULL;
      return e == RBZIP2_OK ? 1 : -1;
   }
   if (!ctx->data)
   {
      struct bz2_whole w;
      memset(&w, 0, sizeof(w));
      w.max = (size_t)-1;
      if (     !bz2_pass(&ctx->src, ctx->state->archive_file,
                  ctx->state->archive_size, bz2_gather, &w)
            || w.total != ctx->size)
      {
         free(w.data);
         return -1;
      }
      ctx->data = w.data;
   }
   handle->data = ctx->data;
   return 1;
}

/* The member of @path, named @needle, into *@buf (NUL after it) or the
 * file @optional_outfile, a block at a time; returns its size, or -1. */
static int64_t bz2_file_read(const char *path, const char *needle,
      void **buf, const char *optional_outfile)
{
   char           inner[PATH_MAX_LENGTH];
   struct bz2_src src;
   bool           ok;
   int64_t        size = -1;
   RFILE         *file;

   bz2_inner_filename(path, inner, sizeof(inner));
   if (!string_is_equal(inner, needle))
      return -1;
   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               file_archive_open_hints(path_get_size(path)))))
      return -1;
   memset(&src, 0, sizeof(src));

   if (optional_outfile)
   {
      RFILE *out = filestream_open(optional_outfile,
            RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      ok = out && bz2_pass(&src, file, filestream_get_size(file),
            bz2_write, out);
      if (out)
      {
         if (ok)
            size = filestream_tell(out);
         if (filestream_close(out) != 0)
            size = -1;
         /* no part of a member that did not decode is left */
         if (size < 0)
            filestream_delete(optional_outfile);
      }
   }
   else
   {
      struct bz2_whole w;
      memset(&w, 0, sizeof(w));
      w.max = (size_t)-1;
      ok    = bz2_pass(&src, file, filestream_get_size(file), bz2_gather,
            &w);
      if (ok && (w.data || (w.data = (uint8_t*)malloc(1))))
      {
         w.data[w.len] = '\0';
         *buf          = w.data;
         size          = (int64_t)w.len;
      }
      else
         free(w.data);
   }
   bz2_src_free(&src);
   filestream_close(file);
   return size;
}

const struct file_archive_file_backend bz2_backend = {
   bz2_parse_file_init,
   bz2_parse_file_iterate_step,
   bz2_parse_file_free,
   bz2_stream_decompress_data_to_file_init,
   bz2_stream_decompress_data_to_file_iterate,
   encoding_crc32,
   bz2_file_read,
   "bzip2"
};
