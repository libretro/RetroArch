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
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>

/* The ZIP DEFLATE backend can be built against zlib or against the
 * clean-room inflate implementation in encodings/deflate.h.  Define
 * ARCHIVE_HAVE_ZLIB to use zlib, ARCHIVE_USE_BUILTIN_DEFLATE to use the
 * built-in decoder; when neither is set the choice follows HAVE_ZLIB.  Only
 * the raw inflate path differs -- everything else (ZIP parsing, STORED mode,
 * CRC32, zstd) is unchanged. */
#if !defined(ARCHIVE_HAVE_ZLIB) && !defined(ARCHIVE_USE_BUILTIN_DEFLATE)
#if defined(HAVE_ZLIB)
#define ARCHIVE_HAVE_ZLIB 1
#else
#define ARCHIVE_USE_BUILTIN_DEFLATE 1
#endif
#endif

#ifdef ARCHIVE_HAVE_ZLIB
#include <zlib.h>
#else
#include <encodings/deflate.h>
#endif

#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

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
   ZIP_MODE_DEFLATED = 8,
   ZIP_MODE_ZSTD     = 93
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
#ifdef ARCHIVE_HAVE_ZLIB
   z_stream *zstream;
#else
   void *zstream;          /* rinflate stream handle */
   int    out_bound;       /* output buffer already handed to the decoder */
#endif
   uint8_t *tmpbuf;
   uint8_t *decompressed_data;
} zip_context_t;

static INLINE uint32_t read_le(const uint8_t *data, size_t len)
{
   unsigned i;
   uint32_t val = 0;
   len *= 8;
   for (i = 0; i < len; i += 8)
      val |= (uint32_t)*data++ << i;
   return val;
}

static void zip_context_free_stream(
      zip_context_t *zip_context, bool keep_decompressed)
{
   if (zip_context->zstream)
   {
#ifdef ARCHIVE_HAVE_ZLIB
      inflateEnd(zip_context->zstream);
      free(zip_context->zstream);
#else
      rinflate_free(zip_context->zstream);
      zip_context->out_bound = 0;
#endif
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
   int64_t offsetData;
   uint8_t *local_header;
   uint32_t offsetNL, offsetEL;
   uint8_t local_header_buf[4];
   zip_context_t *zip_context = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;

   /* free previous data and stream if left unfinished */
   zip_context_free_stream(zip_context, false);

   /* seek past most of the local directory header */
#ifdef HAVE_MMAP
   if (state->archive_mmap_data)
      local_header = state->archive_mmap_data + (size_t)cdata + 26;
   else
#endif
   {
      filestream_seek(state->archive_file, (int64_t)(size_t)cdata + 26, RETRO_VFS_SEEK_POSITION_START);
      if (filestream_read(state->archive_file, local_header_buf, 4) != 4)
      {
         zip_context_free_stream(zip_context, false);
         return false;
      }
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

   /* NULL-check the decompressed_data malloc: subsequent
    * consumers (zip_context_iterate, and line 155 below where
    * zstream->next_out = decompressed_data seeds the inflate
    * output) unconditionally dereference it.  On OOM fail the
    * whole context setup via zip_context_free_stream which is
    * NULL-safe for both zstream and tmpbuf. */
   if (!zip_context->decompressed_data)
   {
      zip_context_free_stream(zip_context, false);
      return false;
   }

   if (cmode == ZIP_MODE_DEFLATED)
   {
#ifdef ARCHIVE_HAVE_ZLIB
      /* Initialize the zlib inflate machinery */
      zip_context->zstream            = (z_stream*)malloc(sizeof(z_stream));
      zip_context->tmpbuf             = (uint8_t*)malloc(_READ_CHUNK_SIZE);

      /* NULL-check both mallocs: the zstream->next_in etc.
       * field writes below NULL-deref on OOM for zstream, and
       * inflate() later reads from tmpbuf.  Fail the context
       * setup on either failure. */
      if (!zip_context->zstream || !zip_context->tmpbuf)
      {
         zip_context_free_stream(zip_context, false);
         return false;
      }

      zip_context->zstream->next_in   = NULL;
      zip_context->zstream->avail_in  = 0;
      zip_context->zstream->total_in  = 0;
      zip_context->zstream->next_out  = zip_context->decompressed_data;
      zip_context->zstream->avail_out = size;
      zip_context->zstream->total_out = 0;

      zip_context->zstream->zalloc    = NULL;
      zip_context->zstream->zfree     = NULL;
      zip_context->zstream->opaque    = NULL;

      if (inflateInit2(zip_context->zstream, -MAX_WBITS) != Z_OK)
      {
         free(zip_context->zstream);
         zip_context->zstream = NULL;
         zip_context_free_stream(zip_context, false);
         return false;
      }
#else
      /* Initialize the built-in raw-DEFLATE decoder.  The output buffer
       * (the full decompressed_data) is bound once on the first iterate
       * call; only compressed input is fed incrementally after that. */
      zip_context->zstream            = rinflate_new(-15);
      zip_context->tmpbuf             = (uint8_t*)malloc(_READ_CHUNK_SIZE);
      zip_context->out_bound          = 0;

      if (!zip_context->zstream || !zip_context->tmpbuf)
      {
         zip_context_free_stream(zip_context, false);
         return false;
      }
#endif
   }
#ifdef HAVE_ZSTD
   else if (cmode == ZIP_MODE_ZSTD)
   {
      /* Allocate a buffer to read compressed data into;
       * decompression is done in one shot during iterate */
      zip_context->tmpbuf = (uint8_t*)malloc(csize);
      if (!zip_context->tmpbuf)
      {
         zip_context_free_stream(zip_context, false);
         return false;
      }
   }
#endif

   return true;
}

static int zlib_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   int64_t rd;
   zip_context_t *zip_context = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;

   if (zip_context->cmode == ZIP_MODE_STORED)
   {
#ifdef HAVE_MMAP
      /* Simply copy the data to the output buffer */
      if (zip_context->state->archive_mmap_data)
         memcpy(zip_context->decompressed_data,
               zip_context->state->archive_mmap_data + (size_t)zip_context->fdoffset,
               zip_context->usize);
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
      uint8_t *dptr;
      int to_read = MIN(zip_context->csize - zip_context->boffset, _READ_CHUNK_SIZE);
      /* File was uncompressed or decompression finished before */
      if (!zip_context->zstream)
         return 1;

#ifdef HAVE_MMAP
      if (state->archive_mmap_data)
      {
         /* Decompress from the mapped file */
         dptr = state->archive_mmap_data + (size_t)zip_context->fdoffset + zip_context->boffset;
         rd   = to_read;
      }
      else
#endif
      {
         /* Read some compressed data from file to the temp buffer */
         filestream_seek(state->archive_file,
               zip_context->fdoffset + zip_context->boffset,
               RETRO_VFS_SEEK_POSITION_START);
         rd = filestream_read(state->archive_file, zip_context->tmpbuf, to_read);
         if (rd < 0)
            return -1;
         dptr = zip_context->tmpbuf;
      }

      zip_context->boffset           += rd;

#ifdef ARCHIVE_HAVE_ZLIB
      zip_context->zstream->next_in   = dptr;
      zip_context->zstream->avail_in  = (uInt)rd;

      if (inflate(zip_context->zstream, 0) < 0)
         return -1;
#else
      /* Bind the output buffer once, then feed this compressed chunk.  The
       * decoder keeps its own output cursor across calls, so set_out is only
       * issued the first time. */
      if (!zip_context->out_bound)
      {
         rinflate_set_out(zip_context->zstream,
               zip_context->decompressed_data, zip_context->usize);
         zip_context->out_bound = 1;
      }
      rinflate_set_in(zip_context->zstream, dptr, (size_t)rd);

      for (;;)
      {
         size_t got_in = 0, got_out = 0;
         int    st     = rinflate_process(zip_context->zstream,
               &got_in, &got_out);
         if (st == RDEFLATE_PROCESS_ERROR)
            return -1;
         /* END: stream finished.  NEXT with no further progress means this
          * input chunk is drained; fetch the next one on the outer loop. */
         if (st == RDEFLATE_PROCESS_END)
            break;
         if (got_in == 0 && got_out == 0)
            break;
      }
#endif

      if (zip_context->boffset >= zip_context->csize)
      {
#ifdef ARCHIVE_HAVE_ZLIB
         inflateEnd(zip_context->zstream);
         free(zip_context->zstream);
#else
         rinflate_free(zip_context->zstream);
         zip_context->out_bound = 0;
#endif
         zip_context->zstream = NULL;

         handle->data = zip_context->decompressed_data;
         return 1;
      }

      return 0;   /* still more data to process */
   }
#ifdef HAVE_ZSTD
   else if (zip_context->cmode == ZIP_MODE_ZSTD)
   {
      size_t result;

#ifdef HAVE_MMAP
      if (state->archive_mmap_data)
      {
         result = ZSTD_decompress(
               zip_context->decompressed_data, zip_context->usize,
               state->archive_mmap_data + (size_t)zip_context->fdoffset,
               zip_context->csize);
      }
      else
#endif
      {
         /* Read all compressed data, then decompress */
         filestream_seek(state->archive_file,
               zip_context->fdoffset,
               RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file,
                  zip_context->tmpbuf, zip_context->csize) < 0)
            return -1;

         result = ZSTD_decompress(
               zip_context->decompressed_data, zip_context->usize,
               zip_context->tmpbuf, zip_context->csize);
      }

      if (ZSTD_isError(result))
         return -1;

      free(zip_context->tmpbuf);
      zip_context->tmpbuf = NULL;

      handle->data = zip_context->decompressed_data;
      return 1;
   }
#endif

   /* No idea what kind of compression this is */
   return -1;
}

static uint32_t zlib_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t len)
{
   return encoding_crc32(crc, data, len);
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
   size_t name_len              = name ? strlen(name) : 0;
   char last_char;
   /* Reject empty or NULL name -- strlen-1 on empty would read
    * name[SIZE_MAX].  Malformed archives can have 0-length filename
    * entries. */
   if (name_len == 0)
      return 1;
   last_char = name[name_len - 1];
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
   int ret                                  = 0;

   if (needle)
      decomp.needle          = strdup(needle);
   if (optional_outfile)
      decomp.opt_file        = strdup(optional_outfile);

   /* NULL-check strdups: zip_file_decompressed (line ~396)
    * calls strstr(name, decomp_state->needle) which NULL-derefs
    * if needle was requested but strdup failed.  Bail out of
    * the extraction; caller treats -1 as 'not found / failed'.
    * Free whatever strdup succeeded to avoid a leak on
    * partial-success OOM. */
   if ((needle && !decomp.needle) ||
       (optional_outfile && !decomp.opt_file))
   {
      free(decomp.needle);
      free(decomp.opt_file);
      return -1;
   }

   state.type                = ARCHIVE_TRANSFER_INIT;
   userdata.transfer         = &state;
   userdata.cb_data          = &decomp;
   decomp.buf                = buf;

   do
   {
      bool returnerr = true;
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

   /* Combined sanity check: the directory must fit entirely within
    * the archive.  Without this, offset + size could wrap or point
    * past EOF, producing a large bogus allocation and a short read. */
   if ((int64_t)directory_offset + (int64_t)directory_size > state->archive_size)
      return -1;

   /* Reject sizes that would overflow the allocation on 32-bit hosts.
    * size_t is 32-bit on 3DS / Vita / PSP / Wii / Wii U, where a
    * directory_size near UINT32_MAX would wrap
    *     sizeof(zip_context_t) + (size_t)directory_size
    * to a tiny value, after which directory_end runs off the end of
    * the allocation. */
   if ((size_t)directory_size > SIZE_MAX - sizeof(zip_context_t))
      return -1;

   /* This is a ZIP file, allocate one block of memory for both the
    * context and the entire directory, then read the directory.
    */
   zip_context = (zip_context_t*)malloc(sizeof(zip_context_t) + (size_t)directory_size);
   if (!zip_context)
      return -1;
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

   /* Central-directory fixed header is 46 bytes (highest-offset read
    * is at +42..+45).  Reject a truncated trailing entry before any
    * out-of-bounds read. */
   if ((size_t)(zip_context->directory_end - entry) < 46)
      return -1;

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

   /* Variable-length fields (name, extra, comment) follow the 46-byte
    * fixed header.  Reject if the declared sizes would run past the
    * end of the directory block. */
   if ((size_t)(zip_context->directory_end - entry)
         < (size_t)46 + namelength + extralength + commentlength)
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
   const uint8_t *cdata       = NULL;
   uint32_t checksum          = 0;
   uint32_t size              = 0;
   uint32_t csize             = 0;
   unsigned cmode             = 0;
   unsigned payload           = 0;
   int ret                    = zip_parse_file_iterate_step_internal(zip_context,
         userdata->current_file_path, &cdata, &cmode, &size, &csize, &checksum, &payload);

   if (ret != 1)
      return ret;

   userdata->crc  = checksum;
   userdata->size = size;

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

/* ---- incremental archive entry source (see archive_file.h) ---- */

struct file_archive_entry_source
{
   file_archive_transfer_t state;      /* keeps the mapping/file open */
   int64_t  fdoffset;                  /* entry data start in archive */
   uint32_t csize, usize;
   uint32_t in_off, out_off;
   unsigned cmode;
   void    *z;                         /* z_stream / rinflate handle  */
   uint8_t *tmpbuf;                    /* input chunks without mmap   */
   uint8_t *out_expect;                /* rinflate cursor guard       */
   const char *needle;                 /* entry name, during open only */
   uint8_t  found;
};

/* capture callback: record the needle entry's location, extract nothing */
static int file_archive_entry_source_capture(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   file_archive_entry_source_t *s =
         (file_archive_entry_source_t*)userdata->cb_data;
   size_t name_len = name ? strlen(name) : 0;
   if (name_len == 0)
      return 1;
   if (name[name_len - 1] == '/' || name[name_len - 1] == '\\')
      return 1;
   if (!strstr(name, s->needle))
      return 1;
   /* the local-header hop, both arms, mirroring the extract init */
   {
      struct file_archive_transfer *state = &s->state;
      uint8_t local_header_buf[4];
      uint8_t *local_header;
      uint32_t nl, el;
#ifdef HAVE_MMAP
      if (state->archive_mmap_data)
         local_header = state->archive_mmap_data + (size_t)cdata + 26;
      else
#endif
      {
         filestream_seek(state->archive_file, (int64_t)(size_t)cdata + 26,
               RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file, local_header_buf, 4) != 4)
            return -1;
         local_header = local_header_buf;
      }
      nl          = read_le(local_header,     2);
      el          = read_le(local_header + 2, 2);
      s->fdoffset = (int64_t)(size_t)cdata + 26 + 4 + nl + el;
   }
   s->csize = csize;
   s->usize = size;
   s->cmode = cmode;
   s->found = 1;
   return 1;
}

file_archive_entry_source_t *file_archive_entry_source_open(
      const char *path, int64_t *usize)
{
   file_archive_entry_source_t *s   = NULL;
   char *archive                    = NULL;
   const char *delim                = NULL;
   struct archive_extract_userdata userdata = {0};
   int ret                          = 0;

   if (file_archive_get_file_backend(path) != &zlib_backend)
      return NULL;                  /* 7z solid blocks: classic path */
   if (!(delim = path_get_archive_delim(path)) || !delim[1])
      return NULL;
   if (!(archive = (char*)malloc((size_t)(delim - path) + 1)))
      return NULL;
   memcpy(archive, path, (size_t)(delim - path));
   archive[delim - path] = '\0';
   if (!(s = (file_archive_entry_source_t*)calloc(1, sizeof(*s))))
      goto error;

   s->state.type         = ARCHIVE_TRANSFER_INIT;
   userdata.transfer     = &s->state;
   userdata.cb_data      = s;
   s->needle             = delim + 1;

   do
   {
      bool returnerr = true;
      ret = file_archive_parse_file_iterate(&s->state, &returnerr,
            archive, "",
            file_archive_entry_source_capture, &userdata);
      if (!returnerr)
         break;
   } while (ret == 0 && !s->found);

   if (!s->found)
      goto error_stop;

   if (s->cmode == ZIP_MODE_DEFLATED)
   {
#ifdef ARCHIVE_HAVE_ZLIB
      z_stream *z = (z_stream*)calloc(1, sizeof(z_stream));
      if (!z || inflateInit2(z, -MAX_WBITS) != Z_OK)
      {
         free(z);
         goto error_stop;
      }
      s->z = z;
#else
      if (!(s->z = rinflate_new(-15)))
         goto error_stop;
#endif
#ifdef HAVE_MMAP
      if (!s->state.archive_mmap_data)
#endif
      {
         if (!(s->tmpbuf = (uint8_t*)malloc(_READ_CHUNK_SIZE)))
            goto error_stop;
      }
   }
   else if (s->cmode != ZIP_MODE_STORED)
      goto error_stop;               /* unknown method */

   free(archive);
   s->needle = NULL;               /* borrowed from caller's path */
   if (usize)
      *usize = (int64_t)s->usize;
   return s;

error_stop:
   file_archive_parse_file_iterate_stop(&s->state);
error:
   if (s)
   {
      free(s->tmpbuf);
      free(s);
   }
   free(archive);
   return NULL;
}

int64_t file_archive_entry_source_read(file_archive_entry_source_t *s,
      uint8_t *dst, int64_t n)
{
   int64_t remain_out = (int64_t)s->usize - (int64_t)s->out_off;
   if (n > remain_out)
      n = remain_out;
   if (n <= 0)
      return 0;

   if (s->cmode == ZIP_MODE_STORED)
   {
#ifdef HAVE_MMAP
      if (s->state.archive_mmap_data)
         memcpy(dst, s->state.archive_mmap_data
               + (size_t)s->fdoffset + s->out_off, (size_t)n);
      else
#endif
      {
         filestream_seek(s->state.archive_file,
               s->fdoffset + s->out_off, RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(s->state.archive_file, dst, n) != n)
            return -1;
      }
      s->out_off += (uint32_t)n;
      return n;
   }

   /* DEFLATED: inflate directly into dst until n produced or end */
#ifdef ARCHIVE_HAVE_ZLIB
   {
      z_stream *z = (z_stream*)s->z;
      z->next_out  = dst;
      z->avail_out = (uInt)n;
      while (z->avail_out > 0)
      {
         if (z->avail_in == 0)
         {
            uint32_t chunk = s->csize - s->in_off;
            if (chunk == 0)
               break;
#ifdef HAVE_MMAP
            if (s->state.archive_mmap_data)
            {
               z->next_in  = s->state.archive_mmap_data
                     + (size_t)s->fdoffset + s->in_off;
               z->avail_in = (uInt)chunk;
            }
            else
#endif
            {
               int64_t rd;
               if (chunk > _READ_CHUNK_SIZE)
                  chunk = _READ_CHUNK_SIZE;
               filestream_seek(s->state.archive_file,
                     s->fdoffset + s->in_off,
                     RETRO_VFS_SEEK_POSITION_START);
               rd = filestream_read(s->state.archive_file,
                     s->tmpbuf, chunk);
               if (rd <= 0)
                  return -1;
               z->next_in  = s->tmpbuf;
               z->avail_in = (uInt)rd;
               chunk       = (uint32_t)rd;
            }
            s->in_off += chunk;
         }
         {
            int zr = inflate(z, 0);
            if (zr == Z_STREAM_END)
               break;
            if (zr < 0)
               return -1;
         }
      }
      {
         int64_t produced = n - (int64_t)z->avail_out;
         s->out_off += (uint32_t)produced;
         return produced;
      }
   }
#else
   {
      /* The built-in inflate binds its whole output window once and
       * keeps its own cursor, so per-call production is capped by
       * input granularity, not by n: this arm may overshoot the
       * pacing hint (the API permits it - dst always has room to the
       * entry's end).  Verify the caller honours the sequential-
       * contiguous dst contract before trusting the binding. */
      int64_t produced = 0;
      if (!s->out_expect)
      {
         rinflate_set_out(s->z, dst, s->usize);
         s->out_expect = dst;
      }
      else if (dst != s->out_expect)
         return -1;
      while (produced < n)
      {
         uint32_t chunk = s->csize - s->in_off;
         const uint8_t *iptr;
         if (chunk == 0)
            break;
#ifdef HAVE_MMAP
         if (s->state.archive_mmap_data)
         {
            if (chunk > _READ_CHUNK_SIZE)
               chunk = _READ_CHUNK_SIZE;
            iptr = s->state.archive_mmap_data
                  + (size_t)s->fdoffset + s->in_off;
         }
         else
#endif
         {
            int64_t rd;
            if (chunk > _READ_CHUNK_SIZE)
               chunk = _READ_CHUNK_SIZE;
            filestream_seek(s->state.archive_file,
                  s->fdoffset + s->in_off,
                  RETRO_VFS_SEEK_POSITION_START);
            rd = filestream_read(s->state.archive_file,
                  s->tmpbuf, chunk);
            if (rd <= 0)
               return -1;
            iptr  = s->tmpbuf;
            chunk = (uint32_t)rd;
         }
         rinflate_set_in(s->z, iptr, chunk);
         s->in_off += chunk;
         for (;;)
         {
            size_t got_in = 0, got_out = 0;
            int st = rinflate_process(s->z, &got_in, &got_out);
            produced += (int64_t)got_out;
            if (st == RDEFLATE_PROCESS_ERROR)
               return -1;
            if (st == RDEFLATE_PROCESS_END)
            {
               s->in_off = s->csize;   /* drained */
               break;
            }
            if (got_in == 0 && got_out == 0)
               break;                  /* chunk consumed */
         }
      }
      s->out_off    += (uint32_t)produced;
      s->out_expect += produced;
      return produced;
   }
#endif
}

void file_archive_entry_source_close(file_archive_entry_source_t *s)
{
   if (!s)
      return;
   if (s->z)
   {
#ifdef ARCHIVE_HAVE_ZLIB
      inflateEnd((z_stream*)s->z);
      free(s->z);
#else
      rinflate_free(s->z);
#endif
   }
   free(s->tmpbuf);
   file_archive_parse_file_iterate_stop(&s->state);
   free(s);
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
