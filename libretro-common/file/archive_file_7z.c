/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_sevenzip.c).
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
#include <7zip/7z.h>
#include <7zip/7zCrc.h>
#include <7zip/7zFile.h>

#define SEVENZIP_MAGIC "7z\xBC\xAF\x27\x1C"
#define SEVENZIP_MAGIC_LEN 6
#define SEVENZIP_LOOKTOREAD_BUF_SIZE (1 << 14)

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

struct sevenzip_context_t
{
   uint8_t *output;
   CFileInStream archiveStream;
   CLookToRead2 lookStream;
   ISzAlloc allocImp;
   ISzAlloc allocTempImp;
   CSzArEx db;
   size_t temp_size;
   uint32_t parse_index;
   uint32_t decompress_index;
   uint32_t packIndex;
   uint32_t   block_index;
};

static void *sevenzip_stream_alloc_impl(ISzAllocPtr p, size_t size)
{
   if (size == 0)
      return 0;
   return malloc(size);
}

static void sevenzip_stream_free_impl(ISzAllocPtr p, void *address)
{
   (void)p;

   if (address)
      free(address);
}

static void *sevenzip_stream_alloc_tmp_impl(ISzAllocPtr p, size_t size)
{
   (void)p;
   if (size == 0)
      return 0;
   return malloc(size);
}

static void* sevenzip_stream_new(void)
{
   struct sevenzip_context_t *sevenzip_context =
         (struct sevenzip_context_t*)calloc(1, sizeof(struct sevenzip_context_t));

   /* These are the allocation routines - currently using
    * the non-standard 7zip choices. */
   sevenzip_context->allocImp.Alloc     = sevenzip_stream_alloc_impl;
   sevenzip_context->allocImp.Free      = sevenzip_stream_free_impl;
   sevenzip_context->allocTempImp.Alloc = sevenzip_stream_alloc_tmp_impl;
   sevenzip_context->allocTempImp.Free  = sevenzip_stream_free_impl;
   sevenzip_context->block_index        = 0xFFFFFFFF;
   sevenzip_context->output             = NULL;

   sevenzip_context->lookStream.bufSize = SEVENZIP_LOOKTOREAD_BUF_SIZE * sizeof(Byte);
   sevenzip_context->lookStream.buf     = (Byte*)malloc(sevenzip_context->lookStream.bufSize);

   if (!sevenzip_context->lookStream.buf)
      sevenzip_context->lookStream.bufSize = 0;

   return sevenzip_context;
}

static void sevenzip_parse_file_free(void *context)
{
   struct sevenzip_context_t *sevenzip_context = (struct sevenzip_context_t*)context;

   if (!sevenzip_context)
      return;

   if (sevenzip_context->output)
   {
      IAlloc_Free(&sevenzip_context->allocImp, sevenzip_context->output);
      sevenzip_context->output       = NULL;
   }

   SzArEx_Free(&sevenzip_context->db, &sevenzip_context->allocImp);
   File_Close(&sevenzip_context->archiveStream.file);

   if (sevenzip_context->lookStream.buf)
      free(sevenzip_context->lookStream.buf);

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
   CFileInStream archiveStream;
   CLookToRead2 lookStream;
   ISzAlloc allocImp;
   ISzAlloc allocTempImp;
   CSzArEx db;
   uint8_t *output      = 0;
   int64_t outsize      = -1;

   /*These are the allocation routines.
    * Currently using the non-standard 7zip choices. */
   allocImp.Alloc       = sevenzip_stream_alloc_impl;
   allocImp.Free        = sevenzip_stream_free_impl;
   allocTempImp.Alloc   = sevenzip_stream_alloc_tmp_impl;
   allocTempImp.Free    = sevenzip_stream_free_impl;

   lookStream.bufSize   = SEVENZIP_LOOKTOREAD_BUF_SIZE * sizeof(Byte);
   lookStream.buf       = (Byte*)malloc(lookStream.bufSize);

   if (!lookStream.buf)
      lookStream.bufSize = 0;

#if defined(_WIN32) && defined(USE_WINDOWS_FILE) && !defined(LEGACY_WIN32)
   if (!string_is_empty(path))
   {
      wchar_t *pathW = utf8_to_utf16_string_alloc(path);

      if (pathW)
      {
         /* Could not open 7zip archive? */
         if (InFile_OpenW(&archiveStream.file, pathW))
         {
            free(pathW);
            return -1;
         }

         free(pathW);
      }
   }
#else
   /* Could not open 7zip archive? */
   if (InFile_Open(&archiveStream.file, path))
      return -1;
#endif

   FileInStream_CreateVTable(&archiveStream);
   LookToRead2_CreateVTable(&lookStream, false);
   lookStream.realStream = &archiveStream.vt;
   LookToRead2_Init(&lookStream);
   CrcGenerateTable();

   memset(&db, 0, sizeof(db));

   SzArEx_Init(&db);

   if (SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp) == SZ_OK)
   {
      uint32_t i;
      bool file_found      = false;
      uint16_t *temp       = NULL;
      size_t temp_size     = 0;
      uint32_t block_index   = 0xFFFFFFFF;
      SRes res             = SZ_OK;

      for (i = 0; i < db.NumFiles; i++)
      {
         size_t len;
         char infile[PATH_MAX_LENGTH];
         size_t offset                = 0;
         size_t outSizeProcessed      = 0;

         /* We skip over everything which is not a directory.
          * FIXME: Why continue then if IsDir is true?*/
         if (SzArEx_IsDir(&db, i))
            continue;

         len = SzArEx_GetFileNameUtf16(&db, i, NULL);

         if (len > temp_size)
         {
            if (temp)
               free(temp);
            temp_size = len;
            temp = (uint16_t *)malloc(temp_size * sizeof(temp[0]));

            if (temp == 0)
            {
               res = SZ_ERROR_MEM;
               break;
            }
         }

         SzArEx_GetFileNameUtf16(&db, i, temp);
         res       = SZ_ERROR_FAIL;
         infile[0] = '\0';

         if (temp)
            res = utf16_to_char_string(temp, infile, sizeof(infile))
               ? SZ_OK : SZ_ERROR_FAIL;

         if (string_is_equal(infile, needle))
         {
            size_t output_size   = 0;

            /* C LZMA SDK does not support chunked extraction - see here:
             * sourceforge.net/p/sevenzip/discussion/45798/thread/6fb59aaf/
             * */
            file_found = true;
            res = SzArEx_Extract(&db, &lookStream.vt, i, &block_index,
                  &output, &output_size, &offset, &outSizeProcessed,
                  &allocImp, &allocTempImp);

            if (res != SZ_OK)
               break; /* This goes to the error section. */

            outsize = (int64_t)outSizeProcessed;

            if (optional_outfile)
            {
               const void *ptr = (const void*)(output + offset);

               if (!filestream_write_file(optional_outfile, ptr, outsize))
               {
                  res        = SZ_OK;
                  file_found = true;
                  outsize    = -1;
               }
            }
            else
            {
               /*We could either use the 7Zip allocated buffer,
                * or create our own and use it.
                * We would however need to realloc anyways, because RetroArch
                * expects a \0 at the end, therefore we allocate new,
                * copy and free the old one. */
               *buf = malloc((size_t)(outsize + 1));
               ((char*)(*buf))[outsize] = '\0';
               memcpy(*buf,output + offset,outsize);
            }
            break;
         }
      }

      if (temp)
         free(temp);
      IAlloc_Free(&allocImp, output);

      if (!(file_found && res == SZ_OK))
      {
         /* Error handling
          *
          * Failed to open compressed file inside 7zip archive.
          */

         outsize    = -1;
      }
   }

   SzArEx_Free(&db, &allocImp);
   File_Close(&archiveStream.file);

   if (lookStream.buf)
      free(lookStream.buf);

   return outsize;
}

static bool sevenzip_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size)
{
   struct sevenzip_context_t *sevenzip_context =
         (struct sevenzip_context_t*)context;

   if (!sevenzip_context)
      return false;

   sevenzip_context->decompress_index = (uint32_t)(size_t)cdata;

   return true;
}

static int sevenzip_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   struct sevenzip_context_t *sevenzip_context =
         (struct sevenzip_context_t*)context;

   SRes res                = SZ_ERROR_FAIL;
   size_t output_size      = 0;
   size_t offset           = 0;
   size_t outSizeProcessed = 0;

   res = SzArEx_Extract(&sevenzip_context->db,
         &sevenzip_context->lookStream.vt, sevenzip_context->decompress_index,
         &sevenzip_context->block_index, &sevenzip_context->output,
         &output_size, &offset, &outSizeProcessed,
         &sevenzip_context->allocImp, &sevenzip_context->allocTempImp);

   if (res != SZ_OK)
      return 0;

   if (handle)
      handle->data = sevenzip_context->output + offset;

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
   if (filestream_read(state->archive_file, magic_buf, SEVENZIP_MAGIC_LEN) != SEVENZIP_MAGIC_LEN)
      goto error;

   if (string_is_not_equal_fast(magic_buf, SEVENZIP_MAGIC, SEVENZIP_MAGIC_LEN))
      goto error;

   sevenzip_context = (struct sevenzip_context_t*)sevenzip_stream_new();
   state->context = sevenzip_context;

#if defined(_WIN32) && defined(USE_WINDOWS_FILE) && !defined(LEGACY_WIN32)
   if (!string_is_empty(file))
   {
      wchar_t *fileW = utf8_to_utf16_string_alloc(file);

      if (fileW)
      {
         /* could not open 7zip archive? */
         if (InFile_OpenW(&sevenzip_context->archiveStream.file, fileW))
         {
            free(fileW);
            goto error;
         }

         free(fileW);
      }
   }
#else
   /* could not open 7zip archive? */
   if (InFile_Open(&sevenzip_context->archiveStream.file, file))
      goto error;
#endif

   FileInStream_CreateVTable(&sevenzip_context->archiveStream);
   LookToRead2_CreateVTable(&sevenzip_context->lookStream, false);
   sevenzip_context->lookStream.realStream = &sevenzip_context->archiveStream.vt;
   LookToRead2_Init(&sevenzip_context->lookStream);
   CrcGenerateTable();
   SzArEx_Init(&sevenzip_context->db);

   if (SzArEx_Open(&sevenzip_context->db, &sevenzip_context->lookStream.vt,
         &sevenzip_context->allocImp, &sevenzip_context->allocTempImp) != SZ_OK)
      goto error;

   state->step_total = sevenzip_context->db.NumFiles;

   return 0;

error:
   if (sevenzip_context)
      sevenzip_parse_file_free(sevenzip_context);
   state->context = NULL;
   return -1;
}

static int sevenzip_parse_file_iterate_step_internal(
      struct sevenzip_context_t *sevenzip_context, char *filename,
      const uint8_t **cdata, unsigned *cmode,
      uint32_t *size, uint32_t *csize, uint32_t *checksum,
      unsigned *payback, struct archive_extract_userdata *userdata)
{
   if (sevenzip_context->parse_index < sevenzip_context->db.NumFiles)
   {
      size_t len = SzArEx_GetFileNameUtf16(&sevenzip_context->db,
            sevenzip_context->parse_index, NULL);
      uint64_t compressed_size = 0;

      if (sevenzip_context->packIndex < sevenzip_context->db.db.NumPackStreams)
      {
         compressed_size = sevenzip_context->db.db.PackPositions[sevenzip_context->packIndex + 1] -
               sevenzip_context->db.db.PackPositions[sevenzip_context->packIndex];

         sevenzip_context->packIndex++;
      }

      if (len < PATH_MAX_LENGTH &&
          !SzArEx_IsDir(&sevenzip_context->db, sevenzip_context->parse_index))
      {
         char infile[PATH_MAX_LENGTH];
         SRes res                     = SZ_ERROR_FAIL;
         uint16_t *temp               = (uint16_t*)malloc(len * sizeof(uint16_t));

         if (!temp)
            return -1;

         infile[0] = '\0';

         SzArEx_GetFileNameUtf16(&sevenzip_context->db, sevenzip_context->parse_index,
               temp);

         if (temp)
         {
            res  = utf16_to_char_string(temp, infile, sizeof(infile))
               ? SZ_OK : SZ_ERROR_FAIL;
            free(temp);
         }

         if (res != SZ_OK)
            return -1;

         strlcpy(filename, infile, PATH_MAX_LENGTH);

         *cmode    = 0; /* unused for 7zip */
         *checksum = sevenzip_context->db.CRCs.Vals[sevenzip_context->parse_index];
         *size     = (uint32_t)SzArEx_GetFileSize(&sevenzip_context->db, sevenzip_context->parse_index);
         *csize    = (uint32_t)compressed_size;

         *cdata    = (uint8_t *)(size_t)sevenzip_context->parse_index;
      }
   }
   else
      return 0;

   *payback = 1;

   return 1;
}

static int sevenzip_parse_file_iterate_step(void *context,
      const char *valid_exts,
      struct archive_extract_userdata *userdata, file_archive_file_cb file_cb)
{
   const uint8_t *cdata = NULL;
   uint32_t checksum    = 0;
   uint32_t size        = 0;
   uint32_t csize       = 0;
   unsigned cmode       = 0;
   unsigned payload     = 0;
   struct sevenzip_context_t *sevenzip_context = (struct sevenzip_context_t*)context;
   int ret;

   userdata->current_file_path[0] = '\0';

   ret = sevenzip_parse_file_iterate_step_internal(sevenzip_context,
         userdata->current_file_path,
         &cdata, &cmode, &size, &csize,
         &checksum, &payload, userdata);

   if (ret != 1)
      return ret;

   userdata->crc                 = checksum;

   if (file_cb && !file_cb(userdata->current_file_path, valid_exts,
            cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   sevenzip_context->parse_index += payload;

   return 1;
}

static uint32_t sevenzip_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t length)
{
   return encoding_crc32(crc, data, length);
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
