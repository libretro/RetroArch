/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "../boolean.h"

#ifdef HAVE_LIBRETRO_MANAGEMENT
#include "rarch_console_libretro_mgmt.h"
#endif

#include "rarch_console_rzlib.h"

static int rarch_extract_currentfile_in_zip(unzFile uf, const char *current_dir, char *slash, char *write_filename, size_t write_filename_size)
{
   char filename_inzip[PATH_MAX];
   FILE *file_out = NULL;

   unz_file_info file_info;
   int err = unzGetCurrentFileInfo(uf,
         &file_info, filename_inzip, sizeof(filename_inzip),
         NULL, 0, NULL, 0);

   if (err != UNZ_OK)
   {
      RARCH_ERR("Error %d while trying to get ZIP file information.\n", err);
      return err;
   }

   size_t size_buf = WRITEBUFFERSIZE;
   void *buf = malloc(size_buf);
   if (!buf)
   {
      RARCH_ERR("Error allocating memory for ZIP extract operation.\n");
      return UNZ_INTERNALERROR;
   }

   switch(g_console.zip_extract_mode)
   {
      case ZIP_EXTRACT_TO_CURRENT_DIR:
      case ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE:
         snprintf(write_filename, write_filename_size, "%s%s%s", current_dir, slash, filename_inzip);
         break;
#ifdef HAVE_HDD_CACHE_PARTITION
      case ZIP_EXTRACT_TO_CACHE_DIR:
         snprintf(write_filename, write_filename_size, "%s%s", default_paths.cache_dir, filename_inzip);
         break;
#endif
   }

   err = unzOpenCurrentFile(uf);
   if (err != UNZ_OK)
      RARCH_ERR("Error %d while trying to open ZIP file.\n", err);
   else
   {
      /* success */
      file_out = fopen(write_filename, "wb");

      if (!file_out)
         RARCH_ERR("Error opening %s.\n", write_filename);
   }

   if (file_out)
   {
      RARCH_LOG("Extracting: %s..\n", write_filename);

      do
      {
         err = unzReadCurrentFile(uf, buf, size_buf);
         if (err < 0)
         {
            RARCH_ERR("Error %d while reading from ZIP file.\n", err);
            break;
         }

         if (err > 0)
         {
            if (fwrite(buf, err, 1, file_out) != 1)
            {
               RARCH_ERR("Error while extracting file(s) from ZIP.\n");
               err = UNZ_ERRNO;
               break;
            }
         }
      }while (err > 0);

      if (file_out)
         fclose(file_out);
   }

   if (err == UNZ_OK)
   {
      err = unzCloseCurrentFile (uf);
      if (err != UNZ_OK)
         RARCH_ERR("Error %d while trying to close ZIP file.\n", err);
   }
   else
      unzCloseCurrentFile(uf); 

   free(buf);
   return err;
}

int rarch_extract_zipfile(const char *zip_path, const char *current_dir, char *first_file, size_t first_file_size)
{
   bool found_first_file = false;
   (void)found_first_file;
   unzFile uf = unzOpen(zip_path); 

   unz_global_info gi;
   int err = unzGetGlobalInfo(uf, &gi);
   if (err != UNZ_OK)
      RARCH_ERR("Error %d while trying to get ZIP file global info.\n",err);

   for (unsigned i = 0; i < gi.number_entry; i++)
   {
      static char write_filename[PATH_MAX];
      char slash[6];
#ifdef _XBOX
      snprintf(slash, sizeof(slash), "\\");
#else
      snprintf(slash, sizeof(slash), "/");
#endif
      if (rarch_extract_currentfile_in_zip(uf, current_dir, slash, write_filename, sizeof(write_filename)) != UNZ_OK)
      {
         RARCH_ERR("Failed to extract current file from ZIP archive.\n");
         break;
      }
      else
      {
#ifdef HAVE_LIBRETRO_MANAGEMENT
         if(!found_first_file)
         {
            found_first_file = rarch_manage_libretro_extension_supported(write_filename);

            if(found_first_file)
               snprintf(first_file, first_file_size, write_filename);
         }
#endif
      }

      if ((i + 1) < gi.number_entry)
      {
         err = unzGoToNextFile(uf);
         if (err != UNZ_OK)
         {
            RARCH_ERR("Error %d while trying to go to the next file in the ZIP archive.\n",err);
            break;
         }
      }
   }

   if(g_console.info_msg_enable)
      rarch_settings_msg(S_MSG_EXTRACTED_ZIPFILE, S_DELAY_180);

   return 0;
}
