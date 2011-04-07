/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "file.h"
#include "general.h"
#include <stdlib.h>
#include <stdbool.h>
#include <libsnes.hpp>
#include <string.h>
#include "dynamic.h"
#include "movie.h"
#include "ups.h"
#include "strl.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <dirent.h>
#endif

// Generic file loader.
static ssize_t read_file(const char *path, void **buf)
{
   void *rom_buf = NULL;
   FILE *file = fopen(path, "rb");
   if (!file)
      goto error;

   fseek(file, 0, SEEK_END);
   long len = ftell(file);
   ssize_t rc = 0;
   rewind(file);
   rom_buf = malloc(len);
   if (!rom_buf)
   {
      SSNES_ERR("Couldn't allocate memory!\n");
      goto error;
   }

   if ((rc = fread(rom_buf, 1, len, file)) < len)
      SSNES_WARN("Didn't read whole file.\n");

   *buf = rom_buf;
   fclose(file);
   return rc;

error:
   if (file)
      fclose(file);
   free(rom_buf);
   *buf = NULL;
   return -1;
}

// Load SNES rom only. Applies a hack for headered ROMs.
static ssize_t read_rom_file(FILE* file, void** buf)
{
   ssize_t ret = 0;
   uint8_t *ret_buf = NULL;

   if (file == NULL) // stdin
   {
#ifdef _WIN32
      setmode(0, O_BINARY);
#endif

      SSNES_LOG("Reading ROM from stdin ...\n");
      size_t buf_size = 0xFFFFF; // Some initial guesstimate.
      size_t buf_ptr = 0;
      uint8_t *rom_buf = malloc(buf_size);
      if (rom_buf == NULL)
      {
         SSNES_ERR("Couldn't allocate memory!\n");
         return -1;
      }

      for(;;)
      {
         size_t ret = fread(rom_buf + buf_ptr, 1, buf_size - buf_ptr, stdin);
         buf_ptr += ret;

         // We've reached the end
         if (buf_ptr < buf_size)
            break;

         rom_buf = realloc(rom_buf, buf_size * 2);
         if (rom_buf == NULL)
         {
            SSNES_ERR("Couldn't allocate memory!\n");
            return -1;
         }

         buf_size *= 2;
      }

      ret_buf = rom_buf;
      ret = buf_ptr;
   }
   else
   {
      fseek(file, 0, SEEK_END);
      ret = ftell(file);
      rewind(file);

      void *rom_buf = malloc(ret);
      if (rom_buf == NULL)
      {
         SSNES_ERR("Couldn't allocate memory!\n");
         return -1;
      }

      if (fread(rom_buf, 1, ret, file) < ret)
      {
         SSNES_ERR("Didn't read whole file.\n");
         free(rom_buf);
         return -1;
      }

      ret_buf = rom_buf;
   }

   // Patch with UPS.
   ssize_t ups_patch_size;
   void *ups_patch = NULL;
   if (*g_extern.ups_name && (ups_patch_size = read_file(g_extern.ups_name, &ups_patch)) >= 0)
   {
      SSNES_LOG("Found UPS file in \"%s\", attempting to patch ...\n", g_extern.ups_name);

      size_t target_size = ret * 4; // Just to be sure ...
      uint8_t *patched_rom = malloc(target_size);
      if (patched_rom)
      {
         ups_error_t err = ups_apply_patch(ups_patch, ups_patch_size, ret_buf, ret, patched_rom, &target_size);
         if (err == UPS_SUCCESS)
         {
            free(ret_buf);
            ret_buf = patched_rom;
            ret = target_size;
            SSNES_LOG("ROM patched successfully (UPS)!\n");
         }
         else
         {
            free(patched_rom);
            SSNES_LOG("ROM failed to patch (UPS).\n");
         }

         free(ups_patch);
      }
   }
   else if (*g_extern.ups_name)
      SSNES_LOG("Could not find UPS patch in: \"%s\".\n", g_extern.ups_name);

   // Remove copier header if present (512 first bytes).
   if ((ret & 0x7fff) == 512)
   {
      memmove(ret_buf, ret_buf + 512, ret - 512);
      ret -= 512;
   }

   g_extern.cart_crc = crc32_calculate(ret_buf, ret);
   *buf = ret_buf;
   return ret;
}


// Dump stuff to file.
static bool dump_to_file(const char *path, const void *data, size_t size)
{
   FILE *file = fopen(path, "wb");
   if (!file)
      return false;
   else
   {
      fwrite(data, 1, size, file);
      fclose(file);
      return true;
   }
}

bool save_state(const char* path)
{
   SSNES_LOG("Saving state: \"%s\".\n", path);
   size_t size = psnes_serialize_size();
   if (size == 0)
      return false;

   void *data = malloc(size);
   if (!data)
   {
      SSNES_ERR("Failed to allocate memory for save state buffer.\n");
      return false;
   }

   SSNES_LOG("State size: %d bytes.\n", (int)size);
   psnes_serialize(data, size);
   bool ret = dump_to_file(path, data, size);
   free(data);
   if (!ret)
      SSNES_ERR("Failed to save state to \"%s\".\n", path);
   return ret;
}

bool load_state(const char* path)
{
   SSNES_LOG("Loading state: \"%s\".\n", path);
   void *buf = NULL;
   ssize_t size = read_file(path, &buf);
   if (size < 0)
   {
      SSNES_ERR("Failed to load state from \"%s\".\n", path);
      return false;
   }
   else
   {
      SSNES_LOG("State size: %d bytes.\n", (int)size);
      psnes_unserialize(buf, size);
   }

   free(buf);
   return true;
}

void load_ram_file(const char* path, int type)
{
   size_t size = psnes_get_memory_size(type);
   uint8_t *data = psnes_get_memory_data(type);

   if (size == 0 || !data)
      return;

   void *buf = NULL;
   ssize_t rc = read_file(path, &buf);
   if (rc <= size)
      memcpy(data, buf, size);

   free(buf);
}

void save_ram_file(const char* path, int type)
{
   size_t size = psnes_get_memory_size(type);
   uint8_t *data = psnes_get_memory_data(type);

   if (data && size > 0)
      dump_to_file(path, data, size);
}

static bool load_sgb_rom(void)
{
   void *rom_buf = NULL;
   ssize_t rom_len = 0;

   FILE *extra_rom = NULL;
   void *extra_rom_buf = NULL;
   ssize_t extra_rom_len = 0;

   if ((rom_len = read_rom_file(g_extern.rom_file, &rom_buf)) == -1)
   {
      SSNES_ERR("Could not read ROM file.\n");
      goto error;
   }

   if ((extra_rom_len = read_file(g_extern.gb_rom_path, &extra_rom_buf)) == -1)
   {
      SSNES_ERR("Cannot read GameBoy rom.\n");
      goto error;
   }

   if (!psnes_load_cartridge_super_game_boy(
            NULL, rom_buf, rom_len,
            NULL, extra_rom_buf, extra_rom_len))
   {
      SSNES_ERR("Cannot load SGB/GameBoy rom.\n");
      goto error;
   }

   if (g_extern.rom_file)
      fclose(g_extern.rom_file);
   if (extra_rom)
      fclose(extra_rom);
   free(rom_buf);
   free(extra_rom_buf);
   return true;

error:
   if (g_extern.rom_file)
      fclose(g_extern.rom_file);
   if (extra_rom)
      fclose(extra_rom);
   free(rom_buf);
   free(extra_rom_buf);
   return false;
}

static bool load_bsx_rom(bool slotted)
{
   void *rom_buf = NULL;
   ssize_t rom_len = 0;

   FILE *extra_rom = NULL;
   void *extra_rom_buf = NULL;
   ssize_t extra_rom_len = 0;

   if ((rom_len = read_rom_file(g_extern.rom_file, &rom_buf)) == -1)
   {
      SSNES_ERR("Could not read ROM file.\n");
      goto error;
   }

   if ((extra_rom_len = read_file(g_extern.bsx_rom_path, &extra_rom_buf)) == -1)
   {
      SSNES_ERR("Cannot read BSX game rom.\n");
      goto error;
   }

   if (slotted)
   {   
      if (!psnes_load_cartridge_bsx_slotted(
               NULL, rom_buf, rom_len,
               NULL, extra_rom_buf, extra_rom_len))
      {
         SSNES_ERR("Cannot load BSX slotted rom.\n");
         goto error;
      }

   }
   else
   {
      if (!psnes_load_cartridge_bsx(
               NULL, rom_buf, rom_len,
               NULL, extra_rom_buf, extra_rom_len))
      {
         SSNES_ERR("Cannot load BSX rom.\n");
         goto error;
      }
   }

   if (g_extern.rom_file)
      fclose(g_extern.rom_file);
   if (extra_rom)
      fclose(extra_rom);
   free(rom_buf);
   free(extra_rom_buf);
   return true;

error:
   if (g_extern.rom_file)
      fclose(g_extern.rom_file);
   if (extra_rom)
      fclose(extra_rom);
   free(rom_buf);
   free(extra_rom_buf);
   return false;
}

static bool load_sufami_rom(void)
{
   void *rom_buf = NULL;
   ssize_t rom_len = 0;

   FILE *extra_rom[2] = {NULL};
   void *extra_rom_buf[2] = {NULL};
   ssize_t extra_rom_len[2] = {0};

   if ((rom_len = read_rom_file(g_extern.rom_file, &rom_buf)) == -1)
   {
      SSNES_ERR("Could not read ROM file.\n");
      goto error;
   }
   
   const char *roms[2] = { g_extern.sufami_rom_path[0], g_extern.sufami_rom_path[1] };

   for (int i = 0; i < 2; i++)
   {
      if (strlen(roms[i]) > 0)
      {
         if ((extra_rom_len[i] = read_file(roms[i], &extra_rom_buf[i])) == -1)
         {
            SSNES_ERR("Cannot read Sufami game rom.\n");
            goto error;
         }
      }
   }

   if (!psnes_load_cartridge_sufami_turbo(
            NULL, rom_buf, rom_len,
            NULL, extra_rom_buf[0], extra_rom_len[0],
            NULL, extra_rom_buf[1], extra_rom_len[1]))
   {
      SSNES_ERR("Cannot load Sufami Turbo rom.\n");
      goto error;
   }


   if (g_extern.rom_file)
      fclose(g_extern.rom_file);
   for (int i = 0; i < 2; i++)
   {
      if (extra_rom[i])
         fclose(extra_rom[i]);
      free(extra_rom_buf[i]);
   }
   free(rom_buf);
   return true;

error:
   if (g_extern.rom_file)
      fclose(g_extern.rom_file);
   for (int i = 0; i < 2; i++)
   {
      if (extra_rom[i])
         fclose(extra_rom[i]);
      free(extra_rom_buf[i]);
   }
   free(rom_buf);
   return false;
}

static bool load_normal_rom(void)
{
   void *rom_buf = NULL;
   ssize_t rom_len = 0;

   if ((rom_len = read_rom_file(g_extern.rom_file, &rom_buf)) == -1)
   {
      SSNES_ERR("Could not read ROM file.\n");
      return false;
   }

   if (g_extern.rom_file != NULL)
      fclose(g_extern.rom_file);

   SSNES_LOG("ROM size: %d bytes\n", (int)rom_len);

   if (!psnes_load_cartridge_normal(NULL, rom_buf, rom_len))
   {
      SSNES_ERR("ROM file is not valid!\n");
      free(rom_buf);
      return false;
   }

   free(rom_buf);
   return true;
}


bool init_rom_file(enum ssnes_game_type type)
{
   switch (type)
   {
      case SSNES_CART_SGB:
         if (!load_sgb_rom())
            return false;
         break;

      case SSNES_CART_NORMAL:
         if (!load_normal_rom())
            return false;
         break;

      case SSNES_CART_BSX:
         if (!load_bsx_rom(false))
            return false;
         break;

      case SSNES_CART_BSX_SLOTTED:
         if (!load_bsx_rom(true))
            return false;
         break;

      case SSNES_CART_SUFAMI:
         if (!load_sufami_rom())
            return false;
         break;
         
      default:
         SSNES_ERR("Invalid ROM type!\n");
         return false;
   }

   return true;
}

// Yep, this is C alright ;)
char** dir_list_new(const char *dir, const char *ext)
{
   size_t path_len = strlen(dir);

   size_t cur_ptr = 0;
   size_t cur_size = 32;
   char **dir_list = NULL;

#ifdef _WIN32
   WIN32_FIND_DATAW ffd;
   HANDLE hFind = INVALID_HANDLE_VALUE;


   size_t final_off = MAX_PATH + path_len + 2;

   wchar_t wchar_buf[MAX_PATH + 1];
   char utf8_buf[MAX_PATH + 3];

   strlcpy(utf8_buf, dir, sizeof(utf8_buf));
   strlcat(utf8_buf, "/*", sizeof(utf8_buf));
   utf8_buf[MAX_PATH + 2] = '\0';

   int ret = MultiByteToWideChar(CP_UTF8, 0, utf8_buf, strlen(utf8_buf), wchar_buf, MAX_PATH);
   wchar_buf[ret] = 0;

   hFind = FindFirstFileW(wchar_buf, &ffd);
   if (hFind == INVALID_HANDLE_VALUE)
      goto error;
#else
   DIR *directory = NULL;
   const struct dirent *entry = NULL;
   size_t final_off = sizeof(entry->d_name) + path_len + 2;

   directory = opendir(dir);
   if (!directory)
      goto error;
#endif

   dir_list = calloc(cur_size, sizeof(char*));
   if (!dir_list)
      goto error;

#ifdef _WIN32 // Hard to read? Blame non-POSIX heathens!
   do
#else
   while ((entry = readdir(directory)))
#endif
   {
      // Not a perfect search of course, but hopefully good enough in practice.
#ifdef _WIN32
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
         continue;
      int ret = WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, wcslen(ffd.cFileName), utf8_buf, MAX_PATH, NULL, NULL);
      utf8_buf[ret] = '\0';
      if (ext && !strstr(utf8_buf, ext))
         continue;
#else
      if (ext && !strstr(entry->d_name, ext))
         continue;
#endif

      dir_list[cur_ptr] = malloc(final_off);
      if (!dir_list[cur_ptr])
         goto error;

      strcpy(dir_list[cur_ptr], dir);
#ifdef _WIN32
      dir_list[cur_ptr][path_len] = '\\';
      strcpy(&dir_list[cur_ptr][path_len + 1], utf8_buf);
#else
      dir_list[cur_ptr][path_len] = '/';
      strcpy(&dir_list[cur_ptr][path_len + 1], entry->d_name);
#endif
      dir_list[cur_ptr][final_off - 1] = '\0';

      cur_ptr++;
      if (cur_ptr + 1 == cur_size) // Need to reserve for NULL.
      {
         cur_size *= 2;
         dir_list = realloc(dir_list, cur_size * sizeof(char*));
         if (!dir_list)
            goto error;

         // Make sure it's all NULL'd out since we cannot rely on realloc to do this.
         memset(dir_list + cur_ptr, 0, (cur_size - cur_ptr) * sizeof(char*));
      }
   }
#ifdef _WIN32
   while (FindNextFileW(hFind, &ffd) != 0);

#endif

#ifdef _WIN32
   FindClose(hFind);
#else
   closedir(directory);
#endif
   return dir_list;

error:
   SSNES_ERR("Failed to open directory: \"%s\"\n", dir);
#ifdef _WIN32
   if (hFind != INVALID_HANDLE_VALUE)
      FindClose(hFind);
#else
   if (directory)
      closedir(directory);
#endif
   dir_list_free(dir_list);
   return NULL;
}

void dir_list_free(char **dir_list)
{
   if (!dir_list)
      return;

   char **orig = dir_list;
   while (*dir_list)
      free(*dir_list++);
   free(orig);
}
