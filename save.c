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
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

#include <lists/string_list.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/rzip_stream.h>
#include <rthreads/rthreads.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <time/rtime.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "content.h"
#include "core.h"
#include "core_info.h"
#include "file_path_special.h"
#include "msg_hash.h"
#include "runloop.h"
#include "verbosity.h"
#ifdef HAVE_CHEATS
#include "cheat_manager.h"
#endif

struct ram_type
{
   const char *path;
   int type;
};

static struct string_list *task_save_files = NULL;

#ifdef HAVE_THREADS
typedef struct autosave autosave_t;

/* Autosave support. */
struct autosave_st
{
   autosave_t **list;
   unsigned num;
};

enum autosave_flags
{
   AUTOSAVE_FLAG_QUIT           = (1 << 0),
   AUTOSAVE_FLAG_COMPRESS_FILES = (1 << 1)
};

struct autosave
{
   void *buffer;
   const void *retro_buffer;
   const char *path;
   slock_t *lock;
   slock_t *cond_lock;
   scond_t *cond;
   sthread_t *thread;
   size_t bufsize;
   unsigned interval;
   uint8_t flags;
};

static struct autosave_st autosave_state;


/**
 * autosave_thread:
 * @data            : pointer to autosave object
 *
 * Callback function for (threaded) autosave.
 **/
static void autosave_thread(void *data)
{
   autosave_t *save = (autosave_t*)data;

   for (;;)
   {
      bool differ;

      slock_lock(save->lock);
      differ = memcmp(save->buffer, save->retro_buffer,
            save->bufsize) != 0;
      if (differ)
         memcpy(save->buffer, save->retro_buffer, save->bufsize);
      slock_unlock(save->lock);

      if (differ)
      {
         intfstream_t *file = NULL;

         /* Should probably deal with this more elegantly. */
         if (save->flags & AUTOSAVE_FLAG_COMPRESS_FILES)
            file = intfstream_open_rzip_file(save->path,
                  RETRO_VFS_FILE_ACCESS_WRITE);
         else
            file = intfstream_open_file(save->path,
                  RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);

         if (file)
         {
            intfstream_write(file, save->buffer, save->bufsize);
            intfstream_flush(file);
            intfstream_close(file);
            free(file);
         }
      }

      slock_lock(save->cond_lock);

      if (save->flags & AUTOSAVE_FLAG_QUIT)
      {
         slock_unlock(save->cond_lock);
         break;
      }

      scond_wait_timeout(save->cond,
            save->cond_lock,
#if defined(_MSC_VER) && _MSC_VER <= 1200
            save->interval * 1000000
#else
            save->interval * 1000000LL
#endif
            );

      slock_unlock(save->cond_lock);
   }
}

/**
 * autosave_new:
 * @path            : path to autosave file
 * @data            : pointer to buffer
 * @len             : size of @data buffer
 * @interval        : interval at which saves should be performed.
 *
 * Create and initialize autosave object.
 *
 * @return Pointer to new autosave_t object if successful, otherwise
 * NULL.
 **/
static autosave_t *autosave_new(const char *path,
      const void *data, size_t len,
      unsigned interval, bool compress)
{
   void       *buf               = NULL;
   autosave_t *handle            = (autosave_t*)malloc(sizeof(*handle));
   if (!handle)
      return NULL;

   handle->flags                 = 0;
   handle->bufsize               = len;
   handle->interval              = interval;
   if (compress)
      handle->flags             |= AUTOSAVE_FLAG_COMPRESS_FILES;
   handle->retro_buffer          = data;
   handle->path                  = path;

   if (!(buf = malloc(len)))
   {
      free(handle);
      return NULL;
   }

   handle->buffer                = buf;

   memcpy(handle->buffer, handle->retro_buffer, handle->bufsize);

   handle->lock                  = slock_new();
   handle->cond_lock             = slock_new();
   handle->cond                  = scond_new();
   handle->thread                = sthread_create(autosave_thread, handle);

   return handle;
}

/**
 * autosave_free:
 * @handle          : pointer to autosave object
 *
 * Frees autosave object.
 **/
static void autosave_free(autosave_t *handle)
{
   slock_lock(handle->cond_lock);
   handle->flags |= AUTOSAVE_FLAG_QUIT;
   slock_unlock(handle->cond_lock);
   scond_signal(handle->cond);
   sthread_join(handle->thread);

   slock_free(handle->lock);
   slock_free(handle->cond_lock);
   scond_free(handle->cond);

   if (handle->buffer)
      free(handle->buffer);
   handle->buffer = NULL;
}

bool autosave_init(bool compress_files, unsigned autosave_interval)
{
   unsigned i;
   autosave_t **list          = NULL;

   if (autosave_interval < 1 || !task_save_files)
      return false;

   if (!(list = (autosave_t**)
      calloc(task_save_files->size,
            sizeof(*autosave_state.list))))
      return false;

   autosave_state.list = list;
   autosave_state.num  = (unsigned)task_save_files->size;

   for (i = 0; i < task_save_files->size; i++)
   {
      retro_ctx_memory_info_t mem_info;
      autosave_t *auto_st = NULL;
      const char *path    = task_save_files->elems[i].data;
      unsigned    type    = task_save_files->elems[i].attr.i;

      mem_info.id         = type;

      core_get_memory(&mem_info);

      if (mem_info.size <= 0)
         continue;

      if (!(auto_st = autosave_new(path,
            mem_info.data,
            mem_info.size,
            autosave_interval,
            compress_files)))
      {
         RARCH_WARN("%s\n", msg_hash_to_str(MSG_AUTOSAVE_FAILED));
         continue;
      }

      autosave_state.list[i] = auto_st;
   }

   return true;
}

void autosave_deinit(void)
{
   unsigned i;

   for (i = 0; i < autosave_state.num; i++)
   {
      autosave_t *handle = autosave_state.list[i];
      if (handle)
      {
         autosave_free(handle);
         free(autosave_state.list[i]);
      }
      autosave_state.list[i] = NULL;
   }

   free(autosave_state.list);

   autosave_state.list     = NULL;
   autosave_state.num      = 0;
}

/**
 * autosave_lock:
 *
 * Lock autosave.
 **/
void autosave_lock(void)
{
   unsigned i;

   for (i = 0; i < autosave_state.num; i++)
   {
      autosave_t *handle = autosave_state.list[i];
      if (handle)
         slock_lock(handle->lock);
   }
}

/**
 * autosave_unlock:
 *
 * Unlocks autosave.
 **/
void autosave_unlock(void)
{
   unsigned i;

   for (i = 0; i < autosave_state.num; i++)
   {
      autosave_t *handle = autosave_state.list[i];
      if (handle)
         slock_unlock(handle->lock);
   }
}
#endif

static bool content_get_memory(retro_ctx_memory_info_t *mem_info,
      struct ram_type *ram, unsigned slot)
{
   ram->type     = task_save_files->elems[slot].attr.i;
   ram->path     = task_save_files->elems[slot].data;
   mem_info->id  = ram->type;

   core_get_memory(mem_info);

   if (!mem_info->data || mem_info->size == 0)
      return false;

   return true;
}

/**
 * content_load_ram_file:
 * @path             : path of RAM state that will be loaded from.
 * @type             : type of memory
 *
 * Load a RAM state from disk to memory.
 */
static bool content_load_ram_file(unsigned slot)
{
   int64_t rc;
   struct ram_type ram;
   retro_ctx_memory_info_t mem_info;
   void *buf        = NULL;

   if (!content_get_memory(&mem_info, &ram, slot))
      return false;

   /* On first run of content, SRAM file will
    * not exist. This is a common enough occurrence
    * that we should check before attempting to
    * invoke the relevant read_file() function */
   if (    string_is_empty(ram.path)
       || !path_is_valid(ram.path))
      return false;

#if defined(HAVE_ZLIB)
   /* Always use RZIP interface when reading SRAM
    * files - this will automatically handle uncompressed
    * data */
   if (!rzipstream_read_file(ram.path, &buf, &rc))
#else
   if (!filestream_read_file(ram.path, &buf, &rc))
#endif
      return false;

   if (rc > 0)
   {
      if (rc > (ssize_t)mem_info.size)
      {
         RARCH_WARN("[SRAM]: SRAM is larger than implementation expects, "
               "doing partial load (truncating %u %s %s %u).\n",
               (unsigned)rc,
               msg_hash_to_str(MSG_BYTES),
               msg_hash_to_str(MSG_TO),
               (unsigned)mem_info.size);
         rc = mem_info.size;
      }
      memcpy(mem_info.data, buf, (size_t)rc);
   }

   if (buf)
      free(buf);

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
      size_t len, unsigned type)
{
   char path[PATH_MAX_LENGTH + 256 + 32];
   path            [0]    = '\0';

   if (fill_pathname_application_data(path,
            sizeof(path)))
   {
      size_t _len;
      time_t time_;
      struct tm tm_;
      time(&time_);
      rtime_localtime(&time_, &tm_);
      _len  = strlcat(path, "/RetroArch-recovery-", sizeof(path));
      _len += snprintf(path + _len, sizeof(path) - _len, "%u", type);
      strftime(path + _len, sizeof(path) - _len,
            "%Y-%m-%d-%H-%M-%S", &tm_);

      /* Fallback (emergency) saves are always
       * uncompressed
       * > If a regular save fails, then the host
       *   system is experiencing serious technical
       *   difficulties (most likely some kind of
       *   hardware failure)
       * > In this case, we don't want to further
       *   complicate matters by introducing zlib
       *   compression overheads */
      if (filestream_write_file(path, data, len))
      {
         RARCH_WARN("[SRAM]: Succeeded in saving RAM data to \"%s\".\n", path);
         return true;
      }
   }

   return false;
}

/**
 * content_save_ram_file:
 * @path             : path of RAM state that shall be written to.
 * @type             : type of memory
 *
 * Save a RAM state from memory to disk.
 *
 */
static bool content_save_ram_file(unsigned slot, bool compress)
{
   struct ram_type ram;
   retro_ctx_memory_info_t mem_info;

   if (!content_get_memory(&mem_info, &ram, slot))
      return false;

   RARCH_LOG("[SRAM]: %s #%u %s \"%s\".\n",
         msg_hash_to_str(MSG_SAVING_RAM_TYPE),
         ram.type,
         msg_hash_to_str(MSG_TO),
         ram.path);

#if defined(HAVE_ZLIB)
   if (compress)
   {
      if (!rzipstream_write_file(
            ram.path, mem_info.data, mem_info.size))
         goto fail;
   }
   else
#endif
   {
      if (!filestream_write_file(
            ram.path, mem_info.data, mem_info.size))
         goto fail;
   }

   RARCH_LOG("[SRAM]: %s \"%s\".\n",
         msg_hash_to_str(MSG_SAVED_SUCCESSFULLY_TO),
         ram.path);

   return true;

fail:
   RARCH_ERR("[SRAM]: %s.\n",
         msg_hash_to_str(MSG_FAILED_TO_SAVE_SRAM));
   RARCH_WARN("[SRAM]: Attempting to recover ...\n");

   /* In case the file could not be written to,
    * the fallback function 'dump_to_file_desperate'
    * will be called. */
   if (!dump_to_file_desperate(
            mem_info.data, mem_info.size, ram.type))
      RARCH_WARN("[SRAM]: Failed ... Cannot recover save file.\n");
   return false;
}

bool event_save_files(bool is_sram_used, bool compress_files,
      const char *path_cheat_database)
{
   unsigned i;
#ifdef HAVE_CHEATS
   cheat_manager_save_game_specific_cheats(path_cheat_database);
#endif
   if (!task_save_files || !is_sram_used)
      return false;
   for (i = 0; i < task_save_files->size; i++)
      content_save_ram_file(i, compress_files);
   return true;
}

bool event_load_save_files(bool is_sram_load_disabled)
{
   unsigned i;
   bool success = false;

   if (!task_save_files || is_sram_load_disabled)
      return false;

   /* Report a successful load operation if
    * any type of RAM file is found and
    * processed correctly */
   for (i = 0; i < task_save_files->size; i++)
      success |= content_load_ram_file(i);

   return success;
}

void path_init_savefile_rtc(const char *savefile_path)
{
   union string_list_elem_attr attr;
   char savefile_name_rtc[PATH_MAX_LENGTH];

   attr.i = RETRO_MEMORY_SAVE_RAM;
   string_list_append(task_save_files, savefile_path, attr);

   /* Infer .rtc save path from save RAM path. */
   attr.i = RETRO_MEMORY_RTC;
   fill_pathname(savefile_name_rtc,
         savefile_path, ".rtc",
         sizeof(savefile_name_rtc));
   string_list_append(task_save_files, savefile_name_rtc, attr);
}

void path_deinit_savefile(void)
{
   if (task_save_files)
      string_list_free(task_save_files);
   task_save_files = NULL;
}

void path_init_savefile_new(void)
{
   task_save_files = string_list_new();
}

void *savefile_ptr_get(void)
{
   return task_save_files;
}

