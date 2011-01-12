/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
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

ssize_t read_file(FILE* file, void** buf)
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

void write_file(const char* path, uint8_t* data, size_t size)
{
   FILE *file = fopen(path, "wb");
   if ( file != NULL )
   {
      SSNES_LOG("Saving state \"%s\". Size: %d bytes.\n", path, (int)size);
      psnes_serialize(data, size);
      if ( fwrite(data, 1, size, file) != size )
         SSNES_ERR("Did not save state properly.\n");
      fclose(file);
   }
}

void load_state(const char* path, uint8_t* data, size_t size)
{
   SSNES_LOG("Loading state: \"%s\".\n", path);
   FILE *file = fopen(path, "rb");
   if ( file != NULL )
   {
      //fprintf(stderr, "SSNES: Loading state. Size: %d bytes.\n", (int)size);
      if ( fread(data, 1, size, file) != size )
         SSNES_ERR("Did not load state properly.\n");
      fclose(file);
      psnes_unserialize(data, size);
   }
   else
   {
      SSNES_LOG("No state file found. Will create new.\n");
   }
}

void load_save_file(const char* path, int type)
{
   FILE *file;

   file = fopen(path, "rb");
   if ( !file )
   {
      return;
   }

   size_t size = psnes_get_memory_size(type);
   uint8_t *data = psnes_get_memory_data(type);

   if (size == 0 || !data)
   {
      fclose(file);
      return;
   }

   int rc = fread(data, 1, size, file);
   if ( rc != size )
   {
      SSNES_ERR("Couldn't load save file.\n");
   }

   SSNES_LOG("Loaded save file: \"%s\"\n", path);

   fclose(file);
}

void save_file(const char* path, int type)
{
   size_t size = psnes_get_memory_size(type);
   uint8_t *data = psnes_get_memory_data(type);

   if ( data && size > 0 )
      write_file(path, data, size);
}

static bool load_sgb_rom(void)
{
   void *rom_buf;
   ssize_t rom_len = 0;

   FILE *extra_rom;
   void *extra_rom_buf;
   ssize_t extra_rom_len = 0;

   if ((rom_len = read_file(g_extern.rom_file, &rom_buf)) == -1)
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

   if ((extra_rom_len = read_file(extra_rom, &extra_rom_buf)) == -1)
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

static bool load_normal_rom(void)
{
   void *rom_buf;
   ssize_t rom_len = 0;

   if ((rom_len = read_file(g_extern.rom_file, &rom_buf)) == -1)
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

bool init_rom_file(void)
{
   switch (g_extern.game_type)
   {
      case SSNES_CART_SGB:
         if (!load_sgb_rom())
            return false;
         break;

      case SSNES_CART_NORMAL:
         if (!load_normal_rom())
            return false;
         break;
         
      default:
         SSNES_ERR("Invalid ROM type!\n");
         return false;
   }

   return true;
}

