/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#define setmode _setmode
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif
#endif

#include <boolean.h>

#include <encodings/utf.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#ifdef HAVE_COMPRESSION
#include <file/archive_file.h>
#endif
#include <string/stdstring.h>

#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <retro_stat.h>
#include <retro_assert.h>

#include <lists/string_list.h>
#include <string/stdstring.h>

#include "defaults.h"
#include "msg_hash.h"
#include "content.h"
#include "general.h"
#include "dynamic.h"
#include "movie.h"
#include "patch.h"
#include "system.h"
#include "retroarch.h"
#include "command_event.h"
#include "libretro_version_1.h"
#include "verbosity.h"

#ifdef HAVE_7ZIP
#include "deps/7zip/7z.h"
#include "deps/7zip/7zAlloc.h"
#include "deps/7zip/7zCrc.h"
#include "deps/7zip/7zFile.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif

#define MAX_ARGS 32

struct sram_block
{
   unsigned type;
   void *data;
   size_t size;
};

#ifdef HAVE_COMPRESSION

#ifdef HAVE_7ZIP
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
 * If optional_outfile is set, extract to that instead 
 * and don't allocate buffer.
 */
static int content_7zip_file_read(
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
         res = utf16_to_char_string(temp, infile, sizeof(infile)) 
            ? SZ_OK : SZ_ERROR_FAIL;

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

               if (!filestream_write_file(optional_outfile, ptr, outsize))
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
         res      = utf16_to_char_string(temp, infile, sizeof(infile)) 
            ? SZ_OK : SZ_ERROR_FAIL;
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

static bool content_zip_file_decompressed_handle(
      file_archive_file_handle_t *handle,
      const uint8_t *cdata, uint32_t csize,
      uint32_t size, uint32_t crc32)
{
   int ret   = 0;

   handle->backend = file_archive_get_default_file_backend();

   if (!handle->backend)
      goto error;

   if (!handle->backend->stream_decompress_data_to_file_init(
            handle, cdata, csize, size))
      return false;

   do{
      ret = handle->backend->stream_decompress_data_to_file_iterate(
            handle->stream);
   }while(ret == 0);

   handle->real_checksum = handle->backend->stream_crc_calculate(0,
         handle->data, size);

   if (handle->real_checksum != crc32)
   {
      RARCH_ERR("Inflated checksum did not match CRC32!\n");
      goto error;
   }

   if (handle->stream)
      free(handle->stream);

   return true;

error:
   if (handle->stream)
      free(handle->stream);
   if (handle->data)
      free(handle->data);

   return false;
}

/* Extract the relative path (needle) from a 
 * ZIP archive (path) and allocate a buffer for it to write it in. 
 *
 * optional_outfile if not NULL will be used to extract the file to. 
 * buf will be 0 then.
 */

static int content_zip_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   struct decomp_state *st = (struct decomp_state*)userdata;

   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      return 1;

   RARCH_LOG("[deflate] Path: %s, CRC32: 0x%x\n", name, crc32);

   if (strstr(name, st->needle))
   {
      bool goto_error = false;
      file_archive_file_handle_t handle = {0};

      st->found = true;

      if (content_zip_file_decompressed_handle(&handle,
               cdata, csize, size, crc32))
      {
         if (st->opt_file != 0)
         {
            /* Called in case core has need_fullpath enabled. */
            char *buf       = (char*)malloc(size);

            if (buf)
            {
               RARCH_LOG("Extracting file : %s\n", st->opt_file);
               memcpy(buf, handle.data, size);

               if (!filestream_write_file(st->opt_file, buf, size))
                  goto_error = true;
            }

            free(buf);

            st->size = 0;
         }
         else
         {
            /* Called in case core has need_fullpath disabled.
             * Will copy decompressed content directly into
             * RetroArch's ROM buffer. */
            *st->buf = malloc(size);
            memcpy(*st->buf, handle.data, size);

            st->size = size;
         }
      }

      if (handle.data)
         free(handle.data);

      if (goto_error)
         return 0;
   }

   return 1;
}

static int content_zip_file_read(
      const char *path,
      const char *needle, void **buf,
      const char* optional_outfile)
{
   file_archive_transfer_t zlib;
   struct decomp_state st;
   bool returnerr = true;
   int ret        = 0;

   zlib.type      = ZLIB_TRANSFER_INIT;

   st.needle      = NULL;
   st.opt_file    = NULL;
   st.found       = false;
   st.buf         = buf;

   if (needle)
      st.needle   = strdup(needle);
   if (optional_outfile)
      st.opt_file = strdup(optional_outfile);

   do
   {
      ret = file_archive_parse_file_iterate(&zlib, &returnerr, path,
            "", content_zip_file_decompressed, &st);
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
static int content_file_compressed_read(
      const char * path, void **buf,
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
      *length = content_7zip_file_read(str_list->elems[0].data,
            str_list->elems[1].data, buf, optional_filename);
      if (*length != -1)
         ret = 1;
   }
#endif
#ifdef HAVE_ZLIB
   if (string_is_equal_noncase(file_ext, "zip"))
   {
      *length        = content_zip_file_read(str_list->elems[0].data,
            str_list->elems[1].data, buf, optional_filename);
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
 * content_file_read:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 * @length           : Number of items read, -1 on error.
 *
 * Read the contents of a file into @buf. Will call content_file_compressed_read 
 * if path contains a compressed file, otherwise will call filestream_read_file().
 *
 * Returns: 1 if file read, 0 on error.
 */
static int content_file_read(const char *path, void **buf, ssize_t *length)
{
#ifdef HAVE_COMPRESSION
   if (path_contains_compressed_file(path))
   {
      if (content_file_compressed_read(path, buf, NULL, length))
         return 1;
   }
#endif
   return filestream_read_file(path, buf, length);
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

static void check_defaults_dir_create_dir(const char *path)
{
   if (path_is_directory(path))
      return;
   path_mkdir(path);
}

static void check_defaults_dirs(void)
{
   if (*g_defaults.dir.core_assets)
      check_defaults_dir_create_dir(g_defaults.dir.core_assets);
   if (*g_defaults.dir.remap)
      check_defaults_dir_create_dir(g_defaults.dir.remap);
   if (*g_defaults.dir.screenshot)
      check_defaults_dir_create_dir(g_defaults.dir.screenshot);
   if (*g_defaults.dir.core)
      check_defaults_dir_create_dir(g_defaults.dir.core);
   if (*g_defaults.dir.autoconfig)
      check_defaults_dir_create_dir(g_defaults.dir.autoconfig);
   if (*g_defaults.dir.audio_filter)
      check_defaults_dir_create_dir(g_defaults.dir.audio_filter);
   if (*g_defaults.dir.video_filter)
      check_defaults_dir_create_dir(g_defaults.dir.video_filter);
   if (*g_defaults.dir.assets)
      check_defaults_dir_create_dir(g_defaults.dir.assets);
   if (*g_defaults.dir.playlist)
      check_defaults_dir_create_dir(g_defaults.dir.playlist);
   if (*g_defaults.dir.core)
      check_defaults_dir_create_dir(g_defaults.dir.core);
   if (*g_defaults.dir.core_info)
      check_defaults_dir_create_dir(g_defaults.dir.core_info);
   if (*g_defaults.dir.overlay)
      check_defaults_dir_create_dir(g_defaults.dir.overlay);
   if (*g_defaults.dir.port)
      check_defaults_dir_create_dir(g_defaults.dir.port);
   if (*g_defaults.dir.shader)
      check_defaults_dir_create_dir(g_defaults.dir.shader);
   if (*g_defaults.dir.savestate)
      check_defaults_dir_create_dir(g_defaults.dir.savestate);
   if (*g_defaults.dir.sram)
      check_defaults_dir_create_dir(g_defaults.dir.sram);
   if (*g_defaults.dir.system)
      check_defaults_dir_create_dir(g_defaults.dir.system);
   if (*g_defaults.dir.resampler)
      check_defaults_dir_create_dir(g_defaults.dir.resampler);
   if (*g_defaults.dir.menu_config)
      check_defaults_dir_create_dir(g_defaults.dir.menu_config);
   if (*g_defaults.dir.content_history)
      check_defaults_dir_create_dir(g_defaults.dir.content_history);
   if (*g_defaults.dir.cache)
      check_defaults_dir_create_dir(g_defaults.dir.cache);
   if (*g_defaults.dir.database)
      check_defaults_dir_create_dir(g_defaults.dir.database);
   if (*g_defaults.dir.cursor)
      check_defaults_dir_create_dir(g_defaults.dir.cursor);
   if (*g_defaults.dir.cheats)
      check_defaults_dir_create_dir(g_defaults.dir.cheats);
   if (*g_defaults.dir.thumbnails)
      check_defaults_dir_create_dir(g_defaults.dir.thumbnails);
}

void content_push_to_history_playlist(bool do_push,
      const char *path, void *data)
{
   settings_t *settings             = config_get_ptr();
   struct retro_system_info *info   = (struct retro_system_info*)data;

   /* If the history list is not enabled, early return. */
   if (!settings->history_list_enable)
      return;
   if (!g_defaults.history)
      return;
   if (!do_push)
      return;

   content_playlist_push(g_defaults.history,
         path,
         NULL,
         settings->libretro,
         info->library_name,
         NULL,
         NULL);
}

/**
 * content_load_init_wrap:
 * @args                 : Input arguments.
 * @argc                 : Count of arguments.
 * @argv                 : Arguments.
 *
 * Generates an @argc and @argv pair based on @args
 * of type rarch_main_wrap.
 **/
static void content_load_init_wrap(
      const struct rarch_main_wrap *args,
      int *argc, char **argv)
{
#ifdef HAVE_FILE_LOGGER
   int i;
#endif

   *argc = 0;
   argv[(*argc)++] = strdup("retroarch");

   if (!args->no_content)
   {
      if (args->content_path)
      {
         RARCH_LOG("Using content: %s.\n", args->content_path);
         argv[(*argc)++] = strdup(args->content_path);
      }
#ifdef HAVE_MENU
      else
      {
         RARCH_LOG("No content, starting dummy core.\n");
         argv[(*argc)++] = strdup("--menu");
      }
#endif
   }

   if (args->sram_path)
   {
      argv[(*argc)++] = strdup("-s");
      argv[(*argc)++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[(*argc)++] = strdup("-S");
      argv[(*argc)++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[(*argc)++] = strdup("-c");
      argv[(*argc)++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[(*argc)++] = strdup("-L");
      argv[(*argc)++] = strdup(args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[(*argc)++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   for (i = 0; i < *argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
#endif
}

/**
 * content_load:
 *
 * Loads content file and starts up RetroArch.
 * If no content file can be loaded, will start up RetroArch
 * as-is.
 *
 * Returns: false (0) if rarch_main_init failed, otherwise true (1).
 **/
static bool content_load(content_ctx_info_t *info)
{
   unsigned i;
   bool retval                       = true;
   int rarch_argc                    = 0;
   char *rarch_argv[MAX_ARGS]        = {NULL};
   char *argv_copy [MAX_ARGS]        = {NULL};
   char **rarch_argv_ptr             = (char**)info->argv;
   int *rarch_argc_ptr               = (int*)&info->argc;
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)
      calloc(1, sizeof(*wrap_args));

   if (!wrap_args)
      return false;

   retro_assert(wrap_args);

   if (info->environ_get)
      info->environ_get(rarch_argc_ptr,
            rarch_argv_ptr, info->args, wrap_args);

   if (wrap_args->touched)
   {
      content_load_init_wrap(wrap_args, &rarch_argc, rarch_argv);
      memcpy(argv_copy, rarch_argv, sizeof(rarch_argv));
      rarch_argv_ptr = (char**)rarch_argv;
      rarch_argc_ptr = (int*)&rarch_argc;
   }

   rarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   wrap_args->argc = *rarch_argc_ptr;
   wrap_args->argv = rarch_argv_ptr;

   if (!rarch_ctl(RARCH_CTL_MAIN_INIT, wrap_args))
   {
      retval = false;
      goto error;
   }

#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SHADER_MANAGER_INIT, NULL);
#endif
   event_cmd_ctl(EVENT_CMD_HISTORY_INIT, NULL);
   event_cmd_ctl(EVENT_CMD_RESUME, NULL);
   event_cmd_ctl(EVENT_CMD_VIDEO_SET_ASPECT_RATIO, NULL);

   check_defaults_dirs();

   frontend_driver_process_args(rarch_argc_ptr, rarch_argv_ptr);
   frontend_driver_content_loaded();

error:
   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);
   free(wrap_args);
   return retval;
}

/**
 * read_content_file:
 * @path         : buffer of the content file.
 * @buf          : size   of the content file.
 * @length       : size of the content file that has been read from.
 *
 * Read the content file. If read into memory, also performs soft patching
 * (see patch_content function) in case soft patching has not been
 * blocked by the enduser.
 *
 * Returns: true if successful, false on error.
 **/
static bool read_content_file(unsigned i, const char *path, void **buf,
      ssize_t *length)
{
#ifdef HAVE_ZLIB
   content_stream_t stream_info;
   uint32_t *content_crc_ptr = NULL;
#endif
   uint8_t *ret_buf          = NULL;
   global_t *global          = global_get_ptr();

   RARCH_LOG("%s: %s.\n",
         msg_hash_to_str(MSG_LOADING_CONTENT_FILE), path);
   if (!content_file_read(path, (void**) &ret_buf, length))
      return false;

   if (*length < 0)
      return false;

   if (i != 0)
      return true;

   /* Attempt to apply a patch. */
   if (!global->patch.block_patch)
      patch_content(&ret_buf, length);

#ifdef HAVE_ZLIB
   content_ctl(CONTENT_CTL_GET_CRC, &content_crc_ptr);

   stream_info.a = 0;
   stream_info.b = ret_buf;
   stream_info.c = *length;

   content_ctl(CONTENT_CTL_STREAM_CRC_CALCULATE, &stream_info);

   *content_crc_ptr = stream_info.crc;


   RARCH_LOG("CRC32: 0x%x .\n", (unsigned)*content_crc_ptr);
#endif
   *buf = ret_buf;

   return true;
}

/**
 * dump_to_file_desperate:
 * @data         : pointer to data buffer.
 * @size         : size of @data.
 * @type         : type of file to be saved.
 *
 * Attempt to save valuable RAM data somewhere.
 **/
static bool dump_to_file_desperate(const void *data,
      size_t size, unsigned type)
{
   time_t time_;
   char timebuf[256];
   char path[PATH_MAX_LENGTH];
#if defined(_WIN32) && !defined(_XBOX)
   const char *base = getenv("APPDATA");
#elif defined(__CELLOS_LV2__) || defined(_XBOX)
   const char *base = NULL;
#else
   const char *base = getenv("HOME");
#endif

   if (!base)
      return false;

   snprintf(path, sizeof(path), "%s/RetroArch-recovery-%u", base, type);

   time(&time_);

   strftime(timebuf, sizeof(timebuf), "%Y-%m-%d-%H-%M-%S", localtime(&time_));
   strlcat(path, timebuf, sizeof(path));

   if (!filestream_write_file(path, data, size))
      return false;

   RARCH_WARN("Succeeded in saving RAM data to \"%s\".\n", path);
   return true;
}


/**
 * save_state:
 * @path      : path of saved state that shall be written to.
 *
 * Save a state from memory to disk.
 *
 * Returns: true if successful, false otherwise.
 **/
static bool content_save_state(const char *path)
{
   retro_ctx_serialize_info_t serial_info;
   retro_ctx_size_info_t info;
   bool ret    = false;
   void *data  = NULL;

   core_ctl(CORE_CTL_RETRO_SERIALIZE_SIZE, &info);

   RARCH_LOG("%s: \"%s\".\n",
         msg_hash_to_str(MSG_SAVING_STATE),
         path);

   if (info.size == 0)
      return false;

   data = malloc(info.size);

   if (!data)
      return false;

   RARCH_LOG("%s: %d %s.\n",
         msg_hash_to_str(MSG_STATE_SIZE),
         (int)info.size,
         msg_hash_to_str(MSG_BYTES));

   serial_info.data = data;
   serial_info.size = info.size;
   ret = core_ctl(CORE_CTL_RETRO_SERIALIZE, &serial_info);

   if (ret)
      ret = filestream_write_file(path, data, info.size);
   else
   {
      RARCH_ERR("%s \"%s\".\n",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
            path);
   }

   free(data);

   return ret;
}

/**
 * content_load_state:
 * @path      : path that state will be loaded from.
 *
 * Load a state from disk to memory.
 *
 * Returns: true if successful, false otherwise.
 **/
static bool content_load_state(const char *path)
{
   unsigned i;
   ssize_t size;
   retro_ctx_serialize_info_t serial_info;
   unsigned num_blocks       = 0;
   void *buf                 = NULL;
   struct sram_block *blocks = NULL;
   settings_t *settings      = config_get_ptr();
   global_t *global          = global_get_ptr();
   bool ret                  = filestream_read_file(path, &buf, &size);

   RARCH_LOG("%s: \"%s\".\n",
         msg_hash_to_str(MSG_LOADING_STATE),
         path);

   if (!ret || size < 0)
      goto error;

   RARCH_LOG("%s: %u %s.\n",
         msg_hash_to_str(MSG_STATE_SIZE),
         (unsigned)size,
         msg_hash_to_str(MSG_BYTES));

   if (settings->block_sram_overwrite && global->savefiles
         && global->savefiles->size)
   {
      RARCH_LOG("%s.\n",
            msg_hash_to_str(MSG_BLOCKING_SRAM_OVERWRITE));
      blocks = (struct sram_block*)
         calloc(global->savefiles->size, sizeof(*blocks));

      if (blocks)
      {
         num_blocks = global->savefiles->size;
         for (i = 0; i < num_blocks; i++)
            blocks[i].type = global->savefiles->elems[i].attr.i;
      }
   }


   for (i = 0; i < num_blocks; i++)
   {
      retro_ctx_memory_info_t    mem_info;

      mem_info.id = blocks[i].type;
      core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

      blocks[i].size = mem_info.size;
   }

   for (i = 0; i < num_blocks; i++)
      if (blocks[i].size)
         blocks[i].data = malloc(blocks[i].size);

   /* Backup current SRAM which is overwritten by unserialize. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         retro_ctx_memory_info_t    mem_info;
         const void *ptr = NULL;

         mem_info.id = blocks[i].type;

         core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

         ptr = mem_info.data;
         if (ptr)
            memcpy(blocks[i].data, ptr, blocks[i].size);
      }
   }

   serial_info.data_const = buf;
   serial_info.size       = size;
   ret = core_ctl(CORE_CTL_RETRO_UNSERIALIZE, &serial_info);

   /* Flush back. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         retro_ctx_memory_info_t    mem_info;
         void *ptr = NULL;

         mem_info.id = blocks[i].type;

         core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

         ptr = mem_info.data;
         if (ptr)
            memcpy(ptr, blocks[i].data, blocks[i].size);
      }
   }

   for (i = 0; i < num_blocks; i++)
      free(blocks[i].data);
   free(blocks);
   free(buf);
   
   if (!ret)
      goto error;

   return true;

error:
   RARCH_ERR("%s \"%s\".\n",
         msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
         path);
   return false;
}

/**
 * load_ram_file:
 * @path             : path of RAM state that will be loaded from.
 * @type             : type of memory
 *
 * Load a RAM state from disk to memory.
 */
static bool load_ram_file(void *data)
{
   ssize_t rc;
   retro_ctx_memory_info_t mem_info;
   void *buf       = NULL;
   ram_type_t *ram = (ram_type_t*)data;

   if (!ram)
      return false;

   mem_info.id  = ram->type;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   if (mem_info.size == 0 || !mem_info.data)
      return false;

   if (!filestream_read_file(ram->path, &buf, &rc))
      return false;

   if (rc > 0)
   {
      if (rc > (ssize_t)mem_info.size)
      {
         RARCH_WARN("SRAM is larger than implementation expects, "
               "doing partial load (truncating %u %s %s %u).\n",
               (unsigned)rc,
               msg_hash_to_str(MSG_BYTES),
               msg_hash_to_str(MSG_TO),
               (unsigned)mem_info.size);
         rc = mem_info.size;
      }
      memcpy(mem_info.data, buf, rc);
   }

   if (buf)
      free(buf);

   return true;
}

/**
 * save_ram_file:
 * @path             : path of RAM state that shall be written to.
 * @type             : type of memory
 *
 * Save a RAM state from memory to disk.
 *
 */
static bool save_ram_file(ram_type_t *ram)
{
   retro_ctx_memory_info_t mem_info;

   if (!ram)
      return false;

   mem_info.id = ram->type;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   if (!mem_info.data || mem_info.size == 0)
      return false;

   if (!filestream_write_file(ram->path, mem_info.data, mem_info.size))
   {
      RARCH_ERR("%s.\n",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_SRAM));
      RARCH_WARN("Attempting to recover ...\n");

      /* In case the file could not be written to, 
       * the fallback function 'dump_to_file_desperate'
       * will be called. */
      if (!dump_to_file_desperate(mem_info.data, mem_info.size, ram->type))
      {
         RARCH_WARN("Failed ... Cannot recover save file.\n");
      }
      return false;
   }

   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_SAVED_SUCCESSFULLY_TO),
         ram->path);

   return true;
}

/* Load the content into memory. */
static bool load_content_into_memory(
      struct retro_game_info *info,
      unsigned i,
      const char *path)
{
   ssize_t len;
   bool ret = false;

   if (i == 0)
   {
      /* First content file is significant, attempt to do patching,
       * CRC checking, etc. */
      ret = read_content_file(i, path, (void**)&info->data, &len);
   }
   else
      ret = content_file_read(path, (void**)&info->data, &len);

   if (!ret || len < 0)
      goto error;

   info->size = len;

   return true;

error:
   RARCH_ERR("%s \"%s\".\n",
         msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
         path);
   return false;
}

#ifdef HAVE_COMPRESSION
static bool load_content_from_compressed_archive(
      struct string_list *temporary_content,
      struct retro_game_info *info, unsigned i,
      struct string_list* additional_path_allocs,
      bool need_fullpath, const char *path)
{
   union string_list_elem_attr attributes;
   char new_path[PATH_MAX_LENGTH];
   char new_basedir[PATH_MAX_LENGTH];
   ssize_t new_path_len              = 0;
   bool ret                          = false;
   settings_t *settings              = config_get_ptr();
   rarch_system_info_t      *sys_info= NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &sys_info);

   if (sys_info && sys_info->info.block_extract)
      return true;
   if (!need_fullpath || !path_contains_compressed_file(path))
      return true;

   RARCH_LOG("Compressed file in case of need_fullpath."
         " Now extracting to temporary directory.\n");

   strlcpy(new_basedir, settings->cache_directory,
         sizeof(new_basedir));

   if (string_is_empty(new_basedir) || !path_is_directory(new_basedir))
   {
      RARCH_WARN("Tried extracting to cache directory, but "
            "cache directory was not set or found. "
            "Setting cache directory to directory "
            "derived by basename...\n");
      fill_pathname_basedir(new_basedir, path,
            sizeof(new_basedir));
   }

   attributes.i = 0;
   fill_pathname_join(new_path, new_basedir,
         path_basename(path), sizeof(new_path));

   ret = content_file_compressed_read(path, NULL, new_path, &new_path_len);

   if (!ret || new_path_len < 0)
   {
      RARCH_ERR("%s \"%s\".\n",
            msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
            path);
      return false;
   }

   RARCH_LOG("New path is: [%s]\n", new_path);

   string_list_append(additional_path_allocs, new_path, attributes);
   info[i].path = 
      additional_path_allocs->elems[additional_path_allocs->size -1 ].data;

   if (!string_list_append(temporary_content, new_path, attributes))
      return false;

   return true;
}
#endif

/**
 * load_content:
 * @special          : subsystem of content to be loaded. Can be NULL.
 * content           :
 *
 * Load content file (for libretro core).
 *
 * Returns : true if successful, otherwise false.
 **/
static bool load_content(
      struct string_list *temporary_content,
      struct retro_game_info *info,
      const struct string_list *content,
      const struct retro_subsystem_info *special,
      struct string_list* additional_path_allocs
      )
{
   unsigned i;
   retro_ctx_load_content_info_t load_info;

   if (!info || !additional_path_allocs)
      return false;

   for (i = 0; i < content->size; i++)
   {
      int         attr     = content->elems[i].attr.i;
      bool need_fullpath   = attr & 2;
      bool require_content = attr & 4;
      const char *path     = content->elems[i].data;

      if (require_content && string_is_empty(path))
      {
         RARCH_LOG("libretro core requires content, "
               "but nothing was provided.\n");
         return false;
      }

      info[i].path = NULL;

      if (*path)
         info[i].path = path;

      if (!need_fullpath && !string_is_empty(path))
      {
         if (!load_content_into_memory(&info[i], i, path))
            return false;
      }
      else
      {
         RARCH_LOG("Content loading skipped. Implementation will"
               " load it on its own.\n");

#ifdef HAVE_COMPRESSION
         if (!load_content_from_compressed_archive(
                  temporary_content,
                  &info[i], i,
                  additional_path_allocs, need_fullpath, path))
            return false;
#endif
      }
   }

   load_info.content = content;
   load_info.special = special;
   load_info.info    = info;

   if (!core_ctl(CORE_CTL_RETRO_LOAD_GAME, &load_info))
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
      return false;
   }

#ifdef HAVE_CHEEVOS
   if (!special)
   {
      const void *load_data = NULL;

      cheevos_ctl(CHEEVOS_CTL_SET_CHEATS, NULL);

      if (*content->elems[0].data)
         load_data = info;
      cheevos_ctl(CHEEVOS_CTL_LOAD, (void*)load_data);
   }
#endif

   return true;
}

static const struct retro_subsystem_info *init_content_file_subsystem(
      bool *ret, rarch_system_info_t *system)
{
   global_t *global = global_get_ptr();
   const struct retro_subsystem_info *special = 
      libretro_find_subsystem_info(system->subsystem.data,
         system->subsystem.size, global->subsystem);

   if (!special)
   {
      RARCH_ERR(
            "Failed to find subsystem \"%s\" in libretro implementation.\n",
            global->subsystem);
      goto error;
   }

   if (special->num_roms && !global->subsystem_fullpaths)
   {
      RARCH_ERR("libretro core requires special content, "
            "but none were provided.\n");
      goto error;
   }
   else if (special->num_roms && special->num_roms
         != global->subsystem_fullpaths->size)
   {
      RARCH_ERR("libretro core requires %u content files for "
            "subsystem \"%s\", but %u content files were provided.\n",
            special->num_roms, special->desc,
            (unsigned)global->subsystem_fullpaths->size);
      goto error;
   }
   else if (!special->num_roms && global->subsystem_fullpaths
         && global->subsystem_fullpaths->size)
   {
      RARCH_ERR("libretro core takes no content for subsystem \"%s\", "
            "but %u content files were provided.\n",
            special->desc,
            (unsigned)global->subsystem_fullpaths->size);
      goto error;
   }

   *ret = true;
   return special;

error:
   *ret = false;
   return NULL;
}

#ifdef HAVE_ZLIB
static bool init_content_file_extract(
      struct string_list *temporary_content,
      struct string_list *content,
      rarch_system_info_t *system,
      const struct retro_subsystem_info *special,
      union string_list_elem_attr *attr
      )
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   for (i = 0; i < content->size; i++)
   {
      const char *ext       = NULL;
      const char *valid_ext = system->info.valid_extensions;

      /* Block extract check. */
      if (content->elems[i].attr.i & 1)
         continue;

      ext                   = path_get_extension(content->elems[i].data);

      if (special)
         valid_ext          = special->roms[i].valid_extensions;

      if (!ext)
         continue;

      if (string_is_equal_noncase(ext, "zip"))
      {
         char new_path[PATH_MAX_LENGTH];
         char temp_content[PATH_MAX_LENGTH];

         strlcpy(temp_content, content->elems[i].data,
               sizeof(temp_content));

         if (!file_archive_extract_first_content_file(temp_content,
                  sizeof(temp_content), valid_ext,
                  *settings->cache_directory ?
                  settings->cache_directory : NULL,
                  new_path, sizeof(new_path)))
         {
            RARCH_ERR("Failed to extract content from zipped file: %s.\n",
                  temp_content);
            return false;
         }

         string_list_set(content, i, new_path);
         if (!string_list_append(temporary_content,
                  new_path, *attr))
            return false;
      }
   }
   
   return true;
}
#endif

static bool init_content_file_set_attribs(
      struct string_list *temporary_content,
      struct string_list *content,
      rarch_system_info_t *system,
      const struct retro_subsystem_info *special)
{
   union string_list_elem_attr attr;
   global_t *global        = global_get_ptr();

   attr.i                  = 0;

   if (*global->subsystem)
   {
      unsigned i;

      for (i = 0; i < global->subsystem_fullpaths->size; i++)
      {
         attr.i            = special->roms[i].block_extract;
         attr.i           |= special->roms[i].need_fullpath << 1;
         attr.i           |= special->roms[i].required      << 2;

         string_list_append(content,
               global->subsystem_fullpaths->elems[i].data, attr);
      }
   }
   else
   {
      settings_t *settings = config_get_ptr();

      attr.i               = system->info.block_extract;
      attr.i              |= system->info.need_fullpath << 1;
      attr.i              |= (!content_ctl(CONTENT_CTL_DOES_NOT_NEED_CONTENT, NULL))  << 2;

      if (content_ctl(CONTENT_CTL_DOES_NOT_NEED_CONTENT, NULL)
            && settings->set_supports_no_game_enable)
         string_list_append(content, "", attr);
      else
      {
         char *fullpath    = NULL;
         runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

         string_list_append(content, fullpath, attr);
      }
   }

#ifdef HAVE_ZLIB
   /* Try to extract all content we're going to load if appropriate. */
   if (!init_content_file_extract(temporary_content,
            content, system, special, &attr))
      return false;
#endif
   return true;
}

/**
 * content_init_file:
 *
 * Initializes and loads a content file for the currently
 * selected libretro core.
 *
 * Returns : true if successful, otherwise false.
 **/
static bool content_file_init(struct string_list *temporary_content)
{
   unsigned i;
   struct retro_game_info               *info = NULL;
   bool ret                                   = false;
   struct string_list* additional_path_allocs = NULL;
   struct string_list *content                = NULL;
   const struct retro_subsystem_info *special = NULL;
   rarch_system_info_t *system                = NULL;
   global_t *global                           = global_get_ptr();

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   if (*global->subsystem)
   {
      special = init_content_file_subsystem(&ret, system);
      if (!ret)
         goto error;
   }

   content = string_list_new();

   if (!content)
      goto error;

   if (!init_content_file_set_attribs(temporary_content,
            content, system, special))
      goto error;

   info                   = (struct retro_game_info*)
      calloc(content->size, sizeof(*info));
   additional_path_allocs = string_list_new();

   ret = load_content(temporary_content,
         info, content, special, additional_path_allocs); 

   for (i = 0; i < content->size; i++)
      free((void*)info[i].data);

   string_list_free(additional_path_allocs);
   if (info)
      free(info);

error:
   if (content)
      string_list_free(content);
   return ret;
}

static bool content_file_free(struct string_list *temporary_content)
{
   unsigned i;

   if (!temporary_content)
      return false;

   for (i = 0; i < temporary_content->size; i++)
   {
      const char *path = temporary_content->elems[i].data;

      RARCH_LOG("%s: %s.\n",
            msg_hash_to_str(MSG_REMOVING_TEMPORARY_CONTENT_FILE), path);
      if (remove(path) < 0)
         RARCH_ERR("%s: %s.\n",
               msg_hash_to_str(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE),
               path);
   }
   string_list_free(temporary_content);

   return true;
}

bool content_ctl(enum content_ctl_state state, void *data)
{
   static const struct file_archive_file_backend *stream_backend = NULL;
   static struct string_list *temporary_content                  = NULL;
   static bool content_is_inited                                 = false;
   static bool core_does_not_need_content                        = false;
   static uint32_t content_crc                                   = 0;

   switch(state)
   {
      case CONTENT_CTL_LOAD_RAM_FILE:
         return load_ram_file(data);
      case CONTENT_CTL_SAVE_RAM_FILE:
         return save_ram_file((ram_type_t*)data);
      case CONTENT_CTL_DOES_NOT_NEED_CONTENT:
         return core_does_not_need_content;
      case CONTENT_CTL_SET_DOES_NOT_NEED_CONTENT:
         core_does_not_need_content = true;
         break;
      case CONTENT_CTL_UNSET_DOES_NOT_NEED_CONTENT:
         core_does_not_need_content = false;
         break;
      case CONTENT_CTL_GET_CRC:
         {
            uint32_t **content_crc_ptr = (uint32_t**)data;
            if (!content_crc_ptr)
               return false;
            *content_crc_ptr = &content_crc;
         }
         break;
      case CONTENT_CTL_LOAD_STATE:
         {
            const char *path = (const char*)data;
            if (!path)
               return false;
            return content_load_state(path);
         }
      case CONTENT_CTL_SAVE_STATE:
         {
            const char *path = (const char*)data;
            if (!path)
               return false;
            return content_save_state(path);
         }
      case CONTENT_CTL_IS_INITED:
         return content_is_inited;
      case CONTENT_CTL_DEINIT:
         content_ctl(CONTENT_CTL_TEMPORARY_FREE, NULL);
         content_crc                = 0;
         content_is_inited          = false;
         core_does_not_need_content = false;
         break;
      case CONTENT_CTL_INIT:
         content_is_inited = false;
         temporary_content = string_list_new();
         if (!temporary_content)
            return false;
         if (content_file_init(temporary_content))
         {
            content_is_inited = true;
            return true;
         }
         content_ctl(CONTENT_CTL_DEINIT, NULL);
         return false;
      case CONTENT_CTL_TEMPORARY_FREE:
         content_file_free(temporary_content);
         temporary_content = NULL;
         break;
      case CONTENT_CTL_STREAM_INIT:
#ifdef HAVE_ZLIB
         if (!stream_backend)
            stream_backend = file_archive_get_default_file_backend();
#endif
         break;
      case CONTENT_CTL_STREAM_CRC_CALCULATE:
         {
            content_stream_t *stream = NULL;
            content_ctl(CONTENT_CTL_STREAM_INIT, NULL);

            stream = (content_stream_t*)data;
#ifdef HAVE_ZLIB
            stream->crc = stream_backend->stream_crc_calculate(
                  stream->a, stream->b, stream->c);
#endif
         }
         break;
      case CONTENT_CTL_LOAD:
         return content_load((content_ctx_info_t*)data);
      case CONTENT_CTL_NONE:
      default:
         break;
   }

   return true;
}
