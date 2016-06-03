/*  RetroArch - A frontend for libretro.
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

#include <boolean.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <rthreads/rthreads.h>

#include "../core.h"
#include "../msg_hash.h"
#include "../verbosity.h"
#include "../autosave.h"
#include "../configuration.h"
#include "../msg_hash.h"
#include "../runloop.h"

#include "tasks_internal.h"

/* TODO/FIXME - turn this into actual task */

struct ram_type
{
   const char *path;
   int type;
};

#ifdef HAVE_THREADS
/* Autosave support. */
struct autosave_st
{
   autosave_t **list;
   unsigned num;
};

struct autosave
{
   volatile bool quit;
   slock_t *lock;

   slock_t *cond_lock;
   scond_t *cond;
   sthread_t *thread;

   void *buffer;
   const void *retro_buffer;
   const char *path;
   size_t bufsize;
   unsigned interval;
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
   bool first_log   = true;
   autosave_t *save = (autosave_t*)data;

   while (!save->quit)
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
         /* Should probably deal with this more elegantly. */
         FILE *file = fopen(save->path, "wb");

         if (file)
         {
            bool failed = false;

            /* Avoid spamming down stderr ... */
            if (first_log)
            {
               RARCH_LOG("Autosaving SRAM to \"%s\", will continue to check every %u seconds ...\n",
                     save->path, save->interval);
               first_log = false;
            }
            else
               RARCH_LOG("SRAM changed ... autosaving ...\n");

            failed |= fwrite(save->buffer, 1, save->bufsize, file)
               != save->bufsize;
            failed |= fflush(file) != 0;
            failed |= fclose(file) != 0;
            if (failed)
               RARCH_WARN("Failed to autosave SRAM. Disk might be full.\n");
         }
      }

      slock_lock(save->cond_lock);

      if (!save->quit)
         scond_wait_timeout(save->cond, save->cond_lock,
               save->interval * 1000000LL);

      slock_unlock(save->cond_lock);
   }
}

/**
 * autosave_new:
 * @path            : path to autosave file
 * @data            : pointer to buffer
 * @size            : size of @data buffer
 * @interval        : interval at which saves should be performed.
 *
 * Create and initialize autosave object.
 *
 * Returns: pointer to new autosave_t object if successful, otherwise
 * NULL.
 **/
static autosave_t *autosave_new(const char *path,
      const void *data, size_t size,
      unsigned interval)
{
   autosave_t *handle   = (autosave_t*)calloc(1, sizeof(*handle));
   if (!handle)
      goto error;

   handle->bufsize      = size;
   handle->interval     = interval;
   handle->path         = path;
   handle->buffer       = malloc(size);
   handle->retro_buffer = data;

   if (!handle->buffer)
      goto error;

   memcpy(handle->buffer, handle->retro_buffer, handle->bufsize);

   handle->lock         = slock_new();
   handle->cond_lock    = slock_new();
   handle->cond         = scond_new();

   handle->thread       = sthread_create(autosave_thread, handle);

   return handle;

error:
   if (handle)
      free(handle);
   return NULL;
}

/**
 * autosave_free:
 * @handle          : pointer to autosave object
 *
 * Frees autosave object.
 **/
static void autosave_free(autosave_t *handle)
{
   if (!handle)
      return;

   slock_lock(handle->cond_lock);
   handle->quit = true;
   slock_unlock(handle->cond_lock);
   scond_signal(handle->cond);
   sthread_join(handle->thread);

   slock_free(handle->lock);
   slock_free(handle->cond_lock);
   scond_free(handle->cond);

   free(handle->buffer);
   free(handle);
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
      if (autosave_state.list[i])
         slock_lock(autosave_state.list[i]->lock);
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
      if (autosave_state.list[i])
         slock_unlock(autosave_state.list[i]->lock);
   }
}

void autosave_init(void)
{
   unsigned i;
   autosave_t **list    = NULL;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (settings->autosave_interval < 1 || !global->savefiles)
      return;

   list = (autosave_t**)calloc(global->savefiles->size,
               sizeof(*autosave_state.list));
   if (!list)
      return;

   autosave_state.list = list;
   autosave_state.num  = global->savefiles->size;

   for (i = 0; i < global->savefiles->size; i++)
   {
      retro_ctx_memory_info_t mem_info;
      const char *path = global->savefiles->elems[i].data;
      unsigned    type = global->savefiles->elems[i].attr.i;

      mem_info.id = type;

      core_get_memory(&mem_info);

      if (mem_info.size <= 0)
         continue;

      autosave_state.list[i] = autosave_new(path,
            mem_info.data,
            mem_info.size,
            settings->autosave_interval);

      if (!autosave_state.list[i])
         RARCH_WARN("%s\n", msg_hash_to_str(MSG_AUTOSAVE_FAILED));
   }
}

void autosave_deinit(void)
{
   unsigned i;

   for (i = 0; i < autosave_state.num; i++)
      autosave_free(autosave_state.list[i]);

   if (autosave_state.list)
      free(autosave_state.list);

   autosave_state.list     = NULL;
   autosave_state.num      = 0;
}
#endif

/**
 * content_load_ram_file:
 * @path             : path of RAM state that will be loaded from.
 * @type             : type of memory
 *
 * Load a RAM state from disk to memory.
 */
bool content_load_ram_file(unsigned slot)
{
   ssize_t rc;
   ram_type_t ram;
   retro_ctx_memory_info_t mem_info;
   global_t *global = global_get_ptr();
   void *buf        = NULL;

   ram.path = global->savefiles->elems[slot].data;
   ram.type = global->savefiles->elems[slot].attr.i;

   mem_info.id  = ram.type;

   core_get_memory(&mem_info);

   if (mem_info.size == 0 || !mem_info.data)
      return false;

   if (!filestream_read_file(ram.path, &buf, &rc))
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
 * content_save_ram_file:
 * @path             : path of RAM state that shall be written to.
 * @type             : type of memory
 *
 * Save a RAM state from memory to disk.
 *
 */
bool content_save_ram_file(unsigned slot)
{
   ram_type_t ram;
   retro_ctx_memory_info_t mem_info;
   global_t  *global         = global_get_ptr();

   if (!global)
      return false;

   ram.type                  = global->savefiles->elems[slot].attr.i;
   ram.path                  = global->savefiles->elems[slot].data;

   mem_info.id               = ram.type;

   core_get_memory(&mem_info);

   if (!mem_info.data || mem_info.size == 0)
      return false;

   RARCH_LOG("%s #%u %s \"%s\".\n",
         msg_hash_to_str(MSG_SAVING_RAM_TYPE),
         ram.type,
         msg_hash_to_str(MSG_TO),
         ram.path);

   if (!filestream_write_file(ram.path, mem_info.data, mem_info.size))
   {
      RARCH_ERR("%s.\n",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_SRAM));
      RARCH_WARN("Attempting to recover ...\n");

      /* In case the file could not be written to, 
       * the fallback function 'dump_to_file_desperate'
       * will be called. */
      if (!dump_to_file_desperate(mem_info.data, mem_info.size, ram.type))
      {
         RARCH_WARN("Failed ... Cannot recover save file.\n");
      }
      return false;
   }

   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_SAVED_SUCCESSFULLY_TO),
         ram.path);

   return true;
}
