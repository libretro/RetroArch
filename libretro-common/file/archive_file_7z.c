/* Copyright  (C) 2010-2016 The RetroArch team
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

#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>
#include <encodings/crc32.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include "../../deps/7zip/7z.h"
#include "../../deps/7zip/7zAlloc.h"
#include "../../deps/7zip/7zCrc.h"
#include "../../deps/7zip/7zFile.h"

#define SEVENZIP_MAGIC "7z\xBC\xAF\x27\x1C"
#define SEVENZIP_MAGIC_LEN 6

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

static void* sevenzip_stream_new(void)
{
   struct sevenzip_context_t *sevenzip_context =
         (struct sevenzip_context_t*)calloc(1, sizeof(struct sevenzip_context_t));

   memset(sevenzip_context, 0, sizeof(struct sevenzip_context_t));

   /* These are the allocation routines - currently using
    * the non-standard 7zip choices. */
   sevenzip_context->allocImp.Alloc     = SzAlloc;
   sevenzip_context->allocImp.Free      = SzFree;
   sevenzip_context->allocTempImp.Alloc = SzAllocTemp;
   sevenzip_context->allocTempImp.Free  = SzFreeTemp;
   sevenzip_context->block_index = 0xFFFFFFFF;
   sevenzip_context->output = NULL;
   sevenzip_context->handle = NULL;

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
      sevenzip_context->output = NULL;
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
   allocImp.Alloc       = SzAlloc;
   allocImp.Free        = SzFree;
   allocTempImp.Alloc   = SzAllocTemp;
   allocTempImp.Free    = SzFreeTemp;

   /* Could not open 7zip archive? */
   if (InFile_Open(&archiveStream.file, path))
      return -1;

   FileInStream_CreateVTable(&archiveStream);
   LookToRead_CreateVTable(&lookStream, False);
   lookStream.realStream = &archiveStream.s;
   LookToRead_Init(&lookStream);
   CrcGenerateTable();
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
         char infile[PATH_MAX_LENGTH] = {0};
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
         res = SZ_ERROR_FAIL;
         if (temp)
            res = utf16_to_char_string(temp, infile, sizeof(infile))
               ? SZ_OK : SZ_ERROR_FAIL;

         if (string_is_equal(infile, needle))
         {
            size_t output_size   = 0;

            /*RARCH_LOG_OUTPUT("Opened archive %s. Now trying to extract %s\n",
                  path, needle);*/

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
                  /*RARCH_ERR("Could not open outfilepath %s.\n",
                        optional_outfile);*/
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

   return outsize;
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

   SRes res = SZ_ERROR_FAIL;
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
      return -1;

   if (memcmp(state->data, SEVENZIP_MAGIC, SEVENZIP_MAGIC_LEN) != 0)
      return -1;

   state->stream = sevenzip_context;

   /* could not open 7zip archive? */
   if (InFile_Open(&sevenzip_context->archiveStream.file, file))
      return -1;

   FileInStream_CreateVTable(&sevenzip_context->archiveStream);
   LookToRead_CreateVTable(&sevenzip_context->lookStream, False);
   sevenzip_context->lookStream.realStream = &sevenzip_context->archiveStream.s;
   LookToRead_Init(&sevenzip_context->lookStream);
   CrcGenerateTable();
   SzArEx_Init(&sevenzip_context->db);

   SRes res = SzArEx_Open(&sevenzip_context->db, &sevenzip_context->lookStream.s,
         &sevenzip_context->allocImp, &sevenzip_context->allocTempImp);

   if (res != SZ_OK)
      return -1;

   return 0;
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
         SRes res                     = SZ_ERROR_FAIL;
         char infile[PATH_MAX_LENGTH] = {0};
         uint16_t *temp               = (uint16_t*)malloc(len * sizeof(uint16_t));

         if (!temp)
            return -1;

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

         *cmode = ARCHIVE_MODE_COMPRESSED;
         *checksum = file->Crc;
         *size = file->Size;
         *csize = compressed_size;
      }
   }

   *payback = 1;

   return 1;
}

static int sevenzip_parse_file_iterate_step(file_archive_transfer_t *state,
      const char *valid_exts, struct archive_extract_userdata *userdata, file_archive_file_cb file_cb)
{
   const uint8_t *cdata = NULL;
   uint32_t checksum    = 0;
   uint32_t size        = 0;
   uint32_t csize       = 0;
   unsigned cmode       = 0;
   unsigned payload     = 0;
   struct sevenzip_context_t *sevenzip_context = NULL;
   char filename[PATH_MAX_LENGTH] = {0};
   int ret = sevenzip_parse_file_iterate_step_internal(state, filename,
         &cdata, &cmode, &size, &csize,
         &checksum, &payload, userdata);

   if (ret != 1)
      return ret;

   userdata->extracted_file_path = filename;

   if (!file_cb(filename, valid_exts, cdata, cmode,
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
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   sevenzip_stream_decompress_data_to_file_init,
   sevenzip_stream_decompress_data_to_file_iterate,
   NULL,
   NULL,
   NULL,
   sevenzip_stream_crc32_calculate,
   sevenzip_file_read,
   sevenzip_parse_file_init,
   sevenzip_parse_file_iterate_step,
   "7z"
};
