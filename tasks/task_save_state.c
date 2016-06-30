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

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>

#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

#include "../core.h"
#include "../configuration.h"
#include "../msg_hash.h"
#include "../runloop.h"
#include "../verbosity.h"
#include "tasks_internal.h"

struct save_state_buf
{
   void* data;
   char path[PATH_MAX_LENGTH];
   size_t size;
};

/* Holds the previous saved state
 * Can be restored to disk with undo_save_state(). */
static struct save_state_buf undo_save_buf;

/* Holds the data from before a load_state() operation
 * Can be restored with undo_load_state(). */
static struct save_state_buf undo_load_buf;

struct sram_block
{
   unsigned type;
   void *data;
   size_t size;
};

/**
 * undo_load_state:
 * Revert to the state before a state was loaded.
 *
 * Returns: true if successful, false otherwise.
 **/
bool content_undo_load_state()
{
   unsigned i;
   retro_ctx_serialize_info_t serial_info;
   size_t temp_data_size;
   void* temp_data           = NULL;
   unsigned num_blocks       = 0;
   struct sram_block *blocks = NULL;
   settings_t *settings      = config_get_ptr();
   global_t *global          = global_get_ptr();

   RARCH_LOG("%s: \"%s\".\n",
         msg_hash_to_str(MSG_LOADING_STATE),
         undo_load_buf.path);

   RARCH_LOG("%s: %u %s.\n",
         msg_hash_to_str(MSG_STATE_SIZE),
         undo_load_buf.size,
         msg_hash_to_str(MSG_BYTES));


   /* TODO/FIXME - This checking of SRAM overwrite, the backing up of it and
   its flushing could all be in their own functions... */
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

         ptr = mem_info.data;
         if (ptr)
            memcpy(blocks[i].data, ptr, blocks[i].size);
      }
   }
   
   /* We need to make a temporary copy of the buffer, to allow the swap below */
   temp_data              = malloc(undo_load_buf.size);
   temp_data_size         = undo_load_buf.size;
   memcpy(temp_data, undo_load_buf.data, undo_load_buf.size);

   serial_info.data_const = temp_data;
   serial_info.size       = temp_data_size;

   /* Swap the current state with the backup state. This way, we can undo
   what we're undoing */
   content_save_state("RAM", false);
   bool ret               = core_unserialize(&serial_info);

   /* Clean up the temporary copy */
   free(temp_data);
   temp_data              = NULL;

   /* Flush back. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         retro_ctx_memory_info_t    mem_info;
         void *ptr = NULL;

         mem_info.id = blocks[i].type;

         core_get_memory(&mem_info);

         ptr = mem_info.data;
         if (ptr)
            memcpy(ptr, blocks[i].data, blocks[i].size);
      }
   }

   for (i = 0; i < num_blocks; i++)
      free(blocks[i].data);
   free(blocks);

   if (!ret)
   {
      RARCH_ERR("%s \"%s\".\n",
         msg_hash_to_str(MSG_FAILED_TO_UNDO_LOAD_STATE),
         undo_load_buf.path);
   } 

   return ret;
}

/**
 * undo_save_state:
 * Reverts the last save operation
 *
 * Returns: true if successful, false otherwise.
 **/
bool content_undo_save_state(void)
{
   bool ret = filestream_write_file(undo_save_buf.path, undo_save_buf.data, undo_save_buf.size);

   /* Wipe the save file buffer as it's intended to be one use only */
   undo_save_buf.path[0] = '\0';
   undo_save_buf.size    = 0;
   if (undo_save_buf.data)
   {
      free(undo_save_buf.data);
      undo_save_buf.data = NULL;
   }

   if (!ret)
   {
      RARCH_ERR("%s \"%s\".\n",
         msg_hash_to_str(MSG_FAILED_TO_UNDO_SAVE_STATE),
         undo_save_buf.path);
   }

   return ret;
}


/* TODO/FIXME - turn this into actual task */

/**
 * save_state:
 * @path      : path of saved state that shall be written to.
 * @save_to_disk: If false, saves the state onto undo_load_buf.
 * Save a state from memory to disk.
 *
 * Returns: true if successful, false otherwise.
 **/
bool content_save_state(const char *path, bool save_to_disk)
{
   retro_ctx_serialize_info_t serial_info;
   retro_ctx_size_info_t info;
   bool ret    = false;
   void *data  = NULL;

   core_serialize_size(&info);

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
   ret              = core_serialize(&serial_info);

   if (ret)
   {
      if (save_to_disk)
      {
         if (path_file_exists(path))
         {
            /* Before overwritting the savestate file, load it into a buffer
            to allow undo_save_state() to work */
            /* TODO/FIXME - Use msg_hash_to_str here */
            RARCH_LOG("%s\n",
               "File already exists. Saving to backup buffer...");

            content_load_state(path, true);
         }

         ret = filestream_write_file(path, data, info.size);
      }
      
      else 
      {
         /* save_to_disk is false, which means we are saving the state
         in undo_load_buf to allow content_undo_load_state() to restore it */

         /* If we were holding onto an old state already, clean it up first */
         if (undo_load_buf.data)
         {
            free(undo_load_buf.data);
            undo_load_buf.data = NULL;
         }

         undo_load_buf.data = malloc(info.size);
         if (!undo_load_buf.data)
         {
            free(data);
            return false;
         }

         memcpy(undo_load_buf.data, data, info.size);
         undo_load_buf.size = info.size;
         strlcpy(undo_load_buf.path, path, sizeof(undo_load_buf.path));
      }
   }
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
 * @load_to_backup_buffer: If true, the state will be loaded into undo_save_buf.
 * Load a state from disk to memory.
 *
 * Returns: true if successful, false otherwise.
 *
 *
 **/
bool content_load_state(const char *path, bool load_to_backup_buffer)
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

   /* This means we're backing up the file in memory, so content_undo_save_state()
   can restore it */
   if (load_to_backup_buffer)
   {
      /* If we were previously backing up a file, let go of it first */
      if (undo_save_buf.data)
      {
         free(undo_save_buf.data);
         undo_save_buf.data = NULL;
      }

      undo_save_buf.data = malloc(size);
      if (!undo_save_buf.data)
         goto error;

      memcpy(undo_save_buf.data, buf, size);
      undo_save_buf.size = size;
      strlcpy(undo_save_buf.path, path, sizeof(undo_save_buf.path));

      free(buf);
      return true;
   }

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

         ptr = mem_info.data;
         if (ptr)
            memcpy(blocks[i].data, ptr, blocks[i].size);
      }
   }

   serial_info.data_const = buf;
   serial_info.size       = size;
   
   /* Backup the current state so we can undo this load */
   content_save_state("RAM", false);
   ret                    = core_unserialize(&serial_info);

   /* Flush back. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         retro_ctx_memory_info_t    mem_info;
         void *ptr = NULL;

         mem_info.id = blocks[i].type;

         core_get_memory(&mem_info);

         ptr = mem_info.data;
         if (ptr)
            memcpy(ptr, blocks[i].data, blocks[i].size);
      }
   }

   for (i = 0; i < num_blocks; i++)
      free(blocks[i].data);
   free(blocks);
   
   if (!ret)
      goto error;

   free(buf);

   return true;

error:
   RARCH_ERR("%s \"%s\".\n",
         msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
         path);
   free(buf);
   return false;
}

bool content_rename_state(const char *origin, const char *dest)
{
   int ret = 0;
   if (path_file_exists(dest))
      unlink(dest);

   ret = rename (origin, dest);
   if (!ret)
      return true;

   RARCH_LOG("Error %d renaming file %s\n", ret, origin);
   return false;
}

/*
* 
* TODO/FIXME: Figure out when and where this should be called.
* As it is, when e.g. closing Gambatte, we get the same printf message 4 times.
*
*/
bool content_reset_savestate_backups(void)
{
   RARCH_LOG("Resetting undo buffers.\n");

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

   return true;
}

bool content_undo_load_buf_is_empty(void)
{
   return undo_load_buf.data == NULL || undo_load_buf.size == 0;
}

bool content_undo_save_buf_is_empty(void)
{
   return undo_save_buf.data == NULL || undo_save_buf.size == 0;
}
