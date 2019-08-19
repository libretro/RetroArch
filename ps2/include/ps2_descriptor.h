/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PS2_DESCRIPTOR_H
#define PS2_DESCRIPTOR_H

#include <stddef.h>
#include <fileXio_cdvd.h>

#define MAX_OPEN_FILES 256
#define FILEENTRY_SIZE 2048

typedef struct {
   int  dircheck;
   char filename[256];
} entries;

typedef struct
{
   int ref_count;
   int items;
   int current_folder_position;
   entries *FileEntry;
} DescriptorTranslation;

void _init_ps2_io(void);
void _free_ps2_io(void);
int is_fd_valid(int fd);
int __ps2_acquire_descriptor(void);
int __ps2_release_descriptor(int fd);
DescriptorTranslation *__ps2_fd_grab(int fd);

#endif /* PS2_DESCRIPTOR_H */
