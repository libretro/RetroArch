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



#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#include <string.h>
#include "../miscellaneous.h"
#include "../file_path.h"
#include "zip_support.h"

//ZLIB minizip implementation
#include "../deps/rzlib/unzip.h"


/* Extract the relative path relative_path from a zip archive archive_path and allocate a buf for it to write it in. */
/* This code is inspired by:
 * http://stackoverflow.com/questions/10440113/simple-way-to-unzip-a-zip-file-using-zlib
 */
int read_zip_file(const char * archive_path, const char *relative_path, void **buf)
{
   size_t bytes_read = -1;
   bool finished_reading = false;
   unzFile *zipfile = unzOpen( archive_path );
   if ( ! zipfile )
   {
      RARCH_ERR("Could not open zipfile %s.\n",archive_path);
      return -1;
   }

   // Get info about the zip file
   unz_global_info global_info;
   if ( unzGetGlobalInfo( zipfile, &global_info ) != UNZ_OK )
   {
      RARCH_ERR("Could not get global zipfile info of %s."
                "Could be only a gzip file without the zip part.\n",archive_path);
      unzClose( zipfile );
      return -1;
   }

   // Loop to extract all files
   uLong i;
   for ( i = 0; i < global_info.number_entry; ++i )
   {
      // Get info about current file.
      unz_file_info file_info;
      char filename[ PATH_MAX ];
      if ( unzGetCurrentFileInfo(
            zipfile,
            &file_info,
            filename,
            PATH_MAX,
            NULL, 0, NULL, 0 ) != UNZ_OK )
      {
         RARCH_ERR("Could not read file info in zip %s.\n",archive_path);
         unzClose( zipfile );
         return -1;
      }

      // Check if this entry is a directory or file.
      char last_char = ' ';
      last_char = filename[strlen(filename)-1];
      if ( last_char == '/' || last_char == '\\' )
      {
         //We skip directories
      }
      else if (strcmp(filename,relative_path) == 0)
      {
         /* We found the correct file in the zip, now extract it to *buf */
         if ( unzOpenCurrentFile( zipfile ) != UNZ_OK )
         {
            RARCH_ERR("The file %s in %s could not be read.\n",relative_path,archive_path);
            unzClose( zipfile );
            return -1;
         }

         /* Allocate outbuffer */
         *buf = malloc(file_info.uncompressed_size + 1 );

         bytes_read = unzReadCurrentFile( zipfile, *buf, file_info.uncompressed_size );
         if (bytes_read != file_info.uncompressed_size)
         {
            RARCH_ERR("We tried to read %d bytes, but only got %d of file %s in zip %s.\n",
                  (unsigned int) file_info.uncompressed_size,(int)bytes_read,relative_path,archive_path);
            free(*buf);
            unzCloseCurrentFile( zipfile );
            unzClose( zipfile );
            return -1;
         }
         ((char*)(*buf))[file_info.uncompressed_size] = '\0';
         finished_reading = true;
      }
      unzCloseCurrentFile( zipfile );
      if (finished_reading)
         break;

      if ( ( i+1 ) < global_info.number_entry )
      {
         if ( unzGoToNextFile( zipfile ) != UNZ_OK )
         {
            RARCH_ERR( "Could not iterate to next file in %s. Zipfile might be corrupt.\n",archive_path );
            unzClose( zipfile );
            return -1;
         }
      }
   }
   unzClose( zipfile );
   return bytes_read;
}

struct string_list *compressed_zip_file_list_new(const char *path,
      const char* ext)
{


   struct string_list *ext_list = NULL;
   struct string_list *list = (struct string_list*)string_list_new();
   if (!list)
   {
      RARCH_ERR("Could not allocate list memory in compressed_7zip_file_list_new\n.");
      return NULL;
   }

   if (ext)
      ext_list = string_split(ext, "|");


   size_t bytes_read = -1;
   bool finished_reading = false;
   unzFile *zipfile = unzOpen( path );
   if ( ! zipfile )
   {
      RARCH_ERR("Could not open zipfile %s.\n",path);
      return NULL;
   }

   // Get info about the zip file
   unz_global_info global_info;
   if ( unzGetGlobalInfo( zipfile, &global_info ) != UNZ_OK )
   {
      RARCH_ERR("Could not get global zipfile info of %s."
                "Could be only a gzip file without the zip part.\n",path);
      unzClose( zipfile );
      return NULL;
   }

   // Loop to extract all files
   uLong i;
   for ( i = 0; i < global_info.number_entry; ++i )
   {
      // Get info about current file.
      unz_file_info file_info;
      char filename[ PATH_MAX ];
      if ( unzGetCurrentFileInfo(
            zipfile,
            &file_info,
            filename,
            PATH_MAX,
            NULL, 0, NULL, 0 ) != UNZ_OK )
      {
         RARCH_ERR("Could not read file info in zip %s.\n",path);
         unzClose( zipfile );
         return NULL;
      }

      // Check if this entry is a directory or file.
      char last_char = ' ';
      last_char = filename[strlen(filename)-1];
      if ( last_char == '/' || last_char == '\\' )
      {
         //We skip directories
      }
      else
      {
         const char *file_ext = path_get_extension(filename);
         bool supported_by_core  = false;
         union string_list_elem_attr attr;
         if (string_list_find_elem_prefix(ext_list, ".", file_ext))
            supported_by_core = true;

         if (supported_by_core)
         {
            attr.i = RARCH_COMPRESSED_FILE_IN_ARCHIVE;
            if (!string_list_append(list, filename, attr))
            {
               RARCH_ERR("Could not append item to stringlist in zip_support.\n");
               unzCloseCurrentFile( zipfile );
               break;
            }
         }

      }
      unzCloseCurrentFile( zipfile );
      if ( ( i+1 ) < global_info.number_entry )
      {
         if ( unzGoToNextFile( zipfile ) != UNZ_OK )
         {
            RARCH_ERR( "Could not iterate to next file in %s. Zipfile might be corrupt.\n",path );
            unzClose( zipfile );
            return NULL;
         }
      }
   }
   unzClose( zipfile );
   return list;
}
