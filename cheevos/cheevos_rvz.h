/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - RetroArch Team
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

#ifndef __RARCH_CHEEVOS_RVZ_H
#define __RARCH_CHEEVOS_RVZ_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

/**
 * RVZ (and WIA) disc image support for RetroAchievements hashing.
 *
 * Provides a virtual file interface that transparently decompresses
 * RVZ/WIA files on-demand, allowing rcheevos to hash them as if they
 * were raw disc images.
 */

/**
 * rcheevos_rvz_open:
 * @path          : Path to RVZ/WIA file
 *
 * Opens an RVZ/WIA file for reading. The file will be parsed and
 * prepared for on-demand decompression.
 *
 * Returns: Handle to RVZ file, or NULL on error
 */
void* rcheevos_rvz_open(const char* path);

/**
 * rcheevos_rvz_seek:
 * @file_handle   : RVZ file handle
 * @offset        : Offset to seek to
 * @origin        : SEEK_SET, SEEK_CUR, or SEEK_END
 *
 * Seeks within the virtual decompressed disc image.
 */
void rcheevos_rvz_seek(void* file_handle, int64_t offset, int origin);

/**
 * rcheevos_rvz_tell:
 * @file_handle   : RVZ file handle
 *
 * Gets current position in virtual decompressed disc image.
 *
 * Returns: Current file position
 */
int64_t rcheevos_rvz_tell(void* file_handle);

/**
 * rcheevos_rvz_read:
 * @file_handle   : RVZ file handle
 * @buffer        : Buffer to read into
 * @size          : Number of bytes to read
 *
 * Reads data from virtual decompressed disc image. Will decompress
 * chunks as needed.
 *
 * Returns: Number of bytes actually read
 */
size_t rcheevos_rvz_read(void* file_handle, void* buffer, size_t size);

/**
 * rcheevos_rvz_close:
 * @file_handle   : RVZ file handle
 *
 * Closes RVZ file and frees all resources.
 */
void rcheevos_rvz_close(void* file_handle);

/**
 * rcheevos_rvz_get_console_id:
 * @path          : Path to RVZ/WIA file
 *
 * Determines the console type (GameCube or Wii) by reading the disc
 * magic words from the decompressed image.
 *
 * Returns: RC_CONSOLE_GAMECUBE, RC_CONSOLE_WII, or RC_CONSOLE_UNKNOWN
 */
uint32_t rcheevos_rvz_get_console_id(const char* path);

#endif /* __RARCH_CHEEVOS_RVZ_H */
