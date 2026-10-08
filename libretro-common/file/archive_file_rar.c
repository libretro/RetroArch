/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_rar.c).
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

#include <retro_posix_source.h>

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include <rar/rrar_archive.h>

/* The first six bytes of both containers: RAR 1.5 to 4.x carries 0x00
 * after them, RAR 5 0x01 0x00. rrar_archive_open() tells them apart. */
#define RAR_MAGIC     "Rar!\x1A\x07"
#define RAR_MAGIC_LEN 6

/* rrar_archive parses from a buffer rather than a stream.
 *
 * archive_file.c has already opened the file and, for archives up to
 * 256 MiB, mapped it before this backend is entered. Where that
 * mapping exists it is used directly, as the zip, 7z and zstd backends
 * do; only when there is none does this read the file itself, and
 * owns_data records which of the two happened. */
struct rar_context_t
{
   rrar_archive_t *archive;
   const uint8_t  *data;
   size_t          data_len;
   /* The last extracted member, owned here so the handle handed to the
    * caller stays valid until the next call. */
   uint8_t        *output;
   uint32_t        parse_index;
   uint32_t        decompress_index;
   int             owns_data;
};

/* Read a whole file into memory, for the no-mapping case only. */
static uint8_t *rar_slurp(const char *path, size_t *out_len)
{
   RFILE   *f;
   int64_t  len;
   uint8_t *buf;

   if (string_is_empty(path))
      return NULL;

   if (!(f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return NULL;

   filestream_seek(f, 0, SEEK_END);
   len = filestream_tell(f);
   filestream_seek(f, 0, SEEK_SET);

   if (     len <= 0
         || (uint64_t)len > (uint64_t)((size_t)-1)
         || !(buf = (uint8_t *)malloc((size_t)len)))
   {
      filestream_close(f);
      return NULL;
   }

   if (filestream_read(f, buf, (int64_t)len) != len)
   {
      free(buf);
      filestream_close(f);
      return NULL;
   }

   filestream_close(f);
   *out_len = (size_t)len;
   return buf;
}

static int rar_bind_data(struct rar_context_t *ctx,
      file_archive_transfer_t *state, const char *file)
{
#ifdef VFS_HAVE_FILE_MAPPING
   if (state->archive_mmap_data && state->archive_size > 0)
   {
      ctx->data      = state->archive_mmap_data;
      ctx->data_len  = (size_t)state->archive_size;
      ctx->owns_data = 0;
      return 1;
   }
#else
   (void)state;
#endif
   if (!(ctx->data = rar_slurp(file, &ctx->data_len)))
      return 0;
   ctx->owns_data = 1;
   return 1;
}

static void rar_parse_file_free(void *context)
{
   struct rar_context_t *ctx = (struct rar_context_t*)context;

   if (!ctx)
      return;

   free(ctx->output);
   rrar_archive_close(ctx->archive);
   /* The mapping belongs to archive_file.c and is unmapped there. */
   if (ctx->owns_data)
      free((void*)ctx->data);
   free(ctx);
}

/* State for the compressed_file_read driver below. */
typedef struct
{
   char   *opt_file;
   char   *needle;
   void  **buf;
   size_t  size;
   bool    found;
} rar_decomp_state_t;

/* Called once per entry while the driver walks the archive: takes the
 * wanted member and stops the scan. */
static int rar_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   rar_decomp_state_t *st = (rar_decomp_state_t*)userdata->cb_data;

   if (!name || !*name || !st || !st->needle)
      return 1;
   if (!string_is_equal(name, st->needle))
      return 1;

   if (st->opt_file)
   {
      if (file_archive_perform_mode_start(st->opt_file, valid_exts,
               cdata, cmode, csize, size, crc32, userdata) == -1)
         return 1;
      st->size  = size;
      st->found = true;
      return 0;
   }

   {
      struct rar_context_t *ctx =
         (struct rar_context_t*)userdata->transfer->context;
      uint8_t *member     = NULL;
      size_t   member_len = 0;

      if (     !ctx || !ctx->archive
            || rrar_archive_extract(ctx->archive, (uint32_t)(size_t)cdata,
                  &member, &member_len) != RRAR_OK)
         return 1;

      /* RetroArch expects a NUL after the data. */
      if ((*st->buf = malloc(member_len + 1)))
      {
         ((char*)(*st->buf))[member_len] = '\0';
         if (member_len)
            memcpy(*st->buf, member, member_len);
         st->size  = member_len;
         st->found = true;
      }
      free(member);
   }

   return 0;
}

/* Extract the relative path (needle) from a RAR archive (path) into an
 * allocated buffer, or to optional_outfile when that is set. Driven
 * through file_archive_parse_file_iterate(), as the zip and 7z
 * backends are. */
static int64_t rar_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   file_archive_transfer_t state;
   rar_decomp_state_t decomp;
   struct archive_extract_userdata userdata;
   int ret = 0;

   memset(&state,    0, sizeof(state));
   memset(&decomp,   0, sizeof(decomp));
   memset(&userdata, 0, sizeof(userdata));

   if (needle)
      decomp.needle   = strdup(needle);
   if (optional_outfile)
      decomp.opt_file = strdup(optional_outfile);

   if (     (needle && !decomp.needle)
         || (optional_outfile && !decomp.opt_file))
   {
      free(decomp.needle);
      free(decomp.opt_file);
      return -1;
   }

   state.type        = ARCHIVE_TRANSFER_INIT;
   userdata.transfer = &state;
   userdata.cb_data  = &decomp;
   decomp.buf        = buf;

   do
   {
      bool returnerr = true;
      ret = file_archive_parse_file_iterate(&state, &returnerr, path,
            "", rar_file_decompressed, &userdata);
      if (!returnerr)
         break;
   } while (ret == 0 && (!decomp.found || state.pending_active));

   file_archive_parse_file_iterate_stop(&state);

   free(decomp.opt_file);
   free(decomp.needle);

   if (!decomp.found)
      return -1;

   return (int64_t)decomp.size;
}

static bool rar_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size)
{
   struct rar_context_t *ctx = (struct rar_context_t*)context;

   (void)handle;
   (void)cmode;
   (void)csize;
   (void)size;

   if (!ctx)
      return false;

   /* The parse step hands the entry index through cdata. */
   ctx->decompress_index = (uint32_t)(size_t)cdata;
   return true;
}

/* 1 when the member is in handle->data, -1 on failure: the member is
 * unpacked whole, so there is no "call again". */
static int rar_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   struct rar_context_t *ctx = (struct rar_context_t*)context;
   uint8_t *member     = NULL;
   size_t   member_len = 0;

   if (     !ctx || !ctx->archive
         || rrar_archive_extract(ctx->archive, ctx->decompress_index,
               &member, &member_len) != RRAR_OK)
      return -1;

   /* The caller reads through handle->data until it calls back in, so
    * the previous member cannot be released any earlier than this. */
   free(ctx->output);
   ctx->output = member;

   if (handle)
      handle->data = ctx->output;

   return 1;
}

static int rar_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   uint8_t magic_buf[RAR_MAGIC_LEN];
   struct rar_context_t *ctx = NULL;

   if (state->archive_size < RRAR_SIGNATURE_SIZE)
      goto error;

   filestream_seek(state->archive_file, 0, SEEK_SET);
   if (filestream_read(state->archive_file, magic_buf, RAR_MAGIC_LEN)
         != RAR_MAGIC_LEN)
      goto error;

   if (memcmp(magic_buf, RAR_MAGIC, RAR_MAGIC_LEN) != 0)
      goto error;

   if (!(ctx = (struct rar_context_t*)calloc(1, sizeof(*ctx))))
      goto error;

   state->context = ctx;

   if (!rar_bind_data(ctx, state, file))
      goto error;

   if (rrar_archive_open(&ctx->archive, ctx->data, ctx->data_len)
         != RRAR_OK)
      goto error;

   state->step_total = rrar_archive_num_entries(ctx->archive);

   return 0;

error:
   if (ctx)
      rar_parse_file_free(ctx);
   state->context = NULL;
   return -1;
}

static int rar_parse_file_iterate_step(void *context,
      const char *valid_exts,
      struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb)
{
   const rrar_entry_t *entry;
   struct rar_context_t *ctx = (struct rar_context_t*)context;

   userdata->current_file_path[0] = '\0';

   if (!ctx || !ctx->archive)
      return -1;

   if (ctx->parse_index >= rrar_archive_num_entries(ctx->archive))
      return 0;

   entry = rrar_archive_entry(ctx->archive, ctx->parse_index);

   /* Directories, members this reader cannot unpack (solid, encrypted,
    * an older packer) and members of 4 GiB or more, which the uint32_t
    * sizes here cannot describe, are passed over: the rest of the
    * archive stays usable. */
   if (     entry
         && !entry->is_dir
         && entry->supported
         && entry->size <= (uint64_t)0xFFFFFFFFu)
   {
      uint32_t size = (uint32_t)entry->size;

      strlcpy(userdata->current_file_path, entry->name,
            sizeof(userdata->current_file_path));
      userdata->crc  = entry->crc;
      userdata->size = size;

      /* The entry index travels in cdata; the packed size is reported
       * only for progress. */
      if (file_cb && !file_cb(userdata->current_file_path, valid_exts,
               (const uint8_t*)(size_t)ctx->parse_index, 0,
               entry->packed_size > (uint64_t)0xFFFFFFFFu
               ? 0xFFFFFFFFu : (uint32_t)entry->packed_size,
               size, entry->crc, userdata))
         return 0;
   }

   ctx->parse_index++;
   return 1;
}

const struct file_archive_file_backend rar_backend = {
   rar_parse_file_init,
   rar_parse_file_iterate_step,
   rar_parse_file_free,
   rar_stream_decompress_data_to_file_init,
   rar_stream_decompress_data_to_file_iterate,
   encoding_crc32,
   rar_file_read,
   "rar"
};
