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

#include <file/file_path.h>
#include <file/archive_file.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#include "tasks_internal.h"
#include "../file_path_special.h"
#include "../msg_hash.h"

#define CALLBACK_ERROR_SIZE 4200

static int file_decompressed_target_file(const char *name,
      const char *valid_exts, const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   return 0;
}

/* path_mkdir() for an extraction target, remembering the last
 * directory it confirmed so a run of members in the same directory
 * costs one probe instead of one per member. */
static bool task_decompress_mkdir(decompress_state_t *dec, const char *dir)
{
   if (string_is_equal(dec->last_dir, dir))
      return true;
   if (!path_mkdir(dir))
      return false;
   strlcpy(dec->last_dir, dir, sizeof(dec->last_dir));
   return true;
}

/* Rejects archive member names that could escape the intended
 * extraction directory ("Zip Slip"). Returns true if the name is
 * safe to concatenate onto a target-directory prefix. */
static bool archive_name_is_safe(const char *name)
{
   const char *p;
   const char *seg_start;

   if (!name || !*name)
      return false;

   /* Reject absolute paths in either Unix or Windows form. */
   if (name[0] == '/' || name[0] == '\\')
      return false;
   /* Reject drive-letter prefix e.g. "C:..." */
   if (name[1] == ':')
      return false;

   /* Reject any ".." path segment.  Treat both '/' and '\\' as
    * separators so Windows-authored archives can't traverse when
    * extracted on a POSIX host. */
   p = seg_start = name;
   for (;;)
   {
      char c = *p;
      if (c == '/' || c == '\\' || c == '\0')
      {
         if ((size_t)(p - seg_start) == 2
               && seg_start[0] == '.' && seg_start[1] == '.')
            return false;
         if (c == '\0')
            break;
         seg_start = p + 1;
      }
      p++;
   }
   return true;
}

static int file_decompressed_subdir(const char *name,
      const char *valid_exts,
      const uint8_t *cdata,
      unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   char path_dir[DIR_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   size_t _len;
   /* Reject empty names (strlen-1 OOB) and traversal attempts. */
   if (!archive_name_is_safe(name))
      return 1;
   _len = strlen(name);

   /* Look at last character. Ignore directories, go to next file. */
   if (name[_len - 1] == '/' || name[_len - 1] == '\\')
      return 1;

   /* Loop multiple subdirs */
   if (strchr(userdata->dec->subdir, '|'))
   {
      const char *subdir = userdata->dec->subdir;
      bool found         = false;

      while (*subdir)
      {
         const char *end = strchr(subdir, '|');
         size_t len      = end ? (size_t)(end - subdir) : strlen(subdir);

         if (strncmp(name, subdir, len) == 0
               && (name[len] == '/' || name[len] == '\\' || name[len] == '\0'))
         {
            found = true;
            break;
         }

         if (end)
            subdir = end + 1;
         else
            break;
      }

      if (!found)
         return 1;
   }
   else if (strstr(name, userdata->dec->subdir) != name)
      return 1;

   /* Remove desired subdir from the file path
    * if not extracting multiple subdirs */
   if (!strchr(userdata->dec->subdir, '|'))
      name += strlen(userdata->dec->subdir) + 1;

   fill_pathname_join_special(path,
         userdata->dec->target_dir, name, sizeof(path));
   fill_pathname_basedir(path_dir, path, sizeof(path_dir));

   /* Make directory */
   if (task_decompress_mkdir(userdata->dec, path_dir))
   {
      /* Start the decode rather than driving it to completion here.
       * Whatever is left is parked in the transfer and finished by
       * file_archive_parse_file_iterate() on later ticks, one slice
       * per tick, so a large member no longer holds the frame. */
      if (file_archive_perform_mode_start(path, valid_exts,
               cdata, cmode, csize, size, crc32, userdata) != -1)
         return 1;
   }

   userdata->dec->callback_error = (char*)malloc(CALLBACK_ERROR_SIZE);
   /* NULL-check: the strlcpy below NULL-derefs on OOM.  The
    * downstream task_set_error calls (lines 246, 274, 304) accept
    * NULL, so skipping the error-string population leaves the task
    * with a NULL error and the user sees no 'Failed to deflate'
    * message - which is strictly better than segfaulting.  Any
    * subsequent 'if (dec->callback_error)' downstream is already
    * NULL-safe. */
   if (userdata->dec->callback_error)
   {
      _len  = strlcpy_lit(userdata->dec->callback_error,
            "Failed to deflate ",
            CALLBACK_ERROR_SIZE);
      _len += strlcpy(
            userdata->dec->callback_error + _len,
            path,
            CALLBACK_ERROR_SIZE - _len);
      userdata->dec->callback_error[  _len] = '.';
      userdata->dec->callback_error[++_len] = '\n';
      userdata->dec->callback_error[++_len] = '\0';
   }

   return 0;
}

static int file_decompressed(const char *name, const char *valid_exts,
   const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
   uint32_t crc32, struct archive_extract_userdata *userdata)
{
   char path[PATH_MAX_LENGTH];
   decompress_state_t *dec = userdata->dec;
   size_t _len;
   /* Reject empty names (strlen-1 OOB) and traversal attempts. */
   if (!archive_name_is_safe(name))
      return 1;
   _len = strlen(name);
   /* Look at last character. Ignore directories, go to next file. */
   if (name[_len - 1] == '/' || name[_len - 1] == '\\')
      return 1;
   /* Make directory */
   fill_pathname_join_special(path, dec->target_dir, name, sizeof(path));
   path_basedir_wrapper(path);

   if (task_decompress_mkdir(dec, path))
   {
      fill_pathname_join_special(path, dec->target_dir, name, sizeof(path));

      /* Started, not completed; see the twin in file_archived. */
      if (file_archive_perform_mode_start(path, valid_exts,
               cdata, cmode, csize, size, crc32, userdata) != -1)
         return 1;
   }

   dec->callback_error = (char*)malloc(CALLBACK_ERROR_SIZE);
   /* NULL-check: see twin in file_archived for reasoning. */
   if (dec->callback_error)
   {
      _len  = strlcpy_lit(dec->callback_error, "Failed to deflate ",
              CALLBACK_ERROR_SIZE);
      _len += strlcpy(dec->callback_error + _len,
              path, CALLBACK_ERROR_SIZE     - _len);
      dec->callback_error[  _len] = '.';
      dec->callback_error[++_len] = '\n';
      dec->callback_error[++_len] = '\0';
   }

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

   if (!task_get_error(task))
   {
      decompress_task_data_t *data =
         (decompress_task_data_t*)calloc(1, sizeof(*data));

      /* The completion callback owns 'data' and frees both it and
       * data->source_file, so it is handed a copy of the path
       * rather than the state's own buffer: dec->source_file must
       * stay live until the task is fully retired (see the note
       * below).  On allocation failure a task error is set instead,
       * so downstream consumers see a clean error state (a NULL
       * task_data, which they all check) rather than partial
       * success. */
      if (data)
         data->source_file = strdup(dec->source_file);
      if (data && data->source_file)
         task_set_data(task, data);
      else
      {
         free(data);
         task_set_error(task, strdup("Out of memory"));
      }
   }

   /* Deliberately NOT freeing 'dec' (task->state) here.
    *
    * This runs on the worker thread, inside the task handler, with
    * no queue lock held: the task is still sitting in the running
    * queue at this point, and after the handler returns it stays
    * reachable through the finished and retiring lists until the
    * main thread fully retires it - many frames later.
    * task_decompress_finder() dereferences task->state->source_file
    * on every candidate whose handler matches, so freeing the state
    * here left it dangling for that entire window, and any
    * task_push_decompress() racing a finishing extraction - e.g.
    * cb_generic_download pushing the next archive from within
    * task_queue gather while the previous one retires - called
    * strcmp() on freed memory: a hard SIGSEGV.  (Same bug and same
    * fix as task_http.c's finder; see the note at the end of
    * task_http_transfer_handler().)
    *
    * Teardown now happens in task_decompress_cleanup(), which the
    * task queue calls during retirement, after the task has been
    * pruned from every list and is no longer findable. */
}

static void task_decompress_cleanup(retro_task_t *task)
{
   decompress_state_t *dec = (decompress_state_t*)task->state;

   if (!dec)
      return;
   /* Cleared first so that a finder which somehow still reaches
    * this task sees no state rather than a stale pointer. */
   task->state = NULL;

   free(dec->source_file);
   free(dec->target_dir);
   free(dec->target_file);
   free(dec->subdir);
   free(dec->valid_ext);
   free(dec->callback_error);
   free(dec->userdata);
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
      /* Ownership of the buffer moved to task->error (freed at
       * retirement); cleared so task_decompress_cleanup() does
       * not free it a second time. */
      dec->callback_error = NULL;
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
      /* Ownership of the buffer moved to task->error (freed at
       * retirement); cleared so task_decompress_cleanup() does
       * not free it a second time. */
      dec->callback_error = NULL;
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
         &dec->archive, &retdec, dec->source_file,
         dec->valid_ext, file_decompressed_subdir, dec->userdata);

   task_set_progress(task,
         file_archive_parse_file_progress(&dec->archive));

   flg = task_get_flags(task);

   if (((flg & RETRO_TASK_FLG_CANCELLED) > 0) || ret != 0)
   {
      task_set_error(task, dec->callback_error);
      /* Ownership of the buffer moved to task->error (freed at
       * retirement); cleared so task_decompress_cleanup() does
       * not free it a second time. */
      dec->callback_error = NULL;
      file_archive_parse_file_iterate_stop(&dec->archive);

      task_decompress_handler_finished(task, dec);
   }
}

static bool task_decompress_finder(
      retro_task_t *task, void *user_data)
{
   decompress_state_t *dec = (decompress_state_t*)task->state;
   /* Every handler variant reads its source archive and keeps a
    * decompress_state_t in task->state, so all of them count as
    * 'this archive is being extracted' for dedup and for callers
    * deciding whether the file is safe to overwrite. */
   if (   task->handler != task_decompress_handler
       && task->handler != task_decompress_handler_subdir
       && task->handler != task_decompress_handler_target_file)
      return false;
   /* The state stays attached until the queue retires the task
    * (task_decompress_cleanup), so a finished-but-not-yet-retired
    * extraction still matches here.  That is deliberate: find()
    * must stay truthful for the whole window or a duplicate
    * extraction could be pushed against the same destination.  The
    * NULL checks are belt-and-braces for a task whose cleanup has
    * already run. */
   if (!dec || !dec->source_file)
      return false;
   return !strcmp(dec->source_file, (const char*)user_data);
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
   if (!target_dir || !*target_dir || !source_file || !*source_file)
      return NULL;
   ext = path_get_extension(source_file);
   /* ZIP or APK only */
   if (
         !path_is_valid(source_file) ||
         (
             strcasecmp(ext, "zip") != 0
          && strcasecmp(ext, "apk") != 0
#ifdef HAVE_7ZIP
          && strcasecmp(ext, "7z")  != 0
#endif
#if defined(HAVE_ZSTD) || defined(HAVE_RZSTD)
          && strcasecmp(ext, "zst") != 0
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
   if (!s->source_file || !s->target_dir)
      goto error;
   s->valid_ext        = valid_ext ? strdup(valid_ext) : NULL;
   s->archive.type     = ARCHIVE_TRANSFER_INIT;
   s->userdata         = (struct archive_extract_userdata*)
      calloc(1, sizeof(*s->userdata));
   if (!s->userdata)
      goto error;
   t->frontend_userdata= frontend_userdata;
   t->state            = s;
   t->handler          = task_decompress_handler;
   /* Releases t->state; must run at retirement, not at FINISHED
    * time - see the note in task_decompress_handler_finished(). */
   t->cleanup          = task_decompress_cleanup;
   if (subdir && *subdir)
   {
      s->subdir        = strdup(subdir);
      if (!s->subdir)
         goto error;
      t->handler       = task_decompress_handler_subdir;
   }
   else if (target_file && *target_file)
   {
      s->target_file   = strdup(target_file);
      if (!s->target_file)
         goto error;
      t->handler       = task_decompress_handler_target_file;
   }
   t->callback         = cb;
   t->user_data        = user_data;
   _len                = strlcpy(tmp,
		   msg_hash_to_str(MSG_EXTRACTING), sizeof(tmp));
   tmp[  _len]         = ':';
   tmp[++_len]         = ' ';
   tmp[++_len]         = '\0';
   strlcpy(tmp + _len,
         path_basename(source_file),
                         sizeof(tmp) - _len);
   t->title            = strdup(tmp);
   if (!t->title)
      goto error;
   t->flags           |=  RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   if (mute)
      t->flags        |=  RETRO_TASK_FLG_MUTE;
   else
      t->flags        &= ~RETRO_TASK_FLG_MUTE;
   task_queue_push(t);
   return t;
error:
   free(s->source_file);
   free(s->target_dir);
   free(s->valid_ext);
   free(s->subdir);
   free(s->target_file);
   free(s->userdata);
   free(s);
   free(t);
   return NULL;
}
