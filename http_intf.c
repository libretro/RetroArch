/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Alfred Agrell
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
#include <assert.h>
#include "http_intf.h"
#include "retroarch_logger.h"
#include <retro_miscellaneous.h>
#include "general.h"

#include <file/file_path.h>

/**
 * http_get_file:
 * @url                 : URL to file.
 * @buf                 : Buffer.
 * @len                 : Size of @buf.
 *
 * Loads the contents of a file at specified URL into
 * buffer @buf. Sets length of data buffer as well.
 *
 * Returns: HTTP return code on success, otherwise 
 * negative on failure.
 **/
http_retcode http_get_file(char *url, char **buf, int *len)
{
   char *urlfilename = NULL;

   if (http_parse_url(url, &urlfilename) != 0)
      return ERRRDDT;

   return http_get(urlfilename, buf, len, NULL);
}

/**
 * http_download_file:
 * @url                 : URL to file.
 * @output_dir          : Output directory for new file.
 * @output_basename     : Output basename  for new file.
 *
 * Downloads a file at specified URL.
 *
 * Returns: bool (1) if successful, otherwise false (0).
 **/
bool http_download_file(char *url, const char *output_dir,
      const char *output_basename)
{
   http_retcode status;
   int len;
   FILE *f;
   char output_path[PATH_MAX_LENGTH];
   char *buf;

   status = http_get_file(url, &buf, &len);

   if (status < 0)
   {
      RARCH_ERR("%i - Failure.\n", status);
      return false;
   }

   fill_pathname_join(output_path, output_dir, output_basename,
         sizeof(output_path));

   f = fopen(output_path, "wb");

   if (!f)
      goto cleanup;

   fwrite(buf, 1, len, f);

   fclose(f);

cleanup:
   if (buf)
      free(buf);

   return true;
}
