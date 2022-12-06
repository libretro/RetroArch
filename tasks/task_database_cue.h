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
int detect_gc_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_scd_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_sat_game(intfstream_t *fd,
      char *s, size_t len, const char *filename);
int detect_dc_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_wii_game(intfstream_t *fd, char *s, size_t len,
      const char *filename);
int detect_system(intfstream_t *fd, const char **system_name,
      const char * filename);
int cue_find_track(const char *cue_path, bool first, uint64_t *offset,
      uint64_t *size, char *track_path, uint64_t max_len);
bool cue_next_file(intfstream_t *fd, const char *cue_path,
      char *s, uint64_t len);
int gdi_find_track(const char *gdi_path, bool first, char *track_path,
      uint64_t max_len);
bool gdi_next_file(intfstream_t *fd, const char *gdi_path, char *path,
      uint64_t max_len);

RETRO_END_DECLS

#endif

