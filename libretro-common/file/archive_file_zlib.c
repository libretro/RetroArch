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

/* Read 128KiB compressed chunks */
#define _READ_CHUNK_SIZE   (128*1024)

/* Minimum size of a valid ZIP file (End of Central Dir record) */
#define ZIP_EOCD_SIZE      22

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

/* Read a little-endian value of 1–4 bytes from data.
 * Unrolled for the common fixed sizes (1, 2, 4) to avoid
 * the loop overhead on every directory entry field read. */
static INLINE uint32_t read_le(const uint8_t *data, size_t len)
{
   switch (len)
   {
      case 1:
         return (uint32_t)data[0];
      case 2:
         return (uint32_t)data[0]
              | ((uint32_t)data[1] << 8);
      case 4:
         return (uint32_t)data[0]
              | ((uint32_t)data[1] <<  8)
              | ((uint32_t)data[2] << 16)
              | ((uint32_t)data[3] << 24);
      default:
      {
         /* Fallback for uncommon sizes */
         unsigned i;
         uint32_t val = 0;
         for (i = 0; i < (unsigned)(len * 8); i += 8)
            val |= (uint32_t)*data++ << i;
         return val;
      }
   }
}

static void zip_context_free_stream(
      zip_context_t *zip_context, bool keep_decompressed)
{
   if (zip_context->zstream)
   {
      inflateEnd(zip_context->zstream);
      free(zip_context->zstream);
      zip_context->zstream = NULL;
      zip_context->fdoffset = 0;
      zip_context->csize    = 0;
      zip_context->usize    = 0;
   }
   if (zip_context->tmpbuf)
   {
      free(zip_context->tmpbuf);
      zip_context->tmpbuf = NULL;
   }
   if (!keep_decompressed && zip_context->decompressed_data)
   {
      free(zip_context->decompressed_data);
      zip_context->decompressed_data = NULL;
   }
}

static bool zlib_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size)
{
   int64_t offsetData;
   uint8_t *local_header;
   uint32_t offsetNL, offsetEL;
   uint8_t local_header_buf[4];
   zip_context_t *zip_context     = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;

   /* Free any previous stream/data left unfinished */
   zip_context_free_stream(zip_context, false);

   /* Seek to offset 26 in the local file header to read
    * the file-name and extra-field length fields (4 bytes). */
#ifdef HAVE_MMAP
   if (state->archive_mmap_data)
      local_header = state->archive_mmap_data + (size_t)cdata + 26;
   else
#endif
   {
      filestream_seek(state->archive_file,
            (int64_t)(size_t)cdata + 26,
            RETRO_VFS_SEEK_POSITION_START);
      if (filestream_read(state->archive_file, local_header_buf, 4) != 4)
      {
         zip_context_free_stream(zip_context, false);
         return false;
      }
      local_header = local_header_buf;
   }

   offsetNL   = read_le(local_header,     2); /* file name length   */
   offsetEL   = read_le(local_header + 2, 2); /* extra field length */
   offsetData = (int64_t)(size_t)cdata + 26 + 4 + offsetNL + offsetEL;

   zip_context->fdoffset          = (uint64_t)offsetData;
   zip_context->usize             = size;
   zip_context->csize             = csize;
   zip_context->boffset           = 0;
   zip_context->cmode             = cmode;
   zip_context->zstream           = NULL;
   zip_context->tmpbuf            = NULL;

   /* Allocate output buffer once up-front */
   zip_context->decompressed_data = (uint8_t*)malloc(size);
   if (!zip_context->decompressed_data)
      return false;

   if (cmode == ZIP_MODE_DEFLATED)
   {
      z_stream *zs = (z_stream*)malloc(sizeof(z_stream));
      if (!zs)
      {
         zip_context_free_stream(zip_context, false);
         return false;
      }

      zip_context->tmpbuf = (uint8_t*)malloc(_READ_CHUNK_SIZE);
      if (!zip_context->tmpbuf)
      {
         free(zs);
         zip_context_free_stream(zip_context, false);
         return false;
      }

      /* Initialise the z_stream in one memset, then set non-zero fields */
      memset(zs, 0, sizeof(*zs));
      zs->next_out  = zip_context->decompressed_data;
      zs->avail_out = size;

      if (inflateInit2(zs, -MAX_WBITS) != Z_OK)
      {
         free(zs);
         zip_context_free_stream(zip_context, false);
         return false;
      }

      zip_context->zstream = zs;
   }

   return true;
}

static int zlib_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   zip_context_t *zip_context         = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;

   if (zip_context->cmode == ZIP_MODE_STORED)
   {
#ifdef HAVE_MMAP
      if (state->archive_mmap_data)
      {
         /* Zero-copy path: point directly into the mapped region */
         memcpy(zip_context->decompressed_data,
               state->archive_mmap_data + (size_t)zip_context->fdoffset,
               zip_context->usize);
      }
      else
#endif
      {
         filestream_seek(state->archive_file,
               (int64_t)zip_context->fdoffset,
               RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file,
                  zip_context->decompressed_data,
                  zip_context->usize) < 0)
            return -1;
      }

      handle->data = zip_context->decompressed_data;
      return 1;
   }

   if (zip_context->cmode == ZIP_MODE_DEFLATED)
   {
      int64_t  rd;
      uint8_t *dptr;
      uint32_t to_read = zip_context->csize - zip_context->boffset;

      /* Guard: decompression already finished */
      if (!zip_context->zstream)
         return 1;

      if (to_read > _READ_CHUNK_SIZE)
         to_read = _READ_CHUNK_SIZE;

#ifdef HAVE_MMAP
      if (state->archive_mmap_data)
      {
         dptr = state->archive_mmap_data
              + (size_t)zip_context->fdoffset
              + zip_context->boffset;
         rd   = (int64_t)to_read;
      }
      else
#endif
      {
         filestream_seek(state->archive_file,
               (int64_t)(zip_context->fdoffset + zip_context->boffset),
               RETRO_VFS_SEEK_POSITION_START);
         rd = filestream_read(state->archive_file,
               zip_context->tmpbuf, to_read);
         if (rd < 0)
            return -1;
         dptr = zip_context->tmpbuf;
      }

      zip_context->boffset              += (uint32_t)rd;
      zip_context->zstream->next_in      = dptr;
      zip_context->zstream->avail_in     = (uInt)rd;

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

      return 0; /* still more data to process */
   }

   /* Unknown compression mode */
   return -1;
}

static uint32_t zlib_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t len)
{
   return encoding_crc32(crc, data, len);
}

static bool zip_file_decompressed_handle(
      file_archive_transfer_t *transfer,
      file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize,
      uint32_t size, uint32_t crc32)
{
   int ret;

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
   } while (ret == 0);

   return true;
}

typedef struct
{
   char   *opt_file;
   char   *needle;
   void  **buf;
   size_t  size;
   bool    found;
} decomp_state_t;

/* Extract the relative path (needle) from a ZIP archive (path)
 * and allocate a buffer for it to write it in.
 *
 * optional_outfile, if not NULL, will be used to extract the file to.
 * buf will be 0 then.
 */
static int zip_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   decomp_state_t *decomp_state = (decomp_state_t*)userdata->cb_data;
   size_t name_len              = strlen(name);
   char last_char;

   /* Guard against empty names */
   if (name_len == 0)
      return 1;

   last_char = name[name_len - 1];

   /* Skip directory entries */
   if (last_char == '/' || last_char == '\\')
      return 1;

   if (strstr(name, decomp_state->needle))
   {
      file_archive_file_handle_t handle = {0};

      if (zip_file_decompressed_handle(userdata->transfer,
               &handle, cdata, cmode, csize, size, crc32))
      {
         if (decomp_state->opt_file != NULL)
         {
            /* Called when core has need_fullpath enabled:
             * write the decompressed data directly to disk. */
            bool success = filestream_write_file(
                  decomp_state->opt_file, handle.data, size);

            /* Do not free handle.data here – freed on stream deinit */
            handle.data = NULL;
            decomp_state->size = 0;

            if (!success)
               return -1;
         }
         else
         {
            /* Called when core has need_fullpath disabled:
             * move decompressed content into RetroArch's ROM buffer. */
            zip_context_t *zip_context =
                  (zip_context_t *)userdata->transfer->context;

            *decomp_state->buf             = handle.data;
            decomp_state->size             = size;
            /* Prevent deallocation of the buffer we just handed off */
            zip_context->decompressed_data = NULL;
            handle.data                    = NULL;
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
   file_archive_transfer_t  state    = {0};
   decomp_state_t           decomp   = {0};
   struct archive_extract_userdata userdata = {0};
   int ret                           = 0;

   if (needle)
      decomp.needle   = strdup(needle);
   if (optional_outfile)
      decomp.opt_file = strdup(optional_outfile);

   state.type        = ARCHIVE_TRANSFER_INIT;
   userdata.transfer = &state;
   userdata.cb_data  = &decomp;
   decomp.buf        = buf;

   do
   {
      bool returnerr = true;
      ret = file_archive_parse_file_iterate(&state, &returnerr, path,
            "", zip_file_decompressed, &userdata);
      if (!returnerr)
         break;
   } while (ret == 0 && !decomp.found);

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
   uint8_t *footer;
   int64_t read_pos, read_block;
   int64_t directory_size, directory_offset;
   zip_context_t *zip_context = NULL;

   read_pos   = state->archive_size;
   read_block = (read_pos < (int64_t)sizeof(footer_buf))
              ? read_pos
              : (int64_t)sizeof(footer_buf);

   /* Minimal ZIP file size is 22 bytes */
   if (read_block < ZIP_EOCD_SIZE)
      return -1;

   /* Scan backwards from end of file for the
    * End of Central Directory record. */
   footer = footer_buf; /* will be adjusted below before first use */
   for (;;)
   {
      if (--footer < footer_buf)
      {
         if (read_pos <= 0)
            return -1; /* reached beginning of file without finding EOCD */

         /* Overlap by 21 bytes on subsequent blocks to handle the case
          * where the EOCD signature straddles a block boundary. */
         if (read_pos == state->archive_size)
            read_pos -= read_block;
         else
         {
            read_pos -= read_block - 21;
            if (read_pos < 0)
               read_pos = 0;
         }

         filestream_seek(state->archive_file,
               read_pos, RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file,
                  footer_buf, read_block) != read_block)
            return -1;

         footer = footer_buf + read_block - ZIP_EOCD_SIZE;
      }

      if (read_le(footer, 4) == END_OF_CENTRAL_DIR_SIGNATURE)
      {
         unsigned comment_len = read_le(footer + 20, 2);
         /* Verify that the comment length is consistent with
          * the record being at this exact position in the file. */
         if (read_pos + (footer - footer_buf) + ZIP_EOCD_SIZE + comment_len
               == state->archive_size)
            break; /* found the EOCD */
      }
   }

   /* Read and sanity-check the central directory location/size */
   directory_size   = (int64_t)read_le(footer + 12, 4);
   directory_offset = (int64_t)read_le(footer + 16, 4);
   if (directory_size   > state->archive_size
         || directory_offset > state->archive_size)
      return -1;

   /* Allocate one contiguous block for the context struct and the
    * entire directory data that follows it in memory. */
   zip_context = (zip_context_t*)malloc(
         sizeof(zip_context_t) + (size_t)directory_size);
   if (!zip_context)
      return -1;

   zip_context->state             = state;
   zip_context->directory         = (uint8_t*)(zip_context + 1);
   zip_context->directory_entry   = zip_context->directory;
   zip_context->directory_end     = zip_context->directory + (size_t)directory_size;
   zip_context->zstream           = NULL;
   zip_context->tmpbuf            = NULL;
   zip_context->decompressed_data = NULL;
   zip_context->fdoffset          = 0;
   zip_context->boffset           = 0;
   zip_context->csize             = 0;
   zip_context->usize             = 0;
   zip_context->cmode             = 0;

   filestream_seek(state->archive_file,
         directory_offset, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_read(state->archive_file,
            zip_context->directory, directory_size) != directory_size)
   {
      free(zip_context);
      return -1;
   }

   state->context    = zip_context;
   state->step_total = read_le(footer + 10, 2); /* total entries */

   return 0;
}

static int zip_parse_file_iterate_step_internal(
      zip_context_t *zip_context, char *filename,
      const uint8_t **cdata,
      unsigned *cmode, uint32_t *size, uint32_t *csize,
      uint32_t *checksum, unsigned *payback)
{
   const uint8_t *entry = zip_context->directory_entry;
   uint32_t namelength, extralength, commentlength, offset;

   /* Bounds check before any reads */
   if (entry < zip_context->directory || entry >= zip_context->directory_end)
      return 0;

   /* Validate signature without a separate variable */
   if (read_le(entry + 0, 4) != CENTRAL_FILE_HEADER_SIGNATURE)
      return 0;

   /* Read all fields in order to keep them in cache */
   *cmode         = read_le(entry + 10, 2); /* compression mode       */
   *checksum      = read_le(entry + 16, 4); /* CRC-32                 */
   *csize         = read_le(entry + 20, 4); /* compressed size        */
   *size          = read_le(entry + 24, 4); /* uncompressed size      */
   namelength     = read_le(entry + 28, 2); /* file name length       */
   extralength    = read_le(entry + 30, 2); /* extra field length     */
   commentlength  = read_le(entry + 32, 2); /* file comment length    */
   offset         = read_le(entry + 42, 4); /* local file header offs */

   if (namelength >= PATH_MAX_LENGTH)
      return -1;

   memcpy(filename, entry + 46, namelength);
   filename[namelength] = '\0';

   *cdata   = (const uint8_t*)(size_t)offset;
   *payback = 46 + namelength + extralength + commentlength;

   return 1;
}

static int zip_parse_file_iterate_step(void *context,
      const char *valid_exts, struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb)
{
   zip_context_t  *zip_context = (zip_context_t *)context;
   const uint8_t  *cdata       = NULL;
   uint32_t        checksum    = 0;
   uint32_t        size        = 0;
   uint32_t        csize       = 0;
   unsigned        cmode       = 0;
   unsigned        payload     = 0;
   int ret = zip_parse_file_iterate_step_internal(zip_context,
         userdata->current_file_path,
         &cdata, &cmode, &size, &csize, &checksum, &payload);

   if (ret != 1)
      return ret;

   userdata->crc  = checksum;
   userdata->size = size;

   if (file_cb && !file_cb(userdata->current_file_path, valid_exts, cdata,
            cmode, csize, size, checksum, userdata))
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
