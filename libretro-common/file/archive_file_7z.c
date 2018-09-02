/* Copyright  (C) 2010-2018 The RetroArch team
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

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

struct sevenzip_context_t {
   CFileInStream archiveStream;
   CLookToRead lookStream;
   ISzAlloc allocImp;
   ISzAlloc allocTempImp;
   CSzArEx db;
   size_t temp_size;
   uint32_t block_index;
   uint32_t index;
   uint32_t packIndex;
   uint8_t *output;
   file_archive_file_handle_t *handle;
};

static void *sevenzip_stream_alloc_impl(void *p, size_t size)
{
   if (size == 0)
      return 0;
   return malloc(size);
}

static void sevenzip_stream_free_impl(void *p, void *address)
{
   (void)p;

   if (address)
      free(address);
}

static void *sevenzip_stream_alloc_tmp_impl(void *p, size_t size)
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
   sevenzip_context->handle             = NULL;

   return sevenzip_context;
}

static void sevenzip_stream_free(void *data)
{
   struct sevenzip_context_t *sevenzip_context = (struct sevenzip_context_t*)data;

   if (!sevenzip_context)
      return;

   if (sevenzip_context->output)
   {
      IAlloc_Free(&sevenzip_context->allocImp, sevenzip_context->output);
      sevenzip_context->output       = NULL;
      sevenzip_context->handle->data = NULL;
   }

   SzArEx_Free(&sevenzip_context->db, &sevenzip_context->allocImp);
   File_Close(&sevenzip_context->archiveStream.file);
}

/* Extract the relative path (needle) from a 7z archive
 * (path) and allocate a buf for it to write it in.
 * If optional_outfile is set, extract to that instead
 * and don't allocate buffer.
 */
static int sevenzip_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   CFileInStream archiveStream;
   CLookToRead lookStream;
   ISzAlloc allocImp;
   ISzAlloc allocTempImp;
   CSzArEx db;
   uint8_t *output      = 0;
   long outsize         = -1;

   /*These are the allocation routines.
    * Currently using the non-standard 7zip choices. */
   allocImp.Alloc       = sevenzip_stream_alloc_impl;
   allocImp.Free        = sevenzip_stream_free_impl;
   allocTempImp.Alloc   = sevenzip_stream_alloc_tmp_impl;
   allocTempImp.Free    = sevenzip_stream_free_impl;

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
   LookToRead_CreateVTable(&lookStream, false);
   lookStream.realStream = &archiveStream.s;
   LookToRead_Init(&lookStream);
   CrcGenerateTable();

   db.db.PackSizes               = NULL;
   db.db.PackCRCsDefined         = NULL;
   db.db.PackCRCs                = NULL;
   db.db.Folders                 = NULL;
   db.db.Files                   = NULL;
   db.db.NumPackStreams          = 0;
   db.db.NumFolders              = 0;
   db.db.NumFiles                = 0;
   db.startPosAfterHeader        = 0;
   db.dataPos                    = 0;
   db.FolderStartPackStreamIndex = NULL;
   db.PackStreamStartPositions   = NULL;
   db.FolderStartFileIndex       = NULL;
   db.FileIndexToFolderIndexMap  = NULL;
   db.FileNameOffsets            = NULL;
   db.FileNames.data             = NULL;
   db.FileNames.size             = 0;

   SzArEx_Init(&db);

   if (SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp) == SZ_OK)
   {
      uint32_t i;
      bool file_found      = false;
      uint16_t *temp       = NULL;
      size_t temp_size     = 0;
      uint32_t block_index = 0xFFFFFFFF;
      SRes res             = SZ_OK;

      for (i = 0; i < db.db.NumFiles; i++)
      {
         size_t len;
         char infile[PATH_MAX_LENGTH];
         size_t offset                = 0;
         size_t outSizeProcessed      = 0;
         const CSzFileItem    *f      = db.db.Files + i;

         /* We skip over everything which is not a directory.
          * FIXME: Why continue then if f->IsDir is true?*/
         if (f->IsDir)
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
            res = SzArEx_Extract(&db, &lookStream.s, i, &block_index,
                  &output, &output_size, &offset, &outSizeProcessed,
                  &allocImp, &allocTempImp);

            if (res != SZ_OK)
               break; /* This goes to the error section. */

            outsize = outSizeProcessed;

            if (optional_outfile != NULL)
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
               *buf = malloc(outsize + 1);
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

   return (int)outsize;
}

static bool sevenzip_stream_decompress_data_to_file_init(
      file_archive_file_handle_t *handle,
      const uint8_t *cdata,  uint32_t csize, uint32_t size)
{
   struct sevenzip_context_t *sevenzip_context =
         (struct sevenzip_context_t*)handle->stream;

   if (!sevenzip_context)
      return false;

   sevenzip_context->handle = handle;

   return true;
}

static int sevenzip_stream_decompress_data_to_file_iterate(void *data)
{
   struct sevenzip_context_t *sevenzip_context =
         (struct sevenzip_context_t*)data;

   SRes res                = SZ_ERROR_FAIL;
   size_t output_size      = 0;
   size_t offset           = 0;
   size_t outSizeProcessed = 0;

   res = SzArEx_Extract(&sevenzip_context->db,
         &sevenzip_context->lookStream.s, sevenzip_context->index,
         &sevenzip_context->block_index, &sevenzip_context->output,
         &output_size, &offset, &outSizeProcessed,
         &sevenzip_context->allocImp, &sevenzip_context->allocTempImp);

   if (res != SZ_OK)
      return 0;

   if (sevenzip_context->handle)
      sevenzip_context->handle->data = sevenzip_context->output + offset;

   return 1;
}

static int sevenzip_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   struct sevenzip_context_t *sevenzip_context =
         (struct sevenzip_context_t*)sevenzip_stream_new();

   if (state->archive_size < SEVENZIP_MAGIC_LEN)
      goto error;

   if (string_is_not_equal_fast(state->data, SEVENZIP_MAGIC, SEVENZIP_MAGIC_LEN))
      goto error;

   state->stream = sevenzip_context;

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
   LookToRead_CreateVTable(&sevenzip_context->lookStream, false);
   sevenzip_context->lookStream.realStream = &sevenzip_context->archiveStream.s;
   LookToRead_Init(&sevenzip_context->lookStream);
   CrcGenerateTable();
   SzArEx_Init(&sevenzip_context->db);

   if (SzArEx_Open(&sevenzip_context->db, &sevenzip_context->lookStream.s,
         &sevenzip_context->allocImp, &sevenzip_context->allocTempImp) != SZ_OK)
      goto error;

   return 0;

error:
   if (sevenzip_context)
      sevenzip_stream_free(sevenzip_context);
   return -1;
}

static int sevenzip_parse_file_iterate_step_internal(
      file_archive_transfer_t *state, char *filename,
      const uint8_t **cdata, unsigned *cmode,
      uint32_t *size, uint32_t *csize, uint32_t *checksum,
      unsigned *payback, struct archive_extract_userdata *userdata)
{
   struct sevenzip_context_t *sevenzip_context = (struct sevenzip_context_t*)state->stream;
   const CSzFileItem *file = sevenzip_context->db.db.Files + sevenzip_context->index;

   if (sevenzip_context->index < sevenzip_context->db.db.NumFiles)
   {
      size_t len = SzArEx_GetFileNameUtf16(&sevenzip_context->db,
            sevenzip_context->index, NULL);
      uint64_t compressed_size = 0;

      if (sevenzip_context->packIndex < sevenzip_context->db.db.NumPackStreams)
      {
         compressed_size = sevenzip_context->db.db.PackSizes[sevenzip_context->packIndex];
         sevenzip_context->packIndex++;
      }

      if (len < PATH_MAX_LENGTH && !file->IsDir)
      {
         char infile[PATH_MAX_LENGTH];
         SRes res                     = SZ_ERROR_FAIL;
         uint16_t *temp               = (uint16_t*)malloc(len * sizeof(uint16_t));

         if (!temp)
            return -1;

         infile[0] = '\0';

         SzArEx_GetFileNameUtf16(&sevenzip_context->db, sevenzip_context->index,
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

         *cmode    = ARCHIVE_MODE_COMPRESSED;
         *checksum = file->Crc;
         *size     = (uint32_t)file->Size;
         *csize    = (uint32_t)compressed_size;
      }
   }
   else
      return 0;

   *payback = 1;

   return 1;
}

static int sevenzip_parse_file_iterate_step(file_archive_transfer_t *state,
      const char *valid_exts,
      struct archive_extract_userdata *userdata, file_archive_file_cb file_cb)
{
   char filename[PATH_MAX_LENGTH];
   const uint8_t *cdata = NULL;
   uint32_t checksum    = 0;
   uint32_t size        = 0;
   uint32_t csize       = 0;
   unsigned cmode       = 0;
   unsigned payload     = 0;
   struct sevenzip_context_t *sevenzip_context = NULL;
   int ret;

   filename[0]                   = '\0';

   ret = sevenzip_parse_file_iterate_step_internal(state, filename,
         &cdata, &cmode, &size, &csize,
         &checksum, &payload, userdata);

   if (ret != 1)
      return ret;

   userdata->extracted_file_path = filename;
   userdata->crc                 = checksum;

   if (file_cb && !file_cb(filename, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   sevenzip_context = (struct sevenzip_context_t*)state->stream;

   sevenzip_context->index += payload;

   return 1;
}

static uint32_t sevenzip_stream_crc32_calculate(uint32_t crc,
      const uint8_t *data, size_t length)
{
   return encoding_crc32(crc, data, length);
}

const struct file_archive_file_backend sevenzip_backend = {
   sevenzip_stream_new,
   sevenzip_stream_free,
   sevenzip_stream_decompress_data_to_file_init,
   sevenzip_stream_decompress_data_to_file_iterate,
   sevenzip_stream_crc32_calculate,
   sevenzip_file_read,
   sevenzip_parse_file_init,
   sevenzip_parse_file_iterate_step,
   "7z"
};
