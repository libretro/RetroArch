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

/* r7z_archive parses from a buffer rather than a stream, so the archive
 * is read in whole. That matches how this backend is used: every caller
 * already holds the file open and goes on to decode entire members into
 * memory, and 7z itself cannot decode a solid folder without holding
 * all of it anyway. */
struct sevenzip_context_t
{
   r7z_archive_t *archive;
   uint8_t       *data;
   size_t         data_len;
   /* Cache of the last extracted member, owned here so the handle
    * handed to the caller stays valid until the next call. */
   uint8_t       *output;
   uint32_t       parse_index;
   uint32_t       decompress_index;
};

/* Read a whole file into memory. Returns NULL on any failure. */
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
   free(sevenzip_context->data);
   free(sevenzip_context);
}

/* Extract the relative path (needle) from a 7z archive
 * (path) and allocate a buf for it to write it in.
 * If optional_outfile is set, extract to that instead
 * and don't allocate buffer.
 */
static int64_t sevenzip_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   r7z_archive_t *archive = NULL;
   uint8_t       *data;
   size_t         data_len = 0;
   int64_t        outsize  = -1;
   uint32_t       i;
   uint32_t       num;

   if (!(data = sevenzip_slurp(path, &data_len)))
      return -1;

   if (r7z_archive_open(&archive, data, data_len) != R7Z_OK)
   {
      free(data);
      return -1;
   }

   num = r7z_archive_num_entries(archive);

   for (i = 0; i < num; i++)
   {
      const r7z_entry_t *entry = r7z_archive_entry(archive, i);
      char               infile[PATH_MAX_LENGTH];

      if (!entry || entry->is_dir)
         continue;

      infile[0] = '\0';
      if (!sevenzip_name_to_char(entry->name, infile, sizeof(infile)))
         continue;

      if (!string_is_equal(infile, needle))
         continue;

      {
         uint8_t *member     = NULL;
         size_t   member_len = 0;

         if (r7z_archive_extract(archive, i, &member, &member_len)
               != R7Z_OK)
            break;

         if (optional_outfile)
         {
            if (filestream_write_file(optional_outfile,
                     (const void*)member, (int64_t)member_len))
               outsize = (int64_t)member_len;
         }
         else
         {
            /* RetroArch expects a NUL after the data, so this cannot
             * simply hand over the decoder's buffer. */
            if ((*buf = malloc(member_len + 1)))
            {
               ((char*)(*buf))[member_len] = '\0';
               if (member_len)
                  memcpy(*buf, member, member_len);
               outsize = (int64_t)member_len;
            }
         }

         free(member);
      }
      break;
   }

   r7z_archive_close(archive);
   free(data);
   return outsize;
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

   if (!sevenzip_context || !sevenzip_context->archive)
      return 0;

   if (r7z_archive_extract(sevenzip_context->archive,
            sevenzip_context->decompress_index,
            &member, &member_len) != R7Z_OK)
      return 0;

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

   if (!(sevenzip_context->data =
            sevenzip_slurp(file, &sevenzip_context->data_len)))
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
