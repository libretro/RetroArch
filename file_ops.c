/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Timo Strunk
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <boolean.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <retro_file.h>
#include <retro_stat.h>
#include <string/string_list.h>
#include <string/stdstring.h>
#ifdef HAVE_COMPRESSION
#include <file/file_archive.h>
#endif
#include <encodings/utf.h>

#include "file_ops.h"
#include "verbosity.h"

#ifdef HAVE_COMPRESSION
static const struct file_archive_file_backend *stream_backend;

#ifdef HAVE_7ZIP
#include "deps/7zip/7z.h"
#include "deps/7zip/7zAlloc.h"
#include "deps/7zip/7zCrc.h"
#include "deps/7zip/7zFile.h"

static bool utf16_to_char(uint8_t **utf_data,
      size_t *dest_len, const uint16_t *in)
{
   unsigned len    = 0;

   while (in[len] != '\0')
      len++;

   utf16_conv_utf8(NULL, dest_len, in, len);
   *dest_len  += 1;
   *utf_data   = (uint8_t*)malloc(*dest_len);
   if (*utf_data == 0)
      return false;

   return utf16_conv_utf8(*utf_data, dest_len, in, len);
}

static bool utf16_to_char_string(const uint16_t *in, char *s, size_t len)
{
   size_t     dest_len  = 0;
   uint8_t *utf16_data  = NULL;
   bool            ret  = utf16_to_char(&utf16_data, &dest_len, in);

   if (ret)
   {
      utf16_data[dest_len] = 0;
      strlcpy(s, (const char*)utf16_data, len);
   }

   free(utf16_data);
   utf16_data = NULL;

   return ret;
}

/* Extract the relative path (needle) from a 7z archive 
 * (path) and allocate a buf for it to write it in.
 * If optional_outfile is set, extract to that instead and don't alloc buffer.
 */
static int read_7zip_file(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   CFileInStream archiveStream;
   CLookToRead lookStream;
   CSzArEx db;
   ISzAlloc allocImp;
   ISzAlloc allocTempImp;
   uint8_t *output      = 0;
   long outsize         = -1;

   /*These are the allocation routines.
    * Currently using the non-standard 7zip choices. */
   allocImp.Alloc       = SzAlloc;
   allocImp.Free        = SzFree;
   allocTempImp.Alloc   = SzAllocTemp;
   allocTempImp.Free    = SzFreeTemp;

   if (InFile_Open(&archiveStream.file, path))
   {
      RARCH_ERR("Could not open %s as 7z archive\n.", path);
      return -1;
   }

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
         char infile[PATH_MAX_LENGTH];
         size_t offset           = 0;
         size_t outSizeProcessed = 0;
         const CSzFileItem    *f = db.db.Files + i;

         /* We skip over everything which is not a directory. 
          * FIXME: Why continue then if f->IsDir is true?*/
         if (f->IsDir)
            continue;

         len = SzArEx_GetFileNameUtf16(&db, i, NULL);

         if (len > temp_size)
         {
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
         res = utf16_to_char_string(temp, infile, sizeof(infile)) ? SZ_OK : SZ_ERROR_FAIL;

         if (string_is_equal(infile, needle))
         {
            size_t output_size   = 0;

            RARCH_LOG_OUTPUT("Opened archive %s. Now trying to extract %s\n",
                  path, needle);

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

               if (!retro_write_file(optional_outfile, ptr, outsize))
               {
                  RARCH_ERR("Could not open outfilepath %s.\n",
                        optional_outfile);
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

      free(temp);
      IAlloc_Free(&allocImp, output);

      if (!(file_found && res == SZ_OK))
      {
         /* Error handling */
         if (!file_found)
            RARCH_ERR("File %s not found in %s\n", needle, path);

         RARCH_ERR("Failed to open compressed file inside 7zip archive.\n");

         outsize    = -1;
      }
   }

   SzArEx_Free(&db, &allocImp);
   File_Close(&archiveStream.file);

   return outsize;
}

static struct string_list *compressed_7zip_file_list_new(
      const char *path, const char* ext)
{
   CFileInStream archiveStream;
   CLookToRead lookStream;
   CSzArEx db;
   ISzAlloc allocImp;
   ISzAlloc allocTempImp;
   size_t temp_size             = 0;
   struct string_list     *list = NULL;
   
   /* These are the allocation routines - currently using 
    * the non-standard 7zip choices. */
   allocImp.Alloc     = SzAlloc;
   allocImp.Free      = SzFree;
   allocTempImp.Alloc = SzAllocTemp;
   allocTempImp.Free  = SzFreeTemp;

   if (InFile_Open(&archiveStream.file, path))
   {
      RARCH_ERR("Could not open %s as 7z archive.\n",path);
      return NULL;
   }

   list = string_list_new();

   if (!list)
   {
      File_Close(&archiveStream.file);
      return NULL;
   }

   FileInStream_CreateVTable(&archiveStream);
   LookToRead_CreateVTable(&lookStream, False);
   lookStream.realStream = &archiveStream.s;
   LookToRead_Init(&lookStream);
   CrcGenerateTable();
   SzArEx_Init(&db);

   if (SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp) == SZ_OK)
   {
      uint32_t i;
      struct string_list *ext_list = ext ? string_split(ext, "|"): NULL;
      SRes res                     = SZ_OK;
      uint16_t *temp               = NULL;

      for (i = 0; i < db.db.NumFiles; i++)
      {
         union string_list_elem_attr attr;
         char infile[PATH_MAX_LENGTH];
         const char *file_ext         = NULL;
         size_t                   len = 0;
         bool supported_by_core       = false;
         const CSzFileItem         *f = db.db.Files + i;

         /* we skip over everything, which is a directory. */
         if (f->IsDir)
            continue;

         len = SzArEx_GetFileNameUtf16(&db, i, NULL);

         if (len > temp_size)
         {
            free(temp);
            temp_size = len;
            temp      = (uint16_t *)malloc(temp_size * sizeof(temp[0]));

            if (temp == 0)
            {
               res = SZ_ERROR_MEM;
               break;
            }
         }

         SzArEx_GetFileNameUtf16(&db, i, temp);
         res      = utf16_to_char_string(temp, infile, sizeof(infile)) ? SZ_OK : SZ_ERROR_FAIL;
         file_ext = path_get_extension(infile);

         if (string_list_find_elem_prefix(ext_list, ".", file_ext))
            supported_by_core = true;

         /*
          * Currently we only support files without subdirs in the archives.
          * Folders are not supported (differences between win and lin.
          * Archives within archives should imho never be supported.
          */

         if (!supported_by_core)
            continue;

         attr.i = RARCH_COMPRESSED_FILE_IN_ARCHIVE;

         if (!string_list_append(list, infile, attr))
         {
            res = SZ_ERROR_MEM;
            break;
         }
      }

      string_list_free(ext_list);
      free(temp);

      if (res != SZ_OK)
      {
         /* Error handling */
         RARCH_ERR("Failed to open compressed_file: \"%s\"\n", path);

         string_list_free(list);
         list = NULL;
      }
   }

   SzArEx_Free(&db, &allocImp);
   File_Close(&archiveStream.file);

   return list;
}
#endif

#ifdef HAVE_ZLIB
struct decomp_state
{
   char *opt_file;
   char *needle;
   void **buf;
   size_t size;
   bool found;
};

/* Extract the relative path (needle) from a 
 * ZIP archive (path) and allocate a buffer for it to write it in. 
 *
 * optional_outfile if not NULL will be used to extract the file to. 
 * buf will be 0 then.
 */

static int zip_file_decompressed(const char *name, const char *valid_exts,
   const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
   uint32_t crc32, void *userdata)
{
   struct decomp_state *st = (struct decomp_state*)userdata;
   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      goto next_file;

   RARCH_LOG("[deflate] Path: %s, CRC32: 0x%x\n", name, crc32);

   if (strstr(name, st->needle))
   {
      st->found = true;

      if (st->opt_file != 0)
      {
         if (!retro_write_file(st->opt_file, cdata, size))
            goto error;

         st->size = 0;
      }
      else
      {
         int ret = 0;
         file_archive_file_handle_t handle = {0};
         handle.backend = file_archive_get_default_file_backend();
         if (!handle.backend->stream_decompress_data_to_file_init(&handle, cdata, csize, size))
            return false;

         do{
            ret = handle.backend->stream_decompress_data_to_file_iterate(handle.stream);
         }while(ret == 0);

         handle.real_checksum = handle.backend->stream_crc_calculate(0, handle.data, size);

         if (handle.real_checksum != crc32)
         {
            RARCH_ERR("Inflated checksum did not match CRC32!\n");
            goto error;
         }

         if (handle.stream)
            free(handle.stream);
         *st->buf = malloc(size);
         memcpy(*st->buf, handle.data, size);

         if (handle.data)
            free(handle.data);

         st->size = size;
      }
   }

next_file:
   return 1;

error:
   return 0;
}

static int read_zip_file(const char *path,
      const char *needle, void **buf,
      const char* optional_outfile)
{
   file_archive_transfer_t zlib;
   struct decomp_state st;
   bool returnerr = true;
   int ret        = 0;

   zlib.type      = ZLIB_TRANSFER_INIT;

   st.needle      = needle           ? strdup(needle)           : NULL;
   st.opt_file    = optional_outfile ? strdup(optional_outfile) : NULL;
   st.found       = false;
   st.buf         = buf;

   do
   {
      ret = file_archive_parse_file_iterate(&zlib, &returnerr, path,
            "", zip_file_decompressed, &st);
      if (!returnerr)
         break;
   }while(ret == 0 && !st.found);

   file_archive_parse_file_iterate_stop(&zlib);

   if (st.opt_file)
      free(st.opt_file);
   if (st.needle)
      free(st.needle);

   if (!st.found)
      return -1;

   return st.size;
}
#endif

#endif

#ifdef HAVE_COMPRESSION
/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
int read_compressed_file(const char * path, void **buf,
      const char* optional_filename, ssize_t *length)
{
   int ret                            = 0;
   const char* file_ext               = NULL;
   struct string_list *str_list       = string_split(path, "#");

   /* Safety check.
    * If optional_filename and optional_filename 
    * exists, we simply return 0,
    * hoping that optional_filename is the 
    * same as requested.
    */
   if (optional_filename && path_file_exists(optional_filename))
   {
      *length = 0;
      return 1;
   }

   /* We assure that there is something after the '#' symbol.
    *
    * This error condition happens for example, when
    * path = /path/to/file.7z, or
    * path = /path/to/file.7z#
    */
   if (str_list->size <= 1)
      goto error;

#if defined(HAVE_7ZIP) || defined(HAVE_ZLIB)
   file_ext        = path_get_extension(str_list->elems[0].data);
#endif

#ifdef HAVE_7ZIP
   if (string_is_equal_noncase(file_ext, "7z"))
   {
      *length = read_7zip_file(str_list->elems[0].data, str_list->elems[1].data, buf, optional_filename);
      if (*length != -1)
         ret = 1;
   }
#endif
#ifdef HAVE_ZLIB
   if (string_is_equal_noncase(file_ext, "zip"))
   {
      stream_backend = file_archive_get_default_file_backend();
      *length        = read_zip_file(str_list->elems[0].data, str_list->elems[1].data, buf, optional_filename);
      if (*length != -1)
         ret = 1;
   }

   string_list_free(str_list);
#endif
   return ret;

error:
   RARCH_ERR("Could not extract string and substring from "
         ": %s.\n", path);
   string_list_free(str_list);
   *length = 0;
   return 0;
}
#endif

/**
 * read_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 * @length           : Number of items read, -1 on error.
 *
 * Read the contents of a file into @buf. Will call read_compressed_file
 * if path contains a compressed file, otherwise will call retro_read_file().
 *
 * Returns: 1 if file read, 0 on error.
 */
int read_file(const char *path, void **buf, ssize_t *length)
{
#ifdef HAVE_COMPRESSION
   if (path_contains_compressed_file(path))
   {
      if (read_compressed_file(path, buf, NULL, length))
         return 1;
   }
#endif
   return retro_read_file(path, buf, length);
}

struct string_list *compressed_file_list_new(const char *path,
      const char* ext)
{
#if defined(HAVE_ZLIB) || defined(HAVE_7ZIP)
   const char* file_ext = path_get_extension(path);
#endif

#ifdef HAVE_7ZIP
   if (string_is_equal_noncase(file_ext, "7z"))
      return compressed_7zip_file_list_new(path,ext);
#endif
#ifdef HAVE_ZLIB
   if (string_is_equal_noncase(file_ext, "zip"))
      return file_archive_get_file_list(path, ext);
#endif
   return NULL;
}
