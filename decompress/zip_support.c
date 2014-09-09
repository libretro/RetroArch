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


// Extract the relative path relative_path from a zip archive archive_path and allocate a buf for it to write it in.
int read_zip_file(const char * archive_path, const char *relative_path, void **buf)
{
}

struct string_list *compressed_zip_file_list_new(const char *path,
      const char* ext)
{

}
