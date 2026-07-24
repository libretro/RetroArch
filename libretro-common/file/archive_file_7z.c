/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_7z.c).
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

#include <boolean.h>
#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>
#include <encodings/crc32.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <compat/strl.h>

#include <7z/r7z_archive.h>

#define SEVENZIP_MAGIC "7z\xBC\xAF\x27\x1C"
#define SEVENZIP_MAGIC_LEN 6

/* r7z_archive parses from a buffer rather than a stream.
 *
 * archive_file.c has already opened the file and, for archives up to
 * 256 MiB, mmapped it before this backend is entered. Where that
 * mapping exists it is used directly, exactly as the zip and zstd
 * backends do; reading the file again would mean a second full copy of
 * the archive alongside the mapping. Only when there is no mapping
 * (mmap unavailable, or an archive over the cap) does this read the
 * file itself, and `owns_data` records which of the two happened. */
struct sevenzip_context_t
{
   r7z_archive_t *archive;
   const uint8_t *data;
   size_t         data_len;
   int            owns_data;
   /* Cache of the last extracted member, owned here so the handle
    * handed to the caller stays valid until the next call. */
   uint8_t       *output;
   uint32_t       parse_index;
   uint32_t       decompress_index;
};

/* Read a whole file into memory, for the no-mapping case only.
 * Returns NULL on any failure. */
static uint8_t *sevenzip_slurp(const char *path, size_t *out_len)
{
   RFILE   *f;
   int64_t  len;
   uint8_t *buf;

   if (string_is_empty(path))
      return NULL;

   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f)
      return NULL;

   filestream_seek(f, 0, SEEK_END);
   len = filestream_tell(f);
   filestream_seek(f, 0, SEEK_SET);

   if (len <= 0 || (uint64_t)len > (uint64_t)((size_t)-1))
   {
      filestream_close(f);
      return NULL;
   }

   if (!(buf = (uint8_t *)malloc((size_t)len)))
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

/* Point the context at the archive bytes, preferring the mapping the
 * caller already made. */
static int sevenzip_bind_data(struct sevenzip_context_t *ctx,
      file_archive_transfer_t *state, const char *file)
{
#ifdef HAVE_MMAP
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

   {
      uint8_t *buf = sevenzip_slurp(file, &ctx->data_len);
      if (!buf)
         return 0;
      ctx->data      = buf;
      ctx->owns_data = 1;
   }
   return 1;
}

/* Convert a stored UTF-16 name into the caller's char buffer. */
static bool sevenzip_name_to_char(const uint16_t *name, char *s, size_t len)
{
   if (!name)
      return false;
   return utf16_to_char_string(name, s, len) ? true : false;
}

static void sevenzip_parse_file_free(void *context)
{
   struct sevenzip_context_t *sevenzip_context =
      (struct sevenzip_context_t*)context;

   if (!sevenzip_context)
      return;

   free(sevenzip_context->output);
   r7z_archive_close(sevenzip_context->archive);
   /* Only free what this backend allocated; the mapping belongs to
    * archive_file.c and is unmapped there. */
   if (sevenzip_context->owns_data)
      free((void*)sevenzip_context->data);
   free(sevenzip_context);
}

/* State for the compressed_file_read driver below. */
typedef struct
{
   char   *opt_file;
   char   *needle;
   void  **buf;
   size_t  size;
   bool    found;
} sevenzip_decomp_state_t;

/* Called once per entry while the driver walks the archive. Starts the
 * decode of the wanted member and stops the scan; the iterate loop
 * finishes the decode across later calls. */
static int sevenzip_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   sevenzip_decomp_state_t *st =
      (sevenzip_decomp_state_t*)userdata->cb_data;

   if (!name || !*name || !st || !st->needle)
      return 1;
   if (!string_is_equal(name, st->needle))
      return 1;   /* keep looking */

   if (st->opt_file)
   {
      /* Decode straight to the requested path. */
      if (file_archive_perform_mode_start(st->opt_file, valid_exts,
               cdata, cmode, csize, size, crc32, userdata) == -1)
         return 1;
      st->size  = size;
      st->found = true;
      return 0;
   }

   /* Decode into memory. perform_mode writes to a file, so this path
    * cannot use it; take the member directly and stop. */
   {
      struct sevenzip_context_t *ctx =
         (struct sevenzip_context_t*)userdata->transfer->context;
      uint8_t *member     = NULL;
      size_t   member_len = 0;
      int      res;

      if (!ctx || !ctx->archive)
         return 1;

      /* Sliced, but driven to completion here: the caller of
       * compressed_file_read wants the bytes on return and has nowhere
       * to yield to. */
      do
      {
         res = r7z_archive_extract_slice(ctx->archive,
               (uint32_t)(size_t)cdata, &member, &member_len);
      } while (res == R7Z_PENDING);

      if (res != R7Z_OK)
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

/* Extract the relative path (needle) from a 7z archive
 * (path) and allocate a buf for it to write it in.
 * If optional_outfile is set, extract to that instead
 * and don't allocate buffer.
 *
 * Driven through file_archive_parse_file_iterate() rather than
 * reimplementing the walk, which is how the zip backend does it. That
 * inherits the archive mapping, the per-entry stepping and the shared
 * teardown instead of duplicating all three.
 */
static int64_t sevenzip_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   file_archive_transfer_t state;
   sevenzip_decomp_state_t decomp;
   struct archive_extract_userdata userdata;
   int ret = 0;

   memset(&state,    0, sizeof(state));
   memset(&decomp,   0, sizeof(decomp));
   memset(&userdata, 0, sizeof(userdata));

   if (needle)
      decomp.needle   = strdup(needle);
   if (optional_outfile)
      decomp.opt_file = strdup(optional_outfile);

   /* A needle that failed to duplicate would make the callback compare
    * against NULL. Bail rather than risk it. */
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
            "", sevenzip_file_decompressed, &userdata);
      if (!returnerr)
         break;
      /* found is set when the decode *starts*, and the member may
       * still be parked in the transfer. Keep ticking until it has
       * drained, or the file mode would stop before anything was
       * written. */
   } while (ret == 0 && (!decomp.found || state.pending_active));

   file_archive_parse_file_iterate_stop(&state);

   free(decomp.opt_file);
   free(decomp.needle);

   if (!decomp.found)
      return -1;

   return (int64_t)decomp.size;
}

static bool sevenzip_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size)
{
   struct sevenzip_context_t *sevenzip_context =
      (struct sevenzip_context_t*)context;

   (void)handle;
   (void)cmode;
   (void)csize;
   (void)size;

   if (!sevenzip_context)
      return false;

   /* The parse step hands the entry index through cdata. */
   sevenzip_context->decompress_index = (uint32_t)(size_t)cdata;

   return true;
}

static int sevenzip_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   struct sevenzip_context_t *sevenzip_context =
      (struct sevenzip_context_t*)context;
   uint8_t *member     = NULL;
   size_t   member_len = 0;
   int      res;

   /* The callers loop while this returns 0, so 0 means "not finished,
    * call again" and never "failed". Returning it on error spins them
    * forever, and because each spin decodes the member afresh the hang
    * is not even cheap. Failure is -1. */
   if (!sevenzip_context || !sevenzip_context->archive)
      return -1;

   /* Sliced rather than whole: a solid folder is tens of milliseconds
    * decoded in one go, and this is the return value the archive layer
    * turns into a yield. Callers that cannot yield still get the whole
    * member, they just get it through several calls. */
   res = r7z_archive_extract_slice(sevenzip_context->archive,
         sevenzip_context->decompress_index,
         &member, &member_len);

   if (res == R7Z_PENDING)
      return 0;
   if (res != R7Z_OK)
      return -1;

   /* The caller reads through handle->data until it calls back in, so
    * the previous member cannot be released any earlier than this. */
   free(sevenzip_context->output);
   sevenzip_context->output = member;

   if (handle)
      handle->data = sevenzip_context->output;

   return 1;
}

static int sevenzip_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   uint8_t magic_buf[SEVENZIP_MAGIC_LEN];
   struct sevenzip_context_t *sevenzip_context = NULL;

   if (state->archive_size < SEVENZIP_MAGIC_LEN)
      goto error;

   filestream_seek(state->archive_file, 0, SEEK_SET);
   if (filestream_read(state->archive_file, magic_buf, SEVENZIP_MAGIC_LEN)
         != SEVENZIP_MAGIC_LEN)
      goto error;

   if (memcmp(magic_buf, SEVENZIP_MAGIC, SEVENZIP_MAGIC_LEN) != 0)
      goto error;

   if (!(sevenzip_context = (struct sevenzip_context_t*)
            calloc(1, sizeof(*sevenzip_context))))
      goto error;

   state->context = sevenzip_context;

   if (!sevenzip_bind_data(sevenzip_context, state, file))
      goto error;

   if (r7z_archive_open(&sevenzip_context->archive,
            sevenzip_context->data, sevenzip_context->data_len) != R7Z_OK)
      goto error;

   state->step_total = r7z_archive_num_entries(sevenzip_context->archive);

   return 0;

error:
   if (sevenzip_context)
      sevenzip_parse_file_free(sevenzip_context);
   state->context = NULL;
   return -1;
}

static int sevenzip_parse_file_iterate_step_internal(
      struct sevenzip_context_t *sevenzip_context,
      char *s,
      const uint8_t **cdata,
      unsigned *cmode,
      uint32_t *size,
      uint32_t *csize,
      uint32_t *checksum,
      unsigned *payback,
      struct archive_extract_userdata *userdata)
{
   const r7z_entry_t *entry;

   (void)userdata;

   if (!sevenzip_context->archive)
      return -1;

   if (sevenzip_context->parse_index
         >= r7z_archive_num_entries(sevenzip_context->archive))
      return 0;

   entry = r7z_archive_entry(sevenzip_context->archive,
         sevenzip_context->parse_index);

   if (entry && !entry->is_dir)
   {
      s[0] = '\0';
      if (!sevenzip_name_to_char(entry->name, s, PATH_MAX_LENGTH))
         return -1;

      /* The vtable reports sizes as uint32_t, but 7z stores them as
       * 64-bit. A member of 4 GiB or more cannot be described here, and
       * truncating would hand the caller a wrapped length to write from
       * a full-length buffer. Skip such an entry instead: the scan
       * carries on and everything else in the archive stays usable. */
      if (entry->size > (uint64_t)0xFFFFFFFFu)
      {
         /* Blank the name too: the caller passes whatever is in it to
          * file_cb, and a named entry reporting zero bytes is worse
          * than no entry at all. */
         s[0]     = '\0';
         *payback = 1;
         return 1;
      }

      *cmode    = 0; /* unused for 7zip */
      *checksum = entry->crc;
      *size     = (uint32_t)entry->size;
      /* The packed size of an individual member is not meaningful for
       * 7z: members share a solid folder's compressed stream. Callers
       * use this only for progress reporting. */
      *csize    = (uint32_t)entry->size;

      *cdata    = (uint8_t *)(size_t)sevenzip_context->parse_index;
   }

   *payback = 1;

   return 1;
}

static int sevenzip_parse_file_iterate_step(void *context,
      const char *valid_exts,
      struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb)
{
   const uint8_t *cdata = NULL;
   uint32_t checksum    = 0;
   uint32_t size        = 0;
   uint32_t csize       = 0;
   unsigned cmode       = 0;
   unsigned payload     = 0;
   struct sevenzip_context_t *sevenzip_context =
      (struct sevenzip_context_t*)context;
   int ret;

   userdata->current_file_path[0] = '\0';

   ret = sevenzip_parse_file_iterate_step_internal(sevenzip_context,
         userdata->current_file_path,
         &cdata, &cmode, &size, &csize,
         &checksum, &payload, userdata);

   if (ret != 1)
      return ret;

   userdata->crc                 = checksum;
   userdata->size                = size;

   if (file_cb && !file_cb(userdata->current_file_path, valid_exts,
            cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   sevenzip_context->parse_index += payload;

   return 1;
}

static uint32_t sevenzip_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t len)
{
   return encoding_crc32(crc, data, len);
}

const struct file_archive_file_backend sevenzip_backend = {
   sevenzip_parse_file_init,
   sevenzip_parse_file_iterate_step,
   sevenzip_parse_file_free,
   sevenzip_stream_decompress_data_to_file_init,
   sevenzip_stream_decompress_data_to_file_iterate,
   sevenzip_stream_crc32_calculate,
   sevenzip_file_read,
   "7z"
};
