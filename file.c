/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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

// Load SNES rom only. Applies a hack for headered ROMs.
static ssize_t read_rom_file(FILE* file, void** buf)
{
   ssize_t ret;
   if (file == NULL) // stdin
   {
      SSNES_LOG("Reading ROM from stdin ...\n");
      size_t buf_size = 0xFFFFF; // Some initial guesstimate.
      size_t buf_ptr = 0;
      char *rom_buf = malloc(buf_size);
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

      if ((buf_ptr & 0x7fff) == 512)
      {
         memmove(rom_buf, rom_buf + 512, buf_ptr - 512);
         buf_ptr -= 512;
      }

      *buf = rom_buf;
      ret = buf_ptr;
   }
   else
   {
      fseek(file, 0, SEEK_END);
      long length = ftell(file);
      rewind(file);
      if ((length & 0x7fff) == 512)
      {
         length -= 512;
         fseek(file, 512, SEEK_SET);
      }

      void *rom_buf = malloc(length);
      if ( rom_buf == NULL )
      {
         SSNES_ERR("Couldn't allocate memory!\n");
         return -1;
      }

      if ( fread(rom_buf, 1, length, file) < length )
      {
         SSNES_ERR("Didn't read whole file.\n");
         free(rom_buf);
         return -1;
      }
      *buf = rom_buf;
      ret = length;
   }
   return ret;
}

// Generic file loader.
static ssize_t read_file(const char *path, void **buf)
{
   void *rom_buf = NULL;
   FILE *file = fopen(path, "rb");
   if (!file)
   {
      SSNES_ERR("Couldn't open file: \"%s\"\n", path);
      goto error;
   }

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

// Dump stuff to file.
static void dump_to_file(const char *path, const void *data, size_t size)
{
   FILE *file = fopen(path, "wb");
   if (!file)
   {
      SSNES_ERR("Couldn't dump to file %s\n", path);
   }
   else
   {
      fwrite(data, 1, size, file);
      fclose(file);
   }
}

void save_state(const char* path)
{
   SSNES_LOG("Saving state: \"%s\".\n", path);
   size_t size = psnes_serialize_size();
   if (size == 0)
      return;

   void *data = malloc(size);
   if (!data)
   {
      SSNES_ERR("Failed to allocate memory for save state buffer.\n");
      return;
   }

   SSNES_LOG("State size: %d bytes.\n", (int)size);
   psnes_serialize(data, size);
   dump_to_file(path, data, size);
   free(data);
}

void load_state(const char* path)
{
   SSNES_LOG("Loading state: \"%s\".\n", path);
   void *buf = NULL;
   ssize_t size = read_file(path, &buf);
   if (size < 0)
      SSNES_ERR("Failed to load state.\n");
   else
   {
      SSNES_LOG("State size: %d bytes.\n", (int)size);
      psnes_unserialize(buf, size);
   }

   free(buf);
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

   if ( data && size > 0 )
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

   extra_rom = fopen(g_extern.gb_rom_path, "rb");
   if (!extra_rom)
   {
      SSNES_ERR("Couldn't open GameBoy ROM!\n");
      goto error;
   }

   if ((extra_rom_len = read_rom_file(extra_rom, &extra_rom_buf)) == -1)
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

   extra_rom = fopen(g_extern.bsx_rom_path, "rb");
   if (!extra_rom)
   {
      SSNES_ERR("Couldn't open BSX game rom!\n");
      goto error;
   }

   if ((extra_rom_len = read_rom_file(extra_rom, &extra_rom_buf)) == -1)
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
         extra_rom[i] = fopen(roms[i], "rb");
         if (!extra_rom[i])
         {
            SSNES_ERR("Couldn't open BSX game rom!\n");
            goto error;
         }

         if ((extra_rom_len[i] = read_rom_file(extra_rom[i], &extra_rom_buf[i])) == -1)
         {
            SSNES_ERR("Cannot read BSX game rom.\n");
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

