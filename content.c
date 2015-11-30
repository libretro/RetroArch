/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>

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

#include <compat/strl.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <retro_file.h>
#include <retro_stat.h>

#include "msg_hash.h"
#include "content.h"
#include "file_ops.h"
#include "general.h"
#include "dynamic.h"
#include "movie.h"
#include "patch.h"
#include "system.h"
#include "verbosity.h"

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif

static struct string_list *temporary_content;

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
   uint8_t *ret_buf = NULL;
   global_t *global = global_get_ptr();

   RARCH_LOG("%s: %s.\n",
         msg_hash_to_str(MSG_LOADING_CONTENT_FILE), path);
   if (!read_file(path, (void**) &ret_buf, length))
      return false;

   if (*length < 0)
      return false;

   if (i != 0)
      return true;

   /* Attempt to apply a patch. */
   if (!global->patch.block_patch)
      patch_content(&ret_buf, length);

#ifdef HAVE_ZLIB
   global->content_crc = zlib_crc32_calculate(ret_buf, *length);

   RARCH_LOG("CRC32: 0x%x .\n", (unsigned)global->content_crc);
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
static void dump_to_file_desperate(const void *data,
      size_t size, unsigned type)
{
   time_t time_;
   char path[PATH_MAX_LENGTH]    = {0};
   char timebuf[PATH_MAX_LENGTH] = {0};
#if defined(_WIN32) && !defined(_XBOX)
   const char *base = getenv("APPDATA");
#elif defined(__CELLOS_LV2__) || defined(_XBOX)
   const char *base = NULL;
#else
   const char *base = getenv("HOME");
#endif

   if (!base)
      goto error;

   snprintf(path, sizeof(path), "%s/RetroArch-recovery-%u", base, type);

   time(&time_);

   strftime(timebuf, sizeof(timebuf), "%Y-%m-%d-%H-%M-%S", localtime(&time_));
   strlcat(path, timebuf, sizeof(path));

   if (retro_write_file(path, data, size))
      RARCH_WARN("Succeeded in saving RAM data to \"%s\".\n", path);
   else
      goto error;

   return;

error:
   RARCH_WARN("Failed ... Cannot recover save file.\n");
}

struct sram_block
{
   unsigned type;
   void *data;
   size_t size;
};

/**
 * save_state:
 * @path      : path of saved state that shall be written to.
 *
 * Save a state from memory to disk.
 *
 * Returns: true if successful, false otherwise.
 **/
bool save_state(const char *path)
{
   bool ret    = false;
   void *data  = NULL;
   size_t size = core.retro_serialize_size();

   RARCH_LOG("%s: \"%s\".\n",
         msg_hash_to_str(MSG_SAVING_STATE),
         path);

   if (size == 0)
      return false;

   data = malloc(size);

   if (!data)
      return false;

   RARCH_LOG("%s: %d %s.\n",
         msg_hash_to_str(MSG_STATE_SIZE),
         (int)size,
         msg_hash_to_str(MSG_BYTES));
   ret = core.retro_serialize(data, size);

   if (ret)
      ret = retro_write_file(path, data, size);

   if (!ret)
      RARCH_ERR("%s \"%s\".\n",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
            path);

   free(data);

   return ret;
}

/**
 * load_state:
 * @path      : path that state will be loaded from.
 *
 * Load a state from disk to memory.
 *
 * Returns: true if successful, false otherwise.
 **/
bool load_state(const char *path)
{
   unsigned i;
   ssize_t size;
   unsigned num_blocks       = 0;
   void *buf                 = NULL;
   struct sram_block *blocks = NULL;
   settings_t *settings      = config_get_ptr();
   global_t *global          = global_get_ptr();
   bool ret                  = read_file(path, &buf, &size);

   RARCH_LOG("%s: \"%s\".\n",
         msg_hash_to_str(MSG_LOADING_STATE),
         path);

   if (!ret || size < 0)
   {
      RARCH_ERR("%s \"%s\".\n",
            msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
            path);
      return false;
   }

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
      blocks[i].size = core.retro_get_memory_size(blocks[i].type);

   for (i = 0; i < num_blocks; i++)
      if (blocks[i].size)
         blocks[i].data = malloc(blocks[i].size);

   /* Backup current SRAM which is overwritten by unserialize. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         const void *ptr = core.retro_get_memory_data(blocks[i].type);
         if (ptr)
            memcpy(blocks[i].data, ptr, blocks[i].size);
      }
   }

   ret = core.retro_unserialize(buf, size);

   /* Flush back. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         void *ptr = core.retro_get_memory_data(blocks[i].type);
         if (ptr)
            memcpy(ptr, blocks[i].data, blocks[i].size);
      }
   }

   for (i = 0; i < num_blocks; i++)
      free(blocks[i].data);
   free(blocks);
   free(buf);
   return ret;
}

/**
 * load_ram_file:
 * @path             : path of RAM state that will be loaded from.
 * @type             : type of memory
 *
 * Load a RAM state from disk to memory.
 */
void load_ram_file(const char *path, int type)
{
   ssize_t rc;
   bool ret    = false;
   void *buf   = NULL;
   size_t size = core.retro_get_memory_size(type);
   void *data  = core.retro_get_memory_data(type);

   if (size == 0 || !data)
      return;

   ret = read_file(path, &buf, &rc);

   if (!ret)
      return;

   if (rc > 0)
   {
      if (rc > (ssize_t)size)
      {
         RARCH_WARN("SRAM is larger than implementation expects, doing partial load (truncating %u %s %s %u).\n",
               (unsigned)rc,
               msg_hash_to_str(MSG_BYTES),
               msg_hash_to_str(MSG_TO),
               (unsigned)size);
         rc = size;
      }
      memcpy(data, buf, rc);
   }

   if (buf)
      free(buf);
}

/**
 * save_ram_file:
 * @path             : path of RAM state that shall be written to.
 * @type             : type of memory
 *
 * Save a RAM state from memory to disk.
 *
 * In case the file could not be written to, a fallback function
 * 'dump_to_file_desperate' will be called.
 */
void save_ram_file(const char *path, int type)
{
   size_t size = core.retro_get_memory_size(type);
   void *data  = core.retro_get_memory_data(type);

   if (!data)
      return;
   if (size == 0)
      return;

   if (!retro_write_file(path, data, size))
   {
      RARCH_ERR("%s.\n",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_SRAM));
      RARCH_WARN("Attempting to recover ...\n");
      dump_to_file_desperate(data, size, type);
      return;
   }

   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_SAVED_SUCCESSFULLY_TO),
         path);
}

static bool load_content_dont_need_fullpath(
      struct retro_game_info *info, unsigned i, const char *path)
{
   ssize_t len;
   /* Load the content into memory. */

   /* First content file is significant, attempt to do patching,
    * CRC checking, etc. */
   bool ret = false;

   if (i == 0)
      ret = read_content_file(i, path, (void**)&info->data, &len);
   else
      ret = read_file(path, (void**)&info->data, &len);

   if (!ret || len < 0)
   {
      RARCH_ERR("%s \"%s\".\n",
            msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
            path);
      return false;
   }

   info->size = len;

   return true;
}

static bool load_content_need_fullpath(
      struct retro_game_info *info, unsigned i,
      struct string_list* additional_path_allocs,
      bool need_fullpath, const char *path)
{
#ifdef HAVE_COMPRESSION
   ssize_t len;
   union string_list_elem_attr attributes;
   char new_path[PATH_MAX_LENGTH]    = {0};
   char new_basedir[PATH_MAX_LENGTH] = {0};
   bool ret                          = false;
   settings_t *settings              = config_get_ptr();
   rarch_system_info_t      *sys_info= rarch_system_info_get_ptr();

   if (sys_info && sys_info->info.block_extract)
      return true;
   if (!need_fullpath || !path_contains_compressed_file(path))
      return true;

   RARCH_LOG("Compressed file in case of need_fullpath."
         "Now extracting to temporary directory.\n");

   strlcpy(new_basedir, settings->cache_directory,
         sizeof(new_basedir));

   if ((!strcmp(new_basedir, "")) ||
         !path_is_directory(new_basedir))
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

   ret = read_compressed_file(path,NULL,new_path, &len);

   if (!ret || len < 0)
   {
      RARCH_ERR("%s \"%s\".\n",
            msg_hash_to_str(MSG_COULD_NOT_READ_CONTENT_FILE),
            path);
      return false;
   }

   string_list_append(additional_path_allocs,new_path, attributes);
   info[i].path =
      additional_path_allocs->elems
      [additional_path_allocs->size -1 ].data;

   /* temporary_content is initialized in init_content_file
    * The following part takes care of cleanup of the unzipped files
    * after exit.
    */
   retro_assert(temporary_content != NULL);
   string_list_append(temporary_content,
         new_path, attributes);

#endif
   return true;
}

/**
 * load_content:
 * @special          : subsystem of content to be loaded. Can be NULL.
 * content           :
 *
 * Load content file (for libretro core).
 *
 * Returns : true if successful, otherwise false.
 **/
static bool load_content(const struct retro_subsystem_info *special,
      const struct string_list *content)
{
   unsigned i;
   bool ret = true;
   struct string_list* additional_path_allocs = string_list_new();
   struct retro_game_info *info = (struct retro_game_info*)
      calloc(content->size, sizeof(*info));

   if (!info)
   {
      string_list_free(additional_path_allocs);
      return false;
   }

   for (i = 0; i < content->size; i++)
   {
      const char *path     = content->elems[i].data;
      int         attr     = content->elems[i].attr.i;
      bool need_fullpath   = attr & 2;
      bool require_content = attr & 4;

      if (require_content && !*path)
      {
         RARCH_LOG("libretro core requires content, but nothing was provided.\n");
         ret = false;
         goto end;
      }

      info[i].path = NULL;

      if (*path)
         info[i].path = path;

      if (!need_fullpath && *path)
      {
         if (!load_content_dont_need_fullpath(&info[i], i, path))
            goto end;
      }
      else
      {
         RARCH_LOG("Content loading skipped. Implementation will"
               " load it on its own.\n");

         if (!load_content_need_fullpath(&info[i], i,
                  additional_path_allocs, need_fullpath, path))
            goto end;
      }
   }

   if (special)
      ret = core.retro_load_game_special(special->id, info, content->size);
   else
   {
      ret = core.retro_load_game(*content->elems[0].data ? info : NULL);
      
#ifdef HAVE_CHEEVOS
      /* Load the achievements into memory if the game has content. */

      cheevos_globals.cheats_were_enabled = cheevos_globals.cheats_are_enabled;
      cheevos_load(*content->elems[0].data ? info : NULL);
#endif
   }

   if (!ret)
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));

end:
   for (i = 0; i < content->size; i++)
      free((void*)info[i].data);

   string_list_free(additional_path_allocs);
   if (info)
      free(info);
   return ret;
}

/**
 * init_content_file:
 *
 * Initializes and loads a content file for the currently
 * selected libretro core.
 *
 * global->content_is_init will be set to the return value
 * on exit.
 *
 * Returns : true if successful, otherwise false.
 **/
bool init_content_file(void)
{
   unsigned i;
   union string_list_elem_attr attr;
   bool ret                                   = false;
   struct string_list *content                = NULL;
   const struct retro_subsystem_info *special = NULL;
   settings_t *settings                       = config_get_ptr();
   rarch_system_info_t *system                = rarch_system_info_get_ptr();
   global_t *global                           = global_get_ptr();
   temporary_content                          = string_list_new();

   if (!temporary_content)
      goto error;

   if (*global->subsystem)
   {
      special = libretro_find_subsystem_info(system->special,
            system->num_special, global->subsystem);

      if (!special)
      {
         RARCH_ERR(
               "Failed to find subsystem \"%s\" in libretro implementation.\n",
               global->subsystem);
         goto error;
      }

      if (special->num_roms && !global->subsystem_fullpaths)
      {
         RARCH_ERR("libretro core requires special content, but none were provided.\n");
         goto error;
      }
      else if (special->num_roms && special->num_roms
            != global->subsystem_fullpaths->size)
      {
         RARCH_ERR("libretro core requires %u content files for subsystem \"%s\", but %u content files were provided.\n",
               special->num_roms, special->desc,
               (unsigned)global->subsystem_fullpaths->size);
         goto error;
      }
      else if (!special->num_roms && global->subsystem_fullpaths
            && global->subsystem_fullpaths->size)
      {
         RARCH_ERR("libretro core takes no content for subsystem \"%s\", but %u content files were provided.\n",
               special->desc,
               (unsigned)global->subsystem_fullpaths->size);
         goto error;
      }
   }

   content = string_list_new();

   attr.i = 0;

   if (!content)
      goto error;

   if (*global->subsystem)
   {
      for (i = 0; i < global->subsystem_fullpaths->size; i++)
      {
         attr.i  = special->roms[i].block_extract;
         attr.i |= special->roms[i].need_fullpath << 1;
         attr.i |= special->roms[i].required << 2;
         string_list_append(content,
               global->subsystem_fullpaths->elems[i].data, attr);
      }
   }
   else
   {
      char *fullpath = NULL;

      runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

      attr.i  = system->info.block_extract;
      attr.i |= system->info.need_fullpath << 1;
      attr.i |= (!system->no_content) << 2;
      string_list_append(content,
            (global->inited.core.no_content && settings->core.set_supports_no_game_enable) ? "" : fullpath, attr);
   }

#ifdef HAVE_ZLIB
   /* Try to extract all content we're going to load if appropriate. */
   for (i = 0; i < content->size; i++)
   {
      const char *ext       = NULL;
      const char *valid_ext = NULL;

      /* Block extract check. */
      if (content->elems[i].attr.i & 1)
         continue;

      ext       = path_get_extension(content->elems[i].data);
      valid_ext = special ? special->roms[i].valid_extensions :
         system->info.valid_extensions;

      if (ext && !strcasecmp(ext, "zip"))
      {
         char temp_content[PATH_MAX_LENGTH] = {0};

         strlcpy(temp_content, content->elems[i].data,
               sizeof(temp_content));

         if (!zlib_extract_first_content_file(temp_content,
                  sizeof(temp_content), valid_ext,
                  *settings->cache_directory ?
                  settings->cache_directory : NULL))
         {
            RARCH_ERR("Failed to extract content from zipped file: %s.\n",
                  temp_content);
            goto error;
         }
         string_list_set(content, i, temp_content);
         string_list_append(temporary_content,
               temp_content, attr);
      }
   }
#endif

   /* Set attr to need_fullpath as appropriate. */
   ret = load_content(special, content);

error:
   global->inited.content = (ret) ? true : false;

   if (content)
      string_list_free(content);
   return ret;
}

/**
 * content_free_temporary:
 *
 * Frees temporary content handle.
 **/
void content_temporary_free(void)
{
   unsigned i;

   if (!temporary_content)
      return;

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

   temporary_content = NULL;
}
