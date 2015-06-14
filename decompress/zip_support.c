/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Timo Strunk
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



#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <file/file_path.h>

#include "zip_support.h"

#include "../deps/zlib/unzip.h"

/* Undefined at the end of the file
 * Don't use outside of this file
 */
#define RARCH_ZIP_SUPPORT_BUFFER_SIZE_MAX 16384

/* Extract the relative path relative_path from a 
 * zip archive archive_path and allocate a buf for it to write it in. */
/* This code is inspired by:
 * stackoverflow.com/questions/10440113/simple-way-to-unzip-a-zip-file-using-zlib
 *
 * optional_outfile if not NULL will be used to extract the file. buf will be 0
 * then.
 */

int read_zip_file(const char *archive_path,
      const char *relative_path, void **buf,
      const char* optional_outfile)
{
   uLong i;
   unz_global_info global_info;
   ssize_t    bytes_read = -1;
   bool finished_reading = false;
   unzFile      *zipfile = (unzFile*)unzOpen(archive_path);

   if (!zipfile)
   {
      RARCH_ERR("Could not open ZIP file %s.\n",archive_path);
      return -1;
   }

   /* Get info about the zip file */
   if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK)
   {
      RARCH_ERR("Could not get global ZIP file info of %s."
                "Could be only a GZIP file without the ZIP part.\n",
                archive_path);
      goto error;
   }

   for ( i = 0; i < global_info.number_entry; ++i )
   {
      /* Get info about current file. */
      unz_file_info file_info;
      char filename[PATH_MAX_LENGTH] = {0};
      char last_char = ' ';

      if (unzGetCurrentFileInfo(
            zipfile,
            &file_info,
            filename,
            PATH_MAX_LENGTH,
            NULL, 0, NULL, 0 ) != UNZ_OK )
      {
         RARCH_ERR("Could not read file info in ZIP %s.\n",
               archive_path);
         goto error;
      }

      /* Check if this entry is a directory or file. */
      last_char = filename[strlen(filename)-1];

      if ( last_char == '/' || last_char == '\\' )
      {
         /* We skip directories */
      }
      else if (!strcmp(filename, relative_path))
      {
         /* We found the correct file in the zip, 
          * now extract it to *buf. */
         if (unzOpenCurrentFile(zipfile) != UNZ_OK )
         {
            RARCH_ERR("The file %s in %s could not be read.\n", 
                  relative_path, archive_path);
            goto error;
         }

         if (optional_outfile == 0)
         {
            /* Allocate outbuffer */
            *buf       = malloc(file_info.uncompressed_size + 1 );
            bytes_read = unzReadCurrentFile(zipfile, *buf, file_info.uncompressed_size);

            if (bytes_read != (ssize_t)file_info.uncompressed_size)
            {
               RARCH_ERR(
                     "Tried to read %d bytes, but only got %d of file %s in ZIP %s.\n",
                     (unsigned int) file_info.uncompressed_size, (int)bytes_read,
                     relative_path, archive_path);
               free(*buf);
               goto close;
            }
            ((char*)(*buf))[file_info.uncompressed_size] = '\0';
         }
         else
         {
            char read_buffer[RARCH_ZIP_SUPPORT_BUFFER_SIZE_MAX] = {0};
            FILE* outsink = fopen(optional_outfile,"wb");

            if (outsink == NULL)
            {
               RARCH_ERR("Could not open outfilepath %s.\n", optional_outfile);
               goto close;
            }

            bytes_read = 0;

            do
            {
               ssize_t fwrite_bytes;

               bytes_read = unzReadCurrentFile(zipfile, read_buffer,
                     RARCH_ZIP_SUPPORT_BUFFER_SIZE_MAX );
               fwrite_bytes = fwrite(read_buffer,1,bytes_read,outsink);

               if (fwrite_bytes == bytes_read)
                  continue;

               /* couldn't write all bytes */
               RARCH_ERR("Error writing to %s.\n",optional_outfile);
               fclose(outsink);
               goto close;
            } while(bytes_read > 0);

            fclose(outsink);
         }
         finished_reading = true;
      }

      unzCloseCurrentFile(zipfile);

      if (finished_reading)
         break;

      if ((i + 1) < global_info.number_entry)
      {
         if (unzGoToNextFile(zipfile) == UNZ_OK)
            continue;

         RARCH_ERR(
               "Could not iterate to next file in %s. ZIP file might be corrupt.\n",
               archive_path );
         goto error;
      }
   }

   unzClose(zipfile);

   if(!finished_reading)
   {
      RARCH_ERR("File %s not found in %s\n",
            relative_path, archive_path);
      return -1;
   }

   return bytes_read;

close:
   unzCloseCurrentFile(zipfile);
error:
   unzClose(zipfile);
   return -1;
}

#undef RARCH_ZIP_SUPPORT_BUFFER_SIZE_MAX
