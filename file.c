/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "file.h"
#include "general.h"
#include <stdlib.h>
#include "boolean.h"
#include "libretro.h"
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

static void patch_rom(uint8_t **buf, ssize_t *size)
{
   uint8_t *ret_buf = *buf;
   ssize_t ret_size = *size;

   const char *patch_desc = NULL;
   const char *patch_path = NULL;
   patch_error_t err = PATCH_UNKNOWN;
   patch_func_t func = NULL;

   ssize_t patch_size = 0;
   void *patch_data = NULL;
   bool success = false;

   if (g_extern.ups_pref + g_extern.bps_pref + g_extern.ips_pref > 1)
   {
      RARCH_WARN("Several patches are explicitly defined, ignoring all ...\n");
      return;
   }

   bool allow_bps = !g_extern.ups_pref && !g_extern.ips_pref;
   bool allow_ups = !g_extern.bps_pref && !g_extern.ips_pref;
   bool allow_ips = !g_extern.ups_pref && !g_extern.bps_pref;

   if (allow_ups && *g_extern.ups_name && (patch_size = read_file(g_extern.ups_name, &patch_data)) >= 0)
   {
      patch_desc = "UPS";
      patch_path = g_extern.ups_name;
      func = ups_apply_patch;
   }
   else if (allow_bps && *g_extern.bps_name && (patch_size = read_file(g_extern.bps_name, &patch_data)) >= 0)
   {
      patch_desc = "BPS";
      patch_path = g_extern.bps_name;
      func = bps_apply_patch;
   }
   else if (allow_ips && *g_extern.ips_name && (patch_size = read_file(g_extern.ips_name, &patch_data)) >= 0)
   {
      patch_desc = "IPS";
      patch_path = g_extern.ips_name;
      func = ips_apply_patch;
   }
   else
   {
      RARCH_LOG("Did not find a valid ROM patch.\n");
      return;
   }

   RARCH_LOG("Found %s file in \"%s\", attempting to patch ...\n", patch_desc, patch_path);

   size_t target_size = ret_size * 4; // Just to be sure ...
   uint8_t *patched_rom = (uint8_t*)malloc(target_size);
   if (!patched_rom)
   {
      RARCH_ERR("Failed to allocate memory for patched ROM ...\n");
      goto error;
   }

   err = func((const uint8_t*)patch_data, patch_size, ret_buf, ret_size, patched_rom, &target_size);
   if (err == PATCH_SUCCESS)
   {
      RARCH_LOG("ROM patched successfully (%s).\n", patch_desc);
      success = true;
   }
   else
      RARCH_ERR("Failed to patch %s: Error #%u\n", patch_desc, (unsigned)err);

   if (success)
   {
      free(ret_buf);
      *buf = patched_rom;
      *size = target_size;
   }

   free(patch_data);
   return;

error:
   *buf = ret_buf;
   *size = ret_size;
   free(patch_data);
}

static ssize_t read_rom_file(const char *path, void **buf)
{
   uint8_t *ret_buf = NULL;
   ssize_t ret = read_file(path, (void**)&ret_buf);
   if (ret <= 0)
      return ret;

   if (!g_extern.block_patch)
   {
      // Attempt to apply a patch.
      patch_rom(&ret_buf, &ret);
   }
   
   g_extern.cart_crc = crc32_calculate(ret_buf, ret);
   sha256_hash(g_extern.sha256, ret_buf, ret);
   RARCH_LOG("CRC32: 0x%x, SHA256: %s\n",
         (unsigned)g_extern.cart_crc, g_extern.sha256);
   *buf = ret_buf;
   return ret;
}

// Attempt to save valuable RAM data somewhere ...
static void dump_to_file_desperate(const void *data, size_t size, unsigned type)
{
#if defined(_WIN32) && !defined(_XBOX)
   const char *base = getenv("APPDATA");
#elif defined(__CELLOS_LV2__) || defined(_XBOX)
   const char *base = NULL;
#else
   const char *base = getenv("HOME");
#endif

   if (!base)
      goto error;

   char path[PATH_MAX];
   snprintf(path, sizeof(path), "%s/RetroArch-recovery-%u", base, type);
   char timebuf[PATH_MAX];

   time_t time_;
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

bool save_state(const char *path)
{
   RARCH_LOG("Saving state: \"%s\".\n", path);
   size_t size = pretro_serialize_size();
   if (size == 0)
      return false;

   void *data = malloc(size);
   if (!data)
   {
      RARCH_ERR("Failed to allocate memory for save state buffer.\n");
      return false;
   }

   RARCH_LOG("State size: %d bytes.\n", (int)size);
   bool ret = pretro_serialize(data, size);
   if (ret)
      ret = write_file(path, data, size);

   if (!ret)
      RARCH_ERR("Failed to save state to \"%s\".\n", path);

   free(data);
   return ret;
}

struct sram_block
{
   unsigned type;
   void *data;
   size_t size;
};

bool load_state(const char *path)
{
   unsigned i;
   void *buf = NULL;
   ssize_t size = read_file(path, &buf);

   RARCH_LOG("Loading state: \"%s\".\n", path);

   if (size < 0)
   {
      RARCH_ERR("Failed to load state from \"%s\".\n", path);
      return false;
   }

   bool ret = true;
   RARCH_LOG("State size: %u bytes.\n", (unsigned)size);

   struct sram_block *blocks = NULL;
   unsigned num_blocks = 0;

   if (g_settings.block_sram_overwrite && g_extern.savefiles && g_extern.savefiles->size)
   {
      RARCH_LOG("Blocking SRAM overwrite.\n");
      blocks = (struct sram_block*)calloc(g_extern.savefiles->size, sizeof(*blocks));
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

   // Backup current SRAM which is overwritten by unserialize.
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

   // Flush back :D
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
   return ret;
}

void load_ram_file(const char *path, int type)
{
   size_t size = pretro_get_memory_size(type);
   void *data = pretro_get_memory_data(type);

   if (size == 0 || !data)
      return;

   void *buf = NULL;
   ssize_t rc = read_file(path, &buf);
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

   free(buf);
}

void save_ram_file(const char *path, int type)
{
   size_t size = pretro_get_memory_size(type);
   void *data = pretro_get_memory_data(type);

   if (data && size > 0)
   {
      if (!write_file(path, data, size))
      {
         RARCH_ERR("Failed to save SRAM.\n");
         RARCH_WARN("Attempting to recover ...\n");
         dump_to_file_desperate(data, size, type);
      }
      else
         RARCH_LOG("Saved successfully to \"%s\".\n", path);
   }
}

static bool load_content(const struct retro_subsystem_info *special, const struct string_list *content)
{
   unsigned i;
   bool ret = true;

   struct retro_game_info *info = (struct retro_game_info*)calloc(content->size, sizeof(*info));
   if (!info)
      return false;

   for (i = 0; i < content->size; i++)
   {
      const char *path = content->elems[i].data;
      int attr = content->elems[i].attr.i;

      bool need_fullpath = attr & 2;
      bool require_rom = attr & 4;

      if (require_rom && !*path)
      {
         RARCH_LOG("libretro core requires a ROM, but none were provided.\n");
         ret = false;
         goto end;
      }

      info[i].path = *path ? path : NULL;

      if (!need_fullpath && *path) // Load the ROM into memory.
      {
         RARCH_LOG("Loading ROM file: %s.\n", path);
         // First ROM is significant, attempt to do patching, CRC checking, etc ...
         long size = i == 0 ? read_rom_file(path, (void**)&info[i].data) : read_file(path, (void**)&info[i].data);
         if (size < 0)
         {
            RARCH_ERR("Could not read ROM file \"%s\".\n", path);
            ret = false;
            goto end;
         }

         info[i].size = size;
      }
      else
         RARCH_LOG("ROM loading skipped. Implementation will load it on its own.\n");
   }

   if (special)
      ret = pretro_load_game_special(special->id, info, content->size);
   else
      ret = pretro_load_game(*content->elems[0].data ? info : NULL);

   if (!ret)
      RARCH_ERR("Failed to load game.\n");

end:
   for (i = 0; i < content->size; i++)
      free((void*)info[i].data);
   free(info);
   return ret;
}

bool init_rom_file(void)
{
   unsigned i;

   g_extern.temporary_content = string_list_new();
   if (!g_extern.temporary_content)
      return false;

   const struct retro_subsystem_info *special = NULL;

   if (*g_extern.subsystem)
   {
      special = libretro_find_subsystem_info(g_extern.system.special, g_extern.system.num_special,
            g_extern.subsystem);

      if (!special)
      {
         RARCH_ERR("Failed to find subsystem \"%s\" in libretro implementation.\n",
               g_extern.subsystem);
         return false;
      }

      if (special->num_roms && !g_extern.subsystem_fullpaths)
      {
         RARCH_ERR("libretro core requires special ROMs, but none were provided.\n");
         return false;
      }
      else if (special->num_roms && special->num_roms != g_extern.subsystem_fullpaths->size)
      {
         RARCH_ERR("libretro core requires %u ROMs for subsystem \"%s\", but %u ROMs were provided.\n", special->num_roms, special->desc,
               (unsigned)g_extern.subsystem_fullpaths->size);
         return false;
      }
      else if (!special->num_roms && g_extern.subsystem_fullpaths && g_extern.subsystem_fullpaths->size)
      {
         RARCH_ERR("libretro core takes no ROMs for subsystem \"%s\", but %u ROMs were provided.\n", special->desc,
               (unsigned)g_extern.subsystem_fullpaths->size);
         return false;
      }
   }

   union string_list_elem_attr attr;
   attr.i = 0;

   struct string_list *content = (struct string_list*)string_list_new();
   if (!content)
      return false;

   if (*g_extern.subsystem)
   {
      for (i = 0; i < g_extern.subsystem_fullpaths->size; i++)
      {
         attr.i  = special->roms[i].block_extract;
         attr.i |= special->roms[i].need_fullpath << 1;
         attr.i |= special->roms[i].required << 2;
         string_list_append(content, g_extern.subsystem_fullpaths->elems[i].data, attr);
      }
   }
   else
   {
      attr.i  = g_extern.system.info.block_extract;
      attr.i |= g_extern.system.info.need_fullpath << 1;
      attr.i |= (!g_extern.system.no_game) << 2;
      string_list_append(content, g_extern.libretro_no_rom ? "" : g_extern.fullpath, attr);
   }

#ifdef HAVE_ZLIB
   // Try to extract all content we're going to load if appropriate.
   for (i = 0; i < content->size; i++)
   {
      // block extract check
      if (content->elems[i].attr.i & 1)
         continue;

      const char *ext = path_get_extension(content->elems[i].data);

      const char *valid_ext = special ?
         special->roms[i].valid_extensions :
         g_extern.system.info.valid_extensions;

      if (ext && !strcasecmp(ext, "zip"))
      {
         char temporary_content[PATH_MAX];
         strlcpy(temporary_content, content->elems[i].data, sizeof(temporary_content));
         if (!zlib_extract_first_rom(temporary_content, sizeof(temporary_content), valid_ext,
                  *g_settings.extraction_directory ? g_settings.extraction_directory : NULL))
         {
            RARCH_ERR("Failed to extract ROM from zipped file: %s.\n", temporary_content);
            string_list_free(content);
            return false;
         }
         string_list_set(content, i, temporary_content);
         string_list_append(g_extern.temporary_content, temporary_content, attr);
      }
   }
#endif

   // Set attr to need_fullpath as appropriate.
   
   bool ret = load_content(special, content);
   string_list_free(content);
   return ret;
}

