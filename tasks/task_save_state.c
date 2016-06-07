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
#include "../msg_hash.h"
#include "../verbosity.h"
#include "tasks_internal.h"

struct sram_block
{
   unsigned type;
   void *data;
   size_t size;
};

/* TODO/FIXME - turn this into actual task */

/**
 * save_state:
 * @path      : path of saved state that shall be written to.
 *
 * Save a state from memory to disk.
 *
 * Returns: true if successful, false otherwise.
 **/
bool content_save_state(const char *path)
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
bool content_load_state(const char *path)
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

   RARCH_LOG ("Error %d renaming file %s", ret, origin);
   return false;
}
