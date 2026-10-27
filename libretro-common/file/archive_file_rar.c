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

/* archive_file.c has already opened the file and, where it could,
 * mapped it before this backend is entered. Where that mapping exists
 * rrar_archive parses it in place, as the zip, 7z and zstd backends
 * do. Without one (no mapping on the platform, a network share, an
 * archive over a 32-bit host's ceiling) it reads the open stream through
 * file_archive_read_at(): headers at open, a member's packed bytes
 * only when that member is extracted. */
struct rar_context_t
{
   rrar_archive_t *archive;
   file_archive_transfer_t *state;
   /* The last extracted member, owned here so the handle handed to the
    * caller stays valid until the next call. */
   uint8_t        *output;
   uint32_t        parse_index;
   uint32_t        decompress_index;
   bool            to_sink;   /* the member goes to the file as it comes */
};

/* A member bound for a file, bigger than this, goes to it as it is
 * unpacked rather than whole through memory */
#define RAR_SINK_OVER (256 * 1024)

static int rar_sink_write(void *ud, const uint8_t *data, size_t len)
{
   return filestream_write((RFILE*)ud, data, (int64_t)len) != (int64_t)len;
}

static int rar_crc_write(void *ud, const uint8_t *data, size_t len)
{
   *(uint32_t*)ud = encoding_crc32(*(uint32_t*)ud, data, len);
   return 0;
}

static int rar_open_archive(struct rar_context_t *ctx,
      file_archive_transfer_t *state)
{
#ifdef VFS_HAVE_FILE_MAPPING
   if (state->archive_mmap_data && state->archive_size > 0)
      return rrar_archive_open(&ctx->archive, state->archive_mmap_data,
            (size_t)state->archive_size) == RRAR_OK;
#endif
   return rrar_archive_open_read(&ctx->archive,
         (uint64_t)state->archive_size,
         file_archive_read_at, state->archive_file) == RRAR_OK;
}

static void rar_parse_file_free(void *context)
{
   struct rar_context_t *ctx = (struct rar_context_t*)context;

   if (!ctx)
      return;

   free(ctx->output);
   /* The mapping and the stream belong to archive_file.c, which
    * releases them after this. */
   rrar_archive_close(ctx->archive);
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
      uint64_t csize, uint64_t size,
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

      /* the caller's to keep, with a NUL after the data in the octet
       * to spare */
      member[member_len] = '\0';
      *st->buf  = member;
      st->size  = member_len;
      st->found = true;
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
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size)
{
   struct rar_context_t *ctx = (struct rar_context_t*)context;

   (void)handle;
   (void)cmode;
   (void)csize;

   if (!ctx)
      return false;

   /* The parse step hands the entry index through cdata. */
   ctx->decompress_index = (uint32_t)(size_t)cdata;
   ctx->to_sink          = ctx->state->pending_sink && size > RAR_SINK_OVER;
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

   if (!ctx || !ctx->archive)
      return -1;

   /* in the file as it comes, whole */
   if (ctx->to_sink)
   {
      if (rrar_archive_extract_sink(ctx->archive, ctx->decompress_index,
               rar_sink_write, ctx->state->pending_sink) != RRAR_OK)
         return -1;
      if (handle)
         handle->data = NULL;
      return 1;
   }

   if (rrar_archive_extract(ctx->archive, ctx->decompress_index,
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

   (void)file;

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
   ctx->state     = state;

   if (!rar_open_archive(ctx, state))
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

   /* Directories and members this reader cannot unpack (solid,
    * encrypted, an older packer) are passed over: the rest of the
    * archive stays usable. */
   if (entry && !entry->is_dir && entry->supported)
   {
      strlcpy(userdata->current_file_path, entry->name,
            sizeof(userdata->current_file_path));
      userdata->crc  = entry->crc;
      userdata->size = entry->size;

      /* A RAR 5 member may have a BLAKE2 hash in place of its CRC */
      if (userdata->crc_wanted && !entry->has_crc)
      {
         userdata->crc = 0;
         if (rrar_archive_extract_sink(ctx->archive, ctx->parse_index,
                  rar_crc_write, &userdata->crc) != RRAR_OK)
            return -1;
      }

      /* The entry index travels in cdata; the packed size is reported
       * only for progress. */
      if (file_cb && !file_cb(userdata->current_file_path, valid_exts,
               (const uint8_t*)(size_t)ctx->parse_index, 0,
               entry->packed_size, entry->size, userdata->crc, userdata))
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
