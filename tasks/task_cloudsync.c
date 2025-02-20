/*  RetroArch - A frontend for libretro.
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

#include <features/features_cpu.h>
#include <file/file_path.h>
#include <formats/rjson.h>
#include <lists/dir_list.h>
#include <lists/file_list.h>
#include <lrc_hash.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <time/rtime.h>
#include <retro_inline.h>

#include "../configuration.h"
#include "../file_path_special.h"
#include "../network/cloud_sync_driver.h"
#include "../paths.h"
#include "../tasks/tasks_internal.h"
#include "../verbosity.h"

#define CSPFX "[CloudSync] "

#define MANIFEST_FILENAME_LOCAL  "manifest.local"
#define MANIFEST_FILENAME_SERVER "manifest.server"

#define CS_FILE_HASH(item_file) ((char*)((item_file) ? ((item_file)->userdata) : (NULL)))
#define CS_FILE_KEY(item_file) ((item_file) ? ((item_file)->alt) : (NULL))
#define CS_FILE_DELETED(item_file) (string_is_empty(CS_FILE_HASH(item_file)))

enum task_cloud_sync_phase
{
   CLOUD_SYNC_PHASE_BEGIN,
   CLOUD_SYNC_PHASE_FETCH_SERVER_MANIFEST,
   CLOUD_SYNC_PHASE_READ_LOCAL_MANIFEST,
   CLOUD_SYNC_PHASE_BUILD_CURRENT_MANIFEST,
   CLOUD_SYNC_PHASE_DIFF,
   CLOUD_SYNC_PHASE_UPDATE_MANIFESTS,
   CLOUD_SYNC_PHASE_END
};

typedef struct
{
   enum task_cloud_sync_phase phase;
   uint32_t waiting;
   /* Manifest present on the server (may be modified by other clients)*/
   file_list_t *server_manifest;
   size_t server_idx;
   /* Last-known state to compare agasint.*/
   file_list_t *local_manifest;
   size_t local_idx;
   file_list_t *current_manifest;
   size_t current_idx;
   file_list_t *updated_server_manifest;
   /* local manifest is sometimes different due to conflicts */
   file_list_t *updated_local_manifest;
   bool need_manifest_uploaded;
   bool failures;
   bool conflicts;
   uint32_t uploads;
   uint32_t downloads;
   retro_time_t start_time;
} task_cloud_sync_state_t;

static slock_t *tcs_running_lock = NULL;

static void task_cloud_sync_begin_handler(void *user_data, const char *path, bool success, RFILE *file)
{
   retro_task_t            *task       = (retro_task_t *)user_data;
   task_cloud_sync_state_t *sync_state = NULL;

   if (!task)
      return;

   if (!(sync_state = (task_cloud_sync_state_t *)task->state))
      return;

   if (success)
   {
      RARCH_LOG(CSPFX "begin succeeded\n");
      sync_state->phase = CLOUD_SYNC_PHASE_FETCH_SERVER_MANIFEST;
   }
   else
   {
      RARCH_WARN(CSPFX "begin failed\n");
      task_set_title(task, strdup("Cloud Sync failed"));
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }
   slock_lock(tcs_running_lock);
   sync_state->waiting = 0;
   slock_unlock(tcs_running_lock);
}

static bool tcs_object_member_handler(void *ctx, const char *s, size_t len)
{
   file_list_t      *list = (file_list_t *)ctx;
   struct item_file *item = &list->list[list->size - 1];
   if (string_is_equal(s, "path"))
      item->type = 1;
   else
      item->type = 0;
   return true;
}

static bool tcs_string_handler(void *ctx, const char *s, size_t len)
{
   file_list_t      *list = (file_list_t *)ctx;
   size_t            idx = list->size - 1;
   struct item_file *item = &list->list[idx];
   if (item->type)
      file_list_set_alt_at_offset(list, idx, s);
   else
      list->list[idx].userdata = strdup(s);
   return true;
}

static bool tcs_start_object_handler(void *ctx)
{
   file_list_t *list = (file_list_t *)ctx;
   file_list_append(list, NULL, NULL, 0, 0, 0);
   return true;
}

static bool tcs_end_object_handler(void *ctx)
{
   file_list_t      *list = (file_list_t *)ctx;
   struct item_file *item = &list->list[list->size - 1];
   if (!CS_FILE_KEY(item))
      list->size--;
   else
      item->type = 0;
   return true;
}

static file_list_t *task_cloud_sync_create_manifest(RFILE *file)
{
   file_list_t  *list = NULL;
   rjson_t      *json = NULL;

   if (!(list = (file_list_t *)calloc(1, sizeof(file_list_t))))
      return NULL;

   if (!(json = rjson_open_rfile(file)))
      return NULL;

   rjson_parse(json, list,
               tcs_object_member_handler,
               tcs_string_handler,
               NULL,
               tcs_start_object_handler,
               tcs_end_object_handler,
               NULL,
               NULL,
               NULL,
               NULL);

   rjson_free(json);

   file_list_sort_on_alt(list);

   RARCH_LOG(CSPFX "created manifest with %u files\n", list->size);

   return list;
}

static void task_cloud_sync_manifest_filename(char *s, size_t len, bool server)
{
   const char *path_dir_core_assets = config_get_ptr()->paths.directory_core_assets;
   fill_pathname_join_special(s, path_dir_core_assets,
         server ? MANIFEST_FILENAME_SERVER : MANIFEST_FILENAME_LOCAL,
         len);
}

static void task_cloud_sync_manifest_handler(void *user_data, const char *path,
      bool success, RFILE *file)
{
   task_cloud_sync_state_t *sync_state = (task_cloud_sync_state_t *)user_data;

   if (!sync_state)
      return;

   if (!success)
   {
      RARCH_WARN(CSPFX "server manifest fetch failed\n");
      sync_state->failures = true;
      sync_state->phase    = CLOUD_SYNC_PHASE_END;
      slock_lock(tcs_running_lock);
      sync_state->waiting = 0;
      slock_unlock(tcs_running_lock);
      return;
   }

   RARCH_LOG(CSPFX "server manifest fetch succeeded\n");
   /* it is valid for there not to be a server manifest */
   if (file)
   {
      sync_state->server_manifest = task_cloud_sync_create_manifest(file);
      filestream_close(file);
   }
   sync_state->phase = CLOUD_SYNC_PHASE_READ_LOCAL_MANIFEST;
   slock_lock(tcs_running_lock);
   sync_state->waiting = 0;
   slock_unlock(tcs_running_lock);
}

static void task_cloud_sync_fetch_server_manifest(task_cloud_sync_state_t *sync_state)
{
   char        manifest_path[PATH_MAX_LENGTH];

   task_cloud_sync_manifest_filename(manifest_path, sizeof(manifest_path), true);

   sync_state->waiting = 1;
   if (!cloud_sync_read(MANIFEST_FILENAME_SERVER, manifest_path, task_cloud_sync_manifest_handler, sync_state))
   {
      RARCH_WARN(CSPFX "could not read server manifest\n");
      sync_state->waiting = 0;
      sync_state->phase = CLOUD_SYNC_PHASE_END;
   }
}

static void task_cloud_sync_read_local_manifest(task_cloud_sync_state_t *sync_state)
{
   char manifest_path[PATH_MAX_LENGTH];

   task_cloud_sync_manifest_filename(manifest_path, sizeof(manifest_path), false);

   /* it is valid for there not to be a local manifest, if we have never done a sync before */
   if (path_is_valid(manifest_path))
   {
      RFILE *rfile = filestream_open(manifest_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (rfile)
      {
         RARCH_WARN(CSPFX "opened local manifest\n");
         sync_state->local_manifest = task_cloud_sync_create_manifest(rfile);
         filestream_close(rfile);
      }
   }

   sync_state->phase = CLOUD_SYNC_PHASE_BUILD_CURRENT_MANIFEST;
}

/* takes the filename in manifest format, e.g. "config/retroarch.cfg" */
static bool task_cloud_sync_should_ignore_file(const char *filename)
{
   if (string_starts_with(filename, "config/"))
   {
      const char *path = filename + STRLEN_CONST("config/");

      /* need to exclude FILE_PATH_MAIN_CONFIG, those don't get sync'd */
      if (string_is_equal(path, FILE_PATH_MAIN_CONFIG))
         return true;

      /* ignore playlist files */
      if (string_starts_with(path, "content_") && string_ends_with(path, FILE_PATH_LPL_EXTENSION))
         return true;
   }

   if (string_ends_with(filename, "/.DS_Store"))
       return true;

   return false;
}

/**
 * task_cloud_sync_manifest_append_dir:
 * @manifest         : pointer to the current file_list
 * @dir_fullpath     : the full path to the directory to be added
 * @dir_name         : the name of the directory to be added
 *
 * Adds all the files within the given directory to the provided
 * file list, with the exception of the ones that should be ignored
 */
static void task_cloud_sync_manifest_append_dir(file_list_t *manifest,
      const char *dir_fullpath, char *dir_name)
{
   size_t i;
   struct string_list *dir_list;
   char                dir_fullpath_slash[PATH_MAX_LENGTH];

   strlcpy(dir_fullpath_slash, dir_fullpath, sizeof(dir_fullpath_slash));
   fill_pathname_slash(dir_fullpath_slash, sizeof(dir_fullpath_slash));

   dir_list = dir_list_new(dir_fullpath_slash, NULL, false, true, true, true);

   if (dir_list->size == 0)
   {
	   string_list_free(dir_list);
	   return;
   }

   file_list_reserve(manifest, manifest->size + dir_list->size);
   for (i = 0; i < dir_list->size; i++)
   {
      size_t      idx = manifest->size;
      const char *full_path = dir_list->elems[i].data;
      char        relative_path[PATH_MAX_LENGTH];
      char        alt[PATH_MAX_LENGTH];

      path_relative_to(relative_path, full_path, dir_fullpath_slash, sizeof(relative_path));
      fill_pathname_join_special(alt, dir_name, relative_path, sizeof(alt));

      if (task_cloud_sync_should_ignore_file(alt))
         continue;

      /* The "alt" refers to the relative path of whatever we're syncing relative to the retroarch folder
       * whereas the full_path is the absolute disk path of the file. When building the manifest, adhere
       * to a portable standard, but use that as the portable representation of paths. While the actual
       * "manifest" is comprised of full, local-style paths associated with "alt"s which are portable. */
      pathname_make_slashes_portable(alt);
      file_list_append(manifest, full_path, NULL, 0, 0, 0);
      file_list_set_alt_at_offset(manifest, idx, alt);
   }

   /* TODO Is this freed anywhere else? Am I missing something? The dir_list's contents are strdup'ed, so freeing this shouldn't break anything
    * Remove this comment once a decision has been taken*/
   string_list_free(dir_list);
}

/**
 * task_cloud_sync_directory_map:
 *
 * Returns a string_list containing the folders that should be synced.
 * This is hard-coded for now, and syncs the config, the saves and the states
 */
static struct string_list *task_cloud_sync_directory_map(void)
{
   static struct string_list *list = NULL;
   settings_t *settings = config_get_ptr();

   if (!list)
   {
      union string_list_elem_attr attr = {0};
      char  dir[DIR_MAX_LENGTH];
      list = string_list_new();

      if (settings->bools.cloud_sync_sync_configs)
      {
         string_list_append(list, "config", attr);
         fill_pathname_application_special(dir,
               sizeof(dir), APPLICATION_SPECIAL_DIRECTORY_CONFIG);
         list->elems[list->size - 1].userdata = strdup(dir);
      }

      if (settings->bools.cloud_sync_sync_saves)
      {
         string_list_append(list, "saves", attr);
         list->elems[list->size - 1].userdata = strdup(dir_get_ptr(RARCH_DIR_SAVEFILE));

         string_list_append(list, "states", attr);
         list->elems[list->size - 1].userdata = strdup(dir_get_ptr(RARCH_DIR_SAVESTATE));
      }

      if (settings->bools.cloud_sync_sync_thumbs)
      {
         string_list_append(list, "thumbnails", attr);
         strlcpy(dir, settings->paths.directory_thumbnails, sizeof(dir));
         list->elems[list->size - 1].userdata = strdup(dir);
      }

      if (settings->bools.cloud_sync_sync_system)
      {
         string_list_append(list, "system", attr);
         strlcpy(dir, settings->paths.directory_system, sizeof(dir));
         list->elems[list->size - 1].userdata = strdup(dir);
      }
   }

   return list;
}

/**
 * task_cloud_sync_build_current_manifest:
 * @sync_state       : pointer to the current sync state
 *
 * Create an in-memory manifest of actual, current disk data
 */
static void task_cloud_sync_build_current_manifest(task_cloud_sync_state_t *sync_state)
{
   struct string_list *dirlist = task_cloud_sync_directory_map();
   size_t i;

   if (!(sync_state->current_manifest = (file_list_t *)calloc(1, sizeof(file_list_t))))
   {
      sync_state->phase = CLOUD_SYNC_PHASE_END;
      return;
   }

   if (!(sync_state->updated_server_manifest = (file_list_t *)calloc(1, sizeof(file_list_t))))
   {
      sync_state->phase = CLOUD_SYNC_PHASE_END;
      return;
   }

   if (!(sync_state->updated_local_manifest = (file_list_t *)calloc(1, sizeof(file_list_t))))
   {
      sync_state->phase = CLOUD_SYNC_PHASE_END;
      return;
   }

   /* The userdata of the elements is actually the full path to the directory, while data is the name of the folder itself */
   /* The paths iterated here are not portable, because they are still used for iterating later on */
   for (i = 0; i < dirlist->size; i++)
      task_cloud_sync_manifest_append_dir(sync_state->current_manifest,
            dirlist->elems[i].userdata, dirlist->elems[i].data);

   file_list_sort_on_alt(sync_state->current_manifest);
   sync_state->phase = CLOUD_SYNC_PHASE_DIFF;
   RARCH_LOG(CSPFX "created in-memory manifest of current disk state\n");
}

/**
 * task_cloud_sync_update_progress:
 * @task       : pointer to the retro_task executing us
 *
 * Updates the percentage of the current task's progress based on
 * current sync progress. It results in a percentage which is
 * computed based on the total number of files to deal with and
 * the current cumulative count that's been dealt with across
 * the three types of manifests (server, old local, current local)
 */
static void task_cloud_sync_update_progress(retro_task_t *task)
{
   task_cloud_sync_state_t *sync_state = NULL;
   unsigned long            val   = 0;
   unsigned long            count = 0;

   if (!task)
      return;

   if (!(sync_state = (task_cloud_sync_state_t *)task->state))
      return;

   val = sync_state->server_idx + sync_state->current_idx;

   if (sync_state->server_manifest)
      count += sync_state->server_manifest->size;
   if (sync_state->current_manifest)
      count += sync_state->current_manifest->size;

   if (count != 0)
	   task_set_progress(task, (val * 100) / count);
   else
	   task_set_progress(task, 100);
}

static void task_cloud_sync_add_to_updated_manifest(task_cloud_sync_state_t *sync_state, const char *key, char *hash, bool server)
{
   file_list_t *list;
   size_t       idx;
   slock_lock(tcs_running_lock);
   list = server ? sync_state->updated_server_manifest : sync_state->updated_local_manifest;
   idx = list->size;
   file_list_append(list, NULL, NULL, 0, 0, 0);
   file_list_set_alt_at_offset(list, idx, key);
   list->list[idx].userdata = hash;
   slock_unlock(tcs_running_lock);
}

static INLINE int task_cloud_sync_key_cmp(struct item_file *left, struct item_file *right)
{
   char *left_key  = CS_FILE_KEY(left);
   char *right_key = CS_FILE_KEY(right);

   if (!left_key && !right_key)
      return 0;
   else if (!left_key)
      return 1;
   else if (!right_key)
      return -1;
   else
      return strcasecmp(left_key, right_key);
}

static char *task_cloud_sync_md5_rfile(RFILE *file)
{
   MD5_CTX       md5;
   int           rv;
   char         *hash = malloc(33);
   unsigned char buf[4096];
   unsigned char digest[16];

   if (!hash)
      return NULL;

   MD5_Init(&md5);

   do
   {
      rv = (int)filestream_read(file, buf, sizeof(buf));
      if (rv > 0)
         MD5_Update(&md5, buf, rv);
   } while (rv > 0);

   MD5_Final(digest, &md5);

   snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
            digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
      );

   return hash;
}

/* don't pass a server/local item_file to this, only current has ->path set */
static void task_cloud_sync_backup_file(struct item_file *file)
{
   struct tm   tm_;
   size_t      len;
   char        new_dir[DIR_MAX_LENGTH];
   char        backup_dir[DIR_MAX_LENGTH];
   char        new_path[PATH_MAX_LENGTH];
   const char *path_dir_core_assets = config_get_ptr()->paths.directory_core_assets;
   time_t      cur_time             = time(NULL);
   rtime_localtime(&cur_time, &tm_);

   fill_pathname_join_special(backup_dir,
                              path_dir_core_assets,
                              "cloud_backups",
                              sizeof(backup_dir));
   len = fill_pathname_join_special(new_path,
                                    backup_dir,
                                    CS_FILE_KEY(file),
                                    sizeof(new_path));
   strftime(new_path + len, sizeof(new_path) - len, "-%y%m%d-%H%M%S", &tm_);
   pathname_conform_slashes_to_os(new_path);
   fill_pathname_basedir(new_dir, new_path, sizeof(new_dir));
   path_mkdir(new_dir);
   filestream_rename(file->path, new_path);
}

static void task_cloud_sync_fetch_cb(void *user_data, const char *path, bool success, RFILE *file)
{
   task_cloud_sync_state_t *sync_state = (task_cloud_sync_state_t *)user_data;
   char                    *hash = NULL;

   if (!sync_state)
      return;

   if (success && file)
   {
      hash = task_cloud_sync_md5_rfile(file);
      filestream_close(file);
      RARCH_LOG(CSPFX "successfully fetched %s\n", path);
      task_cloud_sync_add_to_updated_manifest(sync_state, path, hash, false);
      sync_state->downloads++;
   }
   else
   {
      /* on failure, don't add it to local manifest, that will cause a fetch again next time */
      if (!success)
         RARCH_WARN(CSPFX "failed to fetch %s\n", path);
      else
         RARCH_WARN(CSPFX "failed to write file from server: %s\n", path);
      sync_state->failures = true;
   }

   slock_lock(tcs_running_lock);
   sync_state->waiting--;
   slock_unlock(tcs_running_lock);
}

static void task_cloud_sync_fetch_server_file(task_cloud_sync_state_t *sync_state)
{
   size_t              i;
   char                filename[PATH_MAX_LENGTH];
   char                directory[DIR_MAX_LENGTH];
   struct string_list *dirlist     = task_cloud_sync_directory_map();
   struct item_file   *server_file = &sync_state->server_manifest->list[sync_state->server_idx];
   const char         *key         = CS_FILE_KEY(server_file);
   /* the key from the server file is in "portable" format, use '/' */
   const char         *path        = strchr(key, '/') + 1;
   settings_t         *settings    = config_get_ptr();

   /* we're just fetching a file the server has, we can update this now */
   task_cloud_sync_add_to_updated_manifest(sync_state, key, CS_FILE_HASH(server_file), true);
   /* no need to mark need_manifest_uploaded, nothing changed */

   if (task_cloud_sync_should_ignore_file(key))
   {
      /* don't fetch a file we're supposed to ignore, even if the server has it */
      RARCH_LOG(CSPFX "ignoring %s\n", key);
      return;
   }
   RARCH_LOG(CSPFX "fetching %s\n", key);

   filename[0] = '\0';
   for (i = 0; i < dirlist->size; i++)
   {
      if (!string_starts_with(key, dirlist->elems[i].data))
         continue;
      fill_pathname_join_special(filename, dirlist->elems[i].userdata, path, sizeof(filename));
      pathname_conform_slashes_to_os(filename);
      break;
   }
   if (string_is_empty(filename))
   {
      /* how did this end up here? we don't know where to put it... */
      RARCH_WARN(CSPFX "don't know where to put %s!\n", key);
      return;
   }

   if (!settings->bools.cloud_sync_destructive && path_is_valid(filename))
   {
      size_t idx;
      if (file_list_search(sync_state->current_manifest, path, &idx))
         task_cloud_sync_backup_file(&sync_state->current_manifest->list[idx]);
   }

   fill_pathname_basedir(directory, filename, sizeof(directory));
   path_mkdir(directory);
   if (cloud_sync_read(key, filename, task_cloud_sync_fetch_cb, sync_state))
      sync_state->waiting++;
   else
   {
      RARCH_WARN(CSPFX "wanted to fetch %s but failed\n", key);
      sync_state->failures = true;
   }
}

static void task_cloud_sync_resolve_conflict(task_cloud_sync_state_t *sync_state)
{
   /*
    * rather than pop up some UI let's just resolve it ourselves!
    * three options:
    * 1. rename the server file and replace it
    * 2. rename the local file and replace it
    * 3. ignore it
    * If we ignore it then we need to keep it out of the new local manifest
    */
   struct item_file *server_file = &sync_state->server_manifest->list[sync_state->server_idx];
   RARCH_WARN(CSPFX "conflicting change of %s\n", CS_FILE_KEY(server_file));
   task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(server_file), CS_FILE_HASH(server_file), true);
   /* no need to mark need_manifest_uploaded, nothing changed */
   sync_state->conflicts = true;
}

static void task_cloud_sync_upload_cb(void *user_data, const char *path, bool success, RFILE *file)
{
   task_cloud_sync_state_t *sync_state = (task_cloud_sync_state_t *)user_data;
   size_t                   idx;

   if (file)
      filestream_close(file);

   if (!sync_state)
      return;

   if (success)
   {
      /* need to update server manifest as well */
      if (file_list_search(sync_state->current_manifest, path, &idx))
      {
         struct item_file *current_file = &sync_state->current_manifest->list[idx];
         task_cloud_sync_add_to_updated_manifest(sync_state, path, CS_FILE_HASH(current_file), true);
         task_cloud_sync_add_to_updated_manifest(sync_state, path, CS_FILE_HASH(current_file), false);
         sync_state->need_manifest_uploaded = true;
      }
      RARCH_LOG(CSPFX "uploading %s succeeded\n", path);
      sync_state->uploads++;
   }
   else
   {
      /* if the upload fails, try to resurrect the hash from the last sync */
      if (file_list_search(sync_state->local_manifest, path, &idx))
      {
         struct item_file *local_file = &sync_state->local_manifest->list[idx];
         task_cloud_sync_add_to_updated_manifest(sync_state, path, CS_FILE_HASH(local_file), false);
      }
      RARCH_WARN(CSPFX "uploading %s failed\n", path);
      sync_state->failures = true;
   }

   slock_lock(tcs_running_lock);
   sync_state->waiting--;
   slock_unlock(tcs_running_lock);
}

/**
 * task_cloud_sync_update_progress:
 * @sync_state 	: pointer to the current sync task
 *
 * Uploads the current file to the cloud. The current file is defined
 * as whatever file is indicated by the current manifest (that is, the
 * local, actually true manifest) combined with whatever the iteration
 * variable indicates (the one specific to the current manifest)
 */
static void task_cloud_sync_upload_current_file(task_cloud_sync_state_t *sync_state)
{
   struct item_file *item     = &sync_state->current_manifest->list[sync_state->current_idx];
   const char       *path     = CS_FILE_KEY(item);
   const char       *filename = item->path;
   RFILE            *file;

   if (task_cloud_sync_should_ignore_file(path))
   {
      RARCH_LOG(CSPFX "ignoring %s, not uploading\n", path);
      return;
   }

   file = filestream_open(filename,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return;

   RARCH_LOG(CSPFX "uploading %s\n", path);

   item->userdata = task_cloud_sync_md5_rfile(file);

   filestream_seek(file, 0, SEEK_SET);
   sync_state->waiting++;
   if (!cloud_sync_update(path, file, task_cloud_sync_upload_cb, sync_state))
   {
      /* if the upload fails, try to resurrect the hash from the last sync */
      size_t idx;
      if (file_list_search(sync_state->local_manifest, path, &idx))
      {
         struct item_file *local_file = &sync_state->local_manifest->list[idx];
         task_cloud_sync_add_to_updated_manifest(sync_state, path, CS_FILE_HASH(local_file), false);
      }
      filestream_close(file);
      sync_state->waiting--;
      sync_state->failures = true;
      RARCH_WARN(CSPFX "uploading %s failed\n", path);
   }
}

static void task_cloud_sync_delete_current_file(task_cloud_sync_state_t *sync_state)
{
   struct item_file *item      = &sync_state->current_manifest->list[sync_state->current_idx];
   bool cloud_sync_destructive = config_get_ptr()->bools.cloud_sync_destructive;

   RARCH_WARN(CSPFX "server has deleted %s, so shall we\n", CS_FILE_KEY(item));

   if (cloud_sync_destructive)
      filestream_delete(item->path);
   else
      task_cloud_sync_backup_file(item);
}

static void task_cloud_sync_check_server_current(task_cloud_sync_state_t *sync_state, bool include_local)
{
   bool server_changed, current_changed;
   struct item_file *server_file  = &sync_state->server_manifest->list[sync_state->server_idx];
   struct item_file *local_file   = NULL;
   struct item_file *current_file = &sync_state->current_manifest->list[sync_state->current_idx];
   const char       *filename     = current_file->path;
   RFILE            *file;

   if (task_cloud_sync_should_ignore_file(CS_FILE_KEY(server_file)))
   {
      RARCH_LOG(CSPFX "ignoring %s (despite possible conflict)\n", CS_FILE_KEY(server_file));
      return;
   }

   file = filestream_open(filename,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return;

   current_file->userdata = task_cloud_sync_md5_rfile(file);
   filestream_close(file);

   if (string_is_equal(CS_FILE_HASH(server_file), CS_FILE_HASH(current_file)))
   {
      task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(current_file), CS_FILE_HASH(current_file), true);
      task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(current_file), CS_FILE_HASH(current_file), false);
      /* No need to mark need_manifest_uploaded, nothing changed */
      return;
   }

   if (!include_local)
   {
      task_cloud_sync_resolve_conflict(sync_state);
      return;
   }

   local_file      = &sync_state->local_manifest->list[sync_state->local_idx];
   server_changed  = !string_is_equal(CS_FILE_HASH(local_file), CS_FILE_HASH(server_file));
   current_changed = !string_is_equal(CS_FILE_HASH(local_file), CS_FILE_HASH(current_file));

   if (server_changed && current_changed)
      task_cloud_sync_resolve_conflict(sync_state);
   else if (current_changed)
      task_cloud_sync_upload_current_file(sync_state);
   else if (!CS_FILE_DELETED(server_file))
      task_cloud_sync_fetch_server_file(sync_state);
   else
   {
      task_cloud_sync_delete_current_file(sync_state);
      task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(server_file), CS_FILE_HASH(server_file), false);
   }
}

static void task_cloud_sync_delete_cb(void *user_data, const char *path, bool success, RFILE *file)
{
   task_cloud_sync_state_t *sync_state = (task_cloud_sync_state_t *)user_data;

   if (!sync_state)
      return;

   if (!success)
   {
      /* if the delete fails, resurrect the hash from the last sync */
      size_t idx;
      if (file_list_search(sync_state->local_manifest, path, &idx))
      {
         struct item_file *local_file = &sync_state->local_manifest->list[idx];
         task_cloud_sync_add_to_updated_manifest(sync_state, path, CS_FILE_HASH(local_file), false);
      }
      RARCH_WARN(CSPFX "deleting %s failed\n", path);
      sync_state->failures = true;
      slock_lock(tcs_running_lock);
      sync_state->waiting--;
      slock_unlock(tcs_running_lock);
      return;
   }

   RARCH_LOG(CSPFX "deleting %s succeeded\n", path);
   /* need to update server manifest. we don't set the hash as that indicates a
    * deleted file. need to update the local manifest to indicate we sync'd that
    * it is deleted */
   task_cloud_sync_add_to_updated_manifest(sync_state, path, NULL, true);
   task_cloud_sync_add_to_updated_manifest(sync_state, path, NULL, false);
   sync_state->need_manifest_uploaded = true;
   slock_lock(tcs_running_lock);
   sync_state->waiting--;
   slock_unlock(tcs_running_lock);
}

static void task_cloud_sync_delete_server_file(task_cloud_sync_state_t *sync_state)
{
   struct item_file *server_file = &sync_state->server_manifest->list[sync_state->server_idx];
   const char       *key = CS_FILE_KEY(server_file);

   if (task_cloud_sync_should_ignore_file(key))
   {
      RARCH_LOG(CSPFX "ignoring %s, instead of removing from server\n", key);
      return;
   }

   RARCH_LOG(CSPFX "deleting %s\n", key);

   sync_state->waiting++;
   if (!cloud_sync_free(key, task_cloud_sync_delete_cb, sync_state))
   {
      /* if the delete fails, resurrect the hash from the last sync */
      size_t idx;
      if (file_list_search(sync_state->local_manifest, key, &idx))
      {
         struct item_file *local_file = &sync_state->local_manifest->list[idx];
         task_cloud_sync_add_to_updated_manifest(sync_state, key, CS_FILE_HASH(local_file), false);
      }
      task_cloud_sync_add_to_updated_manifest(sync_state, key, CS_FILE_HASH(server_file), true);
      /* we don't mark need_manifest_uploaded here, nothing has changed */
      sync_state->waiting--;
   }
}

static void task_cloud_sync_diff_next(task_cloud_sync_state_t *sync_state)
{
   int server_local_key_cmp;
   int server_current_key_cmp;
   int current_local_key_cmp;
   struct item_file *server_file  = NULL;
   struct item_file *local_file   = NULL;
   struct item_file *current_file = NULL;

   if (   sync_state->server_manifest
       && sync_state->server_idx < sync_state->server_manifest->size)
      server_file = &sync_state->server_manifest->list[sync_state->server_idx];
   if (   sync_state->local_manifest
       && sync_state->local_idx < sync_state->local_manifest->size)
      local_file = &sync_state->local_manifest->list[sync_state->local_idx];
   if (   sync_state->current_manifest
       && sync_state->current_idx < sync_state->current_manifest->size)
      current_file = &sync_state->current_manifest->list[sync_state->current_idx];

   if (!server_file && !local_file && !current_file)
   {
      RARCH_LOG(CSPFX "finished processing manifests\n");
      sync_state->phase = CLOUD_SYNC_PHASE_UPDATE_MANIFESTS;
      return;
   }

   /* Doing a three-way diff of sorted lists of files. grab the first one from
    * each, resolve any difference, move on. */
   server_local_key_cmp = task_cloud_sync_key_cmp(server_file, local_file);

   if (server_local_key_cmp < 0)
   {
      /* server has a file not in the last sync'd manifest */
      server_current_key_cmp = task_cloud_sync_key_cmp(server_file, current_file);
      if (server_current_key_cmp < 0)
      {
         /* the server has a file we don't have, we check the hash */
         if (!CS_FILE_DELETED(server_file))
            task_cloud_sync_fetch_server_file(sync_state);
         else
         {
            /* it's deleted on the server, remember that and mark the sync of the delete */
            task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(server_file), NULL, true);
            task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(server_file), NULL, false);
            /* we don't mark need_manifest_uploaded here, nothing has changed */
         }
         sync_state->server_idx++;
      }
      else if (server_current_key_cmp == 0)
      {
         /* the server has a file that we also have locally but haven't fetched from the server previously */
         task_cloud_sync_check_server_current(sync_state, false);
         sync_state->server_idx++;
         sync_state->current_idx++;
      }
      else
      {
         /* we have a file locally that the server doesn't have */
         task_cloud_sync_upload_current_file(sync_state);
         sync_state->current_idx++;
      }
   }
   else if (server_local_key_cmp == 0)
   {
      /* we've seen this file from the server before */
      current_local_key_cmp = task_cloud_sync_key_cmp(current_file, local_file);
      if (current_local_key_cmp < 0)
      {
         /* we have a file locally that the server doesn't have */
         task_cloud_sync_upload_current_file(sync_state);
         sync_state->current_idx++;
      }
      else if (current_local_key_cmp == 0)
      {
         /* we're all looking at the same file */
         task_cloud_sync_check_server_current(sync_state, true);
         sync_state->current_idx++;
         sync_state->local_idx++;
         sync_state->server_idx++;
      }
      else
      {
         /* the file has been deleted locally */
         if (!CS_FILE_DELETED(server_file))
         {
            if (CS_FILE_DELETED(local_file))
               /* previously saw the delete, now it's resurrected */
               task_cloud_sync_fetch_server_file(sync_state);
            else if (string_is_equal(CS_FILE_HASH(server_file), CS_FILE_HASH(local_file)))
               /* server didn't change, delete from the server */
               task_cloud_sync_delete_server_file(sync_state);
            else
               /* the server changed and local deleted, that's a conflict */
               task_cloud_sync_resolve_conflict(sync_state);
         }
         else
         {
            /* already deleted, oh well */
            task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(server_file), NULL, true);
            task_cloud_sync_add_to_updated_manifest(sync_state, CS_FILE_KEY(server_file), NULL, false);
            /* we don't mark need_manifest_uploaded here, nothing has changed */
         }
         sync_state->local_idx++;
         sync_state->server_idx++;
      }
   }
   else
   {
      /* the server is missing a file that we've sync'd before? should have at
       * least had a deleted record? assume the server state got reset and treat
       * as a missing file on the server */
      current_local_key_cmp = task_cloud_sync_key_cmp(current_file, local_file);
      if (current_local_key_cmp < 0)
      {
         task_cloud_sync_upload_current_file(sync_state);
         sync_state->current_idx++;
      }
      else if (current_local_key_cmp == 0)
      {
         task_cloud_sync_upload_current_file(sync_state);
         sync_state->current_idx++;
         sync_state->local_idx++;
      }
      else
      {
         /* this is odd, it exists in the last sync manifest but not on the
          * server and not on disk? wtf? */
         RARCH_WARN(CSPFX "%s only exists in previous manifest? odd\n", CS_FILE_KEY(local_file));
         sync_state->local_idx++;
      }
   }
}

static void task_cloud_sync_update_manifest_cb(void *user_data, const char *path, bool success, RFILE *file)
{
   task_cloud_sync_state_t *sync_state = (task_cloud_sync_state_t *)user_data;

   if (file)
      filestream_close(file);

   if (!sync_state)
      return;

   RARCH_LOG(CSPFX "uploading updated manifest succeeded\n");
   sync_state->phase = CLOUD_SYNC_PHASE_END;
   slock_lock(tcs_running_lock);
   sync_state->waiting = 0;
   slock_unlock(tcs_running_lock);
}

static RFILE *task_cloud_sync_write_updated_manifest(file_list_t *manifest, char *path)
{
   rjsonwriter_t *writer = NULL;
   size_t         idx    = 0;
   RFILE *file           = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return NULL;

   if (!(writer = rjsonwriter_open_rfile(file)))
   {
      filestream_close(file);
      return NULL;
   }

   /* since we may be transferring files at the same time,
    * the newly created manifest might be out of order */
   file_list_sort_on_alt(manifest);

   rjsonwriter_raw(writer, "[\n", 2);

   for (; idx < manifest->size; idx++)
   {
      struct item_file *item = &manifest->list[idx];

      if (idx)
         rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_raw(writer, "{\n", 2);

      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_add_string(writer, "path");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_add_string(writer, CS_FILE_KEY(item));
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_add_string(writer, "hash");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_add_string(writer, CS_FILE_HASH(item));
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_raw(writer, "}", 1);
   }

   rjsonwriter_raw(writer, "\n]\n", 3);
   rjsonwriter_free(writer);

   RARCH_LOG(CSPFX "wrote %s\n", path);

   return file;
}

static void task_cloud_sync_update_manifests(task_cloud_sync_state_t *sync_state)
{
   char   manifest_path[PATH_MAX_LENGTH];
   RFILE *file   = NULL;

   task_cloud_sync_manifest_filename(manifest_path, sizeof(manifest_path), false);
   file = task_cloud_sync_write_updated_manifest(sync_state->updated_local_manifest, manifest_path);
   if (file)
      filestream_close(file);

   if (sync_state->need_manifest_uploaded)
   {
      RARCH_LOG(CSPFX "uploading updated manifest to server\n");
      task_cloud_sync_manifest_filename(manifest_path, sizeof(manifest_path), true);
      file = task_cloud_sync_write_updated_manifest(sync_state->updated_server_manifest, manifest_path);
      filestream_seek(file, 0, SEEK_SET);
      sync_state->waiting = 1;
      if (!cloud_sync_update(MANIFEST_FILENAME_SERVER, file, task_cloud_sync_update_manifest_cb, sync_state))
      {
         RARCH_LOG(CSPFX "uploading updated manifest failed\n");
         filestream_close(file);
         sync_state->waiting = 0;
         sync_state->failures = true;
         sync_state->phase = CLOUD_SYNC_PHASE_END;
      }
      return;
   }
   else
      sync_state->phase = CLOUD_SYNC_PHASE_END;
}

static void task_cloud_sync_end_handler(void *user_data, const char *path, bool success, RFILE *file)
{
   retro_task_t            *task       = (retro_task_t *)user_data;
   task_cloud_sync_state_t *sync_state = NULL;
   retro_time_t             end_time   = cpu_features_get_time_usec();

   if (!task)
      return;

   if ((sync_state = (task_cloud_sync_state_t *)task->state))
   {
      char title[128];
      size_t _len = strlcpy(title, "Cloud Sync finished", sizeof(title));
      if (sync_state->failures || sync_state->conflicts)
         _len += strlcpy(title + _len, " with ", sizeof(title) - _len);
      if (sync_state->failures)
         _len += strlcpy(title + _len, "failures", sizeof(title) - _len);
      if (sync_state->failures && sync_state->conflicts)
         _len += strlcpy(title + _len, " and ", sizeof(title) - _len);
      if (sync_state->conflicts)
         strlcpy(title + _len, "conflicts", sizeof(title) - _len);
      task_set_title(task, strdup(title));
   }


   RARCH_LOG(CSPFX "finished after %lld.%06lld seconds, %d files uploaded, %d files downloaded\n",
         (end_time - sync_state->start_time) / 1000 / 1000,
         (end_time - sync_state->start_time) % (1000 * 1000),
         sync_state->uploads, sync_state->downloads);

   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void task_cloud_sync_task_handler(retro_task_t *task)
{
   task_cloud_sync_state_t *sync_state = NULL;

   if (!task)
      goto task_finished;

   if (!(sync_state = (task_cloud_sync_state_t *)task->state))
      goto task_finished;

   slock_lock(tcs_running_lock);
   /* we can transfer more than one file at a time */
   if (sync_state->waiting > ((sync_state->phase == CLOUD_SYNC_PHASE_DIFF) ? 4 : 0))
   {
      task->when = cpu_features_get_time_usec() + 17 * 1000; /* 17ms */
      slock_unlock(tcs_running_lock);
      return;
   }
   slock_unlock(tcs_running_lock);

   switch (sync_state->phase)
   {
      case CLOUD_SYNC_PHASE_BEGIN:
         sync_state->waiting = 1;
         if (!cloud_sync_begin(task_cloud_sync_begin_handler, task))
         {
            RARCH_WARN(CSPFX "could not begin\n");
            task_set_title(task, strdup("Cloud Sync failed"));
            goto task_finished;
         }
         break;
      case CLOUD_SYNC_PHASE_FETCH_SERVER_MANIFEST:
         task_cloud_sync_fetch_server_manifest(sync_state);
         break;
      case CLOUD_SYNC_PHASE_READ_LOCAL_MANIFEST:
         task_cloud_sync_read_local_manifest(sync_state);
         break;
      case CLOUD_SYNC_PHASE_BUILD_CURRENT_MANIFEST:
         task_cloud_sync_build_current_manifest(sync_state);
         break;
      case CLOUD_SYNC_PHASE_DIFF:
         task_cloud_sync_update_progress(task);
         task_cloud_sync_diff_next(sync_state);
         break;
      case CLOUD_SYNC_PHASE_UPDATE_MANIFESTS:
         task_cloud_sync_update_manifests(sync_state);
         break;
      case CLOUD_SYNC_PHASE_END:
         sync_state->waiting = 1;
         if (!cloud_sync_end(task_cloud_sync_end_handler, task))
         {
            RARCH_WARN(CSPFX "could not end?!\n");
            goto task_finished;
         }
         break;
   }

   return;

task_finished:
   if (task)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void task_cloud_sync_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   task_cloud_sync_state_t *sync_state = (task_cloud_sync_state_t *)task_data;

   if (!sync_state)
      return;

   if (sync_state->server_manifest)
      file_list_free(sync_state->server_manifest);
   if (sync_state->local_manifest)
      file_list_free(sync_state->local_manifest);
   if (sync_state->current_manifest)
      file_list_free(sync_state->current_manifest);
   if (sync_state->updated_server_manifest)
      file_list_free(sync_state->updated_server_manifest);
   if (sync_state->updated_local_manifest)
      file_list_free(sync_state->updated_local_manifest);

   free(sync_state);
}

static bool task_cloud_sync_task_finder(retro_task_t *task, void *user_data)
{
   if (!task)
      return false;

   /* there can be only one */
   return task->handler == task_cloud_sync_task_handler;
}

void task_push_cloud_sync(void)
{
   char task_title[128];
   task_finder_data_t       find_data;
   task_cloud_sync_state_t *sync_state = NULL;
   retro_task_t            *task       = NULL;
   bool cloud_sync_enable              = config_get_ptr()->bools.cloud_sync_enable;

   if (!cloud_sync_enable)
      return;

   if (!tcs_running_lock)
      tcs_running_lock = slock_new();

   find_data.func = task_cloud_sync_task_finder;
   if (task_queue_find(&find_data))
   {
      RARCH_LOG(CSPFX "already in progress\n");
      return;
   }

   sync_state = (task_cloud_sync_state_t *)calloc(1, sizeof(task_cloud_sync_state_t));
   if (!sync_state)
      return;

   if (!(task = task_init()))
   {
      free(sync_state);
      return;
   }

   sync_state->phase      = CLOUD_SYNC_PHASE_BEGIN;
   sync_state->start_time = cpu_features_get_time_usec();

   strlcpy(task_title, "Cloud Sync in progress", sizeof(task_title));

   task->state    = sync_state;
   task->title    = strdup(task_title);
   task->handler  = task_cloud_sync_task_handler;
   task->callback = task_cloud_sync_cb;

   task_queue_push(task);
}

void task_push_cloud_sync_update_driver(void)
{
   char manifest_path[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   cloud_sync_find_driver(settings->arrays.cloud_sync_driver,
         "cloud sync driver", verbosity_is_enabled());

   /* The sync does a three-way diff: current local <- last sync -> current server.
    * When the server changes it becomes a four way diff, which can lead to odd
    * conflicts or data loss. The easiest way to resolve it is to reset the last sync
    */
   task_cloud_sync_manifest_filename(manifest_path, sizeof(manifest_path), false);
   filestream_delete(manifest_path);
}
