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
 * decodes to, so its member is decoded when it is listed, its size
 * and CRC are then known, and what was decoded is kept for the
 * extraction that usually follows. */

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

struct bz2_file_context
{
   file_archive_transfer_t *state;
   uint8_t *data;             /* the member, once decoded */
   size_t   size;
   char     inner_filename[PATH_MAX_LENGTH];
   bool     parsed;
};

/* The member's name: the .bz2's without its extension */
static void bz2_inner_filename(const char *path, char *s, size_t len)
{
   char *ext;
   strlcpy(s, path_basename(path), len);
   if ((ext = strrchr(s, '.')) && string_is_equal_noncase(ext, ".bz2"))
      *ext = '\0';
}

/* All of @file decoded into *@out (@size octets in it) */
static int bz2_decode_file(RFILE *file, int64_t size, uint8_t **out,
      size_t *out_len)
{
   void          *owned = NULL;
   const uint8_t *src;
   rbzip2_t      *bz;
   int            e = RBZIP2_ERROR_DATA;

   *out     = NULL;
   *out_len = 0;
   if (size <= 0 || !(bz = rbzip2_new()))
      return -1;
   if ((src = file_archive_whole(file, size, &owned)))
      e = rbzip2_decode_alloc(bz, src, (size_t)size, out, out_len);
   free(owned);
   rbzip2_free(bz);
   return e == RBZIP2_OK ? 0 : -1;
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
   uint32_t crc;

   if (!ctx || ctx->parsed)
      return 0;
   ctx->parsed = true;

   if (     bz2_decode_file(ctx->state->archive_file,
               ctx->state->archive_size, &ctx->data, &ctx->size) != 0
         || ctx->size > UINT32_MAX)
      return -1;

   crc = encoding_crc32(0, ctx->data, ctx->size);
   strlcpy(userdata->current_file_path, ctx->inner_filename,
         sizeof(userdata->current_file_path));
   userdata->crc  = crc;
   userdata->size = (uint64_t)ctx->size;

   if (file_cb && !file_cb(ctx->inner_filename, valid_exts, NULL, 0,
            (uint32_t)ctx->state->archive_size, (uint32_t)ctx->size,
            crc, userdata))
      return 0;
   return 1;
}

static void bz2_parse_file_free(void *context)
{
   struct bz2_file_context *ctx = (struct bz2_file_context*)context;
   if (!ctx)
      return;
   free(ctx->data);
   free(ctx);
}

static bool bz2_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size)
{
   (void)handle;
   (void)cdata;
   (void)cmode;
   (void)csize;
   (void)size;
   return context != NULL;
}

/* The member was decoded when it was listed */
static int bz2_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   struct bz2_file_context *ctx = (struct bz2_file_context*)context;
   if (!ctx || !handle || !ctx->data)
      return -1;
   handle->data = ctx->data;
   return 1;
}

/* The member of @path, named @needle, into *@buf (NUL after it) or the
 * file @optional_outfile; returns its size, or -1. */
static int64_t bz2_file_read(const char *path, const char *needle,
      void **buf, const char *optional_outfile)
{
   char     inner[PATH_MAX_LENGTH];
   uint8_t *data = NULL;
   size_t   size = 0;
   RFILE   *file;
   int      e;

   bz2_inner_filename(path, inner, sizeof(inner));
   if (!string_is_equal(inner, needle))
      return -1;
   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               file_archive_open_hints(path_get_size(path)))))
      return -1;
   e = bz2_decode_file(file, filestream_get_size(file), &data, &size);
   filestream_close(file);
   if (e != 0)
      return -1;

   if (optional_outfile)
   {
      bool ok = filestream_write_file(optional_outfile, data, (int64_t)size);
      free(data);
      return ok ? (int64_t)size : -1;
   }
   data[size] = '\0';       /* rbzip2_decode_alloc() leaves the room */
   *buf       = data;
   return (int64_t)size;
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
