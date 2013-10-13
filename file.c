/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

// Dump stuff to file.
bool write_file(const char *path, const void *data, size_t size)
{
   FILE *file = fopen(path, "wb");
   if (!file)
      return false;
   else
   {
      bool ret = fwrite(data, 1, size, file) == size;
      fclose(file);
      return ret;
   }
}

// Generic file loader.
ssize_t read_file(const char *path, void **buf)
{
   void *rom_buf = NULL;
   FILE *file = fopen(path, "rb");
   ssize_t rc = 0;
   size_t len = 0;
   if (!file)
      goto error;

   fseek(file, 0, SEEK_END);
   len = ftell(file);
   rewind(file);
   rom_buf = malloc(len + 1);
   if (!rom_buf)
   {
      RARCH_ERR("Couldn't allocate memory.\n");
      goto error;
   }

   if ((rc = fread(rom_buf, 1, len, file)) < (ssize_t)len)
      RARCH_WARN("Didn't read whole file.\n");

   *buf = rom_buf;
   // Allow for easy reading of strings to be safe.
   // Will only work with sane character formatting (Unix).
   ((char*)rom_buf)[len] = '\0'; 
   fclose(file);
   return rc;

error:
   if (file)
      fclose(file);
   free(rom_buf);
   *buf = NULL;
   return -1;
}

// Reads file content as one string.
bool read_file_string(const char *path, char **buf)
{
   *buf = NULL;
   FILE *file = fopen(path, "r");
   size_t len = 0;
   char *ptr = NULL;

   if (!file)
      goto error;

   fseek(file, 0, SEEK_END);
   len = ftell(file) + 2; // Takes account of being able to read in EOF and '\0' at end.
   rewind(file);

   *buf = (char*)calloc(len, sizeof(char));
   if (!*buf)
      goto error;

   ptr = *buf;

   while (ptr && !feof(file))
   {
      size_t bufsize = (size_t)(((ptrdiff_t)*buf + (ptrdiff_t)len) - (ptrdiff_t)ptr);
      fgets(ptr, bufsize, file);

      ptr += strlen(ptr);
   }

   ptr = strchr(ptr, EOF);
   if (ptr)
      *ptr = '\0';

   fclose(file);
   return true;

error:
   if (file)
      fclose(file);
   if (*buf)
      free(*buf);
   return false;
}

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


static const char *ramtype2str(int type)
{
   switch (type)
   {
      case RETRO_MEMORY_SAVE_RAM:
      case RETRO_MEMORY_SNES_GAME_BOY_RAM:
      case RETRO_MEMORY_SNES_BSX_RAM:
         return ".srm";

      case RETRO_MEMORY_RTC:
      case RETRO_MEMORY_SNES_GAME_BOY_RTC:
         return ".rtc";

      case RETRO_MEMORY_SNES_BSX_PRAM:
         return ".pram";

      case RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM:
         return ".aram";
      case RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM:
         return ".bram";

      default:
         return "";
   }
}

// Attempt to save valuable RAM data somewhere ...
static void dump_to_file_desperate(const void *data, size_t size, int type)
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
   snprintf(path, sizeof(path), "%s/RetroArch-recovery-", base);
   char timebuf[PATH_MAX];

   time_t time_;
   time(&time_);
   strftime(timebuf, sizeof(timebuf), "%Y-%m-%d-%H-%M-%S", localtime(&time_));
   strlcat(path, timebuf, sizeof(path));
   strlcat(path, ramtype2str(type), sizeof(path));

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

bool load_state(const char *path)
{
   RARCH_LOG("Loading state: \"%s\".\n", path);
   void *buf = NULL;
   ssize_t size = read_file(path, &buf);

   if (size < 0)
   {
      RARCH_ERR("Failed to load state from \"%s\".\n", path);
      return false;
   }

   bool ret = true;
   RARCH_LOG("State size: %u bytes.\n", (unsigned)size);

   void *block_buf[2] = {NULL, NULL};
   int block_type[2] = {-1, -1};
   size_t block_size[2] = {0};

   if (g_settings.block_sram_overwrite)
   {
      RARCH_LOG("Blocking SRAM overwrite.\n");
      switch (g_extern.game_type)
      {
         case RARCH_CART_NORMAL:
            block_type[0] = RETRO_MEMORY_SAVE_RAM;
            block_type[1] = RETRO_MEMORY_RTC;
            break;

         case RARCH_CART_BSX:
         case RARCH_CART_BSX_SLOTTED:
            block_type[0] = RETRO_MEMORY_SNES_BSX_RAM;
            block_type[1] = RETRO_MEMORY_SNES_BSX_PRAM;
            break;

         case RARCH_CART_SUFAMI:
            block_type[0] = RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM;
            block_type[1] = RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM;
            break;

         case RARCH_CART_SGB:
            block_type[0] = RETRO_MEMORY_SNES_GAME_BOY_RAM;
            block_type[1] = RETRO_MEMORY_SNES_GAME_BOY_RTC;
            break;
      }
   }

   for (unsigned i = 0; i < 2; i++)
      if (block_type[i] != -1)
         block_size[i] = pretro_get_memory_size(block_type[i]);

   for (unsigned i = 0; i < 2; i++)
      if (block_size[i])
         block_buf[i] = malloc(block_size[i]);

   // Backup current SRAM which is overwritten by unserialize.
   for (unsigned i = 0; i < 2; i++)
   {
      if (block_buf[i])
      {
         const void *ptr = pretro_get_memory_data(block_type[i]);
         if (ptr)
            memcpy(block_buf[i], ptr, block_size[i]);
      }
   }

   ret = pretro_unserialize(buf, size);

   // Flush back :D
   for (unsigned i = 0; i < 2 && ret; i++)
   {
      if (block_buf[i])
      {
         void *ptr = pretro_get_memory_data(block_type[i]);
         if (ptr)
            memcpy(ptr, block_buf[i], block_size[i]);
      }
   }

   for (unsigned i = 0; i < 2; i++)
      if (block_buf[i])
         free(block_buf[i]);

   free(buf);
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
         RARCH_WARN("SRAM is larger than implementation expects, doing partial load.\n");
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

static char *load_xml_map(const char *path)
{
   char *xml_buf = NULL;
   if (*path)
   {
      if (read_file_string(path, &xml_buf))
         RARCH_LOG("Found XML memory map in \"%s\"\n", path);
   }

   return xml_buf;
}

#define MAX_ROMS 4

static bool load_roms(unsigned rom_type, const char **rom_paths, size_t roms)
{
   bool ret = true;

   if (roms == 0)
      return false;

   if (roms > MAX_ROMS)
      return false;

   void *rom_buf[MAX_ROMS] = {NULL};
   ssize_t rom_len[MAX_ROMS] = {0};
   struct retro_game_info info[MAX_ROMS] = {{NULL}};
   char *xml_buf = load_xml_map(g_extern.xml_name);

   if (!g_extern.system.info.need_fullpath)
   {
      RARCH_LOG("Loading ROM file: %s.\n", rom_paths[0]);
      if ((rom_len[0] = read_rom_file(rom_paths[0], &rom_buf[0])) == -1)
      {
         RARCH_ERR("Could not read ROM file.\n");
         ret = false;
         goto end;
      }

      RARCH_LOG("ROM size: %u bytes.\n", (unsigned)rom_len[0]);
   }
   else
      RARCH_LOG("ROM loading skipped. Implementation will load it on its own.\n");

   info[0].path = rom_paths[0];
   info[0].data = rom_buf[0];
   info[0].size = rom_len[0];
   info[0].meta = xml_buf;

   for (size_t i = 1; i < roms; i++)
   {
      if (rom_paths[i] &&
            !g_extern.system.info.need_fullpath &&
            (rom_len[i] = read_file(rom_paths[i], &rom_buf[i])) == -1)
      {
         RARCH_ERR("Could not read ROM file: \"%s\".\n", rom_paths[i]);
         ret = false;
         goto end;
      }
      
      info[i].path = rom_paths[i];
      info[i].data = rom_buf[i];
      info[i].size = rom_len[i];
   }

   if (rom_type == 0)
      ret = pretro_load_game(&info[0]);
   else
      ret = pretro_load_game_special(rom_type, info, roms);

   if (!ret)
      RARCH_ERR("Failed to load game.\n");

end:
   for (unsigned i = 0; i < MAX_ROMS; i++)
      free(rom_buf[i]);
   free(xml_buf);

   return ret;
}

static bool load_normal_rom(void)
{
   if (g_extern.libretro_no_rom && g_extern.system.no_game)
      return pretro_load_game(NULL);
   else if (g_extern.libretro_no_rom && !g_extern.system.no_game)
   {
      RARCH_ERR("No ROM is used, but libretro core does not support this.\n");
      return false;
   }
   else
   {
      const char *path = g_extern.fullpath;
      return load_roms(0, &path, 1);
   }
}

static bool load_sgb_rom(void)
{
   const char *path[2] = {
      *g_extern.fullpath ? g_extern.fullpath : NULL,
      g_extern.gb_rom_path
   };

   return load_roms(RETRO_GAME_TYPE_SUPER_GAME_BOY, path, 2);
}

static bool load_bsx_rom(bool slotted)
{
   const char *path[2] = {
      *g_extern.fullpath ? g_extern.fullpath : NULL,
      g_extern.bsx_rom_path
   };

   return load_roms(slotted ? RETRO_GAME_TYPE_BSX_SLOTTED : RETRO_GAME_TYPE_BSX, path, 2); 
}

static bool load_sufami_rom(void)
{
   const char *path[3] = {
      *g_extern.fullpath ? g_extern.fullpath : NULL,
      *g_extern.sufami_rom_path[0] ? g_extern.sufami_rom_path[0] : NULL,
      *g_extern.sufami_rom_path[1] ? g_extern.sufami_rom_path[1] : NULL,
   };

   return load_roms(RETRO_GAME_TYPE_SUFAMI_TURBO, path, 3);
}

bool init_rom_file(enum rarch_game_type type)
{
#ifdef HAVE_ZLIB
   if (*g_extern.fullpath && !g_extern.system.block_extract)
   {
      const char *ext = path_get_extension(g_extern.fullpath);
      if (ext && !strcasecmp(ext, "zip"))
      {
         g_extern.rom_file_temporary = true;

         if (!zlib_extract_first_rom(g_extern.fullpath, sizeof(g_extern.fullpath), g_extern.system.valid_extensions))
         {
            RARCH_ERR("Failed to extract ROM from zipped file: %s.\n", g_extern.fullpath);
            g_extern.rom_file_temporary = false;
            return false;
         }

         strlcpy(g_extern.last_rom, g_extern.fullpath, sizeof(g_extern.last_rom));
      }
   }
#endif

   switch (type)
   {
      case RARCH_CART_SGB:
         if (!load_sgb_rom())
            return false;
         break;

      case RARCH_CART_NORMAL:
         if (!load_normal_rom())
            return false;
         break;

      case RARCH_CART_BSX:
         if (!load_bsx_rom(false))
            return false;
         break;

      case RARCH_CART_BSX_SLOTTED:
         if (!load_bsx_rom(true))
            return false;
         break;

      case RARCH_CART_SUFAMI:
         if (!load_sufami_rom())
            return false;
         break;
         
      default:
         RARCH_ERR("Invalid ROM type.\n");
         return false;
   }

   return true;
}

