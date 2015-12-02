/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <string/string_list.h>
#include <file/file_path.h>
#include <file/file_extract.h>

#include "tasks.h"
#include "../verbosity.h"
#include "../msg_hash.h"

typedef struct {
   char *source_file;
   char *target_dir;
   char *valid_ext;

   char *callback_error;

   zlib_transfer_t zlib;
} decompress_state_t;

static int file_decompressed(const char *name, const char *valid_exts,
   const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
   uint32_t crc32, void *userdata)
{
   char path[PATH_MAX_LENGTH];
   decompress_state_t *dec = (decompress_state_t*)userdata;

   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      goto next_file;

   /* Make directory */
   fill_pathname_join(path, dec->target_dir, name, sizeof(path));
   path_basedir(path);

   if (!path_mkdir(path))
      goto error;

   fill_pathname_join(path, dec->target_dir, name, sizeof(path));

   if (!zlib_perform_mode(path, valid_exts,
            cdata, cmode, csize, size, crc32, userdata))
      goto error;

   RARCH_LOG("[deflate] Path: %s, CRC32: 0x%x\n", name, crc32);

next_file:
   return 1;

error:
   dec->callback_error = (char*)malloc(PATH_MAX_LENGTH);
   snprintf(dec->callback_error, PATH_MAX_LENGTH, "Failed to deflate %s.\n", path);

   return 0;
}

static void rarch_task_decompress_handler(rarch_task_t *task)
{
   decompress_state_t *dec = (decompress_state_t*)task->state;
   decompress_task_data_t *data = NULL;
   bool returnerr;
   int ret = 0;

   ret = zlib_parse_file_iterate(&dec->zlib, &returnerr, dec->source_file,
         dec->valid_ext, file_decompressed, dec);

   task->progress = zlib_parse_file_progress(&dec->zlib);

   if (task->cancelled || ret != 0)
   {
      task->error = dec->callback_error;
      zlib_parse_file_iterate_stop(&dec->zlib);
      goto task_finished;
   }

   return;

task_finished:
   task->finished = true;

   if (!task->error && task->cancelled)
      task->error = strdup("Task canceled");

   if (!task->error)
   {
      data = (decompress_task_data_t*)calloc(1, sizeof(*data));
      data->source_file = dec->source_file;
      task->task_data = data;
   }
   else
      free(dec->source_file);

   if (dec->valid_ext)
      free(dec->valid_ext);
   free(dec->target_dir);
   free(dec);
}

static bool rarch_task_decompress_finder(rarch_task_t *task, void *user_data)
{
   decompress_state_t *dec = (decompress_state_t*)task->state;

   if (task->handler != rarch_task_decompress_handler)
      return false;

   return strcmp(dec->source_file, (const char*)user_data) == 0;
}

bool rarch_task_push_decompress(const char *source_file, const char *target_dir,
      const char *valid_ext, rarch_task_callback_t cb, void *user_data)
{
   decompress_state_t *s;
   rarch_task_t *t;
   char tmp[PATH_MAX_LENGTH];
   bool is_compressed = false;

   if (!target_dir || !target_dir[0] || !source_file || !source_file[0])
   {
      RARCH_WARN("[decompress] Empty or null source file or target directory arguments.\n");
      return false;
   }

   /* ZIP or APK only */
   is_compressed = !strcmp("zip", path_get_extension(source_file));
   is_compressed = !is_compressed ? !strcmp("apk", path_get_extension(source_file)) : is_compressed;

   if (!path_file_exists(source_file) || !is_compressed)
   {
      RARCH_WARN("[decompress] File '%s' does not exist or is not a compressed file.\n",
            source_file);
      return false;
   }

   if (!valid_ext || !valid_ext[0])
      valid_ext = NULL;

   if (rarch_task_find(rarch_task_decompress_finder, (void*)source_file))
   {
      RARCH_LOG("[decompress] File '%s' already being decompressed.\n", source_file);
      return false;
   }

   s = (decompress_state_t*)calloc(1, sizeof(*s));

   s->source_file = strdup(source_file);
   s->target_dir  = strdup(target_dir);

   s->valid_ext = valid_ext ? strdup(valid_ext) : NULL;
   s->zlib.type = ZLIB_TRANSFER_INIT;

   t = (rarch_task_t*)calloc(1, sizeof(*t));
   t->handler   = rarch_task_decompress_handler;
   t->state     = s;

   t->callback  = cb;
   t->user_data = user_data;

   snprintf(tmp, sizeof(tmp), "%s '%s'", msg_hash_to_str(MSG_EXTRACTING), path_basename(source_file));
   t->title = strdup(tmp);

   rarch_task_push(t);

   return true;
}
