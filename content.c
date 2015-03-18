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

#include "content.h"
#include "file_ops.h"
#include <file/file_path.h>
#include "general.h"
#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include "dynamic.h"
#include "movie.h"
#include "patch.h"
#include "compat/strl.h"
#include "hash.h"
#include "file_extract.h"

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

   RARCH_LOG("Loading content file: %s.\n", path);
   if (!read_file(path, (void**) &ret_buf, length))
      return false;

   if (*length <= 0)
      return false;

   if (i != 0)
      return true;

   /* Attempt to apply a patch. */
   if (!g_extern.block_patch)
      patch_content(&ret_buf, length);
   
   g_extern.content_crc = crc32_calculate(ret_buf, *length);

   RARCH_LOG("CRC32: 0x%x .\n", (unsigned)g_extern.content_crc);
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
   char path[PATH_MAX_LENGTH], timebuf[PATH_MAX_LENGTH];
   time_t time_;
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

   if (write_file(path, data, size))
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
   bool ret = false;
   void *data = NULL;
   size_t size = pretro_serialize_size();

   RARCH_LOG("Saving state: \"%s\".\n", path);

   if (size == 0)
      return false;

   data = malloc(size);

   if (!data)
   {
      RARCH_ERR("Failed to allocate memory for save state buffer.\n");
      return false;
   }

   RARCH_LOG("State size: %d bytes.\n", (int)size);
   ret = pretro_serialize(data, size);

   if (ret)
      ret = write_file(path, data, size);

   if (!ret)
      RARCH_ERR("Failed to save state to \"%s\".\n", path);

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
   unsigned num_blocks = 0;
   bool ret = true;
   void *buf = NULL;
   struct sram_block *blocks = NULL;
   ssize_t size;

   ret = read_file(path, &buf, &size);

   RARCH_LOG("Loading state: \"%s\".\n", path);

   if (!ret || size < 0)
   {
      RARCH_ERR("Failed to load state from \"%s\".\n", path);
      return false;
   }

   RARCH_LOG("State size: %u bytes.\n", (unsigned)size);

   if (g_settings.block_sram_overwrite && g_extern.savefiles
         && g_extern.savefiles->size)
   {
      RARCH_LOG("Blocking SRAM overwrite.\n");
      blocks = (struct sram_block*)
         calloc(g_extern.savefiles->size, sizeof(*blocks));

      if (blocks)
      {
         num_blocks = g_extern.savefiles->size;
         for (i = 0; i < num_blocks; i++)
            blocks[i].type = g_extern.savefiles->elems[i].attr.i;
      }
   }

   for (i = 0; i < num_blocks; i++)
      blocks[i].size = pretro_get_memory_size(blocks[i].type);

   for (i = 0; i < num_blocks; i++)
      if (blocks[i].size)
         blocks[i].data = malloc(blocks[i].size);

   /* Backup current SRAM which is overwritten by unserialize. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         const void *ptr = pretro_get_memory_data(blocks[i].type);
         if (ptr)
            memcpy(blocks[i].data, ptr, blocks[i].size);
      }
   }

   ret = pretro_unserialize(buf, size);

   /* Flush back. */
   for (i = 0; i < num_blocks; i++)
   {
      if (blocks[i].data)
      {
         void *ptr = pretro_get_memory_data(blocks[i].type);
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
   bool ret = false;
   void *buf   = NULL;
   size_t size = pretro_get_memory_size(type);
   void *data  = pretro_get_memory_data(type);

   if (size == 0 || !data)
      return;

   ret = read_file(path, &buf, &rc);

   if (!ret)
      return;

   if (rc > 0)
   {
      if (rc > (ssize_t)size)
      {
         RARCH_WARN("SRAM is larger than implementation expects, doing partial load (truncating %u bytes to %u).\n",
               (unsigned)rc, (unsigned)size);
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
   size_t size = pretro_get_memory_size(type);
   void *data  = pretro_get_memory_data(type);

   if (!data)
      return;
   if (size <= 0)
      return;

   if (!write_file(path, data, size))
   {
      RARCH_ERR("Failed to save SRAM.\n");
      RARCH_WARN("Attempting to recover ...\n");
      dump_to_file_desperate(data, size, type);
      return;
   }

   RARCH_LOG("Saved successfully to \"%s\".\n", path);
}

static bool load_content_dont_need_fullpath(
      struct retro_game_info *info, unsigned i, const char *path)
{
   ssize_t len;
   /* Load the content into memory. */

   /* First content file is significant, attempt to do patching,
    * CRC checking, etc. */
   bool ret = read_content_file(i, path, (void**)&info->data, &len);

   if (!ret || len < 0)
   {
      RARCH_ERR("Could not read content file \"%s\".\n", path);
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
   char new_path[PATH_MAX_LENGTH], new_basedir[PATH_MAX_LENGTH];
   ssize_t len;
   union string_list_elem_attr attributes;
   bool ret = false;

   if (g_extern.system.info.block_extract)
      return true;

   if (!need_fullpath)
      return true;

   if (!path_contains_compressed_file(path))
      return true;

   RARCH_LOG("Compressed file in case of need_fullpath."
         "Now extracting to temporary directory.\n");

   strlcpy(new_basedir, g_settings.extraction_directory,
         sizeof(new_basedir));

   if ((!strcmp(new_basedir, "")) ||
         !path_is_directory(new_basedir))
   {
      RARCH_WARN("Tried extracting to extraction directory, but "
            "extraction directory was not set or found. "
            "Setting extraction directory to directory "
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
      RARCH_ERR("Could not read content file \"%s\".\n", path);
      return false;
   }

   string_list_append(additional_path_allocs,new_path, attributes);
   info[i].path =
      additional_path_allocs->elems
      [additional_path_allocs->size -1 ].data;

   /* g_extern.temporary_content is initialized in init_content_file
    * The following part takes care of cleanup of the unzipped files
    * after exit.
    */
   rarch_assert(g_extern.temporary_content != NULL);
   string_list_append(g_extern.temporary_content,
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
      const char *path = content->elems[i].data;
      int         attr = content->elems[i].attr.i;

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
      ret = pretro_load_game_special(special->id, info, content->size);
   else
      ret = pretro_load_game(*content->elems[0].data ? info : NULL);

   if (!ret)
      RARCH_ERR("Failed to load content.\n");

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
 * g_extern.content_is_init will be set to the return value
 * on exit.
 *
 * Returns : true if successful, otherwise false.
 **/
bool init_content_file(void)
{
   unsigned i;
   union string_list_elem_attr attr;
   bool ret = false;
   struct string_list *content = NULL;
   const struct retro_subsystem_info *special = NULL;

   g_extern.temporary_content = string_list_new();

   if (!g_extern.temporary_content)
      goto error;

   if (*g_extern.subsystem)
   {
      special = libretro_find_subsystem_info(g_extern.system.special,
            g_extern.system.num_special, g_extern.subsystem);

      if (!special)
      {
         RARCH_ERR(
               "Failed to find subsystem \"%s\" in libretro implementation.\n",
               g_extern.subsystem);
         goto error;
      }

      if (special->num_roms && !g_extern.subsystem_fullpaths)
      {
         RARCH_ERR("libretro core requires special content, but none were provided.\n");
         goto error;
      }
      else if (special->num_roms && special->num_roms
            != g_extern.subsystem_fullpaths->size)
      {
         RARCH_ERR("libretro core requires %u content files for subsystem \"%s\", but %u content files were provided.\n",
               special->num_roms, special->desc,
               (unsigned)g_extern.subsystem_fullpaths->size);
         goto error;
      }
      else if (!special->num_roms && g_extern.subsystem_fullpaths
            && g_extern.subsystem_fullpaths->size)
      {
         RARCH_ERR("libretro core takes no content for subsystem \"%s\", but %u content files were provided.\n",
               special->desc,
               (unsigned)g_extern.subsystem_fullpaths->size);
         goto error;
      }
   }

   content = string_list_new();

   attr.i = 0;

   if (!content)
      goto error;

   if (*g_extern.subsystem)
   {
      for (i = 0; i < g_extern.subsystem_fullpaths->size; i++)
      {
         attr.i  = special->roms[i].block_extract;
         attr.i |= special->roms[i].need_fullpath << 1;
         attr.i |= special->roms[i].required << 2;
         string_list_append(content,
               g_extern.subsystem_fullpaths->elems[i].data, attr);
      }
   }
   else
   {
      attr.i  = g_extern.system.info.block_extract;
      attr.i |= g_extern.system.info.need_fullpath << 1;
      attr.i |= (!g_extern.system.no_content) << 2;
      string_list_append(content,
            g_extern.libretro_no_content ? "" : g_extern.fullpath, attr);
   }

#ifdef HAVE_ZLIB
   /* Try to extract all content we're going to load if appropriate. */
   for (i = 0; i < content->size; i++)
   {
      const char *ext = NULL;
      const char *valid_ext = NULL;

      /* Block extract check. */
      if (content->elems[i].attr.i & 1)
         continue;

      ext       = path_get_extension(content->elems[i].data);
      valid_ext = special ? special->roms[i].valid_extensions :
         g_extern.system.info.valid_extensions;

      if (ext && !strcasecmp(ext, "zip"))
      {
         char temporary_content[PATH_MAX_LENGTH];

         strlcpy(temporary_content, content->elems[i].data,
               sizeof(temporary_content));

         if (!zlib_extract_first_content_file(temporary_content,
                  sizeof(temporary_content), valid_ext,
                  *g_settings.extraction_directory ?
                  g_settings.extraction_directory : NULL))
         {
            RARCH_ERR("Failed to extract content from zipped file: %s.\n",
                  temporary_content);
            goto error;
         }
         string_list_set(content, i, temporary_content);
         string_list_append(g_extern.temporary_content,
               temporary_content, attr);
      }
   }
#endif

   /* Set attr to need_fullpath as appropriate. */
   ret = load_content(special, content);

error:
   g_extern.content_is_init = (ret) ? true : false;

   if (content)
      string_list_free(content);
   return ret;
}
