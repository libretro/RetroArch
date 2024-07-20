/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_zlib.c).
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

#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>

#include <zlib.h>

#ifndef CENTRAL_FILE_HEADER_SIGNATURE
#define CENTRAL_FILE_HEADER_SIGNATURE 0x02014b50
#endif

#ifndef END_OF_CENTRAL_DIR_SIGNATURE
#define END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
#endif

#define _READ_CHUNK_SIZE   (128*1024)   /* Read 128KiB compressed chunks */

enum file_archive_compression_mode
{
   ZIP_MODE_STORED   = 0,
   ZIP_MODE_DEFLATED = 8
};

typedef struct
{
   struct file_archive_transfer *state;
   uint8_t *directory;
   uint8_t *directory_entry;
   uint8_t *directory_end;
   uint64_t fdoffset;
   uint32_t boffset, csize, usize;
   unsigned cmode;
   z_stream *zstream;
   uint8_t *tmpbuf;
   uint8_t *decompressed_data;
} zip_context_t;

static INLINE uint32_t read_le(const uint8_t *data, unsigned size)
{
   unsigned i;
   uint32_t val = 0;

   size *= 8;
   for (i = 0; i < size; i += 8)
      val |= (uint32_t)*data++ << i;

   return val;
}

static void zip_context_free_stream(
      zip_context_t *zip_context, bool keep_decompressed)
{
   if (zip_context->zstream)
   {
      inflateEnd(zip_context->zstream);
      free(zip_context->zstream);
      zip_context->fdoffset = 0;
      zip_context->csize = 0;
      zip_context->usize = 0;
      zip_context->zstream = NULL;
   }
   if (zip_context->tmpbuf)
   {
      free(zip_context->tmpbuf);
      zip_context->tmpbuf = NULL;
   }
   if (zip_context->decompressed_data && !keep_decompressed)
   {
      free(zip_context->decompressed_data);
      zip_context->decompressed_data = NULL;
   }
}

static bool zlib_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size)
{
   zip_context_t *zip_context = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;
   uint8_t local_header_buf[4];
   uint8_t *local_header;
   uint32_t offsetNL, offsetEL;
   int64_t offsetData;

   /* free previous data and stream if left unfinished */
   zip_context_free_stream(zip_context, false);

   /* seek past most of the local directory header */
#ifdef HAVE_MMAP
   if (state->archive_mmap_data)
   {
      local_header = state->archive_mmap_data + (size_t)cdata + 26;
   }
   else
#endif
   {
      filestream_seek(state->archive_file, (int64_t)(size_t)cdata + 26, RETRO_VFS_SEEK_POSITION_START);
      if (filestream_read(state->archive_file, local_header_buf, 4) != 4)
         goto error;
      local_header = local_header_buf;
   }

   offsetNL = read_le(local_header,     2); /* file name length */
   offsetEL = read_le(local_header + 2, 2); /* extra field length */
   offsetData = (int64_t)(size_t)cdata + 26 + 4 + offsetNL + offsetEL;

   zip_context->fdoffset              = offsetData;
   zip_context->usize                 = size;
   zip_context->csize                 = csize;
   zip_context->boffset               = 0;
   zip_context->cmode                 = cmode;
   zip_context->decompressed_data     = (uint8_t*)malloc(size);
   zip_context->zstream               = NULL;
   zip_context->tmpbuf                = NULL;

   if (cmode == ZIP_MODE_DEFLATED)
   {
      /* Initialize the zlib inflate machinery */
      zip_context->zstream            = (z_stream*)malloc(sizeof(z_stream));
      zip_context->tmpbuf             = (uint8_t*)malloc(_READ_CHUNK_SIZE);

      zip_context->zstream->next_in   = NULL;
      zip_context->zstream->avail_in  = 0;
      zip_context->zstream->total_in  = 0;
      zip_context->zstream->next_out  = zip_context->decompressed_data;
      zip_context->zstream->avail_out = size;
      zip_context->zstream->total_out = 0;

      zip_context->zstream->zalloc    = NULL;
      zip_context->zstream->zfree     = NULL;
      zip_context->zstream->opaque    = NULL;

      if (inflateInit2(zip_context->zstream, -MAX_WBITS) != Z_OK) {
         free(zip_context->zstream);
         zip_context->zstream = NULL;
         goto error;
      }
   }

   return true;

error:
   zip_context_free_stream(zip_context, false);
   return false;
}

static int zlib_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   zip_context_t *zip_context = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;
   int64_t rd;

   if (zip_context->cmode == ZIP_MODE_STORED)
   {
      #ifdef HAVE_MMAP
      if (zip_context->state->archive_mmap_data)
      {
         /* Simply copy the data to the output buffer */
         memcpy(zip_context->decompressed_data,
                zip_context->state->archive_mmap_data + (size_t)zip_context->fdoffset,
                zip_context->usize);
      }
      else
      #endif
      {
         /* Read the entire file to memory */
         filestream_seek(state->archive_file, zip_context->fdoffset, RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file,
                             zip_context->decompressed_data,
                             zip_context->usize) < 0)
            return -1;
      }

      handle->data = zip_context->decompressed_data;
      return 1;
   }
   else if (zip_context->cmode == ZIP_MODE_DEFLATED)
   {
      int to_read = MIN(zip_context->csize - zip_context->boffset, _READ_CHUNK_SIZE);
      uint8_t *dptr;
      if (!zip_context->zstream)
      {
         /* file was uncompressed or decompression finished before */
         return 1;
      }

      #ifdef HAVE_MMAP
      if (state->archive_mmap_data)
      {
         /* Decompress from the mapped file */
         dptr = state->archive_mmap_data + (size_t)zip_context->fdoffset + zip_context->boffset;
         rd = to_read;
      }
      else
      #endif
      {
         /* Read some compressed data from file to the temp buffer */
         filestream_seek(state->archive_file, zip_context->fdoffset + zip_context->boffset,
                         RETRO_VFS_SEEK_POSITION_START);
         rd = filestream_read(state->archive_file, zip_context->tmpbuf, to_read);
         if (rd < 0)
            return -1;
         dptr = zip_context->tmpbuf;
      }

      zip_context->boffset           += rd;
      zip_context->zstream->next_in   = dptr;
      zip_context->zstream->avail_in  = (uInt)rd;

      if (inflate(zip_context->zstream, 0) < 0)
         return -1;

      if (zip_context->boffset >= zip_context->csize)
      {
         inflateEnd(zip_context->zstream);
         free(zip_context->zstream);
         zip_context->zstream = NULL;

         handle->data = zip_context->decompressed_data;
         return 1;
      }

      return 0;   /* still more data to process */
   }

   /* No idea what kind of compression this is */
   return -1;
}

static uint32_t zlib_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t length)
{
   return encoding_crc32(crc, data, length);
}

static bool zip_file_decompressed_handle(
      file_archive_transfer_t *transfer,
      file_archive_file_handle_t* handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize,
      uint32_t size, uint32_t crc32)
{
   int ret   = 0;

   transfer->backend = &zlib_backend;

   if (!transfer->backend->stream_decompress_data_to_file_init(
            transfer->context, handle, cdata, cmode, csize, size))
      return false;

   do
   {
      ret = transfer->backend->stream_decompress_data_to_file_iterate(
            transfer->context, handle);
      if (ret < 0)
         return false;
   }while (ret == 0);

   return true;
}

typedef struct
{
   char *opt_file;
   char *needle;
   void **buf;
   size_t size;
   bool found;
} decomp_state_t;

/* Extract the relative path (needle) from a
 * ZIP archive (path) and allocate a buffer for it to write it in.
 *
 * optional_outfile if not NULL will be used to extract the file to.
 * buf will be 0 then.
 */

static int zip_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   decomp_state_t* decomp_state = (decomp_state_t*)userdata->cb_data;
   char last_char = name[strlen(name) - 1];
   /* Ignore directories. */
   if (last_char == '/' || last_char == '\\')
      return 1;

   if (strstr(name, decomp_state->needle))
   {
      file_archive_file_handle_t handle = {0};

      if (zip_file_decompressed_handle(userdata->transfer,
               &handle, cdata, cmode, csize, size, crc32))
      {
         if (decomp_state->opt_file != 0)
         {
            /* Called in case core has need_fullpath enabled. */
            bool success = filestream_write_file(decomp_state->opt_file, handle.data, size);

            /* Note: Do not free handle.data here - this
             * will be done when stream is deinitialised */
            handle.data = NULL;

            decomp_state->size = 0;

            if (!success)
               return -1;
         }
         else
         {
            /* Called in case core has need_fullpath disabled.
             * Will move decompressed content directly into
             * RetroArch's ROM buffer. */
            zip_context_t *zip_context = (zip_context_t *)userdata->transfer->context;

            decomp_state->size = 0;
            *decomp_state->buf             = handle.data;
            decomp_state->size             = size;
            /* We keep the data, prevent its deallocation during free */
            zip_context->decompressed_data = NULL;
            handle.data = NULL;
         }
      }

      decomp_state->found = true;
   }

   return 1;
}

static int64_t zip_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   file_archive_transfer_t state            = {0};
   decomp_state_t decomp                    = {0};
   struct archive_extract_userdata userdata = {0};
   bool returnerr                           = true;
   int ret                                  = 0;

   if (needle)
      decomp.needle          = strdup(needle);
   if (optional_outfile)
      decomp.opt_file        = strdup(optional_outfile);

   state.type                = ARCHIVE_TRANSFER_INIT;
   userdata.transfer         = &state;
   userdata.cb_data          = &decomp;
   decomp.buf                = buf;

   do
   {
      ret = file_archive_parse_file_iterate(&state, &returnerr, path,
            "", zip_file_decompressed, &userdata);
      if (!returnerr)
         break;
   }while (ret == 0 && !decomp.found);

   file_archive_parse_file_iterate_stop(&state);

   if (decomp.opt_file)
      free(decomp.opt_file);
   if (decomp.needle)
      free(decomp.needle);

   if (!decomp.found)
      return -1;

   return (int64_t)decomp.size;
}

static int zip_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   uint8_t footer_buf[1024];
   uint8_t *footer = footer_buf;
   int64_t read_pos = state->archive_size;
   int64_t read_block = MIN(read_pos, (ssize_t)sizeof(footer_buf));
   int64_t directory_size, directory_offset;
   zip_context_t *zip_context = NULL;

   /* Minimal ZIP file size is 22 bytes */
   if (read_block < 22)
      return -1;

   /* Find the end of central directory record by scanning
    * the file from the end towards the beginning.
    */
   for (;;)
   {
      if (--footer < footer_buf)
      {
         if (read_pos <= 0)
            return -1; /* reached beginning of file */

         /* Read 21 bytes of overlaps except on the first block. */
         if (read_pos == state->archive_size)
            read_pos = read_pos - read_block;
         else
            read_pos = MAX(read_pos - read_block + 21, 0);

         /* Seek to read_pos and read read_block bytes. */
         filestream_seek(state->archive_file, read_pos, RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file, footer_buf, read_block) != read_block)
            return -1;

         footer = footer_buf + read_block - 22;
      }
      if (read_le(footer, 4) == END_OF_CENTRAL_DIR_SIGNATURE)
      {
         unsigned comment_len = read_le(footer + 20, 2);
         if (read_pos + (footer - footer_buf) + 22 + comment_len == state->archive_size)
            break; /* found it! */
      }
   }

   /* Read directory info and do basic sanity checks. */
   directory_size   = read_le(footer + 12, 4);
   directory_offset = read_le(footer + 16, 4);
   if (directory_size > state->archive_size
         || directory_offset > state->archive_size)
      return -1;

   /* This is a ZIP file, allocate one block of memory for both the
    * context and the entire directory, then read the directory.
    */
   zip_context = (zip_context_t*)malloc(sizeof(zip_context_t) + (size_t)directory_size);
   zip_context->state             = state;
   zip_context->directory         = (uint8_t*)(zip_context + 1);
   zip_context->directory_entry   = zip_context->directory;
   zip_context->directory_end     = zip_context->directory + (size_t)directory_size;
   zip_context->zstream           = NULL;
   zip_context->tmpbuf            = NULL;
   zip_context->decompressed_data = NULL;

   filestream_seek(state->archive_file, directory_offset, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_read(state->archive_file, zip_context->directory, directory_size) != directory_size)
   {
      free(zip_context);
      return -1;
   }

   state->context = zip_context;
   state->step_total = read_le(footer + 10, 2); /* total entries */;

   return 0;
}

static int zip_parse_file_iterate_step_internal(
      zip_context_t * zip_context, char *filename,
      const uint8_t **cdata,
      unsigned *cmode, uint32_t *size, uint32_t *csize,
      uint32_t *checksum, unsigned *payback)
{
   uint8_t *entry = zip_context->directory_entry;
   uint32_t signature, namelength, extralength, commentlength, offset;

   if (entry < zip_context->directory || entry >= zip_context->directory_end)
      return 0;

   signature = read_le(zip_context->directory_entry + 0, 4);

   if (signature != CENTRAL_FILE_HEADER_SIGNATURE)
      return 0;

   *cmode         = read_le(zip_context->directory_entry + 10, 2); /* compression mode, 0 = store, 8 = deflate */
   *checksum      = read_le(zip_context->directory_entry + 16, 4); /* CRC32 */
   *csize         = read_le(zip_context->directory_entry + 20, 4); /* compressed size */
   *size          = read_le(zip_context->directory_entry + 24, 4); /* uncompressed size */

   namelength     = read_le(zip_context->directory_entry + 28, 2); /* file name length */
   extralength    = read_le(zip_context->directory_entry + 30, 2); /* extra field length */
   commentlength  = read_le(zip_context->directory_entry + 32, 2); /* file comment length */

   if (namelength >= PATH_MAX_LENGTH)
      return -1;

   memcpy(filename, zip_context->directory_entry + 46, namelength); /* file name */
   filename[namelength] = '\0';

   offset   = read_le(zip_context->directory_entry + 42, 4); /* relative offset of local file header */

   *cdata   = (uint8_t*)(size_t)offset; /* store file offset in data pointer */

   *payback = 46 + namelength + extralength + commentlength;

   return 1;
}

static int zip_parse_file_iterate_step(void *context,
      const char *valid_exts, struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb)
{
   zip_context_t *zip_context = (zip_context_t *)context;
   const uint8_t *cdata           = NULL;
   uint32_t checksum              = 0;
   uint32_t size                  = 0;
   uint32_t csize                 = 0;
   unsigned cmode                 = 0;
   unsigned payload               = 0;
   int ret                        = zip_parse_file_iterate_step_internal(zip_context,
         userdata->current_file_path, &cdata, &cmode, &size, &csize, &checksum, &payload);

   if (ret != 1)
      return ret;

   userdata->crc = checksum;

   if (file_cb && !file_cb(userdata->current_file_path, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   zip_context->directory_entry += payload;

   return 1;
}

static void zip_parse_file_free(void *context)
{
   zip_context_t *zip_context = (zip_context_t *)context;
   zip_context_free_stream(zip_context, false);
   free(zip_context);
}

const struct file_archive_file_backend zlib_backend = {
   zip_parse_file_init,
   zip_parse_file_iterate_step,
   zip_parse_file_free,
   zlib_stream_decompress_data_to_file_init,
   zlib_stream_decompress_data_to_file_iterate,
   zlib_stream_crc32_calculate,
   zip_file_read,
   "zlib"
};
