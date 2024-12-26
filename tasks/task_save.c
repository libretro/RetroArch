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

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <lists/string_list.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/rzip_stream.h>
#include <rthreads/rthreads.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <time/rtime.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#include "../content.h"
#include "../core.h"
#include "../core_info.h"
#include "../file_path_special.h"
#include "../configuration.h"
#include "../gfx/video_driver.h"
#include "../msg_hash.h"
#include "../runloop.h"
#include "../verbosity.h"
#include "tasks_internal.h"
#ifdef HAVE_CHEATS
#include "../cheat_manager.h"
#endif

#ifdef EMSCRIPTEN
/* Filesystem is in-memory anyway, use huge chunks since each
   read/write is a possible suspend to JS code */
#define SAVE_STATE_CHUNK 4096 * 4096
#elif defined(HAVE_LIBNX) || defined(_3DS)
#define SAVE_STATE_CHUNK 4096 * 10
#else
#define SAVE_STATE_CHUNK 4096
#endif

#define RASTATE_VERSION 1
#define RASTATE_MEM_BLOCK "MEM "
#define RASTATE_CHEEVOS_BLOCK "ACHV"
#define RASTATE_REPLAY_BLOCK "RPLY"
#define RASTATE_END_BLOCK "END "

struct ram_type
{
   const char *path;
   int type;
};

struct save_state_buf
{
   void* data;
   size_t size;
   char path[PATH_MAX_LENGTH];
};

struct ram_save_state_buf
{
   struct save_state_buf state_buf;
   bool to_write_file;
};

struct sram_block
{
   void *data;
   size_t size;
   unsigned type;
};

enum save_task_state_flags
{
   SAVE_TASK_FLAG_LOAD_TO_BACKUP_BUFF   = (1 << 0),
   SAVE_TASK_FLAG_AUTOLOAD              = (1 << 1),
   SAVE_TASK_FLAG_AUTOSAVE              = (1 << 2),
   SAVE_TASK_FLAG_UNDO_SAVE             = (1 << 3),
   SAVE_TASK_FLAG_MUTE                  = (1 << 4),
   SAVE_TASK_FLAG_THUMBNAIL_ENABLE      = (1 << 5),
   SAVE_TASK_FLAG_HAS_VALID_FB          = (1 << 6),
   SAVE_TASK_FLAG_COMPRESS_FILES        = (1 << 7)
};

typedef struct
{
   intfstream_t *file;
   void *data;
   void *undo_data;
   ssize_t size;
   ssize_t undo_size;
   ssize_t written;
   ssize_t bytes_read;
   int state_slot;
   uint8_t flags;
   char path[PATH_MAX_LENGTH];
} save_task_state_t;

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
#endif

typedef save_task_state_t load_task_data_t;

/* Holds the previous saved state
 * Can be restored to disk with undo_save_state(). */
static struct save_state_buf undo_save_buf;

/* Holds the data from before a load_state() operation
 * Can be restored with undo_load_state(). */
static struct save_state_buf undo_load_buf;

/* Buffer that stores state instead of file.
 * This is useful for devices with slow I/O. */
static struct ram_save_state_buf ram_buf;

#ifdef HAVE_THREADS
static struct autosave_st autosave_state;
#endif

static bool save_state_in_background       = false;
static struct string_list *task_save_files = NULL;

typedef struct rastate_size_info
{
   size_t total_size;
   size_t coremem_size;
#ifdef HAVE_CHEEVOS
   size_t cheevos_size;
#endif
#ifdef HAVE_BSV_MOVIE
   size_t replay_size;
#endif
} rastate_size_info_t;

#ifdef HAVE_THREADS
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
 * @size            : size of @data buffer
 * @interval        : interval at which saves should be performed.
 *
 * Create and initialize autosave object.
 *
 * @return Pointer to new autosave_t object if successful, otherwise
 * NULL.
 **/
static autosave_t *autosave_new(const char *path,
      const void *data, size_t size,
      unsigned interval, bool compress)
{
   void       *buf               = NULL;
   autosave_t *handle            = (autosave_t*)malloc(sizeof(*handle));
   if (!handle)
      return NULL;

   handle->flags                 = 0;
   handle->bufsize               = size;
   handle->interval              = interval;
   if (compress)
      handle->flags             |= AUTOSAVE_FLAG_COMPRESS_FILES;
   handle->retro_buffer          = data;
   handle->path                  = path;

   if (!(buf = malloc(size)))
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

bool autosave_init(void)
{
   unsigned i;
   autosave_t **list          = NULL;
   settings_t *settings       = config_get_ptr();
   unsigned autosave_interval = settings->uints.autosave_interval;
#if defined(HAVE_ZLIB)
   bool compress_files        = settings->bools.save_file_compression;
#else
   bool compress_files        = false;
#endif

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

/**
 * undo_load_state:
 * Revert to the state before a state was loaded.
 *
 * Returns: true if successful, false otherwise.
 **/
bool content_undo_load_state(void)
{
   unsigned i;
   size_t temp_data_size;
   bool ret                  = false;
   unsigned num_blocks       = 0;
   void* temp_data           = NULL;
   struct sram_block *blocks = NULL;

   if (!core_info_current_supports_savestate())
   {
      RARCH_LOG("[State]: %s\n",
            msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES));
      return false;
   }

   RARCH_LOG("[State]: %s \"%s\", %u %s.\n",
         msg_hash_to_str(MSG_LOADING_STATE),
         undo_load_buf.path,
         (unsigned)undo_load_buf.size,
         msg_hash_to_str(MSG_BYTES));

   /* TODO/FIXME - This checking of SRAM overwrite,
    * the backing up of it and
    * its flushing could all be in their
    * own functions... */
   if (     config_get_ptr()->bools.block_sram_overwrite
         && task_save_files
         && task_save_files->size)
   {
      RARCH_LOG("[SRAM]: %s.\n",
            msg_hash_to_str(MSG_BLOCKING_SRAM_OVERWRITE));

      if ((blocks = (struct sram_block*)
         calloc(task_save_files->size, sizeof(*blocks))))
      {
         num_blocks = (unsigned)task_save_files->size;
         for (i = 0; i < num_blocks; i++)
            blocks[i].type = task_save_files->elems[i].attr.i;
      }
   }

   for (i = 0; i < num_blocks; i++)
   {
      retro_ctx_memory_info_t    mem_info;

      mem_info.id = blocks[i].type;
      core_get_memory(&mem_info);

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

         core_get_memory(&mem_info);

         if ((ptr = mem_info.data))
            memcpy(blocks[i].data, ptr, blocks[i].size);
      }
   }

   /* We need to make a temporary copy of the buffer, to allow the swap below */
   temp_data              = malloc(undo_load_buf.size);
   temp_data_size         = undo_load_buf.size;
   memcpy(temp_data, undo_load_buf.data, undo_load_buf.size);

   /* Swap the current state with the backup state. This way, we can undo
   what we're undoing */
   content_save_state("RAM", false, false);

   ret = content_deserialize_state(temp_data, temp_data_size);

   /* Clean up the temporary copy */
   free(temp_data);
   temp_data              = NULL;

    /* Flush back. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         retro_ctx_memory_info_t    mem_info;
         void *ptr   = NULL;

         mem_info.id = blocks[i].type;

         core_get_memory(&mem_info);

         if ((ptr = mem_info.data))
            memcpy(ptr, blocks[i].data, blocks[i].size);
      }
   }

   for (i = 0; i < num_blocks; i++)
   {
      free(blocks[i].data);
      blocks[i].data = NULL;
   }
   free(blocks);

   if (!ret)
   {
      RARCH_ERR("[State]: %s \"%s\".\n",
         msg_hash_to_str(MSG_FAILED_TO_UNDO_LOAD_STATE),
         undo_load_buf.path);
      return false;
   }

   return true;
}

static void undo_save_state_cb(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   save_task_state_t *state = (save_task_state_t*)task_data;

   /* Wipe the save file buffer as it's intended to be one use only */
   undo_save_buf.path[0] = '\0';
   undo_save_buf.size    = 0;
   if (undo_save_buf.data)
   {
      free(undo_save_buf.data);
      undo_save_buf.data = NULL;
   }

   free(state);
}

/**
 * task_save_handler_finished:
 * @task : the task to finish
 * @state : the state associated with this task
 *
 * Close the save state file and finish the task.
 **/
static void task_save_handler_finished(retro_task_t *task,
      save_task_state_t *state)
{
   save_task_state_t *task_data = NULL;

   task_set_finished(task, true);

   intfstream_close(state->file);
   free(state->file);

   if (!task_get_error(task) && task_get_cancelled(task))
      task_set_error(task, strdup("Task canceled"));

   task_data = (save_task_state_t*)calloc(1, sizeof(*task_data));
   memcpy(task_data, state, sizeof(*state));

   task_set_data(task, task_data);

   if (state->data)
   {
      if (     (state->flags & SAVE_TASK_FLAG_UNDO_SAVE)
            && (state->data == undo_save_buf.data))
         undo_save_buf.data = NULL;
      free(state->data);
      state->data = NULL;
   }

   free(state);
}

/* Align to 8-byte boundary */
#define CONTENT_ALIGN_SIZE(size) ((((size) + 7) & ~7))

static size_t content_get_rastate_size(rastate_size_info_t* size, bool rewind)
{
   size_t info_size = core_serialize_size();
   if (!info_size)
      return 0;
   size->coremem_size = info_size;
   /* 8-byte identifier, 8-byte block header, content, 8-byte terminator */
   size->total_size   = 8 + 8 + CONTENT_ALIGN_SIZE(info_size) + 8;
#ifdef HAVE_CHEEVOS
   /* 8-byte block header + content */
   if ((size->cheevos_size = rcheevos_get_serialize_size()) > 0)
      size->total_size += 8 + CONTENT_ALIGN_SIZE(size->cheevos_size);
#endif
#ifdef HAVE_BSV_MOVIE
   /* 8-byte block header + content */
   if(!rewind)
   {
      size->replay_size = replay_get_serialize_size();
      if(size->replay_size > 0)
         size->total_size += 8 + CONTENT_ALIGN_SIZE(size->replay_size);
   }
   else
      size->replay_size = 0;
#endif
   return size->total_size;
}

size_t content_get_serialized_size(void)
{
   rastate_size_info_t size;
   return content_get_rastate_size(&size, false);
}
size_t content_get_serialized_size_rewind(void)
{
   rastate_size_info_t size;
   return content_get_rastate_size(&size, true);
}

static void content_write_block_header(unsigned char* output, const char* header, size_t size)
{
   memcpy(output, header, 4);
   output[4] = ((size) & 0xFF);
   output[5] = ((size >> 8) & 0xFF);
   output[6] = ((size >> 16) & 0xFF);
   output[7] = ((size >> 24) & 0xFF);
}

static bool content_write_serialized_state(void* buffer,
                                           rastate_size_info_t* size,
                                           bool rewind)
{
   retro_ctx_serialize_info_t serial_info;
   unsigned char* output = (unsigned char*)buffer;

   /* 8-byte identifier "RASTATE1" where 1 is the version */
   memcpy(output, "RASTATE", 7);
   output[7] = RASTATE_VERSION;
   output   += 8;
  /* Replay block---this has to come before the mem block since its
     contents may prevent the state from loading (e.g., if it's
     incompatible with the current recording). */
#ifdef HAVE_BSV_MOVIE
    {
       input_driver_state_t *input_st = input_state_get_ptr();
#ifdef HAVE_REWIND
       bool frame_is_reversed         = state_manager_frame_is_reversed();
#else
       bool frame_is_reversed         = false;
#endif
       if (    !rewind
             && input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_RECORDING | BSV_FLAG_MOVIE_PLAYBACK)
             && !frame_is_reversed)
       {
          content_write_block_header(output,
             RASTATE_REPLAY_BLOCK, size->replay_size);
          if (replay_get_serialized_data(output + 8))
            output += CONTENT_ALIGN_SIZE(size->replay_size) + 8;
       }
    }
#endif

   /* important - write the unaligned size - some cores fail if they aren't passed the exact right size. */
   content_write_block_header(output, RASTATE_MEM_BLOCK, size->coremem_size);
   output += 8;

   /* important - pass the unaligned size to the core. some fail if it isn't exactly what they're expecting. */
   serial_info.size = size->coremem_size;
   serial_info.data = (void*)output;
   if (!core_serialize(&serial_info))
      return false;

   output += CONTENT_ALIGN_SIZE(size->coremem_size);

#ifdef HAVE_CHEEVOS
   if (size->cheevos_size)
   {
      content_write_block_header(output,
            RASTATE_CHEEVOS_BLOCK, size->cheevos_size);
      if (rcheevos_get_serialized_data(output + 8))
         output += CONTENT_ALIGN_SIZE(size->cheevos_size) + 8;
   }
#endif

   content_write_block_header(output, RASTATE_END_BLOCK, 0);

   return true;
}

bool content_serialize_state_rewind(void* buffer, size_t buffer_size)
{
   rastate_size_info_t size;
   size_t len = content_get_rastate_size(&size, true);
   if (len == 0 || len > buffer_size)
      return false;
   return content_write_serialized_state(buffer, &size, true);
}

static void *content_get_serialized_data(size_t* serial_size)
{
   size_t len;
   void* data;
   rastate_size_info_t size;
   if ((len = content_get_rastate_size(&size, false)) == 0)
      return NULL;

   /* Ensure buffer is initialised to zero
    * > Prevents inconsistent compressed state file
    *   sizes when core requests a larger buffer
    *   than it needs (and leaves the excess
    *   as uninitialised garbage) */
   if (!(data = calloc(len, 1)))
      return NULL;

   if (!content_write_serialized_state(data, &size, false))
   {
      free(data);
      return NULL;
   }

   *serial_size = size.total_size;
   return data;
}

/**
 * task_save_handler:
 * @task : the task being worked on
 *
 * Write a chunk of data to the save state file.
 **/
static void task_save_handler(retro_task_t *task)
{
   ssize_t remaining;
   int written              = 0;
   save_task_state_t *state = (save_task_state_t*)task->state;

   if (!state->file)
   {
      if (state->flags & SAVE_TASK_FLAG_COMPRESS_FILES)
         state->file   = intfstream_open_rzip_file(
               state->path, RETRO_VFS_FILE_ACCESS_WRITE);
      else
         state->file   = intfstream_open_file(
               state->path, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!state->file)
         return;
   }

   if (!state->data)
   {
      size_t size = 0;
      state->data = content_get_serialized_data(&size);
      state->size = (ssize_t)size;
   }

   remaining       = MIN(state->size - state->written, SAVE_STATE_CHUNK);

   if (state->data)
   {
      written         = (int)intfstream_write(state->file,
         (uint8_t*)state->data + state->written, remaining);
      state->written += written;
   }

   task_set_progress(task, (state->written / (float)state->size) * 100);

   if (task_get_cancelled(task) || written != remaining)
   {
      size_t err_size = 8192 * sizeof(char);
      char *err       = (char*)malloc(err_size);

      if (state->flags & SAVE_TASK_FLAG_UNDO_SAVE)
      {
         const char *failed_undo_str = msg_hash_to_str(
               MSG_FAILED_TO_UNDO_SAVE_STATE);
         RARCH_ERR("[State]: %s \"%s\".\n", failed_undo_str,
               undo_save_buf.path);
         err[0]      = '\0';
         snprintf(err, err_size - 1, "%s \"RAM\".", failed_undo_str);
      }
      else
      {
         size_t _len = strlcpy(err,
               msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
               err_size - 1);
         err[  _len] = ' ';
         err[++_len] = '\0';
         strlcat(err, state->path, err_size - 1);
      }

      task_set_error(task, strdup(err));
      free(err);
      task_save_handler_finished(task, state);
      return;
   }

   if (state->written == state->size)
   {
      char       *msg      = NULL;

      task_free_title(task);

      if (state->flags & SAVE_TASK_FLAG_UNDO_SAVE)
         msg = strdup(msg_hash_to_str(MSG_RESTORED_OLD_SAVE_STATE));
      else if (state->state_slot < 0)
         msg = strdup(msg_hash_to_str(MSG_SAVED_STATE_TO_SLOT_AUTO));
      else
      {
         char new_msg[128];
         new_msg[0] = '\0';

         snprintf(new_msg, sizeof(new_msg),
               msg_hash_to_str(MSG_SAVED_STATE_TO_SLOT),
               state->state_slot);
         msg = strdup(new_msg);
      }

      if (!task_get_mute(task) && msg)
      {
         task_set_title(task, msg);
         msg = NULL;
      }

      task_save_handler_finished(task, state);

      if (!string_is_empty(msg))
         free(msg);
   }
}

/**
 * task_push_undo_save_state:
 * @path : file path of the save state
 * @data : the save state data to write
 * @size : the total size of the save state
 *
 * Create a new task to undo the last save of the content state.
 **/
static bool task_push_undo_save_state(const char *path, void *data, size_t size)
{
   settings_t     *settings;
   retro_task_t       *task      = task_init();
   video_driver_state_t *video_st= video_state_get_ptr();
   save_task_state_t *state      = (save_task_state_t*)
      calloc(1, sizeof(*state));

   if (!task || !state)
      goto error;

   settings                      = config_get_ptr();

   strlcpy(state->path, path, sizeof(state->path));
   state->data                   = data;
   state->size                   = size;
   state->flags                 |= SAVE_TASK_FLAG_UNDO_SAVE;
   state->state_slot             = settings->ints.state_slot;
   if (video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID))
      state->flags              |= SAVE_TASK_FLAG_HAS_VALID_FB;
#if defined(HAVE_ZLIB)
   if (settings->bools.savestate_file_compression)
      state->flags              |= SAVE_TASK_FLAG_COMPRESS_FILES;
#endif
   if (!settings->bools.notification_show_save_state)
      state->flags              |= SAVE_TASK_FLAG_MUTE;

   task->type                    = TASK_TYPE_BLOCKING;
   task->state                   = state;
   task->handler                 = task_save_handler;
   task->callback                = undo_save_state_cb;
   task->title                   = strdup(msg_hash_to_str(MSG_UNDOING_SAVE_STATE));
   task->mute                    = (state->flags & SAVE_TASK_FLAG_MUTE) ? true : false;

   task_queue_push(task);

   return true;

error:
   if (data)
      free(data);
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}

/**
 * undo_save_state:
 * Reverts the last save operation
 *
 * Returns: true if successful, false otherwise.
 **/
bool content_undo_save_state(void)
{
   if (core_info_current_supports_savestate())
      return task_push_undo_save_state(
            undo_save_buf.path,
            undo_save_buf.data,
            undo_save_buf.size);
   RARCH_LOG("[State]: %s\n",
         msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES));
   return false;
}

/**
 * task_load_handler_finished:
 * @task : the task to finish
 * @state : the state associated with this task
 *
 * Close the loaded state file and finish the task.
 **/
static void task_load_handler_finished(retro_task_t *task,
      save_task_state_t *state)
{
   load_task_data_t *task_data = NULL;

   task_set_finished(task, true);

   if (state->file)
   {
      intfstream_close(state->file);
      free(state->file);
   }

   if (!task_get_error(task) && task_get_cancelled(task))
      task_set_error(task, strdup("Task canceled"));

   if (!(task_data = (load_task_data_t*)calloc(1, sizeof(*task_data))))
      return;

   memcpy(task_data, state, sizeof(*task_data));

   task_set_data(task, task_data);

   free(state);
}

/**
 * task_load_handler:
 * @task : the task being worked on
 *
 * Load a chunk of data from the save state file.
 **/
static void task_load_handler(retro_task_t *task)
{
   ssize_t remaining, bytes_read;
   save_task_state_t *state = (save_task_state_t*)task->state;

   if (!state->file)
   {
#if defined(HAVE_ZLIB)
      /* Always use RZIP interface when reading state
       * files - this will automatically handle uncompressed
       * data */
      if (!(state->file = intfstream_open_rzip_file(state->path,
                  RETRO_VFS_FILE_ACCESS_READ)))
         goto not_found;
#else
      if (!(state->file = intfstream_open_file(state->path,
                  RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         goto not_found;
#endif

      if ((state->size = intfstream_get_size(state->file)) < 0)
         goto end;

      if (!(state->data = malloc(state->size + 1)))
         goto end;
   }

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      task_set_cancelled(task, true);
#endif

   remaining          = MIN(state->size - state->bytes_read, SAVE_STATE_CHUNK);
   bytes_read         = intfstream_read(state->file,
         (uint8_t*)state->data + state->bytes_read, remaining);
   state->bytes_read += bytes_read;

   if (state->size > 0)
      task_set_progress(task, (state->bytes_read / (float)state->size) * 100);

   if (task_get_cancelled(task) || bytes_read != remaining)
   {
      if (state->flags & SAVE_TASK_FLAG_AUTOLOAD)
      {
         char *msg = (char*)malloc(8192 * sizeof(char));

         msg[0] = '\0';

         snprintf(msg,
               8192 * sizeof(char),
               msg_hash_to_str(MSG_AUTOLOADING_SAVESTATE_FAILED),
               path_basename(state->path));
         task_set_error(task, strdup(msg));
         free(msg);
      }
      else
         task_set_error(task, strdup(msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE)));

      free(state->data);
      state->data = NULL;
      task_load_handler_finished(task, state);
      return;
   }

   if (state->bytes_read == state->size)
   {
      task_free_title(task);

      if (!task_get_mute(task))
      {
         size_t msg_size   = 8192 * sizeof(char);
         char *msg         = (char*)malloc(msg_size);

         msg[0]            = '\0';

         if (state->flags & SAVE_TASK_FLAG_AUTOLOAD)
         {
            snprintf(msg,
                  msg_size - 1,
                  msg_hash_to_str(MSG_AUTOLOADING_SAVESTATE_SUCCEEDED),
                  path_basename(state->path));
         }
         else
         {
            if (state->state_slot < 0)
               strlcpy(msg,
                     msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT_AUTO),
                     msg_size - 1);
            else
               snprintf(msg, msg_size - 1,
                     msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT),
                     state->state_slot);
         }

         task_set_title(task, strdup(msg));
         free(msg);
      }

      goto end;
   }

   return;

not_found:
   {
      size_t msg_size   = 8192 * sizeof(char);
      char *msg         = (char*)malloc(msg_size);

      msg[0] = '\0';

      snprintf(msg,
            msg_size - 1,
            "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
            path_basename(state->path));

      task_set_title(task, strdup(msg));
      free(msg);
   }

end:
   task_load_handler_finished(task, state);
}

static bool content_load_rastate1(unsigned char* input, size_t size)
{
   unsigned char *stop = input + size;
   bool seen_core      = false;
#ifdef HAVE_CHEEVOS
   bool seen_cheevos   = false;
#endif
#ifdef HAVE_BSV_MOVIE
   bool seen_replay = false;
#endif

   input += 8;

   while (input < stop)
   {
      size_t     block_size = ( input[7] << 24
            | input[6] << 16 |  input[5] << 8 | input[4]);
      unsigned char *marker = input;

      input += 8;

      if (memcmp(marker, RASTATE_MEM_BLOCK, 4) == 0)
      {
         retro_ctx_serialize_info_t serial_info;
         serial_info.data_const = (void*)input;
         serial_info.size       = block_size;
#ifdef HAVE_BSV_MOVIE
         {
            input_driver_state_t *input_st = input_state_get_ptr();
#ifdef HAVE_REWIND
            bool frame_is_reversed         = state_manager_frame_is_reversed();
#else
            bool frame_is_reversed         = false;
#endif

            if (BSV_MOVIE_IS_RECORDING() && !seen_replay && !frame_is_reversed)
            {
               /* TODO OSD message */
               RARCH_ERR("[Replay] Can't load state without replay data during recording.\n");
               return false;
            }
            if (BSV_MOVIE_IS_PLAYBACK_ON() && !seen_replay && !frame_is_reversed)
            {
               /* TODO OSD message */
               RARCH_WARN("[Replay] Loading state without replay data during replay will cancel replay.\n");
               movie_stop(input_st);
            }
         }
#endif
         if (!core_unserialize(&serial_info))
            return false;

         seen_core = true;
      }
#ifdef HAVE_CHEEVOS
      else if (memcmp(marker, RASTATE_CHEEVOS_BLOCK, 4) == 0)
      {
         if (rcheevos_set_serialized_data((void*)input))
            seen_cheevos = true;
      }
#endif
#ifdef HAVE_BSV_MOVIE
      else if (memcmp(marker, RASTATE_REPLAY_BLOCK, 4) == 0)
      {
#ifdef HAVE_REWIND
         bool frame_is_reversed         = state_manager_frame_is_reversed();
#else
         bool frame_is_reversed         = false;
#endif
         if (frame_is_reversed || replay_set_serialized_data((void*)input))
            seen_replay = true;
         else
            return false;
      }
#endif
      else if (memcmp(marker, RASTATE_END_BLOCK, 4) == 0)
         break;

      input += CONTENT_ALIGN_SIZE(block_size);
   }

   if (!seen_core) {
      RARCH_LOG("[State] no core\n");
      return false;
    }

#ifdef HAVE_CHEEVOS
   if (!seen_cheevos)
      rcheevos_set_serialized_data(NULL);
#endif
#ifdef HAVE_BSV_MOVIE
   {
#ifdef HAVE_REWIND
      bool frame_is_reversed = state_manager_frame_is_reversed();
#else
      bool frame_is_reversed = false;
#endif
      if (!seen_replay && !frame_is_reversed)
         replay_set_serialized_data(NULL);
   }
#endif

   return true;
}

bool content_deserialize_state(
      const void* serialized_data, size_t serialized_size)
{
   if (memcmp(serialized_data, "RASTATE", 7) != 0)
   {
      /* old format is just core data, load it directly */
      retro_ctx_serialize_info_t serial_info;
      serial_info.data_const = serialized_data;
      serial_info.size       = serialized_size;
      if (!core_unserialize(&serial_info))
         return false;

#ifdef HAVE_CHEEVOS
      rcheevos_set_serialized_data(NULL);
#endif
#ifdef HAVE_BSV_MOVIE
      {
#ifdef HAVE_REWIND
         bool frame_is_reversed = state_manager_frame_is_reversed();
#else
         bool frame_is_reversed = false;
#endif
         if (!frame_is_reversed)
            replay_set_serialized_data(NULL);
      }
#endif
   }
   else
   {
      unsigned char* input = (unsigned char*)serialized_data;
      switch (input[7]) /* version */
      {
         case 1:
            if (content_load_rastate1(input, serialized_size))
               break;
            /* fall-through intentional */
         default:
            return false;
      }
   }

   return true;
}

/**
 * content_load_state_cb:
 * @path      : path that state will be loaded from.
 * Load a state from disk to memory.
 *
 **/
static void content_load_state_cb(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   unsigned i;
   bool ret;
   load_task_data_t *load_data = (load_task_data_t*)task_data;
   ssize_t size                = load_data->size;
   unsigned num_blocks         = 0;
   void *buf                   = load_data->data;
   struct sram_block *blocks   = NULL;
   settings_t *settings        = config_get_ptr();
   bool block_sram_overwrite   = settings->bools.block_sram_overwrite;

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      goto error;
#endif

   RARCH_LOG("[State]: %s \"%s\", %u %s.\n",
         msg_hash_to_str(MSG_LOADING_STATE),
         load_data->path,
         (unsigned)size,
         msg_hash_to_str(MSG_BYTES));

   if (size < 0 || !buf)
      goto error;

   /* This means we're backing up the file in memory,
    * so content_undo_save_state()
    * can restore it */
   if (load_data->flags & SAVE_TASK_FLAG_LOAD_TO_BACKUP_BUFF)
   {
      /* If we were previously backing up a file, let go of it first */
      if (undo_save_buf.data)
      {
         free(undo_save_buf.data);
         undo_save_buf.data = NULL;
      }

      if (!(undo_save_buf.data = malloc(size)))
         goto error;

      memcpy(undo_save_buf.data, buf, size);
      undo_save_buf.size = size;
      strlcpy(undo_save_buf.path, load_data->path, sizeof(undo_save_buf.path));

      free(buf);
      free(load_data);
      return;
   }

   if (block_sram_overwrite && task_save_files
         && task_save_files->size)
   {
      RARCH_LOG("[SRAM]: %s.\n",
            msg_hash_to_str(MSG_BLOCKING_SRAM_OVERWRITE));

      if ((blocks = (struct sram_block*)
         calloc(task_save_files->size, sizeof(*blocks))))
      {
         num_blocks = (unsigned)task_save_files->size;
         for (i = 0; i < num_blocks; i++)
            blocks[i].type = task_save_files->elems[i].attr.i;
      }
   }

   for (i = 0; i < num_blocks; i++)
   {
      retro_ctx_memory_info_t mem_info;

      mem_info.id    = blocks[i].type;
      core_get_memory(&mem_info);

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
         retro_ctx_memory_info_t mem_info;
         const void *ptr = NULL;

         mem_info.id     = blocks[i].type;

         core_get_memory(&mem_info);

         if ((ptr = mem_info.data))
            memcpy(blocks[i].data, ptr, blocks[i].size);
      }
   }

   /* Backup the current state so we can undo this load */
   content_save_state("RAM", false, false);

   ret = content_deserialize_state(buf, size);

   /* Flush back. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         retro_ctx_memory_info_t    mem_info;
         void *ptr   = NULL;

         mem_info.id = blocks[i].type;

         core_get_memory(&mem_info);

         if ((ptr = mem_info.data))
            memcpy(ptr, blocks[i].data, blocks[i].size);
      }
   }

   for (i = 0; i < num_blocks; i++)
      free(blocks[i].data);
   free(blocks);

   if (!ret)
      goto error;

   free(buf);
   free(load_data);

   return;

error:
   RARCH_ERR("[State]: %s \"%s\".\n",
         msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
         load_data->path);
   if (buf)
      free(buf);
   free(load_data);
}

/**
 * save_state_cb:
 *
 * Called after the save state is done. Takes a screenshot if needed.
 **/
static void save_state_cb(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   save_task_state_t *state   = (save_task_state_t*)task_data;
#ifdef HAVE_SCREENSHOTS
   char               *path   = strdup(state->path);
   settings_t     *settings   = config_get_ptr();
   const char *dir_screenshot = settings->paths.directory_screenshot;

   if (state->flags & SAVE_TASK_FLAG_THUMBNAIL_ENABLE)
      take_screenshot(dir_screenshot,
            path, true,
            state->flags & SAVE_TASK_FLAG_HAS_VALID_FB, false, true);
   free(path);
#endif

   free(state);
}

/**
 * task_push_save_state:
 * @path : file path of the save state
 * @data : the save state data to write
 * @size : the total size of the save state
 *
 * Create a new task to save the content state.
 **/
static void task_push_save_state(const char *path, void *data, size_t size, bool autosave)
{
   settings_t     *settings        = config_get_ptr();
   retro_task_t       *task        = task_init();
   video_driver_state_t *video_st  = video_state_get_ptr();
   save_task_state_t *state        = (save_task_state_t*)calloc(1, sizeof(*state));

   if (!task || !state)
      goto error;

   strlcpy(state->path, path, sizeof(state->path));
   state->data                   = data;
   state->size                   = size;
   /* Don't show OSD messages if we are auto-saving */
   if (autosave)
      state->flags              |= (SAVE_TASK_FLAG_AUTOSAVE |
                                    SAVE_TASK_FLAG_MUTE);
   if (settings->bools.savestate_thumbnail_enable)
   {
      /* Delay OSD messages and widgets for a few frames
       * to prevent GPU screenshots from having notifications */
      runloop_state_t *runloop_st = runloop_state_get_ptr();
      runloop_st->msg_queue_delay = 12;
      state->flags               |= SAVE_TASK_FLAG_THUMBNAIL_ENABLE;
   }
   state->state_slot             = settings->ints.state_slot;
   if (video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID))
      state->flags              |= SAVE_TASK_FLAG_HAS_VALID_FB;
#if defined(HAVE_ZLIB)
   if (settings->bools.savestate_file_compression)
      state->flags              |= SAVE_TASK_FLAG_COMPRESS_FILES;
#endif
   if (!settings->bools.notification_show_save_state)
      state->flags              |= SAVE_TASK_FLAG_MUTE;

   task->type                    = TASK_TYPE_BLOCKING;
   task->state                   = state;
   task->handler                 = task_save_handler;
   task->callback                = save_state_cb;
   task->title                   = strdup(msg_hash_to_str(MSG_SAVING_STATE));
   task->mute                    = (state->flags & SAVE_TASK_FLAG_MUTE) ? true : false;

   if (!task_queue_push(task))
   {
      /* Another blocking task is already active. */
      if (data)
         free(data);
      if (task->title)
         task_free_title(task);
      free(task);
      free(state);
   }

   return;

error:
   if (data)
      free(data);
   if (state)
      free(state);
   if (task)
   {
      if (task->title)
         task_free_title(task);
      free(task);
   }
}

/**
 * content_load_and_save_state_cb:
 * @path      : path that state will be loaded from.
 * Load then save a state.
 *
 **/
static void content_load_and_save_state_cb(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   load_task_data_t *load_data = (load_task_data_t*)task_data;
   char                  *path = strdup(load_data->path);
   void                  *data = load_data->undo_data;
   size_t                 size = load_data->undo_size;
   bool               autosave = (load_data->flags & SAVE_TASK_FLAG_AUTOSAVE) ? true : false;

   content_load_state_cb(task, task_data, user_data, error);

   task_push_save_state(path, data, size, autosave);

   free(path);
}

/**
 * task_push_load_and_save_state:
 * @path : file path of the save state
 * @data : the save state data to write
 * @size : the total size of the save state
 * @load_to_backup_buffer : If true, the state will be loaded into undo_save_buf.
 *
 * Create a new task to load current state first into a backup buffer (for undo)
 * and then save the content state.
 **/
static void task_push_load_and_save_state(const char *path, void *data,
      size_t size, bool load_to_backup_buffer, bool autosave)
{
   retro_task_t      *task         = NULL;
   settings_t        *settings     = config_get_ptr();
   video_driver_state_t *video_st  = video_state_get_ptr();
   save_task_state_t *state        = (save_task_state_t*)
      calloc(1, sizeof(*state));

   if (!state)
      return;

   if (!(task = task_init()))
   {
      free(state);
      return;
   }


   strlcpy(state->path, path, sizeof(state->path));
   if (load_to_backup_buffer)
      state->flags              |= SAVE_TASK_FLAG_LOAD_TO_BACKUP_BUFF;
   state->undo_size              = size;
   state->undo_data              = data;
   /* Don't show OSD messages if we are auto-saving */
   if (autosave)
      state->flags              |= (SAVE_TASK_FLAG_AUTOSAVE |
                                    SAVE_TASK_FLAG_MUTE);
   if (load_to_backup_buffer)
      state->flags              |= SAVE_TASK_FLAG_MUTE;
   state->state_slot             = settings->ints.state_slot;
   if (video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID))
      state->flags              |= SAVE_TASK_FLAG_HAS_VALID_FB;
#if defined(HAVE_ZLIB)
   if (settings->bools.savestate_file_compression)
      state->flags              |= SAVE_TASK_FLAG_COMPRESS_FILES;
#endif
   if (!settings->bools.notification_show_save_state)
      state->flags              |= SAVE_TASK_FLAG_MUTE;

   task->state                   = state;
   task->type                    = TASK_TYPE_BLOCKING;
   task->handler                 = task_load_handler;
   task->callback                = content_load_and_save_state_cb;
   task->title                   = strdup(msg_hash_to_str(MSG_LOADING_STATE));
   task->mute                    = (state->flags & SAVE_TASK_FLAG_MUTE) ? true : false;

   if (!task_queue_push(task))
   {
      /* Another blocking task is already active. */
      if (data)
         free(data);
      if (task->title)
         task_free_title(task);
      free(task);
      free(state);
   }
}

/**
 * content_save_state:
 * @path      : path of saved state that shall be written to.
 * @save_to_disk: If false, saves the state onto undo_load_buf.
 * Save a state from memory to disk.
 *
 * Returns: true if successful, false otherwise.
 **/
bool content_save_state(const char *path, bool save_to_disk, bool autosave)
{
   size_t serial_size;
   void *data  = NULL;

   if (!core_info_current_supports_savestate())
   {
      RARCH_LOG("[State]: %s\n",
            msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES));
      return false;
   }

   serial_size = core_serialize_size();

   if (serial_size == 0)
      return false;

   if (!save_state_in_background)
   {
      if (!(data = content_get_serialized_data(&serial_size)))
      {
         RARCH_ERR("[State]: %s \"%s\".\n",
               msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
               path);
         return false;
      }

      RARCH_LOG("[State]: %s \"%s\", %u %s.\n",
            msg_hash_to_str(MSG_SAVING_STATE),
            path,
            (unsigned)serial_size,
            msg_hash_to_str(MSG_BYTES));
   }

   if (save_to_disk)
   {
      if (path_is_valid(path) && !autosave)
      {
         /* Before overwriting the savestate file, load it into a buffer
         to allow undo_save_state() to work */
         /* TODO/FIXME - Use msg_hash_to_str here */
         RARCH_LOG("[State]: %s ...\n",
               msg_hash_to_str(MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER));
         task_push_load_and_save_state(path, data, serial_size, true, autosave);
      }
      else
         task_push_save_state(path, data, serial_size, autosave);
   }
   else
   {
      if (!data)
      {
         if (!(data = content_get_serialized_data(&serial_size)))
         {
            RARCH_ERR("[State]: %s \"%s\".\n",
                  msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
                  path);
            return false;
         }
      }

      /* save_to_disk is false, which means we are saving the state
      in undo_load_buf to allow content_undo_load_state() to restore it */

      /* If we were holding onto an old state already, clean it up first */
      if (undo_load_buf.data)
      {
         free(undo_load_buf.data);
         undo_load_buf.data = NULL;
      }

      if (!(undo_load_buf.data = malloc(serial_size)))
      {
         free(data);
         return false;
      }

      memcpy(undo_load_buf.data, data, serial_size);
      free(data);
      undo_load_buf.size = serial_size;
      strlcpy(undo_load_buf.path, path, sizeof(undo_load_buf.path));
   }

   return true;
}

/**
 * content_ram_state_pending:
 * Check a RAM state write to disk.
 *
 * @return true if need to write, false otherwise.
 **/
bool content_ram_state_pending(void)
{
   return ram_buf.to_write_file;
}

static bool task_save_state_finder(retro_task_t *task, void *user_data)
{
   return (task && task->handler == task_save_handler);
}

/* Returns true if a save state task is in progress */
static bool content_save_state_in_progress(void* data)
{
   task_finder_data_t find_data;

   find_data.func     = task_save_state_finder;
   find_data.userdata = data;

   return task_queue_find(&find_data);
}

void content_wait_for_save_state_task(void)
{
   task_queue_wait(content_save_state_in_progress, NULL);
}


static bool task_load_state_finder(retro_task_t *task, void *user_data)
{
   return (task && task->handler == task_load_handler);
}

/* Returns true if a load state task is in progress */
bool content_load_state_in_progress(void* data)
{
   task_finder_data_t find_data;

   find_data.func     = task_load_state_finder;
   find_data.userdata = data;

   return task_queue_find(&find_data);
}

void content_wait_for_load_state_task(void)
{
   task_queue_wait(content_load_state_in_progress, NULL);
}

/**
 * content_load_state:
 * @path                  : path that state will be loaded from.
 * @load_to_backup_buffer : If true, state will be loaded into undo_save_buf.
 * Load a state from disk to memory.
 *
 * @return true if successful, false otherwise.
 **/
bool content_load_state(const char *path,
      bool load_to_backup_buffer, bool autoload)
{
   retro_task_t       *task        = NULL;
   save_task_state_t *state        = NULL;
   video_driver_state_t *video_st  = video_state_get_ptr();
   settings_t *settings            = config_get_ptr();

   if (!core_info_current_supports_savestate())
   {
      RARCH_LOG("[State]: %s\n",
            msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES));
      goto error;
   }

   task  = task_init();
   state = (save_task_state_t*)calloc(1, sizeof(*state));

   if (!task || !state)
      goto error;

   strlcpy(state->path, path, sizeof(state->path));
   if (load_to_backup_buffer)
      state->flags             |= SAVE_TASK_FLAG_LOAD_TO_BACKUP_BUFF;
   if (autoload)
      state->flags             |= SAVE_TASK_FLAG_AUTOLOAD;
   state->state_slot            = settings->ints.state_slot;
   if (video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID))
      state->flags             |= SAVE_TASK_FLAG_HAS_VALID_FB;
#if defined(HAVE_ZLIB)
   if (settings->bools.savestate_file_compression)
      state->flags             |= SAVE_TASK_FLAG_COMPRESS_FILES;
#endif
   if (!settings->bools.notification_show_save_state)
      state->flags             |= SAVE_TASK_FLAG_MUTE;

   task->type                   = TASK_TYPE_BLOCKING;
   task->state                  = state;
   task->handler                = task_load_handler;
   task->callback               = content_load_state_cb;
   task->title                  = strdup(msg_hash_to_str(MSG_LOADING_STATE));
   task->mute                   = (state->flags & SAVE_TASK_FLAG_MUTE) ? true : false;

   task_queue_push(task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}

bool content_rename_state(const char *origin, const char *dest)
{
   if (filestream_exists(dest))
      filestream_delete(dest);

   if (!filestream_rename(origin, dest))
      return true;

   RARCH_ERR("[State]: Error renaming file \"%s\".\n", origin);
   return false;
}

/*
*
* TODO/FIXME: Figure out when and where this should be called.
* As it is, when e.g. closing Gambatte, we get the
* same printf message 4 times.
*/
void content_reset_savestate_backups(void)
{
   if (undo_save_buf.data)
   {
      free(undo_save_buf.data);
      undo_save_buf.data = NULL;
   }

   undo_save_buf.path[0] = '\0';
   undo_save_buf.size    = 0;

   if (undo_load_buf.data)
   {
      free(undo_load_buf.data);
      undo_load_buf.data = NULL;
   }

   undo_load_buf.path[0] = '\0';
   undo_load_buf.size    = 0;

   if (ram_buf.state_buf.data)
   {
      free(ram_buf.state_buf.data);
      ram_buf.state_buf.data = NULL;
   }

   ram_buf.state_buf.path[0] = '\0';
   ram_buf.state_buf.size    = 0;
   ram_buf.to_write_file     = false;
}

bool content_undo_load_buf_is_empty(void)
{
   return undo_load_buf.data == NULL || undo_load_buf.size == 0;
}

bool content_undo_save_buf_is_empty(void)
{
   return undo_save_buf.data == NULL || undo_save_buf.size == 0;
}

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
bool content_load_ram_file(unsigned slot)
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
      size_t size, unsigned type)
{
   char path[PATH_MAX_LENGTH + 256 + 32];
   path            [0]    = '\0';

   if (fill_pathname_application_data(path,
            sizeof(path)))
   {
      size_t _len;
      time_t time_;
      struct tm tm_;
      char timebuf[256];
      timebuf         [0] = '\0';
      time(&time_);

      rtime_localtime(&time_, &tm_);

      strftime(timebuf, 256 * sizeof(char),
            "%Y-%m-%d-%H-%M-%S", &tm_);

      _len = strlcat(path, "/RetroArch-recovery-", sizeof(path));

      snprintf(path + _len, sizeof(path) - _len,
            "%u%s", type, timebuf);

      /* Fallback (emergency) saves are always
       * uncompressed
       * > If a regular save fails, then the host
       *   system is experiencing serious technical
       *   difficulties (most likely some kind of
       *   hardware failure)
       * > In this case, we don't want to further
       *   complicate matters by introducing zlib
       *   compression overheads */
      if (filestream_write_file(path, data, size))
      {
         RARCH_WARN("[SRAM]: Succeeded in saving RAM data to \"%s\".\n", path);
         return true;
      }
   }

   return false;
}

/**
 * content_load_state_from_ram:
 * Load a state from RAM.
 *
 * @return true if successful, false otherwise.
 **/
bool content_load_state_from_ram(void)
{
   size_t temp_data_size;
   bool ret        = false;
   void* temp_data = NULL;

   if (!core_info_current_supports_savestate())
   {
      RARCH_LOG("[State]: %s\n",
            msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES));
      return false;
   }

   if (!ram_buf.state_buf.data)
      return false;

   RARCH_LOG("[State]: %s, %u %s.\n",
         msg_hash_to_str(MSG_LOADING_STATE),
         (unsigned)ram_buf.state_buf.size,
         msg_hash_to_str(MSG_BYTES));

   /* We need to make a temporary copy of the buffer, to allow the swap below */
   temp_data       = malloc(ram_buf.state_buf.size);
   temp_data_size  = ram_buf.state_buf.size;
   memcpy(temp_data, ram_buf.state_buf.data, ram_buf.state_buf.size);

   /* Swap the current state with the backup state. This way, we can undo
   what we're undoing */
   content_save_state("RAM", false, false);

   ret             = content_deserialize_state(temp_data, temp_data_size);

   /* Clean up the temporary copy */
   free(temp_data);
   temp_data       = NULL;

   if (!ret)
   {
      RARCH_ERR("[State]: %s.\n",
         msg_hash_to_str(MSG_FAILED_TO_LOAD_SRAM));
      return false;
   }

   return true;
}

/**
 * content_save_state_from_ram:
 * Save a state to RAM.
 *
 * @return true if successful, false otherwise.
 **/
bool content_save_state_to_ram(void)
{
   void *data  = NULL;
   size_t serial_size;

   if (!core_info_current_supports_savestate())
   {
      RARCH_LOG("[State]: %s\n",
            msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES));
      return false;
   }

   serial_size = core_serialize_size();

   if (serial_size == 0)
      return false;

   if (!save_state_in_background)
   {
      if (!(data = content_get_serialized_data(&serial_size)))
      {
         RARCH_ERR("[State]: %s.\n",
               msg_hash_to_str(MSG_FAILED_TO_SAVE_SRAM));
         return false;
      }

      RARCH_LOG("[State]: %s, %u %s.\n",
            msg_hash_to_str(MSG_SAVING_STATE),
            (unsigned)serial_size,
            msg_hash_to_str(MSG_BYTES));
   }

   if (!data)
   {
      if (!(data = content_get_serialized_data(&serial_size)))
      {
         RARCH_ERR("[State]: %s.\n",
               msg_hash_to_str(MSG_FAILED_TO_SAVE_SRAM));
         return false;
      }
   }

   /* If we were holding onto an old state already, clean it up first */
   if (ram_buf.state_buf.data)
   {
      free(ram_buf.state_buf.data);
      ram_buf.state_buf.data = NULL;
   }

   if (!(ram_buf.state_buf.data = malloc(serial_size)))
   {
      free(data);
      return false;
   }

   memcpy(ram_buf.state_buf.data, data, serial_size);
   free(data);
   ram_buf.state_buf.size = serial_size;
   ram_buf.to_write_file  = true;

   return true;
}

/**
 * content_ram_state_to_file:
 * @path             : path of RAM state that shall be written to.
 *
 * Save a RAM state from memory to disk.
 *
 * @return true if successful, false otherwise.
 **/
bool content_ram_state_to_file(const char *path)
{
   if (     path
         && ram_buf.state_buf.data
         && ram_buf.to_write_file)
   {
#if defined(HAVE_ZLIB)
      settings_t *settings = config_get_ptr();
      if (settings->bools.save_file_compression)
      {
         if (rzipstream_write_file(
               path, ram_buf.state_buf.data, ram_buf.state_buf.size))
            goto success;
      }
      else
#endif
      {
         if (filestream_write_file(
               path, ram_buf.state_buf.data, ram_buf.state_buf.size))
            goto success;
      }
   }

   return false;

success:
   ram_buf.to_write_file = false;
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
bool content_save_ram_file(unsigned slot, bool compress)
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

bool event_save_files(bool is_sram_used)
{
   unsigned i;
   settings_t *settings            = config_get_ptr();
#ifdef HAVE_CHEATS
   const char *path_cheat_database = settings->paths.path_cheat_database;
#endif
#if defined(HAVE_ZLIB)
   bool compress_files             = settings->bools.save_file_compression;
#endif

#ifdef HAVE_CHEATS
   cheat_manager_save_game_specific_cheats(
         path_cheat_database);
#endif
   if (!task_save_files || !is_sram_used)
      return false;

   for (i = 0; i < task_save_files->size; i++)
   {
#if defined(HAVE_ZLIB)
      content_save_ram_file(i, compress_files);
#else
      content_save_ram_file(i, false);
#endif
   }

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

void set_save_state_in_background(bool state)
{
   save_state_in_background = state;
}
