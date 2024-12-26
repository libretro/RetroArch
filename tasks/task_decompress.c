/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <string/stdstring.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>

#include "tasks_internal.h"
#include "../file_path_special.h"
#include "../msg_hash.h"

#define CALLBACK_ERROR_SIZE 4200

static int file_decompressed_target_file(const char *name,
      const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   /* TODO/FIXME */
   return 0;
}

static int file_decompressed_subdir(const char *name,
      const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize,uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   size_t _len;
   char path_dir[DIR_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   size_t name_len            = strlen(name);
   char last_char             = name[name_len - 1];
   /* Ignore directories, go to next file. */
   if (last_char == '/' || last_char == '\\')
      return 1;
   if (strstr(name, userdata->dec->subdir) != name)
      return 1;

   name += strlen(userdata->dec->subdir) + 1;

   fill_pathname_join_special(path,
         userdata->dec->target_dir, name, sizeof(path));
   fill_pathname_basedir(path_dir, path, sizeof(path_dir));

   /* Make directory */
   if (!path_mkdir(path_dir))
      goto error;

   if (!file_archive_perform_mode(path, valid_exts,
            cdata, cmode, csize, size, crc32, userdata))
      goto error;

   return 1;

error:
   userdata->dec->callback_error = (char*)malloc(CALLBACK_ERROR_SIZE);
   _len  = strlcpy(userdata->dec->callback_error,
         "Failed to deflate ",
         CALLBACK_ERROR_SIZE);
   _len += strlcpy(
		   userdata->dec->callback_error + _len,
		   path,
         CALLBACK_ERROR_SIZE - _len);
   userdata->dec->callback_error[  _len] = '.';
   userdata->dec->callback_error[++_len] = '\n';
   userdata->dec->callback_error[++_len] = '\0';

   return 0;
}

static int file_decompressed(const char *name, const char *valid_exts,
   const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
   uint32_t crc32, struct archive_extract_userdata *userdata)
{
   size_t _len;
   char path[PATH_MAX_LENGTH];
   decompress_state_t    *dec = userdata->dec;
   size_t name_len            = strlen(name);
   char last_char             = name[name_len - 1];
   /* Ignore directories, go to next file. */
   if (last_char == '/' || last_char == '\\')
      return 1;
   /* Make directory */
   fill_pathname_join_special(path, dec->target_dir, name, sizeof(path));
   path_basedir_wrapper(path);

   if (!path_mkdir(path))
      goto error;

   fill_pathname_join_special(path, dec->target_dir, name, sizeof(path));

   if (!file_archive_perform_mode(path, valid_exts,
            cdata, cmode, csize, size, crc32, userdata))
      goto error;

   return 1;

error:
   dec->callback_error = (char*)malloc(CALLBACK_ERROR_SIZE);
   _len  = strlcpy(dec->callback_error, "Failed to deflate ",
		   CALLBACK_ERROR_SIZE);
   _len += strlcpy(dec->callback_error + _len,
		   path, CALLBACK_ERROR_SIZE     - _len);
   dec->callback_error[  _len] = '.';
   dec->callback_error[++_len] = '\n';
   dec->callback_error[++_len] = '\0';

   return 0;
}

static void task_decompress_handler_finished(retro_task_t *task,
      decompress_state_t *dec)
{
   uint8_t flg;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   flg = task_get_flags(task);

   if (!task_get_error(task) && ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
      task_set_error(task, strdup("Task canceled"));

   if (task_get_error(task))
      free(dec->source_file);
   else
   {
      decompress_task_data_t *data =
         (decompress_task_data_t*)calloc(1, sizeof(*data));

      data->source_file = dec->source_file;
      task_set_data(task, data);
   }

   if (dec->subdir)
      free(dec->subdir);
   if (dec->valid_ext)
      free(dec->valid_ext);
   if (dec->userdata)
      free(dec->userdata);
   free(dec->target_dir);
   free(dec);
}

static void task_decompress_handler(retro_task_t *task)
{
   int ret;
   uint8_t flg;
   bool retdec                   = false;
   decompress_state_t *dec       = (decompress_state_t*)task->state;

   dec->userdata->dec            = dec;
   strlcpy(dec->userdata->archive_path,
         dec->source_file, sizeof(dec->userdata->archive_path));

   ret                     = file_archive_parse_file_iterate(
         &dec->archive,
         &retdec, dec->source_file,
         dec->valid_ext, file_decompressed, dec->userdata);

   task_set_progress(task,
         file_archive_parse_file_progress(&dec->archive));

   flg = task_get_flags(task);

   if (((flg & RETRO_TASK_FLG_CANCELLED) > 0) || ret != 0)
   {
      task_set_error(task, dec->callback_error);
      file_archive_parse_file_iterate_stop(&dec->archive);

      task_decompress_handler_finished(task, dec);
   }
}

static void task_decompress_handler_target_file(retro_task_t *task)
{
   int ret;
   uint8_t flg;
   bool retdec;
   decompress_state_t *dec    = (decompress_state_t*)task->state;

   strlcpy(dec->userdata->archive_path,
         dec->source_file, sizeof(dec->userdata->archive_path));

   ret = file_archive_parse_file_iterate(&dec->archive,
         &retdec, dec->source_file,
         dec->valid_ext, file_decompressed_target_file, dec->userdata);

   task_set_progress(task,
         file_archive_parse_file_progress(&dec->archive));

   flg = task_get_flags(task);

   if (((flg & RETRO_TASK_FLG_CANCELLED) > 0) || ret != 0)
   {
      task_set_error(task, dec->callback_error);
      file_archive_parse_file_iterate_stop(&dec->archive);

      task_decompress_handler_finished(task, dec);
   }
}

static void task_decompress_handler_subdir(retro_task_t *task)
{
   int ret;
   uint8_t flg;
   bool retdec;
   decompress_state_t *dec = (decompress_state_t*)task->state;

   dec->userdata->dec      = dec;
   strlcpy(dec->userdata->archive_path,
         dec->source_file,
         sizeof(dec->userdata->archive_path));

   ret                     = file_archive_parse_file_iterate(
         &dec->archive,
         &retdec, dec->source_file,
         dec->valid_ext, file_decompressed_subdir, dec->userdata);

   task_set_progress(task,
         file_archive_parse_file_progress(&dec->archive));

   flg = task_get_flags(task);

   if (((flg & RETRO_TASK_FLG_CANCELLED) > 0) || ret != 0)
   {
      task_set_error(task, dec->callback_error);
      file_archive_parse_file_iterate_stop(&dec->archive);

      task_decompress_handler_finished(task, dec);
   }
}

static bool task_decompress_finder(
      retro_task_t *task, void *user_data)
{
   decompress_state_t *dec = (decompress_state_t*)task->state;

   if (task->handler != task_decompress_handler)
      return false;

   return string_is_equal(dec->source_file, (const char*)user_data);
}

bool task_check_decompress(const char *source_file)
{
   task_finder_data_t find_data;

   /* Prepare find parameters */
   find_data.func     = task_decompress_finder;
   find_data.userdata = (void *)source_file;

   /* Return whether decompressing is in progress or not */
   return task_queue_find(&find_data);
}

void *task_push_decompress(
      const char *source_file,
      const char *target_dir,
      const char *target_file,
      const char *subdir,
      const char *valid_ext,
      retro_task_callback_t cb,
      void *user_data,
      void *frontend_userdata,
      bool mute)
{
   size_t _len;
   char tmp[PATH_MAX_LENGTH];
   const char *ext            = NULL;
   decompress_state_t *s      = NULL;
   retro_task_t *t            = NULL;

   tmp[0] = '\0';

   if (string_is_empty(target_dir) || string_is_empty(source_file))
      return NULL;

   ext = path_get_extension(source_file);

   /* ZIP or APK only */
   if (
         !path_is_valid(source_file) ||
         (
             !string_is_equal_noncase(ext, "zip")
          && !string_is_equal_noncase(ext, "apk")
#ifdef HAVE_7ZIP
          && !string_is_equal_noncase(ext, "7z")
#endif
         )
      )
      return NULL;

   if (!valid_ext || !valid_ext[0])
      valid_ext   = NULL;

   if (task_check_decompress(source_file))
      return NULL;

   if (!(s = (decompress_state_t*)calloc(1, sizeof(*s))))
      return NULL;

   if (!(t = task_init()))
   {
      free(s);
      return NULL;
   }

   s->source_file      = strdup(source_file);
   s->target_dir       = strdup(target_dir);

   s->valid_ext        = valid_ext ? strdup(valid_ext) : NULL;
   s->archive.type     = ARCHIVE_TRANSFER_INIT;
   s->userdata         = (struct archive_extract_userdata*)
      calloc(1, sizeof(*s->userdata));

   t->frontend_userdata= frontend_userdata;

   t->state            = s;
   t->handler          = task_decompress_handler;

   if (!string_is_empty(subdir))
   {
      s->subdir        = strdup(subdir);
      t->handler       = task_decompress_handler_subdir;
   }
   else if (!string_is_empty(target_file))
   {
      s->target_file   = strdup(target_file);
      t->handler       = task_decompress_handler_target_file;
   }

   t->callback         = cb;
   t->user_data        = user_data;

   _len                = strlcpy(tmp,
		   msg_hash_to_str(MSG_EXTRACTING), sizeof(tmp));
   tmp[  _len]         = ' ';
   tmp[++_len]         = '\'';
   tmp[++_len]         = '\0';
   _len               += strlcpy(tmp + _len,
         path_basename(source_file),
                         sizeof(tmp) - _len);
   tmp[_len  ]         = '\'';
   tmp[++_len]         = '\0';

   t->title            = strdup(tmp);
   if (mute)
      t->flags        |=  RETRO_TASK_FLG_MUTE;
   else
      t->flags        &= ~RETRO_TASK_FLG_MUTE;

   task_queue_push(t);

   return t;
}
