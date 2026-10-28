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

#include <retro_posix_source.h>

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

/* archive_file.c has already opened the file and, where it could,
 * mapped it before this backend is entered. Where that mapping
 * exists r7z_archive parses it in place, exactly as the zip and zstd
 * backends do. Without one (no mapping on the platform, a network
 * share, an archive over a 32-bit host's ceiling) it reads the open stream
 * through file_archive_read_at(): the header at open, a folder's
 * packed bytes only when that folder is decoded. */
struct sevenzip_context_t
{
   r7z_archive_t *archive;
   file_archive_transfer_t *state;
   uint8_t       *member;     /* a member gathered out of a stream */
   size_t         gathered;
   uint32_t       parse_index;
   uint32_t       decompress_index;
   bool           streaming;  /* the member goes to the sink in pieces */
   bool           gathering;  /* or into member */
};

/* A member bound for a file, bigger than this, is streamed to it */
#define SEVENZIP_STREAM_OVER (256 * 1024)

/* A folder of other members, bigger than this, is never held whole: a
 * member wanted out of it is streamed, and the next one in the folder
 * goes on from there */
#ifndef SEVENZIP_FOLDER_KEEP
#define SEVENZIP_FOLDER_KEEP (32 * 1024 * 1024)
#endif

static bool sevenzip_stream_whole(r7z_archive_t *a, uint32_t index)
{
   const r7z_entry_t *e = r7z_archive_entry(a, index);
   uint64_t           folder;
   if (!e || e->is_dir || !e->size)
      return false;
   folder = r7z_archive_folder_size(a, e->folder);
   return folder > SEVENZIP_FOLDER_KEEP
      && !(e->offset_in_folder == 0 && e->size == folder);
}

/* Member @index streamed into *@out (a NUL after it), in one go; false
 * where its folder cannot be streamed or it does not come out */
static bool sevenzip_stream_into(r7z_archive_t *a, uint32_t index,
      uint8_t **out, size_t *len)
{
   const r7z_entry_t *e = r7z_archive_entry(a, index);
   const uint8_t     *d;
   size_t             n, at = 0;
   uint8_t           *m;
   int                res;
   if (     !e || e->size >= (uint64_t)((size_t)-1)
         || r7z_archive_stream_begin(a, index) != R7Z_OK)
      return false;
   if (!(m = (uint8_t*)malloc((size_t)e->size + 1)))
   {
      r7z_archive_stream_end(a);
      return false;
   }
   while ((res = r7z_archive_stream_step(a, &d, &n)) == R7Z_PENDING)
   {
      if (n > e->size - at)
         break;
      if (n)
         memcpy(m + at, d, n);
      at += n;
   }
   if (res != R7Z_OK || at != e->size)
   {
      r7z_archive_stream_end(a);
      free(m);
      return false;
   }
   m[at] = '\0';
   *out  = m;
   *len  = at;
   return true;
}

static int sevenzip_open_archive(struct sevenzip_context_t *ctx,
      file_archive_transfer_t *state)
{
#ifdef VFS_HAVE_FILE_MAPPING
   if (state->archive_mmap_data && state->archive_size > 0)
      return r7z_archive_open(&ctx->archive, state->archive_mmap_data,
            (size_t)state->archive_size) == R7Z_OK;
#endif
   return r7z_archive_open_read(&ctx->archive,
         (uint64_t)state->archive_size,
         file_archive_read_at, state->archive_file) == R7Z_OK;
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

   /* The mapping and the stream belong to archive_file.c, which
    * releases them after this. */
   r7z_archive_close(sevenzip_context->archive);
   free(sevenzip_context->member);
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

/* One slice of decoding member @index's folder: R7Z_PENDING while
 * there is more, and on R7Z_OK the member where it is held in the
 * archive's folder cache - decoded there, or, for a stored folder of a
 * mapped archive, the archive itself - valid until another folder is
 * decoded. Nothing is copied. */
static int sevenzip_member_step(r7z_archive_t *a, uint32_t index,
      const uint8_t **data, size_t *len)
{
   const r7z_entry_t *e = r7z_archive_entry(a, index);
   int                res;
   if (!e || e->is_dir)
      return R7Z_ERROR_PARAM;
   if (!e->size)
   {
      *data = (const uint8_t*)"";
      *len  = 0;
      return R7Z_OK;
   }
   if ((res = r7z_archive_decode_step(a, index)) != R7Z_OK)
      return res;
   return r7z_archive_entry_borrow(a, index, data, len);
}

/* Called once per entry while the driver walks the archive. Starts the
 * decode of the wanted member and stops the scan; the iterate loop
 * finishes the decode across later calls. */
static int sevenzip_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint64_t csize, uint64_t size,
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
      const r7z_entry_t *e;
      uint8_t           *member     = NULL;
      size_t             member_len = 0;
      uint32_t           index      = (uint32_t)(size_t)cdata;
      int                res        = R7Z_OK;

      if (     !ctx || !ctx->archive
            || !(e = r7z_archive_entry(ctx->archive, index)) || e->is_dir)
         return 1;

      /* out of a big folder of others, streamed to it */
      if (     sevenzip_stream_whole(ctx->archive, index)
            && sevenzip_stream_into(ctx->archive, index, &member,
               &member_len))
      {
         *st->buf  = member;
         st->size  = member_len;
         st->found = true;
         return 0;
      }

      /* Sliced, but driven to completion here: the caller of
       * compressed_file_read wants the bytes on return and has nowhere
       * to yield to. */
      if (e->size)
         while ((res = r7z_archive_decode_step(ctx->archive, index))
               == R7Z_PENDING) { }

      /* the caller's to keep, with a NUL after the data: a member that
       * is its folder is that folder's buffer, handed over, and any
       * other is copied out of it */
      if (     res != R7Z_OK
            || r7z_archive_extract_detach(ctx->archive, index, &member,
               &member_len) != R7Z_OK)
         return 1;
      member[member_len] = '\0';
      *st->buf  = member;
      st->size  = member_len;
      st->found = true;
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
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size)
{
   struct sevenzip_context_t *sevenzip_context =
      (struct sevenzip_context_t*)context;

   (void)handle;
   (void)cmode;
   (void)csize;

   if (!sevenzip_context)
      return false;

   /* The parse step hands the entry index through cdata. */
   sevenzip_context->decompress_index = (uint32_t)(size_t)cdata;
   sevenzip_context->streaming        = false;
   sevenzip_context->gathering        = false;
   free(sevenzip_context->member);
   sevenzip_context->member           = NULL;
   sevenzip_context->gathered         = 0;

   /* A big member bound for a file goes to it through a window, and
    * one out of a big folder of others into a buffer of its own, where
    * its folder's coders allow; anything else is decoded whole */
   {
      bool sink  =  sevenzip_context->state->pending_sink
                 && size > SEVENZIP_STREAM_OVER;
      bool apart = !sink && sevenzip_stream_whole(sevenzip_context->archive,
            sevenzip_context->decompress_index);
      if (sink || apart)
      {
         int res = r7z_archive_stream_begin(sevenzip_context->archive,
               sevenzip_context->decompress_index);
         if (res == R7Z_OK)
         {
            if (     apart
                  && (size >= (uint64_t)((size_t)-1)
                  || !(sevenzip_context->member = (uint8_t*)malloc(
                        (size_t)size + 1))))
               return false;
            sevenzip_context->streaming = sink;
            sevenzip_context->gathering = apart;
         }
         else if (res != R7Z_ERROR_UNSUPPORTED)
            return false;
      }
   }

   return true;
}

static int sevenzip_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   struct sevenzip_context_t *sevenzip_context =
      (struct sevenzip_context_t*)context;
   const uint8_t *member     = NULL;
   size_t         member_len = 0;
   int            res;

   /* The callers loop while this returns 0, so 0 means "not finished,
    * call again" and never "failed". Returning it on error spins them
    * forever, and because each spin decodes the member afresh the hang
    * is not even cheap. Failure is -1. */
   if (!sevenzip_context || !sevenzip_context->archive)
      return -1;

   if (sevenzip_context->gathering)
   {
      const r7z_entry_t *e = r7z_archive_entry(sevenzip_context->archive,
            sevenzip_context->decompress_index);
      res = r7z_archive_stream_step(sevenzip_context->archive,
            &member, &member_len);
      if (res == R7Z_PENDING)
      {
         if (member_len > e->size - sevenzip_context->gathered)
            return -1;
         if (member_len)
            memcpy(sevenzip_context->member + sevenzip_context->gathered,
                  member, member_len);
         sevenzip_context->gathered += member_len;
         return 0;
      }
      sevenzip_context->gathering = false;
      if (res != R7Z_OK || sevenzip_context->gathered != e->size)
         return -1;
      if (handle)
         handle->data = sevenzip_context->member;
      return 1;
   }

   if (sevenzip_context->streaming)
   {
      res = r7z_archive_stream_step(sevenzip_context->archive,
            &member, &member_len);
      if (res == R7Z_PENDING)
         return (!member_len || filestream_write(
                  sevenzip_context->state->pending_sink, member,
                  (int64_t)member_len) == (int64_t)member_len) ? 0 : -1;
      sevenzip_context->streaming = false;
      if (res != R7Z_OK)
         return -1;
      /* in the sink, whole */
      if (handle)
         handle->data = NULL;
      return 1;
   }

   /* Sliced rather than whole: a solid folder is tens of milliseconds
    * decoded in one go, and this is the return value the archive layer
    * turns into a yield. Callers that cannot yield still get the whole
    * member, they just get it through several calls. */
   res = sevenzip_member_step(sevenzip_context->archive,
         sevenzip_context->decompress_index, &member, &member_len);

   if (res == R7Z_PENDING)
      return 0;
   if (res != R7Z_OK)
      return -1;

   /* The caller only reads the member, and is done with it before it
    * calls back in for another: lent where it is held */
   if (handle)
      handle->data = (uint8_t*)member;

   return 1;
}

static int sevenzip_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   uint8_t magic_buf[SEVENZIP_MAGIC_LEN];
   struct sevenzip_context_t *sevenzip_context = NULL;

   (void)file;

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
   sevenzip_context->state = state;

   if (!sevenzip_open_archive(sevenzip_context, state))
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
      uint64_t *size,
      uint64_t *csize,
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

      *cmode    = 0; /* unused for 7zip */
      *checksum = entry->crc;
      *size     = entry->size;
      /* The packed size of an individual member is not meaningful for
       * 7z: members share a solid folder's compressed stream. Callers
       * use this only for progress reporting. */
      *csize    = entry->size;

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
   uint64_t size        = 0;
   uint64_t csize       = 0;
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

const struct file_archive_file_backend sevenzip_backend = {
   sevenzip_parse_file_init,
   sevenzip_parse_file_iterate_step,
   sevenzip_parse_file_free,
   sevenzip_stream_decompress_data_to_file_init,
   sevenzip_stream_decompress_data_to_file_iterate,
   encoding_crc32,
   sevenzip_file_read,
   "7z"
};
