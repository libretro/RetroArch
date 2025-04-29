/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (runtime_file.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __RUNTIME_FILE_H
#define __RUNTIME_FILE_H

#include <retro_common_api.h>
#include <libretro.h>

#include <time.h>
#include <boolean.h>

#include "playlist.h"
#include "runtime_file_defines.h"

RETRO_BEGIN_DECLS

/* Structs */

typedef struct
{
   unsigned hours;
   unsigned minutes;
   unsigned seconds;
} rtl_runtime_t;

typedef struct
{
   unsigned year;
   unsigned month;
   unsigned day;
   unsigned hour;
   unsigned minute;
   unsigned second;
} rtl_last_played_t;

typedef struct
{
   rtl_runtime_t runtime;           /* unsigned alignment */
   rtl_last_played_t last_played;   /* unsigned alignment */
   char path[PATH_MAX_LENGTH];
} runtime_log_t;

/* Initialisation */

/* Initialise runtime log, loading current parameters
 * if log file exists. Returned object must be free()'d.
 * Returns NULL if core_path is invalid, or content_path
 * is invalid and core does not support contentless
 * operation */
runtime_log_t *runtime_log_init(
      const char *content_path,
      const char *core_path,
      const char *dir_runtime_log,
      const char *dir_playlist,
      bool log_per_core);

/* Setters */

/* Adds specified microseconds value to current runtime */
void runtime_log_add_runtime_usec(runtime_log_t *runtime_log, retro_time_t usec);

/* Sets last played entry to specified value */
void runtime_log_set_last_played(runtime_log_t *runtime_log,
      unsigned year, unsigned month, unsigned day,
      unsigned hour, unsigned minute, unsigned second);

/* Sets last played entry to current date/time */
void runtime_log_set_last_played_now(runtime_log_t *runtime_log);

/* Resets log to default (zero) values */
void runtime_log_reset(runtime_log_t *runtime_log);

/* Getters */
/* (Not strictly required, since we can get everything
 * from runtime_log directly - but perhaps it is logically
 * cleaner to have a symmetrical set/get interface) */

/* Gets runtime as a pre-formatted string */
size_t runtime_log_get_runtime_str(runtime_log_t *runtime_log, char *s, size_t len);

/* Gets last played entry values */
void runtime_log_get_last_played(runtime_log_t *runtime_log,
      unsigned *year, unsigned *month, unsigned *day,
      unsigned *hour, unsigned *minute, unsigned *second);

/* Gets last played entry value as a pre-formatted string */
void runtime_log_get_last_played_str(runtime_log_t *runtime_log,
      char *str, size_t len,
      enum playlist_sublabel_last_played_style_type timedate_style,
      enum playlist_sublabel_last_played_date_separator_type date_separator);

/* Status */

/* Returns true if log has a non-zero runtime entry */
bool runtime_log_has_runtime(runtime_log_t *runtime_log);

/* Saving */

/* Saves specified runtime log to disk */
void runtime_log_save(runtime_log_t *runtime_log);

/* Utility functions */

/* Convert from microseconds to hours, minutes, seconds */
void runtime_log_convert_usec2hms(retro_time_t usec, unsigned *hours, unsigned *minutes, unsigned *seconds);

/* Playlist manipulation */

/* Updates specified playlist entry runtime values with
 * contents of associated log file */
void runtime_update_playlist(
      playlist_t *playlist, size_t idx,
      const char *dir_runtime_log,
      const char *dir_playlist,
      bool log_per_core,
      enum playlist_sublabel_last_played_style_type timedate_style,
      enum playlist_sublabel_last_played_date_separator_type date_separator);

#if defined(HAVE_MENU)
/* Contentless cores manipulation */

/* Updates specified contentless core runtime values with
 * contents of associated log file */
void runtime_update_contentless_core(
      const char *core_path,
      const char *dir_runtime_log,
      const char *dir_playlist,
      bool log_per_core,
      enum playlist_sublabel_last_played_style_type timedate_style,
      enum playlist_sublabel_last_played_date_separator_type date_separator);
#endif

RETRO_END_DECLS

#endif
