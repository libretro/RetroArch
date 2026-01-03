/*  RetroArch - A frontend for libretro.
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
#ifndef TASK_DATABASE_CUE
#define TASK_DATABASE_CUE

#include <ctype.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/chd_stream.h>

RETRO_BEGIN_DECLS

struct magic_entry
{
   const char *system_name;
   const char *magic;
   int32_t offset;
};

int detect_ps1_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_ps2_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_psp_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
size_t detect_gc_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_scd_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_sat_game(intfstream_t *fd,
      char *s, size_t len, const char *filename);
int detect_dc_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
size_t detect_wii_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_system(intfstream_t *fd, const char **system_name,
      const char * filename);
int cue_find_track(const char *cue_path, bool first, uint64_t *offset,
      size_t *size, char *s, size_t len);
bool cue_next_file(intfstream_t *fd, const char *cue_path,
      char *s, uint64_t len);
int gdi_find_track(const char *gdi_path, bool first, char *s,
      size_t len);
size_t gdi_next_file(intfstream_t *fd, const char *gdi_path, char *s,
      size_t len);

int intfstream_get_serial(intfstream_t *fd, char *s, size_t len,
      const char *filename);
bool intfstream_file_get_serial(const char *name,
      uint64_t offset, int64_t size, char *s, size_t len, uint64_t *fsize);
int task_database_cue_get_serial(const char *name, char *s, size_t len,
      uint64_t *filesize);
int task_database_gdi_get_serial(const char *name, char *s, size_t len,
      uint64_t *filesize);
bool is_chd_file_cdi(const char *path);
int task_database_chd_get_serial(const char *name, char *serial, size_t len,
      uint64_t *filesize);
bool intfstream_file_get_crc_and_size(const char *name,
      uint64_t offset, int64_t len, uint32_t *crc, uint64_t *size);
int task_database_cue_get_crc_and_size(const char *name, uint32_t *crc,
      uint64_t *size);
int task_database_gdi_get_crc_and_size(const char *name, uint32_t *crc,
      uint64_t *size);
bool task_database_chd_get_crc_and_size(const char *name, uint32_t *crc,
      uint64_t *size);

RETRO_END_DECLS

#endif

